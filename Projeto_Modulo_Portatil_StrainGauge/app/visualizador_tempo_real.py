#!/usr/bin/env python3
"""SG-6CH: UART V1/V2 + streaming SGB -> atomic CSV (stdlib offline).

V2: AA56/seq u32/time us u64/6 values i32/excitation+battery u16/mask+flags u8/CRC16,46B.
V1: AA55/time ms u32/6 values i32/excitation u16/flags+battery20 u8,34B UART;
28B SD records, marker in ch0 (ambiguous), no CRC. Values are 0.1 microstrain.
UART is a temporal subset: normal frames <=1000/s; priority triggers bypass this limit.
SD keeps accepted scans.
Repeated whole #SG6BIN2..#ENDHDR headers describe configuration epochs.
Channels are sequential; time is relative to boot, FAT date is a placeholder.

python visualizador_tempo_real.py --port COM5 --log pc.csv
python visualizador_tempo_real.py --conv ENS0003.SGB -o ensaio.csv
python visualizador_tempo_real.py --conv ENS0003.SGB --event-time-us 10000000 --pre .5 --post 2
Live requires pyserial/matplotlib; PC log creation is exclusive (choose a new path).
Live defaults to V2 only; --legacy-v1 explicitly selects unchecked legacy UART.
"""
import argparse
import binascii
import csv
import math
import os
from pathlib import Path
import queue
import struct
import sys
import tempfile
import threading
import time
from collections import deque
from collections.abc import Iterable, Iterator

MAX_POINTS = 72000  # Bounded record history: normal <=1000/s plus priority triggers; a full 10+60s window is not guaranteed.
NCH = 6
PKT = struct.Struct("<2sI6iHBB")          # 34 B
REC = struct.Struct("<I6i")               # 28 B (arquivo .SGB)
INACT = 0x7FFFFFFF
V2 = struct.Struct('<2sIQ6iHHBBH')
FOOTER = struct.Struct('<2sIIQBH')
HEADER_LIMIT = 8192
LINE_LIMIT = 1024
CSV_FIELDS = ['seq', 't_us', 't_ms'] + [f'ch{i}_ue' for i in range(NCH)] + ['vexc_mv', 'battery_mv', 'mask', 'flags', 'trigger_mark', 'epoch']


def decode_v2(packet: bytes) -> dict:
    if len(packet) != V2.size or packet[:2] != b'\xaa\x56':
        raise ValueError('Registro V2 deve ter 46 bytes e AA56')
    p = V2.unpack(packet)
    if binascii.crc_hqx(packet[:44], 0xffff) != p[-1]:
        raise ValueError('CRC V2 invalido')
    mask, flags = p[11:13]
    if mask & ~0x3f:
        raise ValueError('Mask V2 invalida')
    return dict(seq=p[1], t_us=p[2], t_ms=p[2] / 1000,
                ch_ue=[v / 10 if mask & (1 << c) and v != INACT else None for c, v in enumerate(p[3:9])],
                vexc_mv=p[9], battery_mv=p[10], mask=mask, flags=flags,
                trigger_mark=int(bool(flags & 0x40)), epoch=0)


def decode_footer(data: bytes) -> dict:
    if len(data) != FOOTER.size or data[:2] != b'\xaa\x57':
        raise ValueError('Rodape BIN3 deve ter 21 bytes e AA57')
    _, count, last_seq, last_time, status, crc = FOOTER.unpack(data)
    if binascii.crc_hqx(data[:19], 0xffff) != crc:
        raise ValueError('CRC do rodape BIN3 invalido')
    if status not in (0, 1, 2, 3):
        raise ValueError('Status do rodape BIN3 invalido')
    return dict(count=count, last_seq=last_seq, last_time=last_time, status=status)


