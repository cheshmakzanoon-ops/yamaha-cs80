import unittest,copy,json,tempfile
from pathlib import Path
import verify as v
class Offline(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.files,cls.docs=v.verify_package()
 def rejects_record(self,fn):
  d=copy.deepcopy(self.docs);fn(d)
  with self.assertRaises((ValueError,KeyError,TypeError)):v.records(d)
 def test_good(self):v.records(self.docs)
 def test_patch_corruption(self):
  with tempfile.TemporaryDirectory()as t:
   root=Path(t)
   for n,b in self.files.items():p=root/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
   with(root/'candidate.patch').open('ab')as f:f.write(b'bad')
   with self.assertRaises(ValueError):v.verify_package(root)
 def test_extra_file(self):
  with tempfile.TemporaryDirectory()as t:
   root=Path(t)
   for n,b in self.files.items():p=root/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
   (root/'extra').write_text('bad')
   with self.assertRaises(ValueError):v.verify_package(root)
 def test_duplicate_json(self):
  with self.assertRaises(ValueError):v.decode('{"a":1,"a":2}')
 def test_wrong_archive(self):self.rejects_record(lambda d:d['provenance.json']['normal_sha256'].__setitem__('libcs80_core.a','0'*64))
 def test_false_native_claim(self):self.rejects_record(lambda d:d['provenance.json'].__setitem__('native_module_invocations',1))
 def test_weakened_protected(self):self.rejects_record(lambda d:d['records/static-v2.json']['critical']['critical'][0].__setitem__('bytes_identical',False))
 def test_v1_render_claim(self):self.rejects_record(lambda d:d['records/rejected-v1.json'].__setitem__('candidate_Engine_renders',1))
 def test_unsupported_gate(self):self.rejects_record(lambda d:d['records/exceptional.json']['observed_gate'].__setitem__('nine_unsupported_fp_target_samples_zero_attempts',False))
 def test_scene_mismatch(self):self.rejects_record(lambda d:d['records/scenes.json']['rows'][0].__setitem__('stock_equal',False))
 def test_pure_confusion(self):self.rejects_record(lambda d:next(x for x in d['records/scenes.json']['rows']if x['scene']=='pure-held'and x['arm']=='observed').__setitem__('stdout','window attempts=8053 hits=8053 consumed=8053 bad=0'))
 def test_path_traversal(self):
  with self.assertRaises(ValueError):v.safe('../private')
 def test_patch_wrong_context(self):
  patch=b'--- a/src/a.cpp\n+++ b/src/a.cpp\n@@ -1 +1 @@\n-wrong\n+new\n'
  with self.assertRaises(ValueError):v.apply_patch({'a.cpp':b'old\n'},patch)
 def test_patch_exact(self):
  patch=b'--- a/src/a.cpp\n+++ b/src/a.cpp\n@@ -1 +1 @@\n-old\n+new\n'
  self.assertEqual(v.apply_patch({'a.cpp':b'old\n'},patch),{'a.cpp':b'new\n'})
 def test_patch_truncated(self):
  with self.assertRaises(ValueError):v.apply_patch({'a.cpp':b'old\n'},b'--- a/src/a.cpp\n+++ b/src/a.cpp\n@@ -1 +1 @@\n-old\n')
if __name__=='__main__':unittest.main()
