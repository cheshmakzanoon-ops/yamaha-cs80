#!/usr/bin/env python3
"""Check original-process exit diagnostics without running a real crash.

SIGSEGV/SIGABRT returns are mocked, so piped kernel core collectors are never
invoked. The only real signal is SIGTERM, which does not generate a core.
"""
import importlib.util
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


WRAPPER = Path(__file__).with_name("run-pluginval-with-core.py")
if len(sys.argv) > 1 and sys.argv[1] == "--wrapper":
    WRAPPER = Path(sys.argv[2]).resolve()
    del sys.argv[1:3]
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("validator_core_wrapper", WRAPPER)
wrapper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wrapper)


class ExitClassificationTests(unittest.TestCase):
    def run_mocked(self, returncode, *, log_failure=False, cleanup_failure=False):
        with tempfile.TemporaryDirectory(prefix="cs80-exit-classification-") as work:
            root = Path(work)
            child_work = root / "child"
            child_work.mkdir()
            log = root / "core.log"
            if log_failure:
                log = root / "missing-parent" / "core.log"
            process = mock.Mock(pid=4242)
            process.wait.return_value = returncode
            argv = [str(WRAPPER), "--core-log", str(log), "--executable",
                    sys.executable, "--", sys.executable, "PRIVATE_ARGUMENT"]
            with (mock.patch.object(sys, "argv", argv),
                  mock.patch.object(wrapper.Path, "read_text", return_value="|collector"),
                  mock.patch.object(wrapper.tempfile, "mkdtemp", return_value=str(child_work)),
                  mock.patch.object(wrapper.resource, "getrlimit", return_value=(0, 0)),
                  mock.patch.object(wrapper.resource, "setrlimit") as limits,
                  mock.patch.object(wrapper.subprocess, "Popen", return_value=process) as spawn,
                  mock.patch.object(wrapper.subprocess, "run") as debugger,
                  mock.patch.object(wrapper.shutil, "rmtree") as cleanup):
                if cleanup_failure:
                    cleanup.side_effect = OSError("PRIVATE_CLEANUP_DETAIL")
                result = wrapper.main()
                spawn.assert_called_once()
                process.wait.assert_called_once_with()
                debugger.assert_not_called()
                # No parent limit changes; the existing callback only limits
                # the child. Verify the callback without changing this process.
                limits.assert_not_called()
                spawn.call_args.kwargs["preexec_fn"]()
                limits.assert_called_once_with(wrapper.resource.RLIMIT_CORE, (0, 0))
                cleanup.assert_called_once_with(str(child_work))
            text = log.read_text() if log.exists() else ""
            self.assertNotIn("PRIVATE_ARGUMENT", text)
            self.assertNotIn("PRIVATE_CLEANUP_DETAIL", text)
            return result, text

    def test_normal_exits_are_not_inferred_signals(self):
        for code in (0, 7, 134, 139, 143):
            with self.subTest(code=code):
                result, text = self.run_mocked(code)
                self.assertEqual(result, code)
                self.assertIn(f"Original validator PID: 4242; exit: {code}", text)
                self.assertIn(f"PID=4242; raw returncode={code}; kind=exit; exit_code={code}", text)
                self.assertNotIn("signal_name=", text)

    def test_native_signal_returns_keep_raw_and_normalized_results(self):
        for sig in (signal.SIGSEGV, signal.SIGABRT, signal.SIGTERM):
            with self.subTest(signal=sig):
                result, text = self.run_mocked(-int(sig))
                self.assertEqual(result, 128 + int(sig))
                self.assertIn(f"Original validator PID: 4242; exit: {128 + int(sig)}", text)
                self.assertIn(f"PID=4242; raw returncode={-int(sig)}; kind=signal; "
                              f"signal_number={int(sig)}; signal_name={sig.name}", text)

    def test_unknown_signal_name_does_not_override_result(self):
        self.assertEqual(wrapper.termination_details(-999),
                         "raw returncode=-999; kind=signal; signal_number=999; signal_name=UNKNOWN")

    def test_log_failure_preserves_every_result(self):
        for code in (0, 7, 139, -int(signal.SIGSEGV), -int(signal.SIGABRT)):
            with self.subTest(code=code):
                result, text = self.run_mocked(code, log_failure=True)
                self.assertEqual(result, code if code >= 0 else 128 - code)
                self.assertEqual(text, "")

    def test_cleanup_failure_preserves_every_result(self):
        for code in (0, 7, 139, -int(signal.SIGSEGV), -int(signal.SIGABRT)):
            with self.subTest(code=code):
                result, text = self.run_mocked(code, cleanup_failure=True)
                self.assertEqual(result, code if code >= 0 else 128 - code)
                self.assertIn("Local core cleanup unavailable: OSError", text)

    def run_cli(self, code):
        with tempfile.TemporaryDirectory(prefix="cs80-exit-cli-") as work:
            log = Path(work) / "core.log"
            result = subprocess.run(
                [sys.executable, str(WRAPPER), "--core-log", str(log),
                 "--executable", sys.executable, "--", sys.executable, "-c", code],
                capture_output=True, text=True, timeout=10)
            self.assertEqual(result.stdout, "")
            self.assertEqual(result.stderr, "")
            return result.returncode, log.read_text()

    def test_real_normal_child_exits(self):
        for code in (0, 7, 139):
            with self.subTest(code=code):
                result, text = self.run_cli(f"raise SystemExit({code})")
                self.assertEqual(result, code)
                self.assertIn(f"raw returncode={code}; kind=exit; exit_code={code}", text)
                self.assertEqual(text.count("Original validator PID:"), 1)

    def test_real_non_core_signal_termination(self):
        result, text = self.run_cli("import os, signal; os.kill(os.getpid(), signal.SIGTERM)")
        self.assertEqual(result, 128 + int(signal.SIGTERM))
        self.assertIn(f"raw returncode={-int(signal.SIGTERM)}; kind=signal; "
                      f"signal_number={int(signal.SIGTERM)}; signal_name=SIGTERM", text)
        self.assertEqual(text.count("Original validator PID:"), 1)


if __name__ == "__main__":
    unittest.main()