def parse_header(lines) -> dict:
    """Preserve every extensible line; derived units never replace raw values."""
    meta = dict(raw_lines=list(lines), channels={}, trigger={})
    try:
        for line in lines:
            words = line.strip().split()
            tag = words[0]
            fields = dict(w.split('=', 1) for w in words[1:] if '=' in w)
            if tag in ('#SG6BIN1', '#SG6BIN2', '#SG6BIN3'):
                if 'version' in meta: raise ValueError('Marcador de formato duplicado')
                meta.update(fields)
                meta['version'] = int(tag[-1])
            elif tag.startswith('#CH') and tag[3:].isdigit():
                ch = int(tag[3:])
                if not 0 <= ch < NCH: raise ValueError('Canal metadata invalido')
                meta['channels'][ch] = fields
                for k, v in list(fields.items()):
                    if k.endswith('_f32'):
                        value = struct.unpack('>f', bytes.fromhex(v))[0]
                        if not math.isfinite(value): raise ValueError('Calibracao metadata invalida')
                        fields[k[:-4]] = value
            elif tag == '#TRIGGER':
                meta['trigger'] = fields
                for k in ('enabled', 'channel', 'level_x10', 'pre_ms', 'post_ms', 'effective_fc_x10', 'analog_hz', 'analog_q_x1000'):
                    if k in fields: fields[k] = int(fields[k])
                for k, source, divisor in [('pre_s', 'pre_ms', 1000), ('post_s', 'post_ms', 1000), ('level_ue', 'level_x10', 10), ('effective_fc_hz', 'effective_fc_x10', 10)]:
                    if source in fields: fields[k] = fields[source] / divisor
                if fields.get('enabled', 0) not in (0, 1) or not 0 <= fields.get('channel', 0) < 6:
                    raise ValueError('Trigger metadata invalido')
                if not 0 <= fields.get('pre_s', .5) <= 10 or not .1 <= fields.get('post_s', 2) <= 60:
                    raise ValueError('Janela metadata invalida')
        if 'version' not in meta: raise ValueError('Formato SGB desconhecido')
        if meta['version'] >= 2:
            summary_keys = {'ch', 'pga', 'drate_x10', 'spsch_x10', 'fc_x10', 'avg', 'vexc_mv', 'bias_valid', 'zero_valid', 'cal_valid', 'time', 'scan', 'uart_max_sps', 'fat_date', 'unit', 'inactive', 'encoding'}
            channel_keys = {'type', 'gf_f32', 'gain_f32', 'bias_v_f32', 'zero_ratio_f32', 'scale_f32'}
            trigger_keys = {'channel', 'level_x10', 'pre_ms', 'post_ms', 'effective_fc_x10', 'analog_hz', 'analog_q_x1000', 'alias_protection'}
            if not summary_keys <= meta.keys() or set(meta['channels']) != set(range(NCH)) or not trigger_keys <= meta['trigger'].keys():
                raise ValueError('Header V2 incompleto: resumo/canais/trigger obrigatorios')
            for channel in meta['channels'].values():
                if not channel_keys <= channel.keys() or channel['type'] not in ('HALF_BENDING', 'QUARTER'):
                    raise ValueError('Header V2 incompleto/invalido: calibracao/tipo de canal')
            for key, expected in [('time', 'boot_relative_us'), ('scan', 'sequential'), ('unit', '0.1ue'), ('inactive', 'INT32_MAX'), ('encoding', 'IEEE754_HEX_F32')]:
                if meta[key] != expected: raise ValueError(f'Header V2 invalido: {key}')
            if lines[-1].strip() != '#ENDHDR': raise ValueError('Header V2 sem #ENDHDR')
        meta['mask'] = int(meta.get('ch', '3F'), 16)
        if meta['mask'] & ~0x3f: raise ValueError('Mask metadata invalida')
        for key, dest in [('drate_x10', 'drate_sps'), ('spsch_x10', 'spsch'), ('fc_x10', 'fc_hz')]:
            if key in meta: meta[dest] = int(meta[key]) / 10
        if meta['version'] == 1:
            meta['legacy_warning'] = 'V1: trigger ch0/zeros ambiguo com medicao; tempo32 ms com wrap irrecuperavel; produtor legado perde identidade de canais em masks esparsas; sem CRC.'
    except (IndexError, TypeError, struct.error, OverflowError) as e:
        raise ValueError('Metadata invalida') from e
    return meta


