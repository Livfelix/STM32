import contextlib
import binascii
import csv
import importlib.util
import io
import os
import pathlib
import queue
import struct
import subprocess
import sys
import tempfile
import threading
import tracemalloc
import unittest
from unittest import mock
from types import SimpleNamespace

ROOT = pathlib.Path(__file__).resolve().parents[1]
APP = ROOT / 'Projeto_Modulo_Portatil_StrainGauge/app/visualizador_tempo_real.py'
spec = importlib.util.spec_from_file_location('visualizador', APP)
sg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sg)
FIX = ROOT / 'tests/fixtures'
PACKET = (FIX / 'record_v2.bin').read_bytes()


def footer(count=1, seq=1, time=4294967297, status=0):
    data = struct.pack('<2sIIQB', b'\xaa\x57', count, seq, time, status)
    return data + struct.pack('<H', binascii.crc_hqx(data, 0xffff))


class Parser(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(dir=ROOT / 'tmp')
        self.addCleanup(self.directory.cleanup)
        self.path = pathlib.Path(self.directory.name)

    def sgb(self, body=PACKET, header=None):
        p = self.path / 'input.SGB'
        p.write_bytes((header or (FIX / 'header_v2.txt').read_bytes()) + body)
        return p

    def test_c_fixture_crc_units_inactive_and_64bit_time(self):
        self.assertTrue(hasattr(sg, 'decode_v2'), 'V2 decoder missing')
        r = sg.decode_v2(PACKET)
        self.assertEqual(r['t_us'], 4294967297)
        self.assertEqual(r['seq'], 1)
        self.assertEqual(r['ch_ue'], [-1234.5, None, None, 6789.0, None, None])
        self.assertEqual((r['vexc_mv'], r['battery_mv'], r['mask'], r['flags']), (5000, 3700, 9, 0x48))
        self.assertTrue(r['trigger_mark'])

    def v3(self, body):
        return self.sgb(body, (FIX / 'header_v2.txt').read_bytes().replace(b'BIN2', b'BIN3'))

    def test_header_rejects_every_second_format_marker(self):
        for first in (1, 2, 3):
            for second in (1, 2, 3):
                header = (FIX / 'header_v2.txt').read_text().replace('BIN2', f'BIN{first}')
                header = header.replace('#ENDHDR\n', f'#SG6BIN{second} \n#ENDHDR\n')
                with self.subTest(first=first, second=second), self.assertRaisesRegex(ValueError, 'Marcador de formato duplicado'):
                    sg.parse_header(header.splitlines(keepends=True))

    def test_v3_duplicate_marker_cannot_downgrade_read_or_conversion(self):
        header = (FIX / 'header_v2.txt').read_bytes().replace(b'BIN2', b'BIN3')
        out = self.path / 'out.csv'
        for second in (1, 2, 3):
            p = self.sgb(PACKET, header.replace(b'#ENDHDR\n', f'#SG6BIN{second} \n#ENDHDR\n'.encode()))
            for recover in (False, True):
                with self.subTest(second=second, recover=recover):
                    with self.assertRaisesRegex(ValueError, 'Marcador de formato duplicado'):
                        _, rows = sg.read_sgb(p, recover_incomplete=recover); list(rows)
                    out.write_text('KEEP')
                    with self.assertRaisesRegex(ValueError, 'Marcador de formato duplicado'):
                        sg.sgb_to_csv(p, out, recover_incomplete=recover)
                    self.assertEqual(out.read_text(), 'KEEP')
                    command = [sys.executable, '-S', str(APP), '--conv', str(p), '-o', str(out)]
                    if recover: command.append('--recover-incomplete')
                    result = subprocess.run(command, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertEqual(out.read_text(), 'KEEP')

    def test_footer_layout_status_crc_and_truncations(self):
        self.assertTrue(hasattr(sg, 'decode_footer'), 'Footer decoder missing')
        for status in range(4):
            self.assertEqual(sg.decode_footer(footer(status=status)), dict(count=1, last_seq=1, last_time=4294967297, status=status))
        for data in [footer()[:n] for n in range(21)] + [footer() + b'x', footer(status=4), b'\x00' + footer()[1:], footer()[:-1] + b'\x00']:
            with self.subTest(data=data), self.assertRaises(ValueError): sg.decode_footer(data)

    def test_v3_clean_close_and_empty_capture(self):
        for body, expected in [(PACKET + footer(status=3), 1), (footer(0, 0xffffffff, 0), 0)]:
            meta, rows = sg.read_sgb(self.v3(body))
            self.assertEqual(len(list(rows)), expected)
            self.assertEqual((meta['version'], meta['clean_close']), (3, 1))
            self.assertEqual(meta['footer']['status'], 3 if expected else 0)

    def test_v3_c_footer_fixture_and_bounded_streaming(self):
        meta, rows = sg.read_sgb(FIX / 'current_v3.SGB')
        self.assertEqual([r['seq'] for r in rows], [0, 1])
        self.assertEqual(meta['footer'], dict(count=2, last_seq=1, last_time=4294967297, status=2))
        self.assertEqual(meta['clean_close'], 1)
        p = self.v3(b'')
        with p.open('ab') as f:
            for _ in range(10000): f.write(PACKET)
            f.write(footer(10000))
        tracemalloc.start()
        try:
            meta, rows = sg.read_sgb(p)
            self.assertEqual(sum(1 for _ in rows), 10000)
            self.assertEqual(meta['clean_close'], 1)
            self.assertLess(tracemalloc.get_traced_memory()[1], 200000)
        finally: tracemalloc.stop()

    def test_v3_rejects_missing_bad_mismatched_or_trailing_footer(self):
        bodies = [PACKET, PACKET + footer()[:-1], PACKET + footer()[:-1] + b'\x00',
                  PACKET + footer(2), PACKET + footer(seq=2), PACKET + footer(time=42),
                  PACKET[:-1], PACKET + footer() + b'x', footer(0, 0, 0), footer(0, 0xffffffff, 1)]
        for body in bodies:
            with self.subTest(body=body), self.assertRaises(ValueError):
                _, rows = sg.read_sgb(self.v3(body)); list(rows)

    def test_v3_recovery_only_complete_valid_records_and_warning(self):
        bad = bytearray(PACKET); bad[19] ^= 1
        bodies = [PACKET, PACKET + footer()[:-1], PACKET + footer(2),
                  PACKET + footer()[:-1] + b'\x00', PACKET + PACKET[:20],
                  PACKET + bytes(bad) + PACKET]
        for body in bodies:
            with self.subTest(body=body), contextlib.redirect_stderr(io.StringIO()) as warning:
                meta, rows = sg.read_sgb(self.v3(body), recover_incomplete=True)
                self.assertEqual(len(list(rows)), 2 if body == bodies[-1] else 1)
                self.assertEqual(meta['clean_close'], 0)
                self.assertIn('clean_close=0', warning.getvalue())

    def test_v3_atomic_conversion_and_explicit_cli_recovery(self):
        p = self.v3(PACKET); out = self.path / 'out.csv'; out.write_text('KEEP')
        command = [sys.executable, '-S', str(APP), '--conv', str(p), '-o', str(out)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2); self.assertEqual(out.read_text(), 'KEEP')
        result = subprocess.run(command + ['--recover-incomplete'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('clean_close=0', result.stderr)
        self.assertIn('clean_close=0', out.read_text())
        self.assertIn('4294967297', out.read_text())
        p = self.v3(PACKET + footer())
        with contextlib.redirect_stdout(io.StringIO()): sg.sgb_to_csv(p, out)
        self.assertIn('clean_close=1', out.read_text())

    def test_recovery_does_not_hide_read_or_write_oserror(self):
        p = self.v3(PACKET)
        _, rows = sg.read_sgb(p, recover_incomplete=True)
        with mock.patch('builtins.open', side_effect=OSError('read failed')):
            with self.assertRaisesRegex(OSError, 'read failed'): list(rows)
        with mock.patch.object(sg.tempfile, 'NamedTemporaryFile', side_effect=OSError('disk full')):
            with self.assertRaisesRegex(OSError, 'disk full'):
                sg.sgb_to_csv(p, self.path / 'out.csv', recover_incomplete=True)

    def test_crc_and_every_truncation_fail(self):
        self.assertTrue(hasattr(sg, 'decode_v2'), 'V2 decoder missing')
        for n in range(46):
            with self.assertRaises(ValueError): sg.decode_v2(PACKET[:n])
        bad = bytearray(PACKET); bad[19] ^= 1
        with self.assertRaises(ValueError): sg.decode_v2(bad)

    def test_metadata_real_rates_calibration_trigger_and_extensions(self):
        self.assertTrue(hasattr(sg, 'read_sgb'), 'Streaming reader missing')
        header = (FIX / 'header_v2.txt').read_bytes().replace(b'#ENDHDR', b'#FUTURE note=retained\n#ENDHDR')
        m, records = sg.read_sgb(self.sgb(header=header))
        self.assertEqual((m['drate_sps'], m['spsch'], m['fc_hz']), (500, 205, 0))
        self.assertAlmostEqual(m['channels'][0]['gf'], 2.08, places=5)
        self.assertEqual(m['channels'][3]['type'], 'QUARTER')
        self.assertEqual(m['trigger']['enabled'], 0)
        self.assertEqual((m['trigger']['pre_s'], m['trigger']['post_s']), (.5, 2))
        self.assertTrue(any('#FUTURE' in s for s in m['raw_lines']))
        self.assertEqual(len(list(records)), 1)

    def test_offline_v2_rejects_missing_summary_channel_and_trigger_fields(self):
        h = (FIX / 'header_v2.txt').read_bytes()
        missing = [h.replace(b'ch=09 ', b''), h.replace(b'time=boot_relative_us ', b''),
                   h.replace(b'gain_f32=42C80000 ', b''), h.replace(b'channel=0 ', b''),
                   h.replace(b'alias_protection=unverified', b'')]
        missing += [b''.join(s for s in h.splitlines(keepends=True) if not s.startswith(tag))
                    for tag in (b'#CH4 ', b'#TRIGGER ')]
        for header in missing:
            with self.subTest(header=header), self.assertRaises(ValueError):
                sg.read_sgb(self.sgb(header=header))

    def test_v1_actual_producer_marker_not_timestamp_bit(self):
        self.assertTrue(hasattr(sg, 'read_sgb'), 'Legacy reader missing')
        m, records = sg.read_sgb(FIX / 'legacy_v1.SGB')
        rows = list(records)
        self.assertEqual(rows[0]['ch_ue'], [-1.2, 2.3, None, 4.5, None, None])
        self.assertEqual(rows[1]['t_us'], 1500000)
        self.assertTrue(rows[1]['trigger_mark'])
        self.assertEqual(rows[1]['ch_ue'], [None] * 6)
        self.assertFalse(rows[2]['trigger_mark'])
        self.assertIn('ambigu', m['legacy_warning'])
        self.assertEqual(rows[2]['t_us'], 0x80000001 * 1000)

    def test_atomic_conversion_preserves_existing_output_on_corruption(self):
        p = self.sgb(PACKET[:-1]); out = self.path / 'out.csv'; out.write_text('KEEP')
        with self.assertRaises(ValueError): sg.sgb_to_csv(p, out)
        self.assertEqual(out.read_text(), 'KEEP')
        self.assertEqual(sorted(x.name for x in self.path.iterdir()), ['input.SGB', 'out.csv'])

    def test_same_input_output_rejected(self):
        p = self.sgb(); before = p.read_bytes()
        with self.assertRaises(ValueError): sg.sgb_to_csv(p, p)
        self.assertEqual(p.read_bytes(), before)

    def test_csv_cross_language_contract(self):
        p = self.sgb(); out = self.path / 'out.csv'
        with contextlib.redirect_stdout(io.StringIO()): sg.sgb_to_csv(p, out)
        rows = list(csv.DictReader((s for s in out.read_text().splitlines() if not s.startswith('#')), delimiter=';'))
        self.assertEqual(rows[0]['t_us'], '4294967297')
        self.assertEqual(rows[0]['ch0_ue'], '-1234.5')
        self.assertEqual(rows[0]['ch1_ue'], '')
        self.assertEqual(rows[0]['trigger_mark'], '1')

    def test_file_sequence_starts_zero_reports_initial_loss_and_reset(self):
        p = self.sgb(frame(1) + frame(4) + frame(0)); out = self.path / 'out.csv'
        with contextlib.redirect_stdout(io.StringIO()): sg.sgb_to_csv(p, out)
        self.assertIn('sequence_gaps=3 sequence_resets=1', out.read_text())

    def test_streaming_long_file_bounded_memory(self):
        self.assertTrue(hasattr(sg, 'read_sgb'), 'Streaming reader missing')
        p = self.sgb()
        with p.open('ab') as f:
            for _ in range(10000): f.write(PACKET)
        tracemalloc.start()
        try:
            _, records = sg.read_sgb(p)
            self.assertIs(iter(records), records)
            count = sum(1 for _ in records)
            self.assertEqual(count, 10001)
            self.assertLess(tracemalloc.get_traced_memory()[1], 200000)
        finally: tracemalloc.stop()

    def test_header_limit_missing_terminator_and_partial_record(self):
        self.assertTrue(hasattr(sg, 'read_sgb'), 'Streaming reader missing')
        for header in (b'#SG6BIN2 ' + b'a' * 9000 + b'\n', b'#SG6BIN2 ch=09\n', b'#WRONG\n'):
            with self.assertRaises(ValueError): sg.read_sgb(self.sgb(b'', header))
        for body in (PACKET[:-1], PACKET + b'a'):
            _, rows = sg.read_sgb(self.sgb(body))
            with self.assertRaises(ValueError): list(rows)

    def test_event_window_inclusive_and_only_selected_materialized(self):
        self.assertTrue(hasattr(sg, 'extract_event_window'), 'Event window missing')
        rows = ({'t_us': t} for t in [9499999, 9500000, 10000000, 12000000, 12000001])
        self.assertEqual([r['t_us'] for r in sg.extract_event_window(rows, 10000000, .5, 2)], [9500000, 10000000, 12000000])
        with self.assertRaises(ValueError): sg.extract_event_window([], 0, -1, 2)

    def test_cli_event_defaults_from_metadata_and_offline_optional_imports(self):
        p = self.sgb(); out = self.path / 'out.csv'
        result = subprocess.run([sys.executable, '-S', str(APP), '--conv', str(p), '-o', str(out), '--event-time-us', '4294967297'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('4294967297', out.read_text())

    def test_cli_uses_actual_windows_overrides_and_validates_outside_selection(self):
        header = (FIX / 'header_v2.txt').read_bytes().replace(b'pre_ms=500', b'pre_ms=1000').replace(b'post_ms=2000', b'post_ms=2500')
        body = []
        for i, t in enumerate([9000000, 9500000, 10000000, 12000000, 12500000]):
            b = bytearray(frame(i)); struct.pack_into('<Q', b, 6, t)
            struct.pack_into('<H', b, 44, binascii.crc_hqx(b[:44], 0xffff)); body.append(bytes(b))
        p = self.sgb(b''.join(body), header); out = self.path / 'out.csv'
        command = [sys.executable, '-S', str(APP), '--conv', str(p), '-o', str(out), '--event-time-us', '10000000']
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        def times():
            return [int(r['t_us']) for r in csv.DictReader((s for s in out.read_text().splitlines() if not s.startswith('#')), delimiter=';')]
        self.assertEqual(times(), [9000000, 9500000, 10000000, 12000000, 12500000])
        result = subprocess.run(command + ['--pre', '.5', '--post', '2'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(times(), [9500000, 10000000, 12000000])
        previous = out.read_bytes(); p.write_bytes(p.read_bytes() + b'broken')
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn('truncado', result.stderr)
        self.assertEqual(out.read_bytes(), previous)


def frame(seq):
    b = bytearray(PACKET)
    struct.pack_into('<I', b, 2, seq)
    struct.pack_into('<H', b, 44, binascii.crc_hqx(b[:44], 0xffff))
    return bytes(b)


class LiveParser(unittest.TestCase):
    def timed(self, seq, t):
        b = bytearray(frame(seq)); struct.pack_into('<Q', b, 6, t)
        struct.pack_into('<H', b, 44, binascii.crc_hqx(b[:44], 0xffff))
        return bytes(b)

    def test_telemetry_keeps_epoch_and_immutable_metadata(self):
        h = (FIX / 'header_v2.txt').read_bytes(); p = self.parser()
        a = p.feed(h + self.timed(10, 1000000))[-1]['record']
        b = p.feed(h.replace(b'vexc_mv=5000', b'vexc_mv=4999') + self.timed(11, 1100000))[-1]['record']
        self.assertEqual(a['epoch'], b['epoch'])
        self.assertEqual(a['metadata']['vexc_mv'], '5000')
        self.assertEqual(b['metadata']['vexc_mv'], '4999')
        self.assertIsNot(a['metadata'], b['metadata'])
        c = p.feed(h.replace(b'scale_f32=3F800000', b'scale_f32=40000000') + self.timed(12, 1200000))[-1]['record']
        self.assertGreater(c['epoch'], b['epoch'])

    def test_boot_reset_time_and_sequence_boundaries_with_wrap_continuous(self):
        h = (FIX / 'header_v2.txt').read_bytes()
        for boundary in (b'#SG6 BOOT\n' + h, b'#SG6 BOOT\n', b''):
            p = self.parser(); a = p.feed(h + self.timed(100, 10000000))[-1]['record']
            events = p.feed(boundary + self.timed(0, 1000000)); b = events[-1]['record']
            self.assertGreater(b['epoch'], a['epoch'])
            self.assertTrue(any(e['kind'] == 'metadata' and e['epoch'] == b['epoch'] for e in events[:-1]))
            self.assertEqual(p.sequence_resets, 1)
        for seq, t in ((101, 1000000), (0, 11000000)):
            p = self.parser(); a = p.feed(h + self.timed(100, 10000000))[-1]['record']
            self.assertGreater(p.feed(self.timed(seq, t))[-1]['record']['epoch'], a['epoch'])
        p = self.parser(); a = p.feed(h + self.timed(0xffffffff, 10000000))[-1]['record']
        b = p.feed(self.timed(0, 11000000))[-1]['record']
        self.assertEqual((a['epoch'], p.sequence_resets), (b['epoch'], 0))
        p = self.parser(); a = p.feed(h + self.timed(0xffffffff, 1000000))[-1]['record']
        events = p.feed(h.splitlines(keepends=True)[0] + b'#SG6 BOOT\n' + h + self.timed(0, 2000000))
        self.assertGreater(events[-1]['record']['epoch'], a['epoch'])

    def test_event_selection_requires_epoch_for_combined_boots(self):
        rows = [{'t_us': 1000000, 'epoch': e} for e in (1, 2)]
        with self.assertRaises(ValueError): sg.extract_event_window(rows, 1000000, .5, 2)
        self.assertEqual(sg.extract_event_window(rows, 1000000, .5, 2, epoch=2), [rows[1]])

    def test_reader_logs_boundary_metadata_before_first_reset_row(self):
        stop = threading.Event(); log = io.StringIO(); h = (FIX / 'header_v2.txt').read_bytes()
        chunks = iter([h + self.timed(100, 10000000), self.timed(0, 1000000), b''])
        class Port:
            in_waiting = 4096
            def read(self, size):
                b = next(chunks)
                if not b: stop.set()
                return b
        sg._serial_reader(Port(), self.parser(), queue.Queue(20), stop, {}, log)
        text = log.getvalue(); self.assertIn('#PC_EPOCH epoch=2', text)
        self.assertLess(text.index('#PC_EPOCH epoch=2'), text.index('0;1000000;'))
        self.assertIn('#ENDHDR', text[text.index('#PC_EPOCH epoch=2'):text.index('0;1000000;')])

    def unknown_boundary(self, initial):
        stop = threading.Event(); state = {}; log = io.StringIO(); q = queue.Queue(20)
        header = (FIX / 'header_v2.txt').read_bytes()
        chunks = iter([initial, header + self.timed(1, 1100000), b''])
        class Port:
            in_waiting = 4096
            def read(self, size):
                b = next(chunks)
                if not b: stop.set()
                return b
        parser = self.parser()
        sg._serial_reader(Port(), parser, q, stop, state, log)
        self.assertNotIn('error', state)
        text = log.getvalue()
        rows = list(csv.DictReader((s for s in text.splitlines() if not s.startswith('#')), delimiter=';'))
        boundary = next(r for r in rows if r['seq'] == '0')
        self.assertEqual(boundary['epoch'], '1')
        self.assertEqual(rows[-1]['seq'], '1')
        self.assertEqual(rows[-1]['epoch'], '2')
        self.assertLess(text.index('#PC_EPOCH epoch=1 metadata=unknown'), text.index('0;1000000;'))
        self.assertNotIn('#SG6BIN2', text[:text.index('0;1000000;')])
        self.assertLess(text.index('#PC_EPOCH epoch=2'), text.index('1;1100000;'))
        records = [e['record'] for e in list(q.queue) if e['kind'] == 'record']
        self.assertEqual(next(r['metadata'] for r in records if r['seq'] == 0), {})
        self.assertEqual(records[-1]['metadata']['version'], 2)
        return parser, rows

    def test_reader_boot_before_header_keeps_unknown_epoch_and_recovers(self):
        _, rows = self.unknown_boundary(b'#SG6 BOOT\n' + self.timed(0, 1000000))
        self.assertEqual([r['seq'] for r in rows], ['0', '1'])

    def test_reader_implicit_reset_before_header_keeps_unknown_epoch_and_recovers(self):
        parser, rows = self.unknown_boundary(self.timed(100, 10000000) + self.timed(0, 1000000))
        self.assertEqual([r['epoch'] for r in rows], ['0', '1', '2'])
        self.assertEqual(parser.sequence_resets, 1)

    def test_reader_incomplete_initial_header_boot_keeps_unknown_epoch_and_recovers(self):
        parser, _ = self.unknown_boundary(b'#SG6BIN2 ch=09\n#ENDHDR\n#SG6 BOOT\n' + self.timed(0, 1000000))
        self.assertEqual(parser.header_errors, 1)

    def test_reader_boundary_disk_error_is_still_explicit(self):
        stop = threading.Event(); state = {}
        class Port:
            in_waiting = 4096
            def read(self, size): return b'#SG6 BOOT\n' + self_outer.timed(0, 1000000)
        class Broken(io.StringIO):
            def write(self, text):
                if text.startswith('#PC_EPOCH'): raise OSError('disk full at boundary')
                return super().write(text)
        self_outer = self
        sg._serial_reader(Port(), self.parser(), queue.Queue(20), stop, state, Broken())
        self.assertTrue(stop.is_set())
        self.assertIn('disk full at boundary', state['error'])

    def parser(self):
        self.assertTrue(hasattr(sg, 'StreamParser'), 'Live stream parser missing')
        return sg.StreamParser()

    def test_all_packet_fragment_boundaries_and_noise(self):
        for cut in range(1, 46):
            p = self.parser()
            events = p.feed(b'noise' + PACKET[:cut]) + p.feed(PACKET[cut:])
            rows = [e['record'] for e in events if e['kind'] == 'record']
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0]['t_us'], 4294967297)
            self.assertEqual(p.noise_bytes, 5)

    def test_corrupt_frame_resync_and_sequence_gap(self):
        p = self.parser(); bad = bytearray(PACKET); bad[30] ^= 1
        events = p.feed(frame(1) + bytes(bad) + frame(4))
        self.assertEqual(len([e for e in events if e['kind'] == 'record']), 2)
        self.assertEqual(p.crc_errors, 1)
        self.assertEqual(p.sequence_gaps, 2)
        p.feed(frame(0xffffffff) + frame(0))
        self.assertEqual(p.sequence_resets, 1)

    def test_rejected_v2_embedded_legacy_sync_never_becomes_measurement(self):
        bad = bytearray(PACKET); bad[10:12] = b'\xaa\x55'
        for header in (b'', (FIX / 'header_v2.txt').read_bytes()):
            for cut in range(1, 46):
                with self.subTest(header=bool(header), cut=cut):
                    p = self.parser(); p.feed(header)
                    events = p.feed(bad[:cut]) + p.feed(bytes(bad[cut:]) + PACKET)
                    rows = [e['record'] for e in events if e['kind'] == 'record']
                    self.assertEqual([r['seq'] for r in rows], [1])
                    self.assertEqual(rows[0]['t_us'], 4294967297)
                    self.assertEqual(p.crc_errors, 1)

    def test_incomplete_v2_header_retains_complete_snapshot_and_epoch(self):
        h = (FIX / 'header_v2.txt').read_bytes()
        partials = [h.splitlines(keepends=True)[0] + b'#ENDHDR\n']
        partials += [b''.join(s for s in h.splitlines(keepends=True) if not s.startswith(tag))
                     for tag in (b'#CH2 ', b'#TRIGGER ')]
        for header in partials:
            with self.subTest(header=header):
                p = self.parser(); p.feed(h); previous = p.metadata
                events = p.feed(header + PACKET)
                self.assertEqual(p.header_errors, 1)
                self.assertEqual(p.epoch, 1)
                self.assertIs(p.metadata, previous)
                self.assertEqual(events[-1]['record']['metadata']['channels'][2]['type'], 'HALF_BENDING')
                self.assertEqual(events[-1]['record']['epoch'], 1)

    def test_complete_v2_old_enabled_optional_unknown_extensions_preserved(self):
        h = (FIX / 'header_v2.txt').read_bytes().replace(b'enabled=0 ', b'').replace(b'#ENDHDR', b'#FUTURE new=ok\n#ENDHDR')
        p = self.parser(); events = p.feed(h + PACKET)
        self.assertEqual(p.header_errors, 0)
        self.assertEqual(p.epoch, 1)
        self.assertNotIn('enabled', events[-1]['record']['metadata']['trigger'])
        self.assertIn('#FUTURE new=ok\n', p.metadata['raw_lines'])

    def test_late_attach_bytewise_whole_repeated_headers_epochs(self):
        p = self.parser()
        h = (FIX / 'header_v2.txt').read_bytes()
        stream = b'middleofpacket#CH2 type=QUARTER\n' + PACKET + h + frame(2) + h + frame(3)
        events = []
        for b in stream: events += p.feed(bytes([b]))
        rows = [e['record'] for e in events if e['kind'] == 'record']
        self.assertEqual([r['epoch'] for r in rows], [0, 1, 1])
        self.assertEqual(rows[0]['metadata'], {})
        self.assertEqual(rows[-1]['metadata']['trigger']['enabled'], 0)
        changed = h.replace(b'enabled=0', b'enabled=1').replace(b'pre_ms=500', b'pre_ms=1200')
        e = p.feed(changed + frame(4))[-1]
        self.assertEqual(e['record']['epoch'], 2)
        self.assertEqual(e['record']['metadata']['trigger']['pre_s'], 1.2)

    def test_bounded_incomplete_header_and_recovery(self):
        p = self.parser()
        p.feed(b'#SG6BIN2 ' + b'x' * 30000)
        self.assertLessEqual(len(p.buffer), sg.HEADER_LIMIT)
        self.assertGreater(p.header_errors, 0)
        events = p.feed((FIX / 'header_v2.txt').read_bytes() + PACKET)
        self.assertEqual(events[-1]['record']['epoch'], 1)

    def test_unterminated_noise_text_cannot_swallow_good_frame(self):
        p = self.parser()
        rows = p.feed(b'#garbage-no-newline' + PACKET)
        self.assertEqual(len([e for e in rows if e['kind'] == 'record']), 1)

    def test_v1_uart_units_inactive_battery(self):
        p = sg.StreamParser(version=1)
        b = sg.PKT.pack(b'\xaa\x55', 1200, -12, sg.INACT, 0, 45, sg.INACT, sg.INACT, 5000, 8, 185)
        rows = p.feed(b)
        self.assertEqual(rows[0]['record']['ch_ue'][0], -1.2)
        self.assertIsNone(rows[0]['record']['ch_ue'][1])
        self.assertEqual(rows[0]['record']['battery_mv'], 3700)

    def test_established_transport_rejects_other_frame_version(self):
        legacy = sg.PKT.pack(b'\xaa\x55', 1200, -12, sg.INACT, 0, 45, sg.INACT, sg.INACT, 5000, 8, 185)
        p = self.parser()
        self.assertEqual([e for e in p.feed(legacy) if e['kind'] == 'record'], [])
        p = sg.StreamParser(version=1)
        self.assertEqual([e for e in p.feed(PACKET) if e['kind'] == 'record'], [])
        h = (FIX / 'legacy_v1.SGB').read_bytes().split(b'\n', 1)[0] + b'\n'
        p = self.parser(); events = p.feed(h + legacy)
        self.assertEqual(events[-1]['record']['metadata']['version'], 1)
        self.assertEqual(events[-1]['record']['ch_ue'][0], -1.2)
        p.feed((FIX / 'header_v2.txt').read_bytes())
        self.assertEqual([e for e in p.feed(legacy) if e['kind'] == 'record'], [])

    def test_reader_logs_all_before_bounded_plot_queue_drop(self):
        self.assertTrue(hasattr(sg, '_serial_reader'), 'Serial worker missing')
        stop = threading.Event(); state = {}; q = queue.Queue(maxsize=1); log = io.StringIO()
        chunks = iter([(FIX / 'header_v2.txt').read_bytes() + frame(1) + frame(2), frame(3), b''])
        class Port:
            in_waiting = 4096
            def read(self, size):
                self.last_size = size
                b = next(chunks)
                if not b: stop.set()
                return b
        port = Port()
        sg._serial_reader(port, self.parser(), q, stop, state, log)
        self.assertLessEqual(port.last_size, 4096)
        rows = list(csv.DictReader((s for s in log.getvalue().splitlines() if not s.startswith('#')), delimiter=';'))
        self.assertEqual([r['seq'] for r in rows], ['1', '2', '3'])
        self.assertGreater(state['plot_drops'], 0)
        self.assertIn('#PC_EPOCH', log.getvalue())
        self.assertIn('plot_drops=', log.getvalue())

    def test_log_write_failure_is_explicit_and_stops_reader(self):
        self.assertTrue(hasattr(sg, '_serial_reader'), 'Serial worker missing')
        stop = threading.Event(); state = {}
        class Port:
            in_waiting = 46
            def read(self, size): return PACKET
        class Broken:
            def write(self, text): raise OSError('disk full')
        sg._serial_reader(Port(), self.parser(), queue.Queue(1), stop, state, Broken())
        self.assertTrue(stop.is_set())
        self.assertIn('disk full', state['error'])


class Plot(unittest.TestCase):
    def test_actual_live_preserves_event_history_on_telemetry_and_separates_boot(self):
        os.environ['MPLCONFIGDIR'] = str(ROOT / 'tmp/matplotlib')
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        import time
        h = (FIX / 'header_v2.txt').read_bytes(); helper = LiveParser()
        initial = h + helper.timed(10, 1000000) + h.replace(b'vexc_mv=5000', b'vexc_mv=4999') + helper.timed(11, 1100000)
        for stream, count in ((initial, 2), (initial + b'#SG6 BOOT\n' + h + helper.timed(0, 100000), 1)):
            captures = []; calls = []; chunks = iter([stream])
            class Port:
                in_waiting = 4096
                def __enter__(self): return self
                def __exit__(self, *args): pass
                def read(self, size): time.sleep(.001); return next(chunks, b'')
            original = sg._plot_records
            def plot(ax, lines, records, metadata, event_time, pre, post):
                if records: captures.append((list(records), event_time))
                original(ax, lines, records, metadata, event_time, pre, post)
            def pause(seconds):
                time.sleep(.02); calls.append(seconds)
                if len(calls) == 4: plt.close('all')
            args = SimpleNamespace(port='FAKE', baud=921600, log=None, event_time_us=None, pre=None, post=None)
            with mock.patch('serial.Serial', return_value=Port()), mock.patch.object(plt, 'pause', side_effect=pause), mock.patch.object(sg, '_plot_records', side_effect=plot), contextlib.redirect_stdout(io.StringIO()):
                sg.live(args)
            self.assertTrue(captures)
            rows, event_time = captures[-1]
            self.assertEqual(len(rows), count)
            self.assertEqual(len({r['epoch'] for r in rows}), 1)
            self.assertEqual(event_time, rows[-1]['t_us'])

    def test_actual_agg_plot_omits_inactive_uses_types_and_metadata_window(self):
        self.assertTrue(hasattr(sg, '_plot_records'), 'Live plot function missing')
        os.environ['MPLCONFIGDIR'] = str(ROOT / 'tmp/matplotlib')
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        fig, ax = plt.subplots(figsize=(11, 6)); self.addCleanup(plt.close, fig)
        lines = [ax.plot([], [], label=f'CH{i}')[0] for i in range(6)]
        metadata, records = sg.read_sgb(FIX / 'current_v2.SGB')
        r = next(records); records.close()
        rows = [dict(r, t_us=t) for t in [9499999, 9500000, 10000000, 12000000, 12000001]]
        sg._plot_records(ax, lines, rows, metadata, 10000000)
        self.assertEqual(list(lines[0].get_xdata()), [9.5, 10, 12])
        self.assertFalse(lines[1].get_visible())
        self.assertIn('flex', lines[0].get_label())
        self.assertIn('local', lines[3].get_label())
        self.assertIn('sequenciais', ax.get_title())
        self.assertIn('UART', ax.get_title())
        self.assertIn('normais <=1000/s', ax.get_title())
        self.assertIn('trigger prioritario extra', ax.get_title())
        fig.tight_layout(); fig.savefig(ROOT / 'tmp/task6-live-event.png')

    def test_live_entrypoint_reader_thread_logging_and_window_cleanup(self):
        os.environ['MPLCONFIGDIR'] = str(ROOT / 'tmp/matplotlib')
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        import time
        with tempfile.TemporaryDirectory(dir=ROOT / 'tmp') as d:
            log = pathlib.Path(d) / 'pc.csv'; thread_ids = []
            class Port:
                in_waiting = 4096
                chunks = iter([(FIX / 'header_v2.txt').read_bytes() + frame(1), frame(2)])
                closed = False
                def __enter__(self): return self
                def __exit__(self, *args): self.closed = True
                def read(self, size):
                    thread_ids.append(threading.get_ident()); time.sleep(.001)
                    return next(self.chunks, b'')
            port = Port(); pauses = []
            def pause(seconds):
                time.sleep(.01); pauses.append(seconds)
                if len(pauses) == 3: plt.close('all')
            args = SimpleNamespace(port='FAKE', baud=921600, log=str(log), event_time_us=None, pre=None, post=None)
            with mock.patch('serial.Serial', return_value=port), mock.patch.object(plt, 'pause', side_effect=pause), contextlib.redirect_stdout(io.StringIO()):
                sg.live(args)
            self.assertTrue(port.closed)
            self.assertTrue(thread_ids)
            self.assertNotIn(threading.get_ident(), thread_ids)
            self.assertIn('#PC_STATUS', log.read_text())
            self.assertIn('max_sps=1000 max_sps_scope=normal_frames trigger_priority=bypass_1ms', log.read_text())
            self.assertIn('4294967297', log.read_text())
            self.assertFalse(plt.get_fignums())


if __name__ == '__main__': unittest.main()
