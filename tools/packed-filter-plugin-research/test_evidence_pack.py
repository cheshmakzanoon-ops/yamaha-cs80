import copy,io,json,os,pathlib,stat,tempfile,unittest,zipfile
from evidence_pack import pack,verify_archive,restore,validate_manifest,caps_check
CAPS={'raw_bytes':100000,'compressed_bytes':100000,'manifest_bytes':50000,'entries':100}
class EvidencePackTests(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.t.name)
        self.a=self.root/'stock';self.b=self.root/'candidate';self.a.mkdir();self.b.mkdir()
        for root in (self.a,self.b):
            (root/'empty').mkdir();(root/'nested').mkdir();(root/'nested/file').write_bytes(b'shared-data');(root/'zero').write_bytes(b'')
            (root/'nested/file').chmod(0o640)
        (self.b/'different').write_bytes(b'candidate')
        self.archive=self.root/'evidence.zip'
    def tearDown(self):self.t.cleanup()
    def pack(self,caps=None):return pack({'stock':self.a,'candidate':self.b},self.archive,caps=caps or CAPS)
    def modified_zip(self,change):
        self.pack()
        with zipfile.ZipFile(self.archive) as z:items=[(i.filename,z.read(i)) for i in z.infolist()]
        items=change(items)
        self.archive.unlink()
        with zipfile.ZipFile(self.archive,'w',compression=zipfile.ZIP_DEFLATED) as z:
            for name,data in items:
                i=zipfile.ZipInfo(name);i.create_system=3;i.external_attr=(stat.S_IFREG|0o600)<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,data)
    def modify_manifest(self,change):
        def mutate(items):
            m=json.loads(items[0][1]);change(m);return [('manifest.json',json.dumps(m).encode()),*items[1:]]
        self.modified_zip(mutate)
    def test_dedup_and_roundtrip(self):
        result=self.pack();self.assertEqual(result['logical_raw_bytes'],31);self.assertEqual(result['unique_blob_bytes'],20)
        r=restore(self.archive,self.root/'restored',caps=CAPS);self.assertFalse(r['roundtrip_reconstruction_required']);self.assertFalse(r['production_qualified'])
        self.assertEqual((self.root/'restored/stock/nested/file').read_bytes(),b'shared-data')
        self.assertEqual(stat.S_IMODE((self.root/'restored/stock/nested/file').stat().st_mode),0o640)
        self.assertTrue((self.root/'restored/candidate/empty').is_dir())
    def test_dedup_compares_actual_bytes_not_only_digest(self):
        from unittest.mock import patch
        with patch('evidence_pack.digest_file',return_value='a'*64):
            with self.assertRaisesRegex(ValueError,'collision/different dedup bytes'):self.pack()
        self.assertFalse(self.archive.exists())
    def test_source_special_file(self):
        os.mkfifo(self.a/'pipe')
        with self.assertRaises(ValueError):self.pack()
    def test_zip_symlink_rejected(self):
        self.pack()
        with zipfile.ZipFile(self.archive) as z:items=[(i.filename,z.read(i)) for i in z.infolist()]
        self.archive.unlink()
        with zipfile.ZipFile(self.archive,'w') as z:
            for index,(name,data) in enumerate(items):
                i=zipfile.ZipInfo(name);i.create_system=3;i.external_attr=((stat.S_IFLNK if index==1 else stat.S_IFREG)|0o600)<<16;z.writestr(i,data)
        with self.assertRaises(ValueError):verify_archive(self.archive,caps=CAPS)
    def test_envelope_caps(self):
        for key,value in [('compressed_bytes',2**31+1),('entries',60001),('manifest_bytes',64*1024*1024+1)]:
            with self.subTest(key=key),self.assertRaises(ValueError):caps_check({**CAPS,key:value})
    def test_input_unchanged(self):
        before=(self.a/'nested/file').read_bytes();self.pack();self.assertEqual((self.a/'nested/file').read_bytes(),before)
    def test_raw_cap_combined(self):
        with self.assertRaises(ValueError):self.pack({**CAPS,'raw_bytes':25})
        self.assertFalse(self.archive.exists())
    def test_compressed_cap_no_published_partial(self):
        with self.assertRaises(ValueError):self.pack({**CAPS,'compressed_bytes':100})
        self.assertFalse(self.archive.exists());self.assertFalse(list(self.root.glob('.evidence-pack-*')))
    def test_manifest_cap(self):
        with self.assertRaises(ValueError):self.pack({**CAPS,'manifest_bytes':10})
    def test_entry_cap(self):
        with self.assertRaises(ValueError):self.pack({**CAPS,'entries':4})
    def test_bad_caps(self):
        for value in (0,-1,float('inf'),float('nan'),True,1.5):
            with self.subTest(value=value),self.assertRaises(ValueError):caps_check({**CAPS,'raw_bytes':value})
    def test_source_symlink(self):
        (self.a/'link').symlink_to(self.a/'nested/file')
        with self.assertRaises(ValueError):self.pack()
    def test_source_hardlink(self):
        os.link(self.a/'nested/file',self.a/'link')
        with self.assertRaises(ValueError):self.pack()
    def test_overlap(self):
        with self.assertRaises(ValueError):pack({'a':self.a,'b':self.a/'nested'},self.archive,caps=CAPS)
    def test_same_arms(self):
        with self.assertRaises(ValueError):pack({'a':self.a,'b':self.a},self.archive,caps=CAPS)
    def test_destination_inside_input(self):
        with self.assertRaises(ValueError):pack({'a':self.a},self.a/'archive.zip',caps=CAPS)
    def test_existing_destination_preserved(self):
        self.archive.write_bytes(b'existing')
        with self.assertRaises(ValueError):self.pack()
        self.assertEqual(self.archive.read_bytes(),b'existing')
    def test_missing_blob(self):
        self.modified_zip(lambda items:items[:-1])
        with self.assertRaises(ValueError):verify_archive(self.archive,caps=CAPS)
    def test_extra_blob(self):
        self.modified_zip(lambda items:[*items,('blobs/'+'a'*64,b'extra')])
        with self.assertRaises(ValueError):verify_archive(self.archive,caps=CAPS)
    def test_corrupt_blob(self):
        self.modified_zip(lambda items:[items[0],(items[1][0],b'corrupt'),*items[2:]])
        with self.assertRaises(ValueError):verify_archive(self.archive,caps=CAPS)
    def test_duplicate_zip_entry(self):
        with self.assertWarns(UserWarning):self.modified_zip(lambda items:[*items,items[1]])
        with self.assertRaises(ValueError):verify_archive(self.archive,caps=CAPS)
    def test_traversal(self):
        self.modify_manifest(lambda m:m['arms']['stock'][1].update(path='../escape'))
        with self.assertRaises(ValueError):restore(self.archive,self.root/'restored',caps=CAPS)
        self.assertFalse((self.root/'restored').exists())
    def test_duplicate_manifest_path(self):
        self.modify_manifest(lambda m:m['arms']['stock'].append(m['arms']['stock'][0]))
        with self.assertRaises(ValueError):verify_archive(self.archive,caps=CAPS)
    def test_missing_parent(self):
        self.modify_manifest(lambda m:m['arms']['stock'].__setitem__(slice(None),[x for x in m['arms']['stock'] if x['path']!='nested']))
        with self.assertRaises(ValueError):verify_archive(self.archive,caps=CAPS)
    def test_nonfinite_size(self):
        self.modify_manifest(lambda m:next(e for e in m['arms']['stock'] if e['type']=='file').update(size=float('nan')))
        with self.assertRaises(ValueError):verify_archive(self.archive,caps=CAPS)
    def test_invalid_mode(self):
        self.modify_manifest(lambda m:m['arms']['stock'][0].update(mode=0o4755))
        with self.assertRaises(ValueError):verify_archive(self.archive,caps=CAPS)
    def test_restore_existing_kept(self):
        self.pack();dest=self.root/'restored';dest.mkdir();(dest/'keep').write_text('safe')
        with self.assertRaises(ValueError):restore(self.archive,dest,caps=CAPS)
        self.assertEqual((dest/'keep').read_text(),'safe')
    def test_duplicate_json_key(self):
        self.modified_zip(lambda items:[('manifest.json',b'{"format":"cs80-evidence-cas-v1","format":"other","arms":{}}'),*items[1:]])
        with self.assertRaises(ValueError):verify_archive(self.archive,caps=CAPS)
    def test_comment_or_trailing_data_rejected(self):
        self.pack()
        with self.archive.open('ab') as f:f.write(b'extra')
        with self.assertRaises(ValueError):verify_archive(self.archive,caps=CAPS)
if __name__=='__main__':unittest.main()