def _decode_v1(packet, metadata, uart=False):
    p = (PKT if uart else REC).unpack(packet)
    t, values = (p[1], p[2:8]) if uart else (p[0], p[1:7])
    # Original log_write writes the marker into ch0, never into timestamp bit31.
    mark = not uart and (values[0] & 0xffffffff) == (0x80000000 | t) and all(v == 0 for v in values[1:])
    mask = metadata.get('mask', 0x3f)
    return dict(seq=None, t_us=t * 1000, t_ms=t, ch_ue=[None if mark or v == INACT or not mask & (1 << c) else v / 10 for c, v in enumerate(values)],
                vexc_mv=p[8] if uart else None, battery_mv=p[10] * 20 if uart else None,
                flags=p[9] if uart else 0, mask=mask, trigger_mark=int(mark), epoch=0)


def read_sgb(path: str, recover_incomplete=False) -> tuple[dict, Iterator[dict]]:
    """Consume the iterator fully to validate BIN3 and finalize meta['clean_close']."""
    with open(path, 'rb') as f:
        first = f.readline(LINE_LIMIT + 1)
        if not first.endswith(b'\n') or len(first) > LINE_LIMIT: raise ValueError('Header ausente/excessivo')
        lines = [first.decode('ascii')]
        if first.startswith((b'#SG6BIN2 ', b'#SG6BIN3 ')):
            total = len(first)
            while True:
                line = f.readline(LINE_LIMIT + 1)
                total += len(line)
                if not line.startswith(b'#') or not line.endswith(b'\n') or len(line) > LINE_LIMIT or total > HEADER_LIMIT:
                    raise ValueError('Header V2 truncado/excessivo ou sem #ENDHDR')
                lines.append(line.decode('ascii'))
                if line.strip() == b'#ENDHDR': break
        elif not first.startswith(b'#SG6BIN1 '):
            raise ValueError('Formato SGB desconhecido')
        meta = parse_header(lines)
        offset = f.tell()
    if meta['version'] == 3: meta['clean_close'] = 0

    def records():
        with open(path, 'rb') as f:
            f.seek(offset)
            size = V2.size if meta['version'] >= 2 else REC.size
            count, last_seq, last_time = 0, 0xffffffff, 0
            damaged = False
            while True:
                prefix = f.read(2)
                is_footer = meta['version'] == 3 and prefix == b'\xaa\x57'
                packet = prefix + f.read((FOOTER.size if is_footer else size) - 2) if prefix else b''
                try:
                    if is_footer:
                        footer = decode_footer(packet)
                        if (footer['count'], footer['last_seq'], footer['last_time']) != (count, last_seq, last_time):
                            raise ValueError('Rodape BIN3 diverge da quantidade/ultimo seq/tempo')
                        if f.read(1): raise ValueError('Dados apos rodape BIN3')
                        meta['footer'] = footer
                        meta['clean_close'] = int(not damaged)
                        break
                    if not packet:
                        if meta['version'] == 3: raise ValueError('BIN3 incompleto: rodape ausente')
                        break
                    if len(packet) != size: raise ValueError('Registro truncado')
                    row = decode_v2(packet) if meta['version'] >= 2 else _decode_v1(packet, meta)
                except ValueError as e:
                    if not recover_incomplete or meta['version'] != 3: raise
                    damaged = True
                    meta['recovery_warning'] = str(e)
                    if is_footer or len(packet) != size: break
                    continue
                count += 1; last_seq, last_time = row['seq'], row['t_us']
                yield row
        if damaged:
            print('AVISO: recuperacao BIN3 clean_close=0; ' + meta['recovery_warning'], file=sys.stderr)
    return meta, records()


def extract_event_window(records: Iterable[dict], event_time_us: int, pre_s: float, post_s: float, *, epoch=None) -> list[dict]:
    if not all(math.isfinite(s) and s >= 0 for s in (pre_s, post_s)):
        raise ValueError('Janela deve ser finita e nao negativa')
    lo, hi = event_time_us - round(pre_s * 1e6), event_time_us + round(post_s * 1e6)
    selected = [r for r in records if lo <= r['t_us'] <= hi and (epoch is None or r.get('epoch', 0) == epoch)]
    if epoch is None and len({r.get('epoch', 0) for r in selected}) > 1:
        raise ValueError('Janela abrange epochs diferentes; selecione epoch')
    return selected


