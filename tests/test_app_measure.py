import os
import pathlib
import shutil
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
FW = ROOT / 'Projeto_Modulo_Portatil_StrainGauge/fw_pio'


class AppMeasure(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = os.environ.get('SG6_HOST_CC') or shutil.which('gcc') or str(ROOT / '.tools/venv/Lib/site-packages/ziglang/zig.exe')
        cls.exe = ROOT / 'tmp/test_app_measure.exe'
        cls.exe.parent.mkdir(exist_ok=True)
        cmd = [cc] + (['cc'] if pathlib.Path(cc).stem == 'zig' else [])
        cmd += ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'tests/host'), '-I' + str(FW / 'include'), '-I' + str(FW / 'src'), str(ROOT / 'tests/host/app_measure_harness.c'), str(FW / 'src/sg_measure.c'), str(FW / 'src/sg_record.c'), '-lm', '-o', str(cls.exe)]
        env = os.environ.copy()
        env['ZIG_GLOBAL_CACHE_DIR'] = str(ROOT / '.tools/zig-cache')
        env['ZIG_LOCAL_CACHE_DIR'] = str(ROOT / 'tmp/zig-cache')
        result = subprocess.run(cmd, capture_output=True, text=True, env=env)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def test_bias_tare_shunt_invalidation_and_v2_transports(self):
        result = subprocess.run([str(self.exe)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
