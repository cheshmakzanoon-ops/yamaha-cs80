#!/usr/bin/env python3
"""Keep native package failure evidence without changing any acceptance rule."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW_PATH = ROOT / '.github/workflows/build.yml'


class InstallerEvidenceWorkflowTests(unittest.TestCase):
    def setUp(self):
        text = WORKFLOW_PATH.read_text()
        self.blocks = re.findall(r'^      - name: Upload test and JUCE source evidence\n((?: {8}.*\n|[ \t]*\n)*)', text, re.MULTILINE)
        self.assertEqual(len(self.blocks), 3, 'retain Linux/macOS/Windows always-upload evidence steps')
        names = [re.search(r'^          name: (.*)$', block, re.MULTILINE).group(1) for block in self.blocks]
        self.assertEqual(set(names), {'cs80-linux-test-evidence', 'cs80-${{ matrix.runner }}-test-evidence', 'cs80-windows-test-evidence'})

    def test_native_evidence_uploads_run_even_after_failure(self):
        for index, block in enumerate(self.blocks):
            with self.subTest(platform_index=index):
                self.assertRegex(block, r'(?m)^        if: always\(\)$')
                self.assertRegex(block, r'(?m)^        uses: actions/upload-artifact@')

    def test_frozen_binary_input_manifest_is_retained(self):
        for index, block in enumerate(self.blocks):
            with self.subTest(platform_index=index):
                self.assertIn('            packages/native-installer-ci/binary-input-manifest.json\n', block)

    def test_binary_verification_receipt_is_retained(self):
        for index, block in enumerate(self.blocks):
            with self.subTest(platform_index=index):
                self.assertIn('            packages/native-installer-ci/binary-verification.json\n', block)

    def test_prior_test_notice_and_visual_evidence_is_preserved(self):
        for index, block in enumerate(self.blocks):
            for path in ('build/Testing/Temporary/LastTest.log', 'build/ci-juce-provenance.txt',
                         'build/ui-smoke/golden/*.png', 'packages/native-installer-ci/*.log',
                         'packages/native-installer-ci/*receipt.json',
                         'packages/native-installer-ci/document-verification.json'):
                with self.subTest(platform_index=index, path=path):
                    self.assertIn('            ' + path + '\n', block)


if __name__ == '__main__':
    unittest.main(verbosity=2)