def _event_bounds(metadata, event_time_us, pre_s, post_s):
    trigger = metadata.get('trigger', {})
    pre = trigger.get('pre_s', .5) if pre_s is None else pre_s
    post = trigger.get('post_s', 2) if post_s is None else post_s
    extract_event_window([], event_time_us, pre, post)
    return event_time_us - round(pre * 1e6), event_time_us + round(post * 1e6)


def _csv_row(record):
    r = dict(record)
    r.update((f'ch{c}_ue', v) for c, v in enumerate(r.pop('ch_ue')))
    return [r.get(k) for k in CSV_FIELDS]


def sgb_to_csv(path_in, path_out, *, event_time_us=None, pre_s=None, post_s=None, recover_incomplete=False):
    source, target = Path(path_in).resolve(), Path(path_out).resolve()
    if source == target or (target.exists() and os.path.samefile(source, target)):
        raise ValueError('Entrada e saida devem ser arquivos diferentes')
    meta, records = read_sgb(source, recover_incomplete=recover_incomplete)
    bounds = _event_bounds(meta, event_time_us, pre_s, post_s) if event_time_us is not None else None
    temporary = None
    n = gaps = resets = incomplete = 0
    previous = 0xffffffff if meta['version'] >= 2 else None
    try:
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', newline='', dir=target.parent, prefix=target.name + '.', suffix='.tmp', delete=False) as o:
            temporary = o.name
            o.writelines(meta['raw_lines'])
            if 'legacy_warning' in meta: o.write('#PC_WARNING ' + meta['legacy_warning'] + '\n')
            o.write('#PC_CSV unit=ue inactive=empty time=boot_relative_us\n')
            w = csv.writer(o, delimiter=';'); w.writerow(CSV_FIELDS)
            for r in records:
                if previous is not None and r['seq'] is not None:
                    delta = (r['seq'] - previous - 1) & 0xffffffff
                    if delta < 0x80000000: gaps += delta
                    else: resets += 1
                previous = r['seq']
                incomplete += bool(r['flags'] & 0x80)
                if bounds is None or bounds[0] <= r['t_us'] <= bounds[1]:
                    w.writerow(_csv_row(r)); n += 1
            close_status = f" clean_close={meta['clean_close']}" if 'clean_close' in meta else ''
            if 'footer' in meta: close_status += f" stop_status={meta['footer']['status']}"
            if 'recovery_warning' in meta: o.write('#PC_WARNING clean_close=0 ' + meta['recovery_warning'] + '\n')
            o.write(f'#PC_STATUS sequence_gaps={gaps} sequence_resets={resets} incomplete_records={incomplete}{close_status}\n')
            o.flush(); os.fsync(o.fileno())
        os.replace(temporary, target)
    finally:
        records.close()
        if temporary and os.path.exists(temporary): os.unlink(temporary)
    if 'legacy_warning' in meta: print(meta['legacy_warning'], file=sys.stderr)
    print(f'{path_in} -> {path_out}: {n} registros; gaps={gaps}, resets={resets}, incompletos={incomplete}')


