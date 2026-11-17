import copy,tempfile,unittest
from pathlib import Path
from compile_inventory import compile_inventory,capture_objects,filter_outputs
class CompileTests(unittest.TestCase):
    def setUp(self):self.d=[{'directory':'/build','file':'/source/a.cpp','command':'/usr/bin/c++ -O3 -o a.o -c /source/a.cpp'}]
    def call(self):return compile_inventory(self.d,source_root='/source',build_root='/build')
    def test_valid(self):self.assertEqual(list(self.call()),['a.o'])
    def test_duplicates(self):
        self.d*=2
        with self.assertRaises(ValueError):self.call()
    def test_mismatched_source(self):
        self.d[0]['file']='/source/b.cpp'
        with self.assertRaises(ValueError):self.call()
    def test_response(self):
        self.d[0]['command']+=' @opts'
        with self.assertRaises(ValueError):self.call()
    def test_bad_outputs(self):
        for out in ('../a.o','/elsewhere/a.o','/build'):
            self.d[0]['command']=f'/usr/bin/c++ -o {out} -c /source/a.cpp'
            with self.subTest(out=out),self.assertRaises(ValueError):self.call()
    def test_source_outside(self):
        self.d[0]['file']='/elsewhere/a.cpp';self.d[0]['command']='/usr/bin/c++ -o a.o -c /elsewhere/a.cpp'
        with self.assertRaises(ValueError):self.call()
    def test_owned_subproject_cwd(self):
        self.d[0]['directory']='/build/dependency'
        self.assertEqual(list(self.call()),['dependency/a.o'])
    def test_cwd_drift(self):
        self.d[0]['directory']='/other'
        with self.assertRaises(ValueError):self.call()
    def test_actual_objects_missing_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            self.d[0]['directory']=t
            with self.assertRaises(ValueError):capture_objects(self.d,source_root='/source',build_root=t)
    def test_capture_actual_object(self):
        with tempfile.TemporaryDirectory() as t:
            self.d[0]['directory']=t;(Path(t)/'a.o').write_bytes(b'object')
            self.assertEqual(set(capture_objects(self.d,source_root='/source',build_root=t)['sha256']),{'a.o'})
    def test_conflicting_argv(self):
        self.d[0]['arguments']=['/usr/bin/c++','-o','b.o','-c','/source/a.cpp']
        with self.assertRaises(ValueError):self.call()
class FilterOutputTests(unittest.TestCase):
    def setUp(self):
        self.core='CMakeFiles/cs80_core.dir/src/core/Ig00156Batch.cpp.o'
        self.test='CMakeFiles/cs80_filter_batch_cap_tests.dir/src/core/Ig00156Batch.cpp.o'
        self.source='/source/src/core/Ig00156Batch.cpp'
        self.d=[{'directory':'/build','file':self.source,'arguments':['/usr/bin/c++',flag,'-o',out,'-c',self.source]} for out,flag in [(self.core,'-fPIC'),(self.test,'-DCS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY=1')]]
    def call(self):return filter_outputs(self.d,source_root='/source',build_root='/build')
    def test_exact_derived_outputs(self):
        self.assertEqual(self.call(),{'all_filter_objects':sorted([self.core,self.test]),'plugin_filter_object':self.core})
    def test_missing_core(self):
        self.d.pop(0)
        with self.assertRaises(ValueError):self.call()
    def test_non_pic(self):
        self.d[0]['arguments'][1]='-fPIE'
        with self.assertRaises(ValueError):self.call()
    def test_experimental_core(self):
        self.d[0]['arguments'].insert(1,'-DCS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY=1')
        with self.assertRaises(ValueError):self.call()
    def test_same_basename_other_source_not_admitted(self):
        self.d[1]['file']='/source/other/Ig00156Batch.cpp'
        self.d[1]['arguments'][-1]=self.d[1]['file']
        self.assertEqual(self.call()['all_filter_objects'],[self.core])
if __name__=='__main__':unittest.main()
