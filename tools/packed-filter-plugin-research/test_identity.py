import copy,unittest,tempfile
from pathlib import Path
from identity import verify_pair,FILTER,capture_files
class IdentityTests(unittest.TestCase):
    def setUp(self):
        self.s={'source':{FILTER:'a'*64,'Engine.cpp':'b'*64},'objects':{'filter.o':'a'*64,'engine.o':'b'*64},'compile_commands':{'commands':'a'*64},'dependencies':{'juce':'a'*64},'test_definitions':{'ctest':'a'*64}}
        self.expected={k:list(v) for k,v in self.s.items()};self.c=copy.deepcopy(self.s);self.c['source'][FILTER]='c'*64;self.c['objects']['filter.o']='c'*64
    def call(self): return verify_pair(self.s,self.c,filter_objects=getattr(self,'filter_objects',['filter.o']),candidate_source='c'*64,expected_inventories=self.expected)
    def add_test_variant(self):
        self.s['objects']['filter_test.o']='a'*64
        self.c['objects']['filter_test.o']='d'*64
        self.expected['objects'].append('filter_test.o')
        self.filter_objects=['filter.o','filter_test.o']
    def test_exact_two_filter_outputs(self):
        self.add_test_variant();self.assertFalse(self.call()['production_qualified'])
    def test_omitted_filter_variant(self):
        self.add_test_variant();self.filter_objects=['filter.o']
        with self.assertRaises(ValueError):self.call()
    def test_stale_filter_variant(self):
        self.add_test_variant();self.c['objects']['filter_test.o']='a'*64
        with self.assertRaises(ValueError):self.call()
    def test_extra_unlisted_output(self):
        self.add_test_variant();self.c['objects']['engine.o']='e'*64
        with self.assertRaises(ValueError):self.call()
    def test_duplicate_filter_outputs(self):
        self.filter_objects=['filter.o','filter.o']
        with self.assertRaises(ValueError):self.call()
    def test_unknown_filter_output(self):
        self.filter_objects=['missing.o']
        with self.assertRaises(ValueError):self.call()
    def test_valid_is_not_qualification(self):self.assertEqual(self.call(),{'identity_admission':'PASS','production_qualified':False})
    def test_engine_source_change(self):
        self.c['source']['Engine.cpp']='c'*64
        with self.assertRaises(ValueError):self.call()
    def test_engine_object_change(self):
        self.c['objects']['engine.o']='c'*64
        with self.assertRaises(ValueError):self.call()
    def test_missing_object(self):
        del self.c['objects']['engine.o']
        with self.assertRaises(ValueError):self.call()
    def test_unmodified_candidate(self):
        self.c['objects']['filter.o']='a'*64
        with self.assertRaises(ValueError):self.call()
    def test_unexpected_candidate(self):
        self.c['source'][FILTER]='d'*64
        with self.assertRaises(ValueError):self.call()
    def test_each_frozen_map(self):
        for key in ('compile_commands','dependencies','test_definitions'):
            with self.subTest(key=key):
                old=self.c[key];self.c[key]={name:'d'*64 for name in old}
                with self.assertRaises(ValueError):self.call()
                self.c[key]=old
    def test_invalid_hash(self):
        self.c['objects']['filter.o']='not-a-hash'
        with self.assertRaises(ValueError):self.call()
    def test_same_omission_both_arms(self):
        del self.s['objects']['engine.o'];del self.c['objects']['engine.o']
        with self.assertRaises(ValueError):self.call()
    def test_duplicate_expected_inventory(self):
        self.expected['objects'].append('filter.o')
        with self.assertRaises(ValueError):self.call()
    def test_extra_evidence_kind(self):
        self.s['extra']={};self.c['extra']={}
        with self.assertRaises(ValueError):self.call()
    def test_invalid_hash_type(self):
        self.c['objects']['filter.o']=1
        with self.assertRaises(ValueError):self.call()
class CaptureTests(unittest.TestCase):
    def test_actual_hash(self):
        with tempfile.TemporaryDirectory() as t:
            (Path(t)/'file').write_text('abc')
            self.assertEqual(capture_files(t,['file']),{'file':'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'})
    def test_missing(self):
        with tempfile.TemporaryDirectory() as t:
            with self.assertRaises(ValueError):capture_files(t,['absent'])
    def test_traversal_and_noncanonical(self):
        with tempfile.TemporaryDirectory() as t:
            for p in ['../file','/file','a/../file','./file','a//file','a/','.',r'a\\file']:
                with self.subTest(path=p),self.assertRaises(ValueError):capture_files(t,[p])
    def test_symlink(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);(root/'file').write_text('x');(root/'link').symlink_to('file')
            with self.assertRaises(ValueError):capture_files(root,['link'])
    def test_same_symlink_root(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);(root/'real').mkdir();(root/'real/file').write_text('x');(root/'link').symlink_to('real',target_is_directory=True)
            with self.assertRaises(ValueError):capture_files(root/'link',['file'])
if __name__=='__main__':unittest.main()