class StreamParser:
    """Incremental UART state; metadata commits only at a whole #ENDHDR."""
    def __init__(self, version=2):
        if version not in (1, 2): raise ValueError('Transporte deve ser V1 ou V2')
        self.version = version
        self.buffer = bytearray()
        self.metadata = {}
        self.header_lines = []
        self.epoch = 0
        self.previous_seq = None
        self.previous_time = None
        self.record_epoch = None
        self.boot_pending = False
        self.noise_bytes = self.crc_errors = self.header_errors = 0
        self.sequence_gaps = self.sequence_resets = self.incomplete_records = 0

    def feed(self, chunk):
        self.buffer.extend(chunk)
        events = []
        while self.buffer:
            if self.buffer[0] == ord('#'):
                end = self.buffer.find(b'\n')
                syncs = [i for sync in (b'\xaa\x55', b'\xaa\x56') if (i := self.buffer.find(sync)) >= 0]
                if syncs and (end < 0 or min(syncs) < end):
                    self.noise_bytes += min(syncs); self.header_errors += 1; self.header_lines = []
                    del self.buffer[:min(syncs)]; continue
                if end < 0 and len(self.buffer) <= LINE_LIMIT: break
                if end < 0 or end + 1 > LINE_LIMIT:
                    self.header_errors += 1; self.header_lines = []
                    del self.buffer[0]; continue
                raw = bytes(self.buffer[:end + 1]); del self.buffer[:end + 1]
                try:
                    line = raw.decode('ascii')
                    if line.strip().split()[:2] == ['#SG6', 'BOOT']:
                        self.boot_pending = True
                        self.header_lines = []
                        events.append(dict(kind='message', text=line))
                    elif line.startswith(('#SG6BIN2 ', '#SG6BIN1 ')):
                        self.header_lines = [line]
                        if line.startswith('#SG6BIN1 '):
                            self.metadata = parse_header(self.header_lines); self.epoch += 1; self.header_lines = []
                            self.version = 1
                            events.append(dict(kind='metadata', metadata=self.metadata, epoch=self.epoch))
                    elif self.header_lines:
                        self.header_lines.append(line)
                        if sum(len(s) for s in self.header_lines) > HEADER_LIMIT: raise ValueError('Header excessivo')
                        if line.strip() == '#ENDHDR':
                            m = parse_header(self.header_lines)
                            # Measured excitation is telemetry; all interpretation/extension fields remain identity.
                            def identity(meta):
                                return tuple(tuple(w for w in line.split() if not (line.startswith('#SG6BIN2 ') and w.startswith('vexc_mv='))) for line in meta.get('raw_lines', []))
                            if self.boot_pending or identity(m) != identity(self.metadata): self.epoch += 1
                            self.boot_pending = False
                            self.metadata = m
                            self.version = 2
                            self.header_lines = []
                            events.append(dict(kind='metadata', metadata=self.metadata, epoch=self.epoch))
                    else:
                        events.append(dict(kind='message', text=line))
                except ValueError:
                    self.header_errors += 1; self.header_lines = []
                continue
            if self.buffer[:2] == (b'\xaa\x56' if self.version == 2 else b'\xaa\x55'):
                size = V2.size if self.version == 2 else PKT.size
                if len(self.buffer) < size: break
                try:
                    packet = bytes(self.buffer[:size])
                    r = decode_v2(packet) if size == V2.size else _decode_v1(packet, self.metadata, True)
                except ValueError:
                    self.crc_errors += 1; del self.buffer[0]; continue
                del self.buffer[:size]
                if self.header_lines:
                    self.header_errors += 1; self.header_lines = []
                reset = self.previous_time is not None and r['t_us'] < self.previous_time
                if r['seq'] is not None:
                    if self.previous_seq is not None:
                        delta = (r['seq'] - self.previous_seq - 1) & 0xffffffff
                        if delta < 0x80000000: self.sequence_gaps += delta
                        else: self.sequence_resets += 1; reset = True
                    self.previous_seq = r['seq']
                if self.boot_pending or (reset and self.record_epoch == self.epoch):
                    self.epoch += 1
                    events.append(dict(kind='metadata', metadata=self.metadata, epoch=self.epoch))
                self.boot_pending = False
                self.previous_time = r['t_us']; self.record_epoch = self.epoch
                self.incomplete_records += bool(r['flags'] & 0x80)
                r.update(epoch=self.epoch, metadata=self.metadata)
                events.append(dict(kind='record', record=r))
                continue
            if len(self.buffer) == 1 and self.buffer[0] == 0xaa: break
            self.noise_bytes += 1; del self.buffer[0]
        return events


