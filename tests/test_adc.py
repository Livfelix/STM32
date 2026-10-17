import os
import pathlib
import shutil
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
FW = ROOT / 'Projeto_Modulo_Portatil_StrainGauge/fw_pio'
HOST = ROOT / 'tests/host'


class ADC(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = os.environ.get('SG6_HOST_CC') or shutil.which('gcc') or str(ROOT / '.tools/venv/Lib/site-packages/ziglang/zig.exe')
        if not pathlib.Path(cc).exists():
            raise RuntimeError('C compiler required: set SG6_HOST_CC')
        cls.exe = ROOT / 'tmp/test_adc.exe'
        cls.exe.parent.mkdir(exist_ok=True)
        cls.legacy = 'ads_read_chan' in (FW / 'include/ads1256.h').read_text()
        cmd = [cc] + (['cc'] if pathlib.Path(cc).stem == 'zig' else [])
        cmd += ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(HOST), '-I' + str(FW / 'include'), '-DSG6_LEGACY=' + str(int(cls.legacy)), str(HOST / 'adc_harness.c'), '-o', str(cls.exe)]
        env = os.environ.copy()
        env['ZIG_GLOBAL_CACHE_DIR'] = str(ROOT / '.tools/zig-cache')
        env['ZIG_LOCAL_CACHE_DIR'] = str(ROOT / 'tmp/zig-cache')
        result = subprocess.run(cmd, capture_output=True, text=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def run_case(self, name):
        result = subprocess.run([str(self.exe), name], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_register_codes_and_spi(self): self.run_case('registers')
    def test_mux_drdy_sequence(self): self.run_case('mux')
    def test_signed_code_and_timeout(self): self.run_case('signed')
    def test_slow_calibration(self): self.run_case('cal')
    def test_clock_wrap(self): self.run_case('clock')
    def test_delay_counter_phase(self): self.run_case('phase')


if __name__ == '__main__':
    unittest.main()
