#!/usr/bin/env python3
"""Offline verifier regression tests; inert header-only artifacts, never loaded."""
import importlib.util
import io
import json
import gzip
import struct
from unittest import mock
from pathlib import Path
import tempfile
import tarfile
import unittest
import zipfile

SPEC = importlib.util.spec_from_file_location('verifier', Path(__file__).with_name('verify-ci-bundle.py'))
v = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(v)
p = v.packager
SHA = 'a' * 40


class VerificationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cs80-offline-verifier-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def fixture(self, system='linux', arch='x86_64'):
        base = self.root / (system + arch) / 'products'
        vst = base / 'VST3' / (p.PRODUCT + '.vst3')
        if system == 'macos':
            products = [vst, base / 'AU' / (p.PRODUCT + '.component'), base / 'Standalone' / (p.PRODUCT + '.app')]
            binaries = [b / 'Contents/MacOS' / p.PRODUCT for b in products]
            for b in products:
                (b / 'Contents').mkdir(parents=True)
                (b / 'Contents/Info.plist').write_bytes(b'fixture plist')
        elif system == 'windows':
            binaries = [vst / 'Contents/x86_64-win' / (p.PRODUCT + '.vst3'), base / 'Standalone' / (p.PRODUCT + '.exe')]
        else:
            binaries = [vst / 'Contents' / (arch + '-linux') / (p.PRODUCT + '.so'), base / 'Standalone' / p.PRODUCT]
        for i, binary in enumerate(binaries):
            binary.parent.mkdir(parents=True, exist_ok=True)
            binary.write_bytes(p.binary_fixture(system, arch, i == len(binaries) - 1))
            binary.chmod(0o755)
        return p.package(base, base.parent / 'out', system, arch, SHA, {}, {'README.txt': b'fixture\n'})[0]

    def sums(self, archive):
        stem = archive.name.removesuffix('.tar.gz').removesuffix('.zip')
        sidecar = archive.with_name(stem + '.manifest.json')
        archive.with_name('SHA256SUMS').write_text(''.join(f'{p.sha_file(f)}  {f.name}\n' for f in (archive, sidecar)))
        return sidecar

    def tar_edit(self, archive, edit, manifest_edit=None):
        with tarfile.open(archive, 'r:gz') as t:
            entries = [(e, t.extractfile(e).read() if e.isfile() else None) for e in t]
        if manifest_edit:
            sidecar = self.sums(archive)
            m = json.loads(sidecar.read_bytes())
            manifest_edit(m)
            data = (json.dumps(m, indent=2, sort_keys=True) + '\n').encode()
            sidecar.write_bytes(data)
            for e, _ in entries:
                if e.name.endswith('/manifest.json'):
                    e.size = len(data)
            entries = [(e, data if e.name.endswith('/manifest.json') else b) for e, b in entries]
        entries = edit(entries)
        with tarfile.open(archive, 'w:gz') as t:
            for entry, data in entries:
                t.addfile(entry, io.BytesIO(data) if data is not None else None)
        self.sums(archive)

    def test_valid_platforms(self):
        for system, arch in [('linux', 'x86_64'), ('linux', 'arm64'), ('macos', 'arm64'), ('macos', 'x86_64'), ('windows', 'x86_64')]:
            with self.subTest(system=system, arch=arch):
                result = v.verify(self.fixture(system, arch), SHA)
                self.assertEqual(result['architecture'], arch)
                self.assertIn('Not authenticity', result['qualification'])

    def test_checksum_and_commit_failures(self):
        a = self.fixture()
        with self.assertRaisesRegex(ValueError, 'commit'):
            v.verify(a, 'b' * 40)
        a.write_bytes(a.read_bytes() + b'corrupt')
        with self.assertRaisesRegex(ValueError, 'Checksum'):
            v.verify(a, SHA)

    def test_duplicate_checksums(self):
        a = self.fixture(); sums = a.with_name('SHA256SUMS')
        sums.write_bytes(sums.read_bytes() * 2)
        with self.assertRaisesRegex(ValueError, 'Duplicate checksum'):
            v.verify(a, SHA)

    def test_unexpected_checksum(self):
        a = self.fixture(); sums = a.with_name('SHA256SUMS')
        sums.write_text(sums.read_text() + '0' * 64 + '  extra\n')
        with self.assertRaisesRegex(ValueError, 'Unexpected checksum'):
            v.verify(a, SHA)

    def test_duplicate_manifest_path(self):
        a = self.fixture()
        self.tar_edit(a, lambda e: e, lambda m: m['files'].append(m['files'][0]))
        with self.assertRaisesRegex(ValueError, 'Duplicate/reserved'):
            v.verify(a, SHA)

    def test_duplicate_member(self):
        a = self.fixture()
        self.tar_edit(a, lambda e: e + [e[0]])
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            v.verify(a, SHA)

    def test_missing_member(self):
        a = self.fixture()
        self.tar_edit(a, lambda entries: [e for e in entries if not e[0].name.endswith('/README.txt')])
        with self.assertRaisesRegex(ValueError, 'Missing archive'):
            v.verify(a, SHA)

    def test_wrong_mode(self):
        a = self.fixture()
        def edit(entries):
            entries[0][0].mode ^= 0o100
            return entries
        self.tar_edit(a, edit)
        with self.assertRaisesRegex(ValueError, 'mode mismatch'):
            v.verify(a, SHA)

    def test_wrong_contents(self):
        a = self.fixture()
        self.tar_edit(a, lambda entries: [(e, b'changed\n' if e.name.endswith('/README.txt') else data) for e, data in entries])
        with self.assertRaisesRegex(ValueError, 'content mismatch'):
            v.verify(a, SHA)

    def test_manifest_limits(self):
        a = self.fixture()
        def edit(m):
            next(i for i in m['files'] if i['type'] == 'file')['size'] = v.MAX_FILE + 1
        self.tar_edit(a, lambda e: e, edit)
        with self.assertRaisesRegex(ValueError, 'file size'):
            v.verify(a, SHA)

    def test_path_aliases(self):
        for path in ('a//b', './a', 'a/../b', 'a\\b', '/a', 'C:/a', 'a\n'):
            with self.subTest(path=path), self.assertRaises(ValueError):
                v.safe_name(path)

    def test_duplicate_json_key(self):
        with self.assertRaisesRegex(ValueError, 'Duplicate JSON'):
            json.loads('{"files": [], "files": []}', object_pairs_hook=v.unique_object)


    def test_zip64_locator_rejected_before_zipfile(self):
        a = self.fixture('windows')
        data = a.read_bytes()
        end = struct.unpack('<4s4H2IH', data[-22:])
        record = struct.pack('<4sQ2H2I4Q', b'PK\x06\x06', 44, 45, 45, 0, 0,
                             end[4], end[4], end[5], end[6])
        locator = struct.pack('<4sIQI', b'PK\x06\x07', 0, len(data) - 22, 1)
        for comment in (b'', b'x' * 65535):
            end_record = data[-22:-2] + len(comment).to_bytes(2, 'little')
            a.write_bytes(data[:-22] + record + locator + end_record + comment)
            self.sums(a)
            # ZIP64 may override otherwise ordinary classic EOCD fields. The
            # maximum ZIP comment pushes its locator outside a 65557-byte tail.
            with self.subTest(comment_length=len(comment)):
                with mock.patch.object(v.zipfile, 'ZipFile', side_effect=AssertionError('ZIP64 reached parser')):
                    with self.assertRaisesRegex(ValueError, 'ZIP'):
                        v.verify(a, SHA)

    def test_zip_central_directory_limit_before_zipfile(self):
        a = self.fixture('windows')
        data = bytearray(a.read_bytes())
        struct.pack_into('<I', data, len(data) - 10, v.MAX_METADATA + 1)
        a.write_bytes(data)
        self.sums(a)
        with mock.patch.object(v.zipfile, 'ZipFile', side_effect=AssertionError('oversized directory reached parser')):
            with self.assertRaisesRegex(ValueError, 'ZIP directory'):
                v.verify(a, SHA)

    def test_tar_extended_metadata_limit_before_parser(self):
        a = self.fixture()
        header = tarfile.TarInfo('pax')
        header.type = tarfile.XHDTYPE
        header.size = v.MAX_METADATA + 1
        a.write_bytes(gzip.compress(header.tobuf() + bytes(1024)))
        self.sums(a)
        with mock.patch.object(v.tarfile, 'open', side_effect=AssertionError('oversized header reached parser')):
            with self.assertRaisesRegex(ValueError, 'Oversized tar'):
                v.verify(a, SHA)

    def test_tar_hardlink_rejected(self):
        a = self.fixture()
        def edit(entries):
            e = tarfile.TarInfo(entries[0][0].name + '/hardlink')
            e.type, e.linkname = tarfile.LNKTYPE, entries[0][0].name
            return entries + [(e, None)]
        self.tar_edit(a, edit)
        with self.assertRaisesRegex(ValueError, 'Unsupported tar member'):
            v.verify(a, SHA)

    def test_tar_alias_and_extra_member(self):
        a = self.fixture()
        def edit(entries):
            e = tarfile.TarInfo(entries[0][0].name + '//extra')
            e.size = 0
            return entries + [(e, b'')]
        self.tar_edit(a, edit)
        with self.assertRaisesRegex(ValueError, 'Unsafe member path'):
            v.verify(a, SHA)

    def test_embedded_manifest_mismatch(self):
        a = self.fixture()
        def edit(entries):
            result = []
            for e, data in entries:
                if e.name.endswith('/manifest.json'):
                    data += b' '
                    e.size = len(data)
                result.append((e, data))
            return result
        self.tar_edit(a, edit)
        with self.assertRaisesRegex(ValueError, 'Embedded manifest differs'):
            v.verify(a, SHA)

    def test_truncated_tar_and_nonzero_trailing_data(self):
        a = self.fixture()
        original = a.read_bytes()
        for data in (original[:-8], gzip.compress(gzip.decompress(original) + b'bad')):
            with self.subTest(truncated=len(data) < len(original)):
                a.write_bytes(data)
                self.sums(a)
                with self.assertRaises((ValueError, EOFError, OSError)):
                    v.verify(a, SHA)

    def test_zip_filename_and_mode_type_consistency(self):
        a = self.fixture('windows')
        original = a.read_bytes()
        for change_directory in (False, True):
            a.write_bytes(original)
            with zipfile.ZipFile(a) as z:
                entries = [(entry, z.read(entry)) for entry in z.infolist()]
            for entry, _ in entries:
                if entry.is_dir() == change_directory:
                    entry.external_attr = (((0o100755 if change_directory else 0o40755) << 16)
                                           | (entry.external_attr & 0xffff))
                    break
            with zipfile.ZipFile(a, 'w') as z:
                for entry, data in entries:
                    z.writestr(entry, data)
            self.sums(a)
            with self.subTest(directory=change_directory), self.assertRaisesRegex(ValueError, 'ZIP.*attributes'):
                v.verify(a, SHA)

    def test_duplicate_embedded_manifest(self):
        a = self.fixture()
        self.tar_edit(a, lambda entries: entries + [next(e for e in entries if e[0].name.endswith('/manifest.json'))])
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            v.verify(a, SHA)

    def test_reserved_embedded_manifest_parent(self):
        a = self.fixture()
        manifest = json.loads(self.sums(a).read_bytes())
        template = next(item for item in manifest['files'] if item['path'] == 'README.txt')
        changed = dict(manifest, files=manifest['files'] + [dict(template, path='manifest.json/child')])
        with self.assertRaises(ValueError):
            v.validate_manifest(changed, SHA)

    def test_windows_manifest_path_aliases(self):
        a = self.fixture('windows')
        manifest = json.loads(self.sums(a).read_bytes())
        template = next(item for item in manifest['files'] if item['path'] == 'README.txt')
        for name in ('notes/readme:stream', 'notes/CON.txt', 'notes/NUL',
                     'notes/COM1.log', 'notes/LPT9.txt', 'notes/COM¹.txt',
                     'notes/LPT².txt', 'notes/COM³', 'README.TXT/child', 'notes/name.',
                     'notes/name ', 'notes/aux/subfile', 'README.TXT', 'MANIFEST.JSON', 'MANIFEST.JSON/child',
                     'notes/question?.txt', 'notes/star*.txt', 'notes/pipe|.txt',
                     'notes/quote".txt', 'notes/less<.txt', 'notes/more>.txt'):
            changed = dict(manifest, files=manifest['files'] + [dict(template, path=name)])
            with self.subTest(name=name), self.assertRaises(ValueError):
                v.validate_manifest(changed, SHA)
        # Ordinary internal spaces are legitimate Windows filenames.
        valid = dict(manifest, files=manifest['files'] + [dict(template, path='notes/license notice.txt')])
        self.assertEqual(v.validate_manifest(valid, SHA)[1], 'windows')

    def test_virtual_links(self):
        R='VST3/CS-80 Reference.vst3'
        def d(p):return {'path':p,'type':'directory'}
        def f(p):return {'path':p,'type':'file'}
        def l(p,t):return {'path':p,'type':'symlink','target':t}
        base=[d(R),d(R+'/Contents'),d(R+'/Contents/MacOS'),f(R+'/Contents/MacOS/bin'),d(R+'/Contents/Resources')]
        checks=0
        def check(extra,ok=True,system='macos'):
         nonlocal checks
         try:v.validate_virtual_links(base+extra,system)
         except ValueError:
          if ok:raise
         else:assert ok,extra
         checks+=1
        check([l(R+'/Contents/Resources/link','../MacOS/bin')])
        check([l(R+'/Contents/Resources/link','../MacOS/./bin')])
        check([l(R+'/Contents/Resources/link','../MacOS//bin')])
        check([l(R+'/Contents/Resources/dir','../MacOS'),l(R+'/Contents/Resources/link','dir/bin')])
        check([l(R+'/Contents/Resources/dir','../MacOS'),l(R+'/Contents/Resources/link','dir/../MacOS/bin')])
        check([l(R+'/Contents/Resources/link','.')])
        check([l(R+'/Contents/Resources/link','../../..')],False)
        check([l(R+'/Contents/Resources/link','../../../Other/file')],False)
        check([l(R+'/Contents/Resources/link','/tmp/file')],False)
        check([l(R+'/Contents/Resources/link','C:/file')],False)
        check([l(R+'/Contents/Resources/link','..\\MacOS\\bin')],False)
        check([l(R+'/Contents/Resources/link','../MacOS/missing')],False)
        check([l(R+'/Contents/Resources/link','link')],False)
        check([l(R+'/Contents/Resources/a','b'),l(R+'/Contents/Resources/b','a')],False)
        check([l(R+'/Contents/Resources/dir','../MacOS'),f(R+'/Contents/Resources/dir/child')],False)
        check([f(R+'/Contents/MacOS/bin/child')],False)
        check([l(R+'/Contents/Resources/link','../MacOS/bin/../bin')],False)
        check([l(R+'/Contents/Resources/link','../MacOS/bin/.')],False)
        check([l(R+'/Contents/Resources/link','../MacOS/bin/')],False)
        check([l(R+'/Contents/Resources/link','../MacOS/bin')],False,'windows')
        check([l('licenses/link',R+'/Contents/MacOS/bin')],False)
        check([l(R+'/Missing/link','../Contents/MacOS/bin')],False)
        check([l(R+'/Contents/Resources/link','../MacOS/bin\n')],False)
        check([f('licenses/NOTICE')])
        check([l(R+'/Contents/Resources/link','')],False)
        # A link to an ancestor directory is safe without any archived child beneath it.
        check([l(R+'/Contents/Resources/back','../..')])
        # POSIX semantics, not lexical normalization: alias/.. traverses alias target first.
        check([d(R+'/Contents/Deep'),d(R+'/Contents/Deep/Sub'),l(R+'/Contents/Resources/dir','../Deep/Sub'),l(R+'/Contents/Resources/link','dir/../../MacOS/bin')])
        chain=[l(R+'/Contents/Resources/l'+str(i),'l'+str(i+1)) for i in range(65)]
        check(chain+[l(R+'/Contents/Resources/l65','../MacOS/bin')],False)
        check([l(R+'/Contents/Resources/l'+str(i),'l'+str(i+1)) for i in range(63)]
              + [l(R+'/Contents/Resources/l63','../MacOS/bin')])
        for kind, suffix in [('AU', '.component'), ('Standalone', '.app')]:
         root=kind+'/CS-80 Reference'+suffix
         v.validate_virtual_links([d(root),d(root+'/Contents'),f(root+'/Contents/bin'),l(root+'/Contents/link','bin')],'macos')
         checks+=1
        check([d('AU/CS-80 Reference.component'),f('AU/CS-80 Reference.component/bin'),
               l(R+'/Contents/Resources/link','../../../../AU/CS-80 Reference.component/bin')],False)
        self.assertEqual(checks, 32)


if __name__ == '__main__':
    unittest.main()
