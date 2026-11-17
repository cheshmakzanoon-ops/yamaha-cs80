import copy,unittest
from roster import admit_resolved_roster
class ResolvedTests(unittest.TestCase):
    def setUp(self):
        self.a={'tests':[{'name':'one','properties':[{'name':'TIMEOUT','value':180}]}]}
        self.b=copy.deepcopy(self.a);self.b['tests'][0]['command']=['/build/one','arg']
    def call(self):return admit_resolved_roster(self.a,self.b)
    def test_valid(self):self.assertEqual(self.call()['resolved_test_count'],1)
    def test_still_unresolved(self):
        del self.b['tests'][0]['command']
        with self.assertRaises(ValueError):self.call()
    def test_property_changed(self):
        self.b['tests'][0]['properties'][0]['value']=240
        with self.assertRaises(ValueError):self.call()
    def test_existing_command_changed(self):
        self.a['tests'][0]['command']=['/build/wrong']
        with self.assertRaises(ValueError):self.call()
    def test_missing(self):
        self.b['tests'][0]['name']='other'
        with self.assertRaises(ValueError):self.call()
    def test_duplicate(self):
        self.a['tests']*=2
        with self.assertRaises(ValueError):self.call()
    def test_extra(self):
        self.b['tests'].append({'name':'extra','command':['/build/x']})
        with self.assertRaises(ValueError):self.call()
    def test_empty_not_unresolved(self):
        self.a['tests'][0]['command']=[]
        with self.assertRaises(ValueError):self.call()

class BuiltBindingTests(unittest.TestCase):
    def setUp(self):
        import tempfile,json
        from pathlib import Path
        from identity import capture_files
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        (self.root/'test').write_bytes(b'executable');(self.root/'CTestTestfile.cmake').write_text('add_test([=[one]=] "'+str(self.root/'test')+'")')
        self.a={'tests':[{'name':'one'}]};self.b={'tests':[{'name':'one','command':[str(self.root/'test')]}]}
        self.kw=dict(configured=self.a,built=self.b,build_root=self.root,
                     definition_hashes=capture_files(self.root,['CTestTestfile.cmake']),
                     executable_hashes={str(self.root/'test'):capture_files(self.root,['test'])['test']},
                     ctest_record={'argv':['ctest','--test-dir',str(self.root),'--show-only=json-v1'],'exit':0},
                     ctest_stdout=json.dumps(self.b).encode(),executable_resolutions={})
    def tearDown(self):self.tmp.cleanup()
    def call(self):
        from roster import admit_built_tests
        return admit_built_tests(**self.kw)
    def test_joined_proof(self):self.assertFalse(self.call()['resolved_executable_provenance_required'])
    def test_other_binary_same_name_properties(self):
        import json
        self.b['tests'][0]['command']=['/other/binary'];self.kw['ctest_stdout']=json.dumps(self.b).encode()
        with self.assertRaises(ValueError):self.call()
    def test_definition_drift(self):
        (self.root/'CTestTestfile.cmake').write_text('different')
        with self.assertRaises(ValueError):self.call()
    def test_executable_drift(self):
        (self.root/'test').write_bytes(b'other')
        with self.assertRaises(ValueError):self.call()
    def test_fake_ctest_stdout(self):
        self.kw['ctest_stdout']=b'{}'
        with self.assertRaises(ValueError):self.call()
    def test_failed_ctest(self):
        self.kw['ctest_record']['exit']=1
        with self.assertRaises(ValueError):self.call()

if __name__=='__main__':unittest.main()
