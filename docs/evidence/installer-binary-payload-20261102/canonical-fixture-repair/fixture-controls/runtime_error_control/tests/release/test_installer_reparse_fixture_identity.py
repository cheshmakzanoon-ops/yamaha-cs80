#!/usr/bin/env python3
"""Exercise actual reparse assertions through a noncanonical temporary root.

The '..' ancestor is a portable spelling change, not a native junction claim.
No filesystem symlink, native paint, or installer execution is required.
"""
import stat
import unittest

import test_installer_binary_payload as payload


class ReparseFixtureIdentityTests(unittest.TestCase):
    def setUp(self):
        self.case = payload.InstallerBinaryPayloadTests('test_legacy_python_directory_reparse_artifact_is_rejected')
        raise RuntimeError('deliberate fixture control crash')
        self.addCleanup(self.case.doCleanups)
        parent = self.case.root / 'noncanonical-ancestor'
        parent.mkdir()
        self.case.root = parent / '..'
        self.case.build = self.case.root / 'build'
        self.case.inputs = self.case.build / 'CS80Reference_artefacts/Release'
        self.case.actual = self.case.root / 'expanded'
        self.assertNotEqual(self.case.root, self.case.root.resolve())
        self.assertTrue(self.case.root.samefile(self.case.root.resolve()))

    def test_noncanonical_artifact_retains_original_reparse_rejection(self):
        self.case.test_legacy_python_directory_reparse_artifact_is_rejected()

    def test_noncanonical_expansion_retains_original_reparse_rejection(self):
        self.case.test_legacy_python_directory_reparse_expansion_is_rejected()

    def test_noncanonical_nested_bundle_reparse_is_rejected(self):
        self.case.fixture('macos')
        target = self.case.inputs / 'VST3' / (payload.PRODUCT + '.vst3') / 'Contents'
        with self.case.simulated_directory_reparse(target):
            with self.assertRaisesRegex(ValueError, 'linked|Linked|reparse|junction'):
                self.case.module.capture_build_payload('macos', self.case.build)

    def test_selected_lexical_and_canonical_spellings_receive_attributes(self):
        self.case.fixture('windows')
        target = self.case.inputs.parent
        canonical = target.resolve()
        with self.case.simulated_directory_reparse(target):
            for selected in (target, canonical):
                with self.subTest(spelling=str(selected)):
                    self.assertTrue(getattr(selected.lstat(), 'st_file_attributes', 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT,
                                    'Registered fixture target did not receive reparse attributes')

    def test_unrelated_directory_does_not_receive_attributes(self):
        self.case.fixture('windows')
        target = self.case.inputs.parent
        with self.case.simulated_directory_reparse(target):
            self.assertFalse(getattr(self.case.actual.lstat(), 'st_file_attributes', 0)
                             & stat.FILE_ATTRIBUTE_REPARSE_POINT)


if __name__ == '__main__':
    unittest.main(verbosity=2)
