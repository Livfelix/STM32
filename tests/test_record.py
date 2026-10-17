import os
import pathlib
import shutil
import subprocess
import struct
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
FW = ROOT / 'Projeto_Modulo_Portatil_StrainGauge/fw_pio'


class Record(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = os.environ.get('SG6_HOST_CC') or shutil.which('gcc') or str(ROOT / '.tools/venv/Lib/site-packages/ziglang/zig.exe')
        cls.exe = ROOT / 'tmp/test_record.exe'
        cls.exe.parent.mkdir(exist_ok=True)
        cmd = [cc] + (['cc'] if pathlib.Path(cc).stem == 'zig' else [])
        cmd += ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(FW / 'include'), str(ROOT / 'tests/host/record_harness.c'), str(FW / 'src/sg_record.c'), '-o', str(cls.exe)]
        env = os.environ.copy()
        env['ZIG_GLOBAL_CACHE_DIR'] = str(ROOT / '.tools/zig-cache')
        env['ZIG_LOCAL_CACHE_DIR'] = str(ROOT / 'tmp/zig-cache')
        result = subprocess.run(cmd, capture_output=True, text=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def test_v2_crc_and_cross_language_fixture(self):
        fixture = ROOT / 'tests/fixtures/record_v2.bin'
        fixture.parent.mkdir(exist_ok=True)
        result = subprocess.run([str(self.exe), str(fixture)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        data = fixture.read_bytes()
        self.assertEqual(len(data), 46)
        self.assertEqual(struct.unpack('<2sIQ6iHHBBH', data)[:-1],
                         (b'\xaa\x56', 1, 0x100000001, -12345, 2147483647, 2147483647, 67890, 2147483647, 2147483647, 5000, 3700, 0x09, 0x48))
        # Hand-checked CRC for this exact wire fixture, independent of C.
        self.assertEqual(data[-2:], b'\x9c\x93')

    def test_footer_layout_and_crc_independent_of_c(self):
        import binascii
        footer = ROOT / 'tmp/footer.bin'
        result = subprocess.run([str(self.exe), str(ROOT / 'tmp/record.bin'), str(footer)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        data = footer.read_bytes()
        self.assertEqual(struct.unpack('<2sIIQBH', data)[:-1], (b'\xaa\x57', 2, 1, 4294967297, 2))
        self.assertEqual(struct.unpack('<H', data[-2:])[0], binascii.crc_hqx(data[:19], 0xffff))


if __name__ == '__main__':
    unittest.main()
