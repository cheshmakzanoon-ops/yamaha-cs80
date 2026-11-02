#!/usr/bin/env python3
"""Synthetic build/install trees only; these are not native binary executions."""
from __future__ import annotations
import hashlib
import copy
import importlib.util
import os
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PRODUCT = 'CS-80 Reference'
requires_posix = unittest.skipIf(os.name == 'nt',
    'POSIX permission/symlink fixture only; native Windows checks use Standalone byte identity')


class InstallerBinaryPayloadTests(unittest.TestCase):
    def setUp(self):
        helper = ROOT / 'scripts/verify_installer_binaries.py'
        self.assertTrue(helper.is_file(), 'Preregistered binary payload verifier is not implemented')
        spec = importlib.util.spec_from_file_location('binary_payload_verifier', helper)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.temp = tempfile.TemporaryDirectory(prefix='cs80-binary-fixture-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / 'build'
        self.inputs = self.build / 'CS80Reference_artefacts/Release'
        self.actual = self.root / 'expanded'
        self.actual.mkdir()

    def file(self, path, data=b'explicitly synthetic nonloadable binary fixture\n', mode=0o755):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        path.chmod(mode)
        return path

    def fixture(self, platform):
        if platform == 'linux':
            vst = self.inputs / 'VST3' / (PRODUCT + '.vst3')
            self.file(vst / 'Contents/x86_64-linux' / (PRODUCT + '.so'))
            self.file(vst / 'Contents/Resources/moduleinfo.json', b'{"fixture":true}\n', 0o644)
            app = self.file(self.inputs / 'Standalone' / PRODUCT)
            manifest = self.module.capture_build_payload(platform, self.build)
            target = self.actual / 'usr/lib/vst3' / vst.name
            target.parent.mkdir(parents=True)
            shutil.copytree(vst, target, symlinks=True)
            standalone = self.actual / 'usr/bin/cs80-reference'
            standalone.parent.mkdir(parents=True)
            shutil.copy2(app, standalone)
            standalone.chmod(0o755)
        elif platform == 'macos':
            definitions = [('vst3', 'VST3', '.vst3', '/Library/Audio/Plug-Ins/VST3'),
                           ('au', 'AU', '.component', '/Library/Audio/Plug-Ins/Components'),
                           ('standalone', 'Standalone', '.app', '/Applications')]
            for key, folder, extension, location in definitions:
                bundle = self.inputs / folder / (PRODUCT + extension)
                self.file(bundle / 'Contents/MacOS' / PRODUCT, b'synthetic already-staged signed-or-unsigned input\n')
                self.file(bundle / 'Contents/Info.plist', b'synthetic plist\n', 0o644)
                component = self.actual / (('app' if key == 'standalone' else key) + '.pkg')
                (component / 'Payload').mkdir(parents=True)
                shutil.copytree(bundle, component / 'Payload' / bundle.name)
                identifier = 'app' if key == 'standalone' else key
                (component / 'PackageInfo').write_text(
                    f'<pkg-info identifier="com.referenceinstruments.cs80.{identifier}" install-location="{location}"/>')
            manifest = self.module.capture_build_payload(platform, self.build)
        else:
            app = self.file(self.inputs / 'Standalone' / (PRODUCT + '.exe'))
            manifest = self.module.capture_build_payload(platform, self.build)
            shutil.copy2(app, self.actual / app.name)
        return manifest

    def verify(self, platform, manifest):
        return self.module.verify_installed_payload(platform, manifest, self.build, self.actual)

    @requires_posix
    def test_linux_exact_inventory_hashes_and_explicit_scope(self):
        manifest = self.fixture('linux')
        before = {str(p): p.read_bytes() for p in self.inputs.rglob('*') if p.is_file()}
        receipt = self.verify('linux', manifest)
        self.assertEqual(receipt['verified_components'], ['standalone', 'vst3'])
        self.assertIn('not compilation/source equivalence', receipt['qualification'])
        self.assertEqual(before, {str(p): p.read_bytes() for p in self.inputs.rglob('*') if p.is_file()})
        self.assertIn(hashlib.sha256(b'explicitly synthetic nonloadable binary fixture\n').hexdigest(), str(manifest))

    @requires_posix
    def test_linux_missing_binary_rejected(self):
        manifest = self.fixture('linux')
        next(self.actual.rglob('*.so')).unlink()
        with self.assertRaisesRegex(ValueError, 'inventory|missing'):
            self.verify('linux', manifest)

    @requires_posix
    def test_linux_stale_or_changed_bytes_rejected(self):
        manifest = self.fixture('linux')
        (self.actual / 'usr/bin/cs80-reference').write_bytes(b'wrong stale binary\n')
        with self.assertRaisesRegex(ValueError, 'bytes|hash'):
            self.verify('linux', manifest)

    @requires_posix
    def test_linux_unexpected_bundle_resource_rejected(self):
        manifest = self.fixture('linux')
        self.file(self.actual / 'usr/lib/vst3' / (PRODUCT + '.vst3') / 'extra.so')
        with self.assertRaisesRegex(ValueError, 'inventory'):
            self.verify('linux', manifest)

    @requires_posix
    def test_linux_executable_mode_loss_rejected(self):
        manifest = self.fixture('linux')
        next(self.actual.rglob('*.so')).chmod(0o644)
        with self.assertRaisesRegex(ValueError, 'mode'):
            self.verify('linux', manifest)

    @requires_posix
    def test_debian_standalone_mode_normalization_is_registered(self):
        manifest = self.fixture('linux')
        source = self.inputs / 'Standalone' / PRODUCT
        source.chmod(0o750)
        manifest = self.module.capture_build_payload('linux', self.build)
        self.assertEqual(self.verify('linux', manifest)['verified_components'], ['standalone', 'vst3'])
        self.assertIn('0755', str(manifest['transformations']))

    @requires_posix
    def test_snapshot_cannot_be_replaced_after_input_changes(self):
        manifest = self.fixture('linux')
        changed = b'changed after pre-package capture\n'
        (self.inputs / 'Standalone' / PRODUCT).write_bytes(changed)
        (self.actual / 'usr/bin/cs80-reference').write_bytes(changed)
        with self.assertRaisesRegex(ValueError, 'bytes|hash'):
            self.verify('linux', manifest)

    @requires_posix
    def test_forged_manifest_cannot_certify_stale_payload(self):
        manifest = self.fixture('linux')
        forged = copy.deepcopy(manifest)
        replacement = b'forged but self-consistent stale payload\n'
        (self.actual / 'usr/bin/cs80-reference').write_bytes(replacement)
        row = forged['components']['standalone']['entries'][0]
        row.update(sha256=hashlib.sha256(replacement).hexdigest(), bytes=len(replacement))
        with self.assertRaisesRegex(ValueError, 'build input|capture contract'):
            self.verify('linux', forged)

    @requires_posix
    def test_external_source_symlink_is_rejected(self):
        self.fixture('linux')
        bundle = self.inputs / 'VST3' / (PRODUCT + '.vst3')
        outside = self.file(self.root / 'outside.so')
        (bundle / 'external.so').symlink_to(outside)
        with self.assertRaisesRegex(ValueError, 'link'):
            self.module.capture_build_payload('linux', self.build)

    @requires_posix
    def test_payload_symlink_substitution_is_rejected(self):
        manifest = self.fixture('linux')
        target = next(self.actual.rglob('*.so'))
        target.unlink()
        target.symlink_to(self.inputs / 'Standalone' / PRODUCT)
        with self.assertRaisesRegex(ValueError, 'type|link'):
            self.verify('linux', manifest)

    @requires_posix
    def test_macos_three_bundles_and_install_destinations(self):
        manifest = self.fixture('macos')
        receipt = self.verify('macos', manifest)
        self.assertEqual(receipt['verified_components'], ['au', 'standalone', 'vst3'])
        self.assertIn('signing validity', receipt['qualification'])

    @requires_posix
    def test_macos_wrong_install_location_rejected(self):
        manifest = self.fixture('macos')
        metadata = self.actual / 'au.pkg/PackageInfo'
        metadata.write_text(metadata.read_text().replace('/Library/Audio/Plug-Ins/Components', '/Applications'))
        with self.assertRaisesRegex(ValueError, 'install.location'):
            self.verify('macos', manifest)

    @requires_posix
    def test_macos_missing_component_rejected(self):
        manifest = self.fixture('macos')
        shutil.rmtree(self.actual / 'au.pkg')
        with self.assertRaisesRegex(ValueError, 'component|missing'):
            self.verify('macos', manifest)

    @requires_posix
    def test_macos_changed_post_capture_signature_bytes_are_not_waived(self):
        manifest = self.fixture('macos')
        next((self.actual / 'app.pkg').rglob('MacOS')).joinpath(PRODUCT).write_bytes(b'changed signature or stripped bytes\n')
        with self.assertRaisesRegex(ValueError, 'bytes|hash'):
            self.verify('macos', manifest)

    def test_windows_only_actual_standalone_scope(self):
        manifest = self.fixture('windows')
        self.file(self.actual / 'unins000.exe', b'synthetic generated uninstaller, not executed\n')
        receipt = self.verify('windows', manifest)
        self.assertEqual(receipt['verified_components'], ['standalone'])
        self.assertIn('VST3 not installed', receipt['platform_limit'])
        self.assertIn('NTFS ACL', receipt['platform_limit'])

    def test_windows_stale_standalone_rejected(self):
        manifest = self.fixture('windows')
        (self.actual / (PRODUCT + '.exe')).write_bytes(b'stale build\n')
        with self.assertRaisesRegex(ValueError, 'bytes|hash'):
            self.verify('windows', manifest)

    def test_windows_missing_standalone_rejected(self):
        manifest = self.fixture('windows')
        (self.actual / (PRODUCT + '.exe')).unlink()
        with self.assertRaisesRegex(ValueError, 'missing'):
            self.verify('windows', manifest)

    def test_platform_identity_mismatch_rejected(self):
        manifest = self.fixture('linux')
        with self.assertRaisesRegex(ValueError, 'platform'):
            self.verify('windows', manifest)

    def test_unknown_platform_rejected(self):
        with self.assertRaisesRegex(ValueError, 'platform'):
            self.module.capture_build_payload('unknown', self.build)

    @requires_posix
    def test_missing_primary_build_binary_is_not_a_resource_only_bundle(self):
        for platform in ('linux', 'macos'):
            with self.subTest(platform=platform):
                if self.inputs.exists():
                    shutil.rmtree(self.inputs)
                shutil.rmtree(self.actual); self.actual.mkdir()
                self.fixture(platform)
                primary = (self.inputs / 'VST3' / (PRODUCT + '.vst3') /
                           ('Contents/x86_64-linux/' + PRODUCT + '.so' if platform == 'linux'
                            else 'Contents/MacOS/' + PRODUCT))
                primary.unlink()
                with self.assertRaisesRegex(ValueError, 'primary.*binary'):
                    self.module.capture_build_payload(platform, self.build)

    def test_empty_primary_build_binary_is_rejected(self):
        self.fixture('windows')
        (self.inputs / 'Standalone' / (PRODUCT + '.exe')).write_bytes(b'')
        with self.assertRaisesRegex(ValueError, 'primary.*binary'):
            self.module.capture_build_payload('windows', self.build)

    @requires_posix
    def test_linked_artifact_root_is_rejected(self):
        self.fixture('linux')
        moved = self.root / 'moved-artifacts'
        self.inputs.rename(moved)
        self.inputs.symlink_to(moved, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'Linked|linked'):
            self.module.capture_build_payload('linux', self.build)

    @requires_posix
    def test_linked_expansion_root_is_rejected(self):
        manifest = self.fixture('linux')
        moved = self.root / 'moved-expanded'
        self.actual.rename(moved)
        self.actual.symlink_to(moved, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'Linked|linked'):
            self.verify('linux', manifest)

    @requires_posix
    def test_internal_relative_resource_link_is_preserved(self):
        self.fixture('linux')
        relative = Path('usr/lib/vst3') / (PRODUCT + '.vst3') / 'Contents/Resources'
        for folder in (self.inputs / 'VST3' / (PRODUCT + '.vst3') / 'Contents/Resources',
                       self.actual / relative):
            (folder / 'alias.json').symlink_to('moduleinfo.json')
        manifest = self.module.capture_build_payload('linux', self.build)
        self.assertEqual(self.verify('linux', manifest)['verified_components'], ['standalone', 'vst3'])

    @requires_posix
    def test_malformed_package_info_is_an_explicit_rejection(self):
        manifest = self.fixture('macos')
        (self.actual / 'au.pkg/PackageInfo').write_text('<pkg-info malformed')
        try:
            self.verify('macos', manifest)
        except Exception as error:
            self.assertIsInstance(error, ValueError)
            self.assertIn('PackageInfo', str(error))
        else:
            self.fail('Malformed PackageInfo was accepted')

    @requires_posix
    def test_internal_relative_link_target_change_is_rejected(self):
        self.fixture('linux')
        folders = (self.inputs / 'VST3' / (PRODUCT + '.vst3') / 'Contents/Resources',
                   self.actual / 'usr/lib/vst3' / (PRODUCT + '.vst3') / 'Contents/Resources')
        for folder in folders:
            shutil.copy2(folder / 'moduleinfo.json', folder / 'second.json')
        (folders[0] / 'alias.json').symlink_to('moduleinfo.json')
        (folders[1] / 'alias.json').symlink_to('second.json')
        manifest = self.module.capture_build_payload('linux', self.build)
        with self.assertRaisesRegex(ValueError, 'link'):
            self.verify('linux', manifest)

    @requires_posix
    def test_special_source_file_is_rejected_without_opening_it(self):
        self.fixture('linux')
        fifo = self.inputs / 'VST3' / (PRODUCT + '.vst3') / 'unexpected.fifo'
        os.mkfifo(fifo)
        with self.assertRaisesRegex(ValueError, 'Special.file'):
            self.module.capture_build_payload('linux', self.build)


if __name__ == '__main__':
    unittest.main(verbosity=2)
