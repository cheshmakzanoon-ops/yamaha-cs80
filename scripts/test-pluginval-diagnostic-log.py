#!/usr/bin/env python3
"""Bounded selector/CLI/seed fixtures and workflow wiring; no validator executes."""
import importlib.util
import itertools
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SELECTOR = ROOT / "scripts/pluginval-diagnostic-log.py"
spec = importlib.util.spec_from_file_location("diagnostic_log", SELECTOR)
selector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(selector)
WORKFLOW = (ROOT / ".github/workflows/build.yml").read_text()
LINUX_VALIDATOR = WORKFLOW.split("\n  pluginval:\n", 1)[1].split("\n  macos:", 1)[0]


def step(name):
    marker = "      - name: " + name + "\n"
    assert LINUX_VALIDATOR.count(marker) == 1, name
    text = LINUX_VALIDATOR.split(marker, 1)[1]
    return text.split("\n      - name:", 1)[0]


class DiagnosticSelectionTests(unittest.TestCase):
    def cli(self, *args, cwd=None):
        return subprocess.run([sys.executable, str(SELECTOR), *args], cwd=cwd,
                              capture_output=True, text=True, check=True)

    def test_all_valid_outcome_triples(self):
        # Cancellation wins; preserve GUI priority before selecting headless.
        # Empty denotes a step that did not produce an outcome.
        for gui, fixed, headless in itertools.product(sorted(selector.OUTCOMES), repeat=3):
            with self.subTest(gui=gui, fixed=fixed, headless=headless):
                expected = None
                if "cancelled" not in (gui, fixed, headless):
                    if gui == "failure":
                        expected = "pluginval-gui.log"
                    elif fixed == "failure":
                        expected = "pluginval-gui-seed-regression.log"
                    elif headless == "failure":
                        expected = "pluginval-headless.log"
                self.assertEqual(selector.select_log(gui, fixed, headless), expected)
                self.assertEqual(self.cli(gui, fixed, headless).stdout,
                                 (expected + "\n") if expected else "")

    def test_existing_two_outcome_callers_are_unchanged(self):
        for gui, fixed in itertools.product(sorted(selector.OUTCOMES), repeat=2):
            with self.subTest(gui=gui, fixed=fixed):
                expected = selector.select_log(gui, fixed, "")
                self.assertEqual(selector.select_log(gui, fixed), expected)
                self.assertEqual(self.cli(gui, fixed).stdout,
                                 (expected + "\n") if expected else "")

    def test_malformed_outcomes_fail_closed(self):
        for malformed in ("FAILED", "Failure", "failure\n", " failure", "unknown",
                          "failure\nlog=evil", "$(touch sentinel)", "x" * 10000):
            for values in ((malformed, "failure"), ("failure", malformed),
                           (malformed, "failure", "failure"),
                           ("failure", malformed, "failure"),
                           ("failure", "failure", malformed)):
                with self.subTest(values=values):
                    self.assertIsNone(selector.select_log(*values))
                    result = self.cli(*values)
                    self.assertEqual(result.stdout, "")
                    self.assertIn("Unrecognized", result.stderr)

    def test_bad_argument_counts_do_not_select(self):
        for args in ((), ("failure",), ("failure", "success", "failure", "extra")):
            self.assertEqual(self.cli(*args).stdout, "")

    def test_same_selected_log_reaches_existing_seed_parser(self):
        for gui, fixed, headless, wanted in (("failure", "success", "success", "0x123"),
                                             ("success", "failure", "success", "0x2da72db"),
                                             ("failure", "failure", "failure", "0x123"),
                                             ("skipped", "skipped", "failure", "0xabc")):
            with tempfile.TemporaryDirectory() as work:
                root = Path(work)
                (root / "pluginval-gui.log").write_text("Random seed: 0x123\nSUCCESS\n")
                (root / "pluginval-gui-seed-regression.log").write_text("Random seed: 0x2da72db\nSUCCESS\n")
                (root / "pluginval-headless.log").write_text("Random seed: 0xabc\nSUCCESS\n")
                selected = self.cli(gui, fixed, headless).stdout.strip()
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

    def test_workflow_selects_once_from_all_actual_outcomes(self):
        fixed = step("Validate GUI state regression seed 0x2da72db")
        self.assertIn("id: validate_gui_seed\n", fixed)
        self.assertIn("if: always() && !cancelled() && (steps.validate_gui.outcome == 'success' || steps.validate_gui.outcome == 'failure')", fixed)
        self.assertNotIn("continue-on-error", fixed)
        self.assertIn("timeout-minutes: 5", fixed)
        self.assertIn("--random-seed 0x2da72db", fixed)
        self.assertIn("id: validate_headless\n", step("Validate VST3 headless"))
        selection = step("Select failed validator log for diagnostics")
        self.assertIn("if: always() && !cancelled()", selection)
        self.assertIn("GUI_OUTCOME: ${{ steps.validate_gui.outcome }}", selection)
        self.assertIn("REGRESSION_OUTCOME: ${{ steps.validate_gui_seed.outcome }}", selection)
        self.assertIn("HEADLESS_OUTCOME: ${{ steps.validate_headless.outcome }}", selection)
        self.assertIn('pluginval-diagnostic-log.py "$GUI_OUTCOME" "$REGRESSION_OUTCOME" "$HEADLESS_OUTCOME"', selection)
        self.assertIn('echo "log=$log" >> "$GITHUB_OUTPUT"', selection)
        self.assertEqual(WORKFLOW.count("pluginval-diagnostic-log.py"), 1)

    def test_both_replays_share_selection_and_remain_bounded(self):
        for name in ("Diagnose validator failure with gdb", "Diagnose validator with fault-thread trace"):
            replay = step(name)
            self.assertIn("if: failure() && !cancelled() && steps.select_validator_diagnostic.outputs.log != ''", replay)
            self.assertIn("DIAGNOSTIC_LOG: ${{ steps.select_validator_diagnostic.outputs.log }}", replay)
            self.assertIn('pluginval-diagnostic-seed.py "$DIAGNOSTIC_LOG"', replay)
            self.assertNotIn("pluginval-diagnostic-seed.py pluginval-gui.log", replay)
            self.assertEqual(replay.count("timeout --signal=TERM --kill-after=10s 240s"), 1)
            self.assertEqual(replay.count("--timeout-ms 120000"), 1)
            self.assertIn('exit "${statuses[0]}"', replay)
            self.assertNotIn("continue-on-error", replay)
        self.assertIn("steps.build_trace.outcome == 'success'", step("Diagnose validator with fault-thread trace"))
        self.assertIn("--return-child-result", step("Diagnose validator failure with gdb"))

    @unittest.skipUnless(os.name == "posix" and shutil.which("bash"),
                         "Linux diagnostic shell fixture requires POSIX Bash")
    def test_actual_replay_shell_preserves_lane_seed_and_exit(self):
        # Execute the exact workflow shell with only timeout intercepted. This
        # observes its final argv without executing a validator or debugger.
        for diagnostic in ("Diagnose validator failure with gdb",
                           "Diagnose validator with fault-thread trace"):
            shell = textwrap.dedent(step(diagnostic).split("        run: |\n", 1)[1])
            for log, headless in (("pluginval-headless.log", True),
                                  ("pluginval-gui.log", False),
                                  ("pluginval-gui-seed-regression.log", False)):
                for original_exit in (0, 139):
                    with self.subTest(diagnostic=diagnostic, log=log, original_exit=original_exit), \
                         tempfile.TemporaryDirectory() as work:
                        root = Path(work)
                        (root / "scripts").symlink_to(ROOT / "scripts", target_is_directory=True)
                        (root / log).write_text("Random seed: 0xabc\nSUCCESS\n")
                        executable = root / "bin" / "timeout"
                        executable.parent.mkdir()
                        executable.write_text(
                            "#!/usr/bin/env python3\nimport json, os, sys\n"
                            "with open(os.environ['CAPTURED_ARGS'], 'w') as out:\n"
                            "    json.dump(sys.argv[1:], out)\n"
                            "print('SUCCESS')\n"
                            "sys.exit(int(os.environ['CAPTURED_EXIT']))\n")
                        executable.chmod(0o755)
                        args_path = root / "args.json"
                        env = dict(os.environ, PATH=str(executable.parent) + os.pathsep + os.environ["PATH"],
                                   DIAGNOSTIC_LOG=log, CAPTURED_ARGS=str(args_path),
                                   CAPTURED_EXIT=str(original_exit))
                        result = subprocess.run(["bash", "-c", shell], cwd=root, env=env,
                                                capture_output=True, text=True)
                        self.assertEqual(result.returncode, original_exit, result.stdout + result.stderr)
                        argv = json.loads(args_path.read_text())
                        self.assertEqual(argv[:3], ["--signal=TERM", "--kill-after=10s", "240s"])
                        self.assertEqual(argv.count("--skip-gui-tests"), int(headless))
                        self.assertEqual(argv.count("xvfb-run"), int(not headless))
                        self.assertEqual(argv[argv.index("--random-seed") + 1], "0xabc")
                        self.assertEqual(argv[argv.index("--strictness-level") + 1], "10")
                        self.assertEqual(argv[argv.index("--timeout-ms") + 1], "120000")
                        self.assertIn("--validate-in-process", argv)
                        self.assertIn("Original validation remains failed", result.stdout)
                        if "gdb" in diagnostic:
                            self.assertIn("--return-child-result", argv)


if __name__ == "__main__":
    unittest.main()