def _serial_reader(ser, parser, output_queue, stop, state, log_file=None):
    """Serial and PC disk writes run independently of matplotlib redraw."""
    state['plot_drops'] = 0
    try:
        writer = csv.writer(log_file, delimiter=';') if log_file else None
        if writer:
            log_file.write('#PC_LOG transport=UART temporal_subset max_sps=1000 max_sps_scope=normal_frames trigger_priority=bypass_1ms time=boot_relative_us scan=sequential metadata_before_header=unknown\n')
            writer.writerow(CSV_FIELDS)
        last_flush = time.monotonic()
        while not stop.is_set():
            chunk = ser.read(min(4096, ser.in_waiting or 1))
            for event in parser.feed(chunk):
                if writer:
                    if event['kind'] == 'metadata':
                        log_file.write(f"#PC_EPOCH epoch={event['epoch']}" + ('' if 'raw_lines' in event['metadata'] else ' metadata=unknown') + '\n')
                        log_file.writelines(event['metadata'].get('raw_lines', ()))
                        if 'legacy_warning' in event['metadata']: log_file.write('#PC_WARNING ' + event['metadata']['legacy_warning'] + '\n')
                    elif event['kind'] == 'record': writer.writerow(_csv_row(event['record']))
                    else: log_file.write(event['text'])
                try: output_queue.put_nowait(event)
                except queue.Full: state['plot_drops'] += 1
            if writer and time.monotonic() - last_flush >= 1:
                log_file.flush(); last_flush = time.monotonic()
        if writer:
            log_file.write(f'#PC_STATUS crc_errors={parser.crc_errors} sequence_gaps={parser.sequence_gaps} sequence_resets={parser.sequence_resets} plot_drops={state["plot_drops"]} incomplete_records={parser.incomplete_records} trailing_bytes={len(parser.buffer)} header_errors={parser.header_errors}\n')
            log_file.flush()
    except Exception as e:
        state['error'] = f'Leitura/log PC incompleto: {e}'
        stop.set()


def _plot_records(ax, lines, records, metadata, event_time_us=None, pre_s=None, post_s=None):
    if event_time_us is not None:
        lo, hi = _event_bounds(metadata, event_time_us, pre_s, post_s)
        records = extract_event_window(records, event_time_us, (event_time_us - lo) / 1e6, (hi - event_time_us) / 1e6)
    else: records = list(records)
    for c, line in enumerate(lines):
        kind = metadata.get('channels', {}).get(c, {}).get('type', '')
        label = 'flexao' if kind == 'HALF_BENDING' else 'local' if kind == 'QUARTER' else 'tipo desconhecido'
        line.set_label(f'CH{c} {label}')
        line.set_data([r['t_us'] / 1e6 for r in records], [float('nan') if r['ch_ue'][c] is None else r['ch_ue'][c] for r in records])
        line.set_visible(any(r['ch_ue'][c] is not None for r in records))
    visible = [l for l in lines if l.get_visible()]
    if visible: ax.legend(handles=visible, loc='upper right')
    elif ax.get_legend(): ax.get_legend().remove()
    uart_sps = (len(records) - 1) * 1e6 / (records[-1]['t_us'] - records[0]['t_us']) if len(records) > 1 and records[-1]['t_us'] > records[0]['t_us'] else 0
    ax.set_title(f'SG-6CH: UART {uart_sps:.1f} SPS (normais <=1000/s; trigger prioritario extra); SD prev {metadata.get("spsch", "?")} SPS/CH\nCanais sequenciais; tempo desde boot; H=flexao Q=local')
    ax.set_xlabel('t desde boot (s)'); ax.set_ylabel('deformacao (microstrain)')
    ax.relim(visible_only=True); ax.autoscale_view()


