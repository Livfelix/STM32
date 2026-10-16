import unittest
import test_acquisition as support


class Logging(unittest.TestCase):
    setUpClass = support.Acquisition.__dict__['setUpClass']
    run_case = support.Acquisition.run_case
    def test_sd_foreground_max_reset_atomic_status_retry_and_generic_failure(self): self.run_case('latency')
    def test_bin3_footer_after_final_batch_and_queue_drain(self): self.run_case('footer')
    def test_no_footer_after_media_or_overflow_failure_and_no_retry(self): self.run_case('footer_failures')
    def test_fast_preset_refuses_csv_using_applied_preset(self): self.run_case('fastcsv')
    def test_binary_batch_full_partial_stop_and_restart(self): self.run_case('batch')
    def test_fast_sd_busy_failure_reports_slow_incomplete(self): self.run_case('slow')
    def test_prior_successful_boundary_busy_cannot_classify_later_failure(self): self.run_case('busy_cause')
    def test_fast_sync_busy_and_close_preserve_slow_reason(self): self.run_case('slow_sync')
    def test_binary_partial_periodic_sync_and_queued_stop(self): self.run_case('batch_sync')
    def test_binary_fault_flush_and_partial_write_never_retry(self): self.run_case('batch_error')
    def test_exclusive_names_and_header_failure(self): self.run_case('open')
    def test_partial_write_sync_and_close(self): self.run_case('errors')
    def test_duration_and_valid_battery_time(self): self.run_case('stops')
    def test_low_warning_requires_continuous_500ms(self): self.run_case('battery_warning')
    def test_battery_percent_curve_interpolation_clamps_and_validity(self): self.run_case('battery_percent')
    def test_invalid_adc_stops_without_stale_voltage(self): self.run_case('battery_invalid')
    def test_invalid_and_low_voltage_timers_are_independent(self): self.run_case('battery_timers')
    def test_csv_metadata_and_units(self): self.run_case('csv')
    def test_csv_u64_time_without_nano_unsupported_printf(self): self.run_case('csv64')
