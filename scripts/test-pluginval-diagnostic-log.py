#!/usr/bin/env python3
"""Bounded selector/CLI/seed fixtures and workflow wiring; no validator executes."""
import importlib.util
import itertools
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SELECTOR = ROOT / "scripts/pluginval-diagnostic-log.py"
spec = importlib.util.spec_from_file_location("diagnostic_log", SELECTOR)
selector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(selector)
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text()


def step(name):
    marker = "      - name: " + name + "\n"
    assert WORKFLOW.count(marker) == 1, name
    text = WORKFLOW.split(marker, 1)[1]
    return text.split("\n      - name:", 1)[0]


class DiagnosticSelectionTests(unittest.TestCase):
    def cli(self, *args, cwd=None):
        return subprocess.run([sys.executable, str(SELECTOR), *args], cwd=cwd,
                              capture_output=True, text=True, check=True)

    def test_all_valid_outcome_pairs(self):
        # Independent truth table: cancellation wins, then randomized failure,
        # then fixed failure. Empty denotes a step that did not produce outcome.
        for gui, fixed in itertools.product(sorted(selector.OUTCOMES), repeat=2):
            with self.subTest(gui=gui, fixed=fixed):
                expected = None
                if "cancelled" not in (gui, fixed):
                    if gui == "failure":
                        expected = "pluginval-gui.log"
                    elif fixed == "failure":
                        expected = "pluginval-gui-seed-regression.log"
                self.assertEqual(selector.select_log(gui, fixed), expected)
                self.assertEqual(self.cli(gui, fixed).stdout, (expected + "\n") if expected else "")

    def test_malformed_outcomes_fail_closed(self):
        for malformed in ("FAILED", "Failure", "failure\n", " failure", "unknown",
                          "failure\nlog=evil", "$(touch sentinel)", "x" * 10000):
            for values in ((malformed, "failure"), ("failure", malformed)):
                with self.subTest(values=values):
                    self.assertIsNone(selector.select_log(*values))
                    result = self.cli(*values)
                    self.assertEqual(result.stdout, "")
                    self.assertIn("Unrecognized", result.stderr)

    def test_bad_argument_counts_do_not_select(self):
        for args in ((), ("failure",), ("failure", "success", "extra")):
            self.assertEqual(self.cli(*args).stdout, "")

    def test_same_selected_log_reaches_existing_seed_parser(self):
        for gui, fixed, wanted in (("failure", "success", "0x123"),
                                   ("success", "failure", "0x2da72db"),
                                   ("failure", "failure", "0x123")):
            with tempfile.TemporaryDirectory() as work:
                root = Path(work)
                (root / "pluginval-gui.log").write_text("Random seed: 0x123\nSUCCESS\n")
                (root / "pluginval-gui-seed-regression.log").write_text("Random seed: 0x2da72db\nSUCCESS\n")
                selected = self.cli(gui, fixed).stdout.strip()
                seeds = [subprocess.run([sys.executable, str(ROOT / "scripts/pluginval-diagnostic-seed.py"),
                                         selected], cwd=root, check=True, capture_output=True, text=True).stdout.strip()
                         for _ in ("gdb", "trace")]
                self.assertEqual(seeds, [wanted, wanted])

    def test_malformed_final_seed_does_not_fall_back(self):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "log"
            path.write_text("Random seed: 0x123\nRandom seed: not-a-seed\n")
            result = subprocess.run([sys.executable, str(ROOT / "scripts/pluginval-diagnostic-seed.py"), str(path)],
                                    check=True, capture_output=True, text=True)
            self.assertEqual(result.stdout, "")

    def test_workflow_selects_once_from_both_actual_outcomes(self):
        fixed = step("Validate GUI state regression seed 0x2da72db")
        self.assertIn("id: validate_gui_seed\n", fixed)
        self.assertIn("if: always() && !cancelled() && (steps.validate_gui.outcome == 'success' || steps.validate_gui.outcome == 'failure')", fixed)
        self.assertNotIn("continue-on-error", fixed)
        self.assertIn("timeout-minutes: 5", fixed)
        self.assertIn("--random-seed 0x2da72db", fixed)
        selection = step("Select failed GUI log for diagnostics")
        self.assertIn("if: always() && !cancelled()", selection)
        self.assertIn("GUI_OUTCOME: ${{ steps.validate_gui.outcome }}", selection)
        self.assertIn("REGRESSION_OUTCOME: ${{ steps.validate_gui_seed.outcome }}", selection)
        self.assertIn('pluginval-diagnostic-log.py "$GUI_OUTCOME" "$REGRESSION_OUTCOME"', selection)
        self.assertIn('echo "log=$log" >> "$GITHUB_OUTPUT"', selection)
        self.assertEqual(WORKFLOW.count("pluginval-diagnostic-log.py"), 1)

    def test_both_replays_share_selection_and_remain_bounded(self):
        for name in ("Diagnose GUI validator failure with gdb", "Diagnose GUI validator with fault-thread trace"):
            replay = step(name)
            self.assertIn("if: failure() && !cancelled() && steps.select_gui_diagnostic.outputs.log != ''", replay)
            self.assertIn("DIAGNOSTIC_LOG: ${{ steps.select_gui_diagnostic.outputs.log }}", replay)
            self.assertIn('pluginval-diagnostic-seed.py "$DIAGNOSTIC_LOG"', replay)
            self.assertNotIn("pluginval-diagnostic-seed.py pluginval-gui.log", replay)
            self.assertEqual(replay.count("timeout --signal=TERM --kill-after=10s 240s"), 1)
            self.assertEqual(replay.count("--timeout-ms 120000"), 1)
            self.assertIn('exit "${statuses[0]}"', replay)
            self.assertNotIn("continue-on-error", replay)
        self.assertIn("steps.build_trace.outcome == 'success'", step("Diagnose GUI validator with fault-thread trace"))
        self.assertIn("--return-child-result", step("Diagnose GUI validator failure with gdb"))


if __name__ == "__main__":
    unittest.main()
