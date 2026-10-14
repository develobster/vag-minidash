"""Compiler-measured regression and future nested configuration stack limits."""
import unittest
from tools.check_ui_stack_budget import evaluate


class UiStackBudgetTests(unittest.TestCase):
    def setUp(self):
        self.frames = {'ui_task': 1440, 'diagnostics_snapshot': 176,
                       'md_configuration_storage_load': 608,
                       'md_configuration_decode': 3104,
                       'md_configuration_validate': 640}

    def test_verified_firmware_paths_fit_with_reserve(self):
        self.assertEqual(evaluate(self.frames, 8192),
                         {'UI diagnostics': 1616, 'UI saved configuration': 5792})

    def test_original_compiler_measured_regression_is_rejected(self):
        self.frames.update(ui_task=4384, diagnostics_snapshot=5840)
        with self.assertRaisesRegex(ValueError, 'UI diagnostics: frames 10224'):
            evaluate(self.frames, 8192)

    def test_configuration_growth_is_rejected_even_with_small_ui_frame(self):
        self.frames['md_configuration_decode'] = 6000
        with self.assertRaisesRegex(ValueError, 'UI saved configuration'):
            evaluate(self.frames, 8192)

    def test_missing_frame_evidence_does_not_pass(self):
        del self.frames['md_configuration_decode']
        with self.assertRaisesRegex(ValueError, 'missing frame reports'):
            evaluate(self.frames, 8192)


if __name__ == '__main__':
    unittest.main()
