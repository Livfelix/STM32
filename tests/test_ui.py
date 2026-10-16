import test_config as support
import os, pathlib, shutil, subprocess, tempfile, unittest
ROOT, FW=support.ROOT,support.FW
class UIDrivers(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=os.environ.get('SG6_HOST_CC') or shutil.which('gcc') or str(ROOT/'.tools/venv/Lib/site-packages/ziglang/zig.exe')
        directory=tempfile.TemporaryDirectory(prefix='ui-',dir=ROOT/'tmp');cls.addClassCleanup(directory.cleanup);cls.exe=pathlib.Path(directory.name)/'test.exe'
        cmd=[cc]+(['cc'] if pathlib.Path(cc).stem=='zig' else [])
        cmd+=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-variable','-I'+str(ROOT/'tests/host'),'-I'+str(FW/'include'),str(ROOT/'tests/host/ui_driver_harness.c'),str(FW/'src/font.c'),'-o',str(cls.exe)]
        env=os.environ.copy();env['ZIG_GLOBAL_CACHE_DIR']=str(ROOT/'.tools/zig-cache');env['ZIG_LOCAL_CACHE_DIR']=str(ROOT/'tmp/zig-cache')
        r=subprocess.run(cmd,capture_output=True,text=True,env=env)
        if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    run_case=support.Config.run_case
    def test_clips_or_rejects_every_outside_rectangle(self):self.run_case('clip')
    def test_font_draws_complete_left_aligned_glyph(self):self.run_case('font')
    def test_equal_touch_points_keep_previous_valid_transform(self):self.run_case('touch')

class GUIMenus(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=os.environ.get('SG6_HOST_CC') or shutil.which('gcc') or str(ROOT/'.tools/venv/Lib/site-packages/ziglang/zig.exe')
        directory=tempfile.TemporaryDirectory(prefix='gui-',dir=ROOT/'tmp');cls.addClassCleanup(directory.cleanup);cls.exe=pathlib.Path(directory.name)/'test.exe'
        cmd=[cc]+(['cc'] if pathlib.Path(cc).stem=='zig' else [])
        cmd+=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-variable','-I'+str(ROOT/'tests/host'),'-I'+str(FW/'include'),'-I'+str(FW/'src'),str(ROOT/'tests/host/gui_harness.c'),str(FW/'src/sg_measure.c'),str(FW/'src/sg_record.c'),str(FW/'src/font.c'),'-lm','-o',str(cls.exe)]
        env=os.environ.copy();env['ZIG_GLOBAL_CACHE_DIR']=str(ROOT/'.tools/zig-cache');env['ZIG_LOCAL_CACHE_DIR']=str(ROOT/'tmp/zig-cache')
        r=subprocess.run(cmd,capture_output=True,text=True,env=env)
        if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    run_case=support.Config.run_case
    def test_sd_latency_max_is_labelled_in_main_status(self): self.run_case('sd_latency')
    def test_full_labels_in_two_by_two_grid(self):self.run_case('grid')
    def test_zero_requires_explicit_confirmation_or_cancel(self):self.run_case('zero')
    def test_calibration_edit_only_affects_selected_channel(self):self.run_case('selected')
    def test_all_sixteen_rates_and_nonempty_mask(self):self.run_case('rates')
    def test_main_fractional_sps_nonzero_and_full_text_fit(self):self.run_case('fractional')
    def test_battery_segments_percent_warning_blink_and_error_rec_priority(self):self.run_case('battery')
    def test_recording_blocks_all_mutating_menu_paths(self):self.run_case('locks')
    def test_signed_extremes_recording_errors_and_full_text_fit(self):self.run_case('extremes')
    def test_elapsed_long_filename_and_error_remain_visible(self):self.run_case('elapsed')
    def test_measured_shunt_editor_feeds_real_math_and_blocks_pending(self):self.run_case('measured_shunt')
    def test_selected_range_uses_actual_guards_and_calibration(self):self.run_case('range')
    def test_shunt_validation_cancel_rec_and_other_channel_pending(self):self.run_case('shunt_validation')
    def test_logical_screens_snapshot_and_complete_text_bounds(self):
        r=subprocess.run([str(self.exe),'render'],capture_output=True,text=True,timeout=10)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        (ROOT/'tmp/task5-render-text.txt').write_text(r.stdout,encoding='utf-8')
