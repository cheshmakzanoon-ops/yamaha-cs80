"""Mock-only input-dispatch controls; never launches a display or a host."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import call, patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("reaper_runner", ROOT / "scripts/run-reaper-smoke.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class BoundedEditorInputControls(unittest.TestCase):
    def test_exactly_one_escape_in_unique_instrument_window(self):
        with tempfile.TemporaryDirectory() as tmp, \
                patch.object(runner, "capture") as capture, \
                patch.object(runner.subprocess, "check_output", return_value="4194575\n") as search, \
                patch.object(runner.subprocess, "run") as run, \
                patch.object(runner.time, "sleep") as sleep:
            out = Path(tmp)
            runner.dismiss_tour_and_capture(out)
            search.assert_called_once_with(
                ["xdotool", "search", "--onlyvisible", "--name",
                 "^VST3i: CS-80 Reference .*CS-80 real-host smoke"], text=True, timeout=10)
            self.assertEqual(run.call_args_list, [
                call(["xdotool", "windowactivate", "--sync", "4194575"], check=True, timeout=10),
                call(["xdotool", "key", "--clearmodifiers", "Escape"], check=True, timeout=10)])
            self.assertEqual(capture.call_args_list,
                             [call(out, "host-editor-before-escape.png"), call(out, "host-editor.png")])
            sleep.assert_called_once_with(1)
            record = json.loads((out / "editor-escape.json").read_text())
            self.assertEqual((record["window"], record["key"], record["count"]), ("4194575", "Escape", 1))
            self.assertIn("visual result must be inspected", record["scope"])

    def test_host_wrapper_escape_is_not_a_tour_dismissal(self):
        # Actual run41510793290 closed the REAPER FX window on Escape before audio.
        # Input must target the visible Close Tour button, never the host key route.
        with tempfile.TemporaryDirectory() as tmp, \
                patch.object(runner, "capture"), \
                patch.object(runner.subprocess, "check_output", return_value="4194575\n"), \
                patch.object(runner.subprocess, "run") as run, \
                patch.object(runner.time, "sleep"):
            runner.dismiss_tour_and_capture(Path(tmp))
            self.assertFalse(any("Escape" in item.args[0] for item in run.call_args_list),
                             "Escape routed to the REAPER wrapper closes the editor")

    def test_no_input_on_absent_ambiguous_or_invalid_window(self):
        for result in ("", "4194575\n4194576\n", "not-a-window\n"):
            with self.subTest(search_result=result), tempfile.TemporaryDirectory() as tmp, \
                    patch.object(runner, "capture") as capture, \
                    patch.object(runner.subprocess, "check_output", return_value=result), \
                    patch.object(runner.subprocess, "run") as run, \
                    patch.object(runner.time, "sleep") as sleep:
                out = Path(tmp)
                with self.assertRaisesRegex(ValueError, "^expected one visible instrument window for bounded tour dismissal$"):
                    runner.dismiss_tour_and_capture(out)
                run.assert_not_called()
                sleep.assert_not_called()
                capture.assert_called_once_with(out, "host-editor-before-escape.png")
                self.assertFalse((out / "editor-escape.json").exists())

    def test_failed_activation_never_sends_escape_or_claims_capture(self):
        with tempfile.TemporaryDirectory() as tmp, \
                patch.object(runner, "capture") as capture, \
                patch.object(runner.subprocess, "check_output", return_value="4194575\n"), \
                patch.object(runner.subprocess, "run", side_effect=subprocess.CalledProcessError(1, "xdotool")) as run, \
                patch.object(runner.time, "sleep") as sleep:
            out = Path(tmp)
            with self.assertRaises(subprocess.CalledProcessError):
                runner.dismiss_tour_and_capture(out)
            run.assert_called_once_with(["xdotool", "windowactivate", "--sync", "4194575"], check=True, timeout=10)
            sleep.assert_not_called()
            capture.assert_called_once_with(out, "host-editor-before-escape.png")
            self.assertFalse((out / "editor-escape.json").exists())


if __name__ == "__main__":
    unittest.main()
