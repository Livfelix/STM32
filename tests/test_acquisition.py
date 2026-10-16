from test_app_measure import ROOT, FW
import os, pathlib, shutil, subprocess, tempfile, unittest


class Acquisition(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = os.environ.get('SG6_HOST_CC') or shutil.which('gcc') or str(ROOT / '.tools/venv/Lib/site-packages/ziglang/zig.exe')
        directory = tempfile.TemporaryDirectory(prefix='acquisition-', dir=ROOT/'tmp')
        cls.addClassCleanup(directory.cleanup)
        cls.exe = pathlib.Path(directory.name)/'test.exe'
        cmd = [cc] + (['cc'] if pathlib.Path(cc).stem == 'zig' else [])
        cmd += ['-std=c11', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT/'tests/host'), '-I'+str(FW/'include'), '-I'+str(FW/'src'), str(ROOT/'tests/host/acquisition_harness.c'), str(FW/'src/sg_measure.c'), str(FW/'src/sg_record.c'), '-lm', '-o', str(cls.exe)]
        env = os.environ.copy()
        env['ZIG_GLOBAL_CACHE_DIR'] = str(ROOT/'.tools/zig-cache')
        env['ZIG_LOCAL_CACHE_DIR'] = str(ROOT/'tmp/zig-cache')
        result = subprocess.run(cmd, capture_output=True, text=True, env=env)
        if result.returncode: raise RuntimeError(result.stdout+result.stderr)

    def run_case(self, case):
        result = subprocess.run([str(self.exe), case], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)

    def test_mask_moving_average_and_trigger(self): self.run_case('pipeline')
    def test_isr_queue_overflow_drains_accepted(self): self.run_case('overflow')
    def test_sd_all_uart_rate_and_atomic_queue(self): self.run_case('rate')
    def test_trigger_bypasses_uart_gate_and_preserves_frame_order(self): self.run_case('trigger_gate')
    def test_normal_frames_leave_final_record_slot_for_trigger(self): self.run_case('trigger_reserve')
    def test_full_uart_rejects_trigger_without_advancing_sequence(self): self.run_case('trigger_full')
    def test_uart_reserve_exact_byte_boundaries_and_wrap(self): self.run_case('trigger_exact_reserve')
    def test_config_and_numeric_validation(self): self.run_case('config')
    def test_filter_input_rate_reset_and_latched_saturation(self): self.run_case('filter')
    def test_watchdog_wrap_newer_isr_and_failure_drain(self): self.run_case('watchdog')
    def test_uart_limit_when_foreground_drains_a_backlog(self): self.run_case('burst')
    def test_unapplied_average_does_not_change_warmed_window(self): self.run_case('draftavg')
    def test_rejected_zero_average_keeps_scanning_safely(self): self.run_case('rejectavg')
    def test_rejected_mask_keeps_record_metadata_and_schedule_coherent(self): self.run_case('rejectmask')
    def test_failed_conversion_start_requires_successful_apply_before_rec(self): self.run_case('applyfailure')
    def test_static_queue_size_and_header(self):
        result = subprocess.run([str(self.exe), 'budget'], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertIn('raw=5120', result.stdout)
        self.assertIn('batch=736', result.stdout)
        self.assertIn('#ENDHDR\n', result.stdout)
        (ROOT/'tmp/task4-header-example.txt').write_text(result.stdout, encoding='utf-8')
