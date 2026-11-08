#!/usr/bin/env python3
"""Inert header tests for downloadable-artifact platform/architecture identity.

Never executes a fixture. Run with --packager PATH for before/after comparison.
"""
import argparse
import importlib.util
import os
import subprocess
from unittest import mock
from pathlib import Path
import struct
import tempfile
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('--packager', type=Path, default=Path(__file__).with_name('package-ci-artifacts.py'))
args, rest = parser.parse_known_args()
spec = importlib.util.spec_from_file_location('packager', args.packager)
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


def fixture(system, arch, standalone):
    if system == 'linux':
        data = bytearray(64)
        data[:7] = b'\x7fELF\x02\x01\x01'
        struct.pack_into('<HH', data, 16, 2 if standalone else 3, 62 if arch == 'x86_64' else 183)
    elif system == 'windows':
        data = bytearray(200)
        data[:2] = b'MZ'
        struct.pack_into('<I', data, 60, 64)
        data[64:70] = b'PE\x00\x00\x64\x86'
        struct.pack_into('<H', data, 84, 112)
        struct.pack_into('<HH', data, 86, 2 if standalone else 0x2002, 0x20b)
    else:
        data = bytearray(32)
        struct.pack_into('<IIII', data, 0, 0xfeedfacf, 0x1000007 if arch == 'x86_64' else 0x100000c, 0, 2 if standalone else 8)
    return data


def fat(arch, standalone, fat64=False):
    data = fixture('macos', arch, standalone)
    width = 32 if fat64 else 20
    header = struct.pack('>II', 0xcafebabf if fat64 else 0xcafebabe, 1)
    cpu = 0x1000007 if arch == 'x86_64' else 0x100000c
    entry = struct.pack('>IIQQII', cpu, 0, 8 + width, len(data), 0, 0) if fat64 else struct.pack('>IIIII', cpu, 0, 8 + width, len(data), 0)
    return bytearray(header + entry + data)


class IdentityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cs80-inert-headers-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name) / 'products'

    def products(self, system, arch):
        vst = self.base / 'VST3' / (packager.PRODUCT + '.vst3')
        if system == 'macos':
            bundles = [vst, self.base / 'AU' / (packager.PRODUCT + '.component'), self.base / 'Standalone' / (packager.PRODUCT + '.app')]
            paths = [b / 'Contents/MacOS' / packager.PRODUCT for b in bundles]
            for b in bundles:
                (b / 'Contents').mkdir(parents=True, exist_ok=True)
                (b / 'Contents/Info.plist').write_text('inert plist')
        elif system == 'linux':
            paths = [vst / f'Contents/{arch}-linux' / (packager.PRODUCT + '.so'), self.base / 'Standalone' / packager.PRODUCT]
        else:
            paths = [vst / 'Contents/x86_64-win' / (packager.PRODUCT + '.vst3'), self.base / 'Standalone' / (packager.PRODUCT + '.exe')]
        for index, p in enumerate(paths):
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(fixture(system, arch, index == len(paths) - 1))
            p.chmod(0o755)
        return paths

    def test_supported_targets(self):
        for system, arch in [('linux', 'x86_64'), ('linux', 'arm64'), ('windows', 'x86_64'), ('macos', 'x86_64'), ('macos', 'arm64')]:
            with self.subTest(system=system, arch=arch):
                self.products(system, arch)
                self.assertTrue(packager.collect_products(self.base, system, arch))

    def test_wrong_target_each_product(self):
        for system, arch in [('linux', 'x86_64'), ('linux', 'arm64'), ('windows', 'x86_64'), ('macos', 'x86_64'), ('macos', 'arm64')]:
            paths = self.products(system, arch)
            for index, path in enumerate(paths):
                with self.subTest(system=system, arch=arch, product=index):
                    original = path.read_bytes()
                    wrong = fixture(system, 'arm64' if arch == 'x86_64' else 'x86_64', index == len(paths) - 1)
                    if system == 'windows':
                        wrong[68:70] = b'\x64\xaa'
                    path.write_bytes(wrong)
                    with self.assertRaises(ValueError):
                        packager.collect_products(self.base, system, arch)
                    path.write_bytes(original)

    def test_wrong_image_kind(self):
        for system in ['linux', 'windows', 'macos']:
            paths = self.products(system, 'x86_64')
            for index, path in enumerate(paths):
                with self.subTest(system=system, product=index):
                    original = path.read_bytes()
                    wrong = bytearray(original)
                    if system == 'linux':
                        wrong[16:18] = (1).to_bytes(2, 'little')  # relocatable .o
                    elif system == 'windows':
                        wrong[86:88] = (0x2002 if index == len(paths) - 1 else 2).to_bytes(2, 'little')
                    else:
                        wrong[12:16] = (1).to_bytes(4, 'little')  # MH_OBJECT
                    path.write_bytes(wrong)
                    with self.assertRaises(ValueError):
                        packager.collect_products(self.base, system, 'x86_64')
                    path.write_bytes(original)

    def test_invalid_headers(self):
        for system in ['linux', 'windows', 'macos']:
            paths = self.products(system, 'x86_64')
            good = paths[0].read_bytes()
            for payload in [b'not a plugin', good[:20], bytes(len(good))]:
                with self.subTest(system=system, payload=payload[:8]):
                    paths[0].write_bytes(payload)
                    with self.assertRaises(ValueError):
                        packager.collect_products(self.base, system, 'x86_64')
            paths[0].write_bytes(good)

    def test_other_elf_class_endian(self):
        paths = self.products('linux', 'x86_64')
        for offset, value in [(4, 1), (5, 2), (6, 0)]:
            bad = fixture('linux', 'x86_64', False)
            bad[offset] = value
            paths[0].write_bytes(bad)
            with self.assertRaises(ValueError):
                packager.collect_products(self.base, 'linux', 'x86_64')

    def test_elf_pie_standalone(self):
        paths = self.products('linux', 'x86_64')
        paths[-1].write_bytes(fixture('linux', 'x86_64', False))
        self.assertTrue(packager.collect_products(self.base, 'linux', 'x86_64'))

    def test_pe_bad_offset_and_32bit(self):
        paths = self.products('windows', 'x86_64')
        for offset in [0, 63, 2**32 - 1]:
            bad = fixture('windows', 'x86_64', False)
            struct.pack_into('<I', bad, 60, offset)
            paths[0].write_bytes(bad)
            with self.assertRaises(ValueError):
                packager.collect_products(self.base, 'windows', 'x86_64')
        bad = fixture('windows', 'x86_64', False)
        bad[88:90] = b'\x0b\x01'
        paths[0].write_bytes(bad)
        with self.assertRaises(ValueError):
            packager.collect_products(self.base, 'windows', 'x86_64')

    def test_pe_optional_header_size(self):
        paths = self.products('windows', 'x86_64')
        for size in [0, 2, 111, 113, 65535]:
            bad = fixture('windows', 'x86_64', False)
            struct.pack_into('<H', bad, 84, size)
            paths[0].write_bytes(bad)
            with self.assertRaises(ValueError):
                packager.collect_products(self.base, 'windows', 'x86_64')

    def test_universal_macho(self):
        for arch in ['x86_64', 'arm64']:
            paths = self.products('macos', arch)
            for fat64 in [False, True]:
                for i, path in enumerate(paths):
                    path.write_bytes(fat(arch, i == len(paths) - 1, fat64))
                self.assertTrue(packager.collect_products(self.base, 'macos', arch))
                paths[0].write_bytes(fat('arm64' if arch == 'x86_64' else 'x86_64', False, fat64))
                with self.assertRaises(ValueError):
                    packager.collect_products(self.base, 'macos', arch)

    def test_malformed_fat_slices(self):
        paths = self.products('macos', 'arm64')
        for offset, value in [(4, 65), (8, 0x1000007), (16, 0), (20, 2**32 - 1), (32, 0x1000007)]:
            bad = fat('arm64', False)
            struct.pack_into('>I' if offset != 32 else '<I', bad, offset, value)
            paths[0].write_bytes(bad)
            with self.assertRaises(ValueError):
                packager.collect_products(self.base, 'macos', 'arm64')

    def test_windows_reparse_rejected_before_walk_or_binary_read(self):
        paths = self.products('windows', 'x86_64')
        bundle = self.base / 'VST3' / (packager.PRODUCT + '.vst3')
        nested = bundle / 'Contents/Resources/junction'
        nested.mkdir(parents=True)
        (nested / 'unshipped.txt').write_text('must not be traversed')
        original_lstat, original_walk = Path.lstat, packager.os.walk

        class WindowsStat:
            def __init__(self, original):
                self.original = original
                self.st_file_attributes = getattr(original, 'st_file_attributes', 0) | 0x400
            def __getattr__(self, name):
                return getattr(self.original, name)

        for marked in (bundle, nested, paths[0], paths[1]):
            with self.subTest(marked=marked):
                walked = []
                def lstat(path):
                    value = original_lstat(path)
                    return WindowsStat(value) if path == marked else value
                def walk(*args, **kwargs):
                    for row in original_walk(*args, **kwargs):
                        walked.append(Path(row[0]))
                        yield row
                with mock.patch.object(Path, 'lstat', lstat), mock.patch.object(packager.os, 'walk', walk), \
                     mock.patch.object(packager, 'verify_binary') as verify:
                    with self.assertRaisesRegex(ValueError, 'reparse'):
                        packager.collect_products(self.base, 'windows', 'x86_64')
                    verify.assert_not_called()
                self.assertNotIn(marked, walked)

    @unittest.skipUnless(os.name == 'nt', 'Native Windows junction fixture')
    def test_native_windows_junction_is_rejected(self):
        self.products('windows', 'x86_64')
        junction = self.base / 'VST3' / (packager.PRODUCT + '.vst3') / 'Contents/junction'
        outside = self.base.parent / 'outside'
        outside.mkdir()
        (outside / 'unshipped.txt').write_text('external fixture')
        result = subprocess.run(['cmd', '/d', '/c', 'mklink', '/J', str(junction), str(outside)],
                                text=True, capture_output=True, timeout=15)
        if result.returncode:
            self.skipTest('Junction creation unavailable: ' + result.stderr.strip())
        try:
            with mock.patch.object(packager, 'verify_binary') as verify:
                with self.assertRaisesRegex(ValueError, 'reparse'):
                    packager.collect_products(self.base, 'windows', 'x86_64')
                verify.assert_not_called()
        finally:
            junction.rmdir()
        self.assertEqual((outside / 'unshipped.txt').read_text(), 'external fixture')

    def test_rejected_before_output(self):
        paths = self.products('linux', 'x86_64')
        paths[-1].write_bytes(fixture('linux', 'arm64', True))
        out = self.base.parent / 'output'
        with self.assertRaises(ValueError):
            packager.package(self.base, out, 'linux', 'x86_64', 'a' * 40, {}, {})
        self.assertFalse(out.exists())


if __name__ == '__main__':
    unittest.main(argv=['test-package-binary-identity.py'] + rest)
