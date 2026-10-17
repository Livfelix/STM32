import test_acquisition as support
ROOT, FW = support.ROOT, support.FW
import os, pathlib, shutil, subprocess, tempfile, unittest
class Config(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=os.environ.get('SG6_HOST_CC') or shutil.which('gcc') or str(ROOT/'.tools/venv/Lib/site-packages/ziglang/zig.exe')
        directory=tempfile.TemporaryDirectory(prefix='config-',dir=ROOT/'tmp');cls.addClassCleanup(directory.cleanup)
        cls.exe=pathlib.Path(directory.name)/'test.exe'
        cmd=[cc]+(['cc'] if pathlib.Path(cc).stem=='zig' else [])
        cmd+=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-variable','-I'+str(ROOT/'tests/host'),'-I'+str(FW/'include'),'-I'+str(FW/'src'),str(ROOT/'tests/host/config_harness.c'),str(FW/'src/sg_measure.c'),str(FW/'src/sg_record.c'),'-lm','-o',str(cls.exe)]
        env=os.environ.copy();env['ZIG_GLOBAL_CACHE_DIR']=str(ROOT/'.tools/zig-cache');env['ZIG_LOCAL_CACHE_DIR']=str(ROOT/'tmp/zig-cache')
        r=subprocess.run(cmd,capture_output=True,text=True,env=env)
        if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    run_case=support.Acquisition.run_case
    def test_physical_gain_bounds_no_mutation(self):self.run_case('gain')
    def test_complete_save_load_of_applied_configuration(self):self.run_case('save')
    def test_metadata_commit_and_publication_hold(self):self.run_case('metadata')
    def test_trigger_loss_status_retries_atomically_and_preserves_counter(self):self.run_case('uartstatus')
    def test_metadata_and_text_preserve_trigger_reserve(self):self.run_case('uartmetadatareserve')
    def test_trigger_enabled_header_tracks_applied_only(self):self.run_case('triggerheader')
    def test_each_flash_update_step_survives_power_cut_or_corruption(self):self.run_case('cuts')
    def test_persists_applied_only_rejects_gain_and_locks_rec(self):self.run_case('rejects')
    def test_all_presets_preserve_individual_calibration(self):self.run_case('presets')
    def test_selected_trigger_source_and_finite_window_filter_validation(self):self.run_case('windows')
