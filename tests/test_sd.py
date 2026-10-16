import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
FW = ROOT / 'Projeto_Modulo_Portatil_StrainGauge/fw_pio'
HOST = ROOT / 'tests/host'


class SD(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = os.environ.get('SG6_HOST_CC') or shutil.which('gcc') or str(ROOT / '.tools/venv/Lib/site-packages/ziglang/zig.exe')
        if not pathlib.Path(cc).exists():
            raise RuntimeError('C compiler required: set SG6_HOST_CC')
        (ROOT / 'tmp').mkdir(exist_ok=True)
        directory = tempfile.TemporaryDirectory(prefix='sd-', dir=ROOT / 'tmp')
        assert pathlib.Path(directory.name).resolve().parent == (ROOT / 'tmp').resolve()
        cls.addClassCleanup(directory.cleanup)
        cls.exe = pathlib.Path(directory.name) / 'test_sd.exe'
        cmd = [cc] + (['cc'] if pathlib.Path(cc).stem == 'zig' else [])
        cmd += ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(HOST), '-I' + str(FW / 'include'), '-I' + str(FW / 'src'), str(HOST / 'sd_harness.c'), '-o', str(cls.exe)]
        env = os.environ.copy()
        env['ZIG_GLOBAL_CACHE_DIR'] = str(ROOT / '.tools/zig-cache')
        env['ZIG_LOCAL_CACHE_DIR'] = str(ROOT / 'tmp/zig-cache')
        result = subprocess.run(cmd, capture_output=True, text=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        if result.stderr:
            print(result.stderr)

    def run_case(self, name):
        result = subprocess.run([str(self.exe), name], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_sdsc_byte_address(self): self.run_case('sdsc')
    def test_sdhc_sector_address(self): self.run_case('sdhc')
    def test_tx_immutable(self): self.run_case('immutable')
    def test_tokens_busy_timeout(self): self.run_case('timeouts')
    def test_fast_recording_busy_69_70_71_ms_and_normal_deadlines(self): self.run_case('fast_busy')
    def test_busy_timeout_cause_is_separate_and_scoped_by_fatfs_caller(self): self.run_case('busy_cause')
    def test_fast_multisector_write_shares_70_ms_busy_budget(self): self.run_case('cumulative_busy')
    def test_ioctl_sizes(self): self.run_case('ioctl')
    def test_r7_ocr_validation(self): self.run_case('init')
    def test_hal_failures(self): self.run_case('hal')
    def test_invalid_parameters(self): self.run_case('parameters')
    def test_shared_spi_restored(self): self.run_case('shared')


if __name__ == '__main__':
    unittest.main()
