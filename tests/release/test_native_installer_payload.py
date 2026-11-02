#!/usr/bin/env python3
"""Controller safety and actual Debian container roundtrip with fake binary bytes."""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import subprocess
import unittest
from unittest import mock

import test_distribution_docs as staging_tests

ROOT = staging_tests.ROOT
sys.path.insert(0, str(ROOT / 'scripts'))
import qualify_native_installer as controller


class NativeInstallerPayloadTests(unittest.TestCase):
    setUp = staging_tests.DistributionDocsTests.setUp
    git = staging_tests.DistributionDocsTests.git

    def test_wrong_platform_rejected_without_output(self):
        other = 'windows' if sys.platform != 'win32' else 'linux'
        output = self.root / 'packages/native'
        with self.assertRaisesRegex(ValueError, 'actual platform'):
            controller.qualify(other, self.build, output, self.commit)
        self.assertFalse(output.exists())

    def test_invalid_source_rejected_without_output(self):
        output = self.root / 'packages/native'
        with self.assertRaisesRegex(ValueError, 'source commit'):
            controller.qualify('linux', self.build, output, '../invalid')
        self.assertFalse(output.exists())

    def test_existing_output_preserved(self):
        native = 'windows' if sys.platform == 'win32' else 'macos' if sys.platform == 'darwin' else 'linux'
        output = self.root / 'packages/native'
        output.mkdir(parents=True)
        (output / 'keep').write_text('existing evidence\n')
        with self.assertRaisesRegex(ValueError, 'overwrite'):
            controller.qualify(native, self.build, output, self.commit)
        self.assertEqual((output / 'keep').read_text(), 'existing evidence\n')

    def partial_windows_install(self, *, cleanup_fails):
        output = self.root / 'packages/native'
        program_files = self.root / 'program-files'
        compiler = program_files / 'Inno Setup 6/iscc.exe'
        compiler.parent.mkdir(parents=True)
        compiler.write_bytes(b'synthetic compiler placeholder; never executed')
        cleanup_calls = []

        def fake_run(command, log, **kwargs):
            command = [str(arg) for arg in command]
            if command[0] == str(compiler):
                (output / 'synthetic-setup.exe').write_bytes(b'synthetic setup, never executed')
            elif Path(command[0]).name == 'unins000.exe':
                cleanup_calls.append(command)
                if cleanup_fails:
                    raise subprocess.CalledProcessError(9, command)
            elif any(arg.startswith('/DIR=') for arg in command):
                installed = Path(next(arg[5:] for arg in command if arg.startswith('/DIR=')))
                installed.mkdir()
                (installed / 'unins000.exe').write_bytes(b'synthetic uninstaller, never executed')
                raise subprocess.CalledProcessError(7, command)

        with mock.patch.object(controller, 'ROOT', self.root), \
             mock.patch.object(controller.sys, 'platform', 'win32'), \
             mock.patch.dict(os.environ, {'ProgramFiles(x86)': str(program_files)}), \
             mock.patch.object(controller, 'run', side_effect=fake_run):
            with self.assertRaises(subprocess.CalledProcessError) as failure:
                controller.qualify('windows', self.build, output, self.commit)
        self.assertEqual(failure.exception.returncode, 7, 'cleanup must retain the original install failure')
        self.assertEqual(len(cleanup_calls), 1, 'partial install must attempt uninstall exactly once')
        self.assertFalse((output / 'native-installer-receipt.json').exists())
        if cleanup_fails:
            self.assertTrue((output / 'uninstall-failure.log').is_file())

    def test_partial_windows_install_is_cleaned_up(self):
        self.partial_windows_install(cleanup_fails=False)

    def test_cleanup_failure_preserves_original_install_failure(self):
        self.partial_windows_install(cleanup_fails=True)

    def test_successful_windows_install_without_uninstaller_is_not_qualified(self):
        output = self.root / 'packages/native'
        program_files = self.root / 'program-files'
        compiler = program_files / 'Inno Setup 6/iscc.exe'
        compiler.parent.mkdir(parents=True)
        compiler.write_bytes(b'synthetic compiler placeholder; never executed')

        def fake_run(command, log, **kwargs):
            command = [str(arg) for arg in command]
            if command[0] == str(compiler):
                (output / 'synthetic-setup.exe').write_bytes(b'synthetic setup; never executed')
            elif any(arg.startswith('/DIR=') for arg in command):
                installed = Path(next(arg[5:] for arg in command if arg.startswith('/DIR=')))
                installed.mkdir()  # A success exit without an uninstaller is insufficient.

        with mock.patch.object(controller, 'ROOT', self.root), \
             mock.patch.object(controller.sys, 'platform', 'win32'), \
             mock.patch.dict(os.environ, {'ProgramFiles(x86)': str(program_files)}), \
             mock.patch.object(controller, 'run', side_effect=fake_run), \
             mock.patch.object(controller, 'verify_documents', return_value={'fixture': 'synthetic'}):
            with self.assertRaisesRegex(ValueError, 'uninstaller'):
                controller.qualify('windows', self.build, output, self.commit)
        self.assertFalse((output / 'native-installer-receipt.json').exists())

    @unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('dpkg-deb'),
                         'actual Debian container roundtrip only runs natively on Linux')
    def test_linux_actual_container_with_explicitly_synthetic_binary_payload(self):
        artifacts = self.build / 'CS80Reference_artefacts/Release'
        for relative in ('VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so',
                         'Standalone/CS-80 Reference'):
            binary = artifacts / relative
            binary.parent.mkdir(parents=True, exist_ok=True)
            binary.write_bytes(b'synthetic, not a loadable or audio-qualified binary\n')
            binary.chmod(0o755)
        output = self.root / 'packages/native'
        with mock.patch.object(controller, 'ROOT', self.root):
            controller.qualify('linux', self.build, output, self.commit)
        receipt = json.loads((output / 'native-installer-receipt.json').read_text())
        documents = json.loads((output / 'document-verification.json').read_text())
        self.assertEqual(receipt['source_commit'], self.commit)
        self.assertEqual(documents['source_commit'], self.commit)
        self.assertEqual(documents['document_count'], 7)
        self.assertIn('-CI-UNSIGNED-' + self.commit[:12], receipt['package'])
        package = output / receipt['package']
        self.assertEqual(receipt['package_sha256'], hashlib.sha256(package.read_bytes()).hexdigest())
        self.assertEqual(receipt['document_verification_sha256'], hashlib.sha256((output / 'document-verification.json').read_bytes()).hexdigest())
        self.assertIn('not a release', receipt['qualification'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
