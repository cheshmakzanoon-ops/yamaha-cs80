"""Controlled shell-contract fixtures; these are not native GUI/Xvfb tests."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


WRAPPER = Path(__file__).resolve().parents[1] / "scripts/run-ui-smoke-linux.sh"


class UiSmokeEnvironmentTests(unittest.TestCase):
    def setUp(self):
        self.fixture = tempfile.TemporaryDirectory(prefix="cs80-ui-wrapper-")
        self.root = Path(self.fixture.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        for name in ("bash", "grep", "mktemp", "rm", "cat", "sleep"):
            (self.bin / name).symlink_to(shutil.which(name))
        self.tool("xvfb-run", """#!/bin/bash
set -eu
[[ $1 == -a && $2 == -s && $3 == '-screen 0 1920x1200x24' ]]
shift 3
export DISPLAY=:991
exec "$@"
""")
        for name in ("Xvfb", "xauth"):
            self.tool(name, "#!/bin/bash\nexit 0\n")
        self.python_tool("openbox", """
import os, pathlib, sys, time
root = pathlib.Path(os.environ['FIXTURE'])
(root / 'wm.pid').write_text(str(os.getpid()))
assert sys.argv[1:] == ['--sm-disable']
mode = os.environ['MODE']
if mode == 'wm-exit':
    sys.exit(23)
if mode != 'wm-never-ready':
    (root / 'wm.ready').touch()
while True:
    time.sleep(10)
""")
        self.python_tool("xprop", """
import os, pathlib, sys
assert sys.argv[1:] == ['-root', '_NET_SUPPORTING_WM_CHECK']
if os.environ['MODE'] == 'xprop-error':
    sys.exit(71)
if (pathlib.Path(os.environ['FIXTURE']) / 'wm.ready').exists():
    print('_NET_SUPPORTING_WM_CHECK(WINDOW): window id # 0x20001')
else:
    print('_NET_SUPPORTING_WM_CHECK: no such atom on any window.')
""")
        self.python_tool("xlsatoms", """
import os, pathlib
print('1 UTF8_STRING')
if os.environ['MODE'] != 'missing-atom' and (pathlib.Path(os.environ['FIXTURE']) / 'wm.ready').exists():
    print('2 WM_PROTOCOLS')
""")
        self.child = self.root / "child.py"
        self.child.write_text("""
import json, os, pathlib, sys
pathlib.Path(os.environ['FIXTURE'], 'child.json').write_text(json.dumps(sys.argv[1:]))
assert os.environ['DISPLAY'] == ':991'
assert os.environ['JUCE_FONT_PATH'] == '/fixture/font path'
sys.exit(int(os.environ['CHILD_EXIT']))
""")

    def tearDown(self):
        self.fixture.cleanup()

    def tool(self, name, contents):
        path = self.bin / name
        path.write_text(contents)
        path.chmod(0o755)

    def python_tool(self, name, contents):
        self.tool(name, f"#!{sys.executable}\n" + contents)

    def run_wrapper(self, mode="ready", child_exit=0):
        env = dict(os.environ, PATH=str(self.bin), FIXTURE=str(self.root), MODE=mode,
                   CHILD_EXIT=str(child_exit), JUCE_FONT_PATH="/fixture/font path")
        result = subprocess.run(
            [str(self.bin / "bash"), str(WRAPPER), sys.executable, str(self.child),
             "argument with spaces", "literal;$HOME"], env=env,
            text=True, capture_output=True, timeout=25)
        pid_file = self.root / "wm.pid"
        if pid_file.exists():
            pid = int(pid_file.read_text())
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                pass
            else:
                # This is solely our fixture's captured process, never a name
                # or process-group cleanup against a user's window manager.
                os.kill(pid, 15)
                self.fail("wrapper left its own window-manager process running")
        return result

    def test_success_arguments_and_scoped_cleanup(self):
        result = self.run_wrapper()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("WM_PROTOCOLS absent before", result.stdout)
        self.assertIn("Openbox ready", result.stdout)
        self.assertEqual(json.loads((self.root / "child.json").read_text()),
                         ["argument with spaces", "literal;$HOME"])

    def test_child_failure_is_preserved(self):
        self.assertEqual(self.run_wrapper(child_exit=37).returncode, 37)

    def test_missing_dependency_fails_before_running_child(self):
        (self.bin / "openbox").unlink()
        result = self.run_wrapper()
        self.assertEqual(result.returncode, 127)
        self.assertIn("requires openbox", result.stderr)
        self.assertFalse((self.root / "child.json").exists())

    def test_window_manager_startup_failure(self):
        result = self.run_wrapper(mode="wm-exit")
        self.assertEqual(result.returncode, 70, result.stderr)
        self.assertFalse((self.root / "child.json").exists())

    def test_missing_protocol_atom_fails(self):
        result = self.run_wrapper(mode="missing-atom")
        self.assertEqual(result.returncode, 70)
        self.assertIn("WM_PROTOCOLS is still absent", result.stderr)
        self.assertFalse((self.root / "child.json").exists())

    def test_x_query_failure_is_not_suppressed(self):
        self.assertEqual(self.run_wrapper(mode="xprop-error").returncode, 71)
        self.assertFalse((self.root / "child.json").exists())

    def test_readiness_deadline_is_bounded(self):
        result = self.run_wrapper(mode="wm-never-ready")
        self.assertEqual(result.returncode, 70)
        self.assertIn("within 15 seconds", result.stderr)
        self.assertFalse((self.root / "child.json").exists())


if __name__ == "__main__":
    unittest.main()
