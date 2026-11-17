import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('artifact_seal', Path(__file__).with_name('artifact_seal.py'))
a = importlib.util.module_from_spec(spec); spec.loader.exec_module(a)


def bundle(path, value=b'candidate'):
    binary = path / a.BINARY
    binary.parent.mkdir(parents=True)
    binary.write_bytes(value); binary.chmod(0o755)
    metadata = path / a.MODULEINFO
    metadata.parent.mkdir(parents=True)
    metadata.write_text('{"Name":"CS-80 Reference"}\n'); metadata.chmod(0o644)
    return path


class SealTests(unittest.TestCase):
    def test_seal_restore_full_bundle_and_modes(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); build = root / 'build'; canonical = bundle(build / a.CANONICAL, b'baseline')
            sealed = root / 'sealed'
            os.utime(canonical / a.BINARY, ns=(1234567890000000000, 1234567890000000000))
            manifest = a.seal_bundle(canonical, sealed)
            (canonical / a.BINARY).write_bytes(b'candidate')
            a.restore_bundle(sealed, canonical, manifest, build_root=build)
            self.assertEqual((canonical / a.BINARY).read_bytes(), b'baseline')
            self.assertEqual((canonical / a.MODULEINFO).read_bytes(), (sealed / a.MODULEINFO).read_bytes())
            self.assertEqual(a.manifest_bundle(canonical), manifest)
            self.assertEqual((canonical / a.BINARY).stat().st_mtime_ns, (sealed / a.BINARY).stat().st_mtime_ns)
            self.assertEqual(list(canonical.parent.glob('.cs80-bundle-restore-*')), [])

    def test_seal_is_new_only_and_source_overlap_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); source = bundle(root / 'bundle'); sealed = root / 'sealed'
            a.seal_bundle(source, sealed)
            with self.assertRaises(ValueError): a.seal_bundle(source, sealed)
            with self.assertRaises(ValueError): a.seal_bundle(source, source / 'nested')

    def test_corruption_missing_extra_empty_directory_and_mode(self):
        for change in ('bytes', 'missing', 'extra', 'directory', 'mode', 'empty'):
            with self.subTest(change=change), tempfile.TemporaryDirectory() as t:
                p = bundle(Path(t) / 'bundle'); manifest = a.manifest_bundle(p)
                if change == 'bytes': (p / a.BINARY).write_bytes(b'corrupt')
                elif change == 'missing': (p / a.MODULEINFO).unlink()
                elif change == 'extra': (p / 'extra').write_bytes(b'new')
                elif change == 'directory': (p / 'empty-dir').mkdir()
                elif change == 'mode': (p / a.BINARY).chmod(0o744)
                else: (p / a.BINARY).write_bytes(b'')
                with self.assertRaises(ValueError): a.verify_bundle(p, manifest)

    def test_empty_bundle_and_nonregular_entries_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); empty = root / 'empty'; empty.mkdir()
            with self.assertRaises(ValueError): a.manifest_bundle(empty)
            p = bundle(root / 'bundle'); os.mkfifo(p / 'fifo')
            with self.assertRaises(ValueError): a.manifest_bundle(p)

    def test_symlinks_and_hardlinks_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); p = bundle(root / 'bundle')
            (p / 'escape').symlink_to(root)
            with self.assertRaises(ValueError): a.manifest_bundle(p)
            (p / 'escape').unlink()
            os.link(p / a.BINARY, p / 'hardlink')
            with self.assertRaises(ValueError): a.manifest_bundle(p)
            (p / 'hardlink').unlink()
            alias = root / 'alias'; alias.symlink_to(p, target_is_directory=True)
            with self.assertRaises(ValueError): a.manifest_bundle(alias)
            with self.assertRaises(ValueError): a.manifest_bundle(alias / 'Contents')

    def test_outside_or_noncanonical_destination_cannot_be_modified(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); build = root / 'build'; canonical = bundle(build / a.CANONICAL)
            sealed = root / 'sealed'; manifest = a.seal_bundle(canonical, sealed)
            outside = bundle(root / 'outside', b'unrelated')
            for destination, owned in ((outside, build), (canonical, root), (canonical, Path('/'))):
                with self.assertRaises(ValueError): a.restore_bundle(sealed, destination, manifest, build_root=owned)
            self.assertEqual((outside / a.BINARY).read_bytes(), b'unrelated')
            self.assertEqual((canonical / a.BINARY).read_bytes(), b'candidate')

    def test_corrupt_seal_fails_before_canonical_mutation(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); build = root / 'build'; canonical = bundle(build / a.CANONICAL)
            sealed = root / 'sealed'; manifest = a.seal_bundle(canonical, sealed)
            (sealed / a.BINARY).write_bytes(b'bad')
            with self.assertRaises(ValueError): a.restore_bundle(sealed, canonical, manifest, build_root=build)
            self.assertEqual((canonical / a.BINARY).read_bytes(), b'candidate')

    def test_failure_after_swap_rolls_back_original(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); build = root / 'build'; canonical = bundle(build / a.CANONICAL, b'baseline')
            sealed = root / 'sealed'; manifest = a.seal_bundle(canonical, sealed)
            (canonical / a.BINARY).write_bytes(b'original-active')
            original = a.verify_bundle
            def check(path, expected):
                if Path(path) == canonical: raise ValueError('injected final verification failure')
                return original(path, expected)
            with mock.patch.object(a, 'verify_bundle', side_effect=check), self.assertRaises(ValueError):
                a.restore_bundle(sealed, canonical, manifest, build_root=build)
            self.assertEqual((canonical / a.BINARY).read_bytes(), b'original-active')
            self.assertEqual(a.manifest_bundle(sealed), manifest)

    def test_parent_symlink_and_traversal_cannot_redirect_restore(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); build = root / 'build'; canonical = bundle(build / a.CANONICAL)
            sealed = root / 'sealed'; manifest = a.seal_bundle(canonical, sealed)
            alias = root / 'alias'; alias.symlink_to(build, target_is_directory=True)
            with self.assertRaises(ValueError): a.restore_bundle(sealed, alias / a.CANONICAL, manifest, build_root=alias)
            with self.assertRaises(ValueError): a.restore_bundle(sealed, build / '..' / 'build' / a.CANONICAL, manifest, build_root=build)


if __name__ == '__main__': unittest.main()
