"""Independent safety/identity controls for pinned verifier-fixture installation."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import stat
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[2]
NAMES = (
    'audit/latency.csv', 'audit/alias.csv', 'audit/fx.csv', 'audit/phrase.csv',
    'audit/click-causality.csv', 'audit-summary.md', 'audit-summary.json',
    'audit-provenance.json', 'audit.complete', 'audit-run.log',
)


def archive(names=NAMES, symlink=False):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_STORED) as z:
        for index, name in enumerate(names):
            info = zipfile.ZipInfo(name)
            if symlink and index == 0:
                info.create_system = 3
                info.external_attr = (stat.S_IFLNK | 0o777) << 16
            z.writestr(info, b'fixture')
    return stream.getvalue()


class IntegrityFixtureMaterializationTests(unittest.TestCase):
    def helper(self):
        path = ROOT / 'scripts/materialize_release_integrity_fixtures.py'
        self.assertTrue(path.is_file(), 'Required pinned-fixture materializer is absent')
        spec = importlib.util.spec_from_file_location('cs80_fixture_materializer', path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_pins_match_preregistered_contract(self):
        m = self.helper()
        contract = json.loads((ROOT / 'tests/release/integrity-fixture-contract.json').read_text())
        self.assertEqual(m.AUDIT_COMMIT, '0151b8699febc47f07c3b4d8cc9ebbf642692a5c')
        self.assertEqual(m.CPU_COMMIT, '3a85d66aee90f8cfaee69d875ee21997b61ad940')
        self.assertEqual(m.AUDIT_SHA, contract['audit_sha256'])
        self.assertEqual(m.AUDIT_BYTES, 227720)
        self.assertEqual(set(m.AUDIT_MEMBERS), set(NAMES))

    def test_real_fixtures_materialize_with_exact_identities(self):
        m = self.helper()
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / 'fixtures'
            receipt = m.materialize(ROOT, target)
            self.assertEqual(receipt['audit_sha256'], m.AUDIT_SHA)
            self.assertEqual(receipt['cpu_sha256'], m.CPU_FILES)
            self.assertEqual(set(p.relative_to(target / 'original-audit').as_posix()
                                 for p in (target / 'original-audit').rglob('*') if p.is_file()), set(NAMES))
            self.assertIn('not', receipt['qualification'].lower())
            for name, digest in m.CPU_FILES.items():
                self.assertEqual(hashlib.sha256((target / 'original-cpu' / name).read_bytes()).hexdigest(), digest)
            self.assertEqual(json.loads((target / 'fixture-receipt.json').read_text()), receipt)

    def test_wrong_digest_and_size_are_rejected(self):
        m = self.helper()
        data = b'known fixture'
        digest = hashlib.sha256(data).hexdigest()
        m.verify_bytes(data, digest, len(data), 'positive')
        for raw, sha, size in ((data + b'x', digest, len(data)),
                               (data, '0' * 64, len(data)),
                               (data, digest, len(data) + 1)):
            with self.subTest(raw=raw, sha=sha, size=size), self.assertRaises(ValueError):
                m.verify_bytes(raw, sha, size, 'negative')

    def test_traversal_duplicate_and_unexpected_members_rejected(self):
        m = self.helper()
        m.validate_zip(archive())
        cases = (('../escape',) + NAMES[1:], (NAMES[1],) + NAMES[1:],
                 ('/absolute',) + NAMES[1:], ('C:/drive',) + NAMES[1:],
                 ('unexpected',) + NAMES[1:])
        for names in cases:
            with self.subTest(names=names), self.assertRaises(ValueError):
                m.validate_zip(archive(names))

    def test_symlink_member_rejected(self):
        with self.assertRaises(ValueError):
            self.helper().validate_zip(archive(symlink=True))

    def test_bad_crc_rejected(self):
        m = self.helper()
        raw = bytearray(archive())
        with zipfile.ZipFile(io.BytesIO(raw)) as z:
            first = z.infolist()[0]
            offset = first.header_offset + 30 + len(first.filename.encode()) + len(first.extra)
        raw[offset] ^= 1
        with self.assertRaises((ValueError, zipfile.BadZipFile)):
            m.validate_zip(bytes(raw))

    def test_existing_destination_is_preserved(self):
        m = self.helper()
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / 'fixtures'
            target.mkdir()
            (target / 'keep').write_bytes(b'owner content')
            with self.assertRaises(FileExistsError):
                m.materialize(ROOT, target)
            self.assertEqual(list(target.iterdir()), [target / 'keep'])
            self.assertEqual((target / 'keep').read_bytes(), b'owner content')

    def test_corrupt_cpu_is_rejected_before_writes(self):
        m = self.helper()
        original = m.git_blob
        def corrupted(repo, commit, path):
            raw = original(repo, commit, path)
            return raw + b'corruption' if commit == m.CPU_COMMIT else raw
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(m, 'git_blob', corrupted):
            target = Path(tmp) / 'fixtures'
            with self.assertRaises(ValueError):
                m.materialize(ROOT, target)
            self.assertFalse(target.exists())

    def test_missing_git_evidence_is_not_synthesized(self):
        m = self.helper()
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(m, 'git_blob', side_effect=RuntimeError('missing pinned object')):
            target = Path(tmp) / 'fixtures'
            with self.assertRaises(RuntimeError):
                m.materialize(ROOT, target)
            self.assertFalse(target.exists())


if __name__ == '__main__':
    unittest.main()
