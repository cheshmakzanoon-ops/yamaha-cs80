#!/usr/bin/env python3
"""Pre-registered architecture-label contract; native lipo execution is separate."""
from __future__ import annotations
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MacPackageArchitectureTests(unittest.TestCase):
    def helper(self):
        spec = importlib.util.spec_from_file_location('cs80_macos_package_architecture', ROOT / 'scripts/macos_package_architecture.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_native_labels_are_not_universal(self):
        helper = self.helper()
        self.assertEqual(helper.classify_slices(['arm64'] * 3), 'arm64')
        self.assertEqual(helper.classify_slices(['x86_64'] * 3), 'x86_64')

    def test_universal_requires_both_slices_in_every_binary(self):
        helper = self.helper()
        self.assertEqual(helper.classify_slices(['arm64 x86_64', 'x86_64 arm64', 'arm64 x86_64']), 'universal')
        for slices in (['arm64 x86_64', 'arm64', 'arm64 x86_64'], ['arm64', 'x86_64', 'arm64']):
            with self.subTest(slices=slices), self.assertRaises(ValueError):
                helper.classify_slices(slices)

    def test_empty_unknown_duplicate_and_incomplete_inventory_rejected(self):
        helper = self.helper()
        for slices in ([], ['arm64'], ['arm64'] * 4, ['', '', ''], ['i386'] * 3, ['arm64 arm64'] * 3):
            with self.subTest(slices=slices), self.assertRaises(ValueError):
                helper.classify_slices(slices)


if __name__ == '__main__':
    unittest.main(verbosity=2)
