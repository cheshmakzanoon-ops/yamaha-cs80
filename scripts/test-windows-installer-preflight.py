#!/usr/bin/env python3
"""Inert header fixtures only; no installer or packaged product is executed."""
import importlib.util
import os
from pathlib import Path
import shutil
import stat
from types import SimpleNamespace
from unittest import mock
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('packager', ROOT / 'scripts/package-ci-artifacts.py')
p = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(p)

HELPER_SPEC = importlib.util.spec_from_file_location('installer_preflight', ROOT / 'scripts/check-windows-installer.py')
h = importlib.util.module_from_spec(HELPER_SPEC)
HELPER_SPEC.loader.exec_module(h)


def find_iscc():
    found = shutil.which('ISCC.exe')
    if found:
        return found
    for variable in ('ProgramFiles(x86)', 'ProgramFiles'):
        directory = os.environ.get(variable)
        if directory:
            candidate = Path(directory) / 'Inno Setup 6/ISCC.exe'
            if candidate.is_file():
                return str(candidate)
    return None


class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cs80 installer space ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / 'build products'
        self.plugin = self.build / 'VST3' / (p.PRODUCT + '.vst3') / 'Contents/x86_64-win' / (p.PRODUCT + '.vst3')
        self.standalone = self.build / 'Standalone' / (p.PRODUCT + '.exe')
        for i, binary in enumerate((self.plugin, self.standalone)):
            binary.parent.mkdir(parents=True)
            binary.write_bytes(p.binary_fixture('windows', 'x86_64', i == 1))
        resource = self.plugin.parent.parent / 'Resources/moduleinfo.json'
        resource.parent.mkdir()
        resource.write_text('{}')

    def check(self, success):
        before = sorted(str(f.relative_to(self.root)) for f in self.root.rglob('*'))
        result = subprocess.run([sys.executable, str(ROOT / 'scripts/check-windows-installer.py'),
                                 '--build-dir', str(self.build)], text=True, capture_output=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        self.assertEqual(before, sorted(str(f.relative_to(self.root)) for f in self.root.rglob('*')))
        if not success:
            self.assertIn('preflight failed', result.stderr)
        return result

    def test_valid_pair_and_spaces(self):
        self.assertIn('x64 VST3 DLL', self.check(True).stdout)

    def test_resources_only_bundle(self):
        self.plugin.unlink()
        self.assertTrue(list(self.plugin.parent.parent.rglob('*')))
        self.check(False)

    def test_missing_standalone(self):
        self.standalone.unlink()
        self.check(False)

    def test_wrong_architecture_each_product(self):
        for binary in (self.plugin, self.standalone):
            original = binary.read_bytes()
            for machine in (0x14c, 0xaa64):
                with self.subTest(binary=binary.name, machine=machine):
                    wrong = bytearray(original)
                    wrong[68:70] = machine.to_bytes(2, 'little')
                    binary.write_bytes(wrong)
                    self.check(False)
            binary.write_bytes(original)

    def test_wrong_role_each_product(self):
        for binary in (self.plugin, self.standalone):
            original = binary.read_bytes()
            wrong = bytearray(original)
            wrong[86:88] = (0x2 if binary == self.plugin else 0x2002).to_bytes(2, 'little')
            binary.write_bytes(wrong)
            self.check(False)
            binary.write_bytes(original)

    def test_non_pe_empty_and_truncated(self):
        for binary in (self.plugin, self.standalone):
            original = binary.read_bytes()
            for data in (b'', b'not a PE image', original[:70]):
                with self.subTest(binary=binary.name, length=len(data)):
                    binary.write_bytes(data)
                    self.check(False)
            binary.write_bytes(original)

    def test_pe32_optional_header(self):
        wrong = bytearray(self.plugin.read_bytes())
        wrong[88:90] = (0x10b).to_bytes(2, 'little')
        self.plugin.write_bytes(wrong)
        self.check(False)

    @unittest.skipIf(os.name == 'nt', 'Native Windows symlink creation may require extra privileges')
    def test_linked_payload_rejected(self):
        external = self.root / 'external.vst3'
        shutil.copyfile(self.plugin, external)
        self.plugin.unlink()
        self.plugin.symlink_to(external)
        self.check(False)

    def test_reparse_points_rejected_before_descent(self):
        for kind in (stat.S_IFDIR, stat.S_IFREG):
            info = SimpleNamespace(st_mode=kind | 0o755, st_file_attributes=0x400)
            with self.subTest(kind=kind), mock.patch.object(Path, 'lstat', return_value=info):
                with mock.patch.object(h.os, 'scandir', side_effect=AssertionError('reparse tree scanned')) as scan:
                    with self.assertRaisesRegex(ValueError, 'reparse'):
                        h.inspect_tree(Path('inert-reparse-fixture'))
                    scan.assert_not_called()

    @unittest.skipUnless(os.name == 'nt', 'Native directory-junction fixture requires Windows')
    def test_native_directory_junction_rejected(self):
        external = self.root / 'external resources'
        external.mkdir()
        (external / 'never scanned.txt').write_text('inert fixture')
        junction = self.plugin.parent.parent / 'linked resources'
        result = subprocess.run(['cmd', '/d', '/c', 'mklink', '/J', str(junction), str(external)],
                                capture_output=True, text=True, timeout=15)
        if result.returncode:
            self.skipTest('Directory-junction creation unavailable: ' + result.stdout + result.stderr)
        self.addCleanup(os.rmdir, junction)
        self.check(False)

    def test_compile_guard_contract(self):
        script = (ROOT / 'installer/windows/cs80-reference.iss').read_text()
        guard = '#if Exec(PythonExe, AddQuotes(PreflightScript) + " --build-dir " + AddQuotes(BuildDir), SourcePath, 1) != 0'
        self.assertIn(guard, script)
        self.assertLess(script.index(guard), script.index('[Setup]'))
        self.assertIn('#error Windows installer payload preflight failed', script)
        self.assertNotIn('[Run]', script)

    @unittest.skipUnless(os.name == 'nt' and find_iscc(), 'Native ISCC compilation requires Windows and Inno Setup 6')
    def test_native_inno_accepts_valid_and_rejects_bad_payloads(self):
        source = self.root / 'source checkout'
        for relative in ('installer/windows/cs80-reference.iss', 'scripts/check-windows-installer.py',
                         'scripts/package-ci-artifacts.py', 'LICENSE',
                         'assets/fonts/barlow-condensed/OFL.txt', 'assets/fonts/barlow-condensed/SOURCE.txt',
                         'src/vendor/mts-esp/LICENSE'):
            destination = source / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        juce = self.root / 'JUCE-LICENSE.md'
        juce.write_text('inert fixture notice')
        original = self.plugin.read_bytes()
        # Regression witness only: compile a temporary copy with exactly the
        # new preflight block removed, retaining the original [Files] wildcard.
        guarded_script = source / 'installer/windows/cs80-reference.iss'
        guarded_text = guarded_script.read_text()
        begin = guarded_text.index('; Compile-time read-only guard')
        end = guarded_text.index('[Setup]', begin)
        baseline_script = guarded_script.with_name('baseline-resources-only.iss')
        baseline_script.write_text(guarded_text[:begin] + guarded_text[end:])
        for case in ('valid', 'resources-only', 'wrong-role', 'wrong-architecture', 'missing-python'):
            with self.subTest(case=case):
                self.plugin.write_bytes(original)
                if case == 'resources-only':
                    self.plugin.unlink()
                elif case in ('wrong-role', 'wrong-architecture'):
                    wrong = bytearray(original)
                    if case == 'wrong-role':
                        wrong[86:88] = (2).to_bytes(2, 'little')
                    else:
                        wrong[68:70] = (0xaa64).to_bytes(2, 'little')
                    self.plugin.write_bytes(wrong)
                out = self.root / ('installer output ' + case)
                command = [find_iscc(), '/Qp', '/DAppVersion=1.0.0', '/DBuildDir=' + str(self.build),
                           '/DOutDir=' + str(out), '/DJuceLicense=' + str(juce),
                           '/DPythonExe=' + (str(self.root / 'missing-python.exe') if case == 'missing-python' else sys.executable),
                           str(source / 'installer/windows/cs80-reference.iss')]
                if case == 'resources-only':
                    baseline_out = self.root / 'installer output baseline resources-only'
                    baseline_command = [('/DOutDir=' + str(baseline_out)) if arg.startswith('/DOutDir=')
                                        else arg for arg in command[:-1]] + [str(baseline_script)]
                    baseline_result = subprocess.run(baseline_command, capture_output=True, text=True, timeout=60)
                    self.assertEqual(baseline_result.returncode, 0,
                                     'Baseline wildcard witness failed: ' + baseline_result.stdout + baseline_result.stderr)
                    self.assertTrue(list(baseline_out.glob('*.exe')))
                    # The compiler produced this inert fixture installer; it is
                    # never launched. The candidate below must reject the same
                    # resources-only payload before creating an installer.
                result = subprocess.run(command, capture_output=True, text=True, timeout=60)
                if case == 'valid':
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertTrue(list(out.glob('*.exe')))
                else:
                    self.assertNotEqual(result.returncode, 0)
                    if case != 'missing-python':
                        self.assertIn('Windows installer payload preflight failed', result.stdout + result.stderr)
                    self.assertFalse(list(out.glob('*.exe')))


if __name__ == '__main__':
    unittest.main()