def live(args):
    import serial
    import matplotlib.pyplot as plt

    # Validate manual overrides before opening serial or PC log.
    _event_bounds({}, 0, args.pre, args.post)
    parser = StreamParser(version=1 if getattr(args, 'legacy_v1', False) else 2)
    output = queue.Queue(maxsize=4096)
    stop = threading.Event(); state = {}
    records = deque(maxlen=MAX_POINTS)
    event_time = args.event_time_us
    metadata = {}
    fig, ax = plt.subplots(figsize=(11, 6))
    lines = [ax.plot([], [], label=f'CH{i}')[0] for i in range(NCH)]
    ax.grid(True)
    status = fig.text(.01, .01, 'Aguardando header completo; UART e subconjunto do SD', fontsize=9)
    fig.subplots_adjust(bottom=.16)
    log_file = None
    worker = None
    plt.ion()
    try:
        if args.log: log_file = open(args.log, 'x', encoding='utf-8', newline='')
        with serial.Serial(args.port, args.baud, timeout=.1) as ser:
            print(f'Conectado em {args.port} @ {args.baud}; fechar janela/Ctrl+C encerra.')
            worker = threading.Thread(target=_serial_reader, args=(ser, parser, output, stop, state, log_file), daemon=True)
            worker.start()
            previous_status = None
            while plt.fignum_exists(fig.number) and not stop.is_set():
                for _ in range(output.maxsize):
                    try: e = output.get_nowait()
                    except queue.Empty: break
                    if e['kind'] == 'record':
                        r = e['record']
                        if records and (r['epoch'] != records[-1]['epoch'] or r['t_us'] < records[-1]['t_us']):
                            records.clear(); event_time = args.event_time_us
                        metadata = r['metadata']; records.append(r)
                        if r['trigger_mark'] and args.event_time_us is None: event_time = r['t_us']
                        pre = metadata.get('trigger', {}).get('pre_s', .5) if args.pre is None else args.pre
                        post = metadata.get('trigger', {}).get('post_s', 2) if args.post is None else args.post
                        # shortcut: live history holds72000 records; use SD/CLI for older event windows.
                        if event_time is None:
                            while records and records[0]['t_us'] < r['t_us'] - max(3, pre + post) * 1e6: records.popleft()
                    elif e['kind'] == 'metadata': metadata = e['metadata']
                    else: print(e['text'].rstrip())
                _plot_records(ax, lines, records, metadata, event_time, args.pre, args.post)
                text = f'CRC {parser.crc_errors} / saltos {parser.sequence_gaps} / resets {parser.sequence_resets} / ruido {parser.noise_bytes} / header {parser.header_errors} / incompletos {parser.incomplete_records} / fila descartou {state.get("plot_drops", 0)}'
                status.set_text(text)
                if text != previous_status:
                    print(text); previous_status = text
                plt.pause(.05)
            stop.set(); worker.join(timeout=2)
            if worker.is_alive(): raise RuntimeError('Leitor serial nao encerrou; log PC incompleto')
            if state.get('error'): raise RuntimeError(state['error'])
    finally:
        stop.set()
        if worker: worker.join(timeout=2)
        if log_file: log_file.close()
        plt.close(fig)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="SG-6CH tempo real / conversor")
    ap.add_argument("--port", help="porta serial do USB-TTL (P4)")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--log", help="CSV opcional gravado no PC")
    ap.add_argument("--conv", help="arquivo .SGB p/ converter")
    ap.add_argument("-o", "--out", help="CSV de saída do conversor")
    ap.add_argument('--event-time-us', type=int, help='extrair/mostrar janela no instante boot-relative us')
    ap.add_argument('--pre', type=float, help='pre-evento em s (padrao: metadata)')
    ap.add_argument('--post', type=float, help='pos-evento em s (padrao: metadata)')
    ap.add_argument('--legacy-v1', action='store_true', help='UART legado sem CRC (padrao: somente V2)')
    ap.add_argument('--recover-incomplete', action='store_true', help='recuperar registros validos de BIN3 incompleto com aviso clean_close=0')
    args = ap.parse_args()
    if args.conv:
        try:
            sgb_to_csv(args.conv, args.out or args.conv.rsplit(".", 1)[0] + ".csv", event_time_us=args.event_time_us, pre_s=args.pre, post_s=args.post, recover_incomplete=args.recover_incomplete)
        except (ValueError, OSError) as e:
            ap.exit(2, f'Erro: {e}\n')
    elif args.port:
        try:
            live(args)
        except KeyboardInterrupt:
            print("\nEncerrado.")
        except (ValueError, OSError, RuntimeError, ImportError) as e:
            ap.exit(2, f"Erro: {e}\n")
    else:
        ap.print_help()
        sys.exit(1)
