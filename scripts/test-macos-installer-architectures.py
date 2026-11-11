#!/usr/bin/env python3
"""Offline installer preflight tests. All bundles/tools are inert fixtures."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PRODUCT = 'CS-80 Reference'
NOTICE_FIXTURES = ('assets/fonts/barlow-condensed/OFL.txt',
                   'assets/fonts/barlow-condensed/SOURCE.txt',
                   'src/vendor/mts-esp/LICENSE', 'src/vendor/libxml2/Copyright',
                   'src/vendor/libxml2/SOURCE.txt', 'src/vendor/libxml2/ADDITIONAL-NOTICES.txt')

class Architectures(unittest.TestCase):
    def run_fixture(self, architectures, missing=None, nonexecutable=None, missing_notice=None, caller_umask=-1, inspect_modes=False):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            script = root / 'installer/macos/make-pkg.sh'
            script.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / 'installer/macos/make-pkg.sh', script)
            for relative in ('CMakeLists.txt', 'LICENSE', 'docs/INSTALL_AND_QUICKSTART.md') + NOTICE_FIXTURES:
                if relative == missing_notice:
                    continue
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / relative, target)
            build = root / 'build space'
            license = build / '_deps/juce-src/LICENSE.md'
            license.parent.mkdir(parents=True)
            license.write_text('fixture licence')
            for index, (folder, suffix) in enumerate((('VST3', 'vst3'), ('AU', 'component'), ('Standalone', 'app'))):
                binary = build / 'CS80Reference_artefacts/Release' / folder / f'{PRODUCT}.{suffix}' / 'Contents/MacOS' / PRODUCT
                binary.parent.mkdir(parents=True)
                if missing != index:
                    binary.write_text(architectures[index])
                    binary.chmod(0o644 if nonexecutable == index else (0o751 if inspect_modes else 0o755))
            tools = root / 'tools'
            tools.mkdir()
            mock = '''#!/usr/bin/env python3
import os, pathlib, shutil, sys
name = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
with open(os.environ['CALLS'], 'a') as f: f.write(name + '\\n')
if name == 'lipo':
    assert args[:3] == ['-verify_arch', 'x86_64', 'arm64']
    sys.exit(0 if set(pathlib.Path(args[3]).read_text().split()) >= {'x86_64', 'arm64'} else 1)
elif name == 'ditto': shutil.copytree(*args)
elif name == 'pkgbuild':
    stage = pathlib.Path(args[args.index('--root') + 1])
    if os.environ.get('INSPECT_MODES') == '1':
        assert stage.stat().st_mode & 0o777 == 0o755, 'staging root is not publicly traversable'
        if stage.name == 'root-docs':
            notices = list(stage.iterdir())
            assert len(notices) == 9
            assert all(p.stat().st_mode & 0o777 == 0o644 for p in notices), 'notices are not publicly readable'
        else:
            binary, = stage.glob('*/Contents/MacOS/*')
            assert binary.stat().st_mode & 0o777 == 0o751, 'bundle executable mode changed'
            assert binary.read_text() == 'x86_64 arm64', 'bundle bytes changed'
    pathlib.Path(args[-1]).touch()
elif name == 'productbuild': pathlib.Path(args[-1]).touch()
'''
            for name in ('lipo', 'ditto', 'pkgbuild', 'productbuild', 'pkgutil'):
                tool = tools / name
                tool.write_text(mock)
                tool.chmod(0o755)
            calls = root / 'calls'
            env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'], CALLS=str(calls), INSPECT_MODES='1' if inspect_modes else '0')
            result = subprocess.run(['bash', str(script), str(build), str(root / 'out space')], env=env, text=True, capture_output=True, umask=caller_umask)
            return result.returncode, result.stderr, calls.read_text().splitlines() if calls.exists() else [], list((root / 'out space').glob('*.pkg'))

    def test_universal(self):
        code, err, calls, packages = self.run_fixture(['x86_64 arm64'] * 3)
        self.assertEqual(code, 0, err)
        self.assertEqual(calls[:3], ['lipo'] * 3)
        self.assertEqual(len(packages), 1)
        self.assertEqual(calls.count('ditto'), 3)
        self.assertEqual(calls.count('pkgbuild'), 4)
        self.assertEqual(calls.count('productbuild'), 1)
        self.assertEqual(calls[-1], 'pkgutil')

    def test_restrictive_umask_preserves_public_notices_and_bundle_modes(self):
        code, err, calls, packages = self.run_fixture(
            ['x86_64 arm64'] * 3, caller_umask=0o077, inspect_modes=True)
        self.assertEqual(code, 0, err)
        self.assertEqual(calls.count('pkgbuild'), 4)
        self.assertEqual(calls.count('productbuild'), 1)
        self.assertEqual(len(packages), 1)

    def test_missing_notice_prevents_distribution(self):
        for notice in NOTICE_FIXTURES:
            with self.subTest(notice=notice):
                code, err, calls, packages = self.run_fixture(
                    ['x86_64 arm64'] * 3, missing_notice=notice)
                self.assertNotEqual(code, 0)
                self.assertIn(Path(notice).name, err)
                self.assertNotIn('productbuild', calls)
                self.assertFalse(packages)

    def test_native_and_mixed_rejected_before_staging(self):
        for index in range(3):
            for arch in ('x86_64', 'arm64', 'not-mach-o'):
                with self.subTest(index=index, arch=arch):
                    architectures = ['x86_64 arm64'] * 3
                    architectures[index] = arch
                    code, err, calls, packages = self.run_fixture(architectures)
                    self.assertNotEqual(code, 0)
                    self.assertIn('universal', err)
                    self.assertNotIn('ditto', calls)
                    self.assertFalse(packages)

    def test_missing_binaries_rejected(self):
        for index in range(3):
            with self.subTest(index=index):
                code, err, calls, packages = self.run_fixture(['x86_64 arm64'] * 3, missing=index)
                self.assertNotEqual(code, 0)
                self.assertNotIn('ditto', calls)
                self.assertFalse(packages)

    def test_nonexecutable_binaries_rejected(self):
        for index in range(3):
            with self.subTest(index=index):
                code, err, calls, packages = self.run_fixture(['x86_64 arm64'] * 3, nonexecutable=index)
                self.assertNotEqual(code, 0)
                self.assertIn('non-executable', err)
                self.assertNotIn('ditto', calls)
                self.assertFalse(packages)

if __name__ == '__main__':
    unittest.main()
