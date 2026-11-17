import copy
from pathlib import Path
import tempfile
import unittest
from configure_preflight import configure_argv,provisional_tests,fresh_path
class PreflightTests(unittest.TestCase):
    def setUp(self):self.doc={'tests':[{'name':'test','properties':[]}]}
    def test_only_normal_configuration(self):
        self.assertEqual(configure_argv('/repo','/build'),['cmake','-S','/repo','-B','/build','-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCS80_BUILD_PLUGIN=ON','-DCS80_BUILD_TESTS=ON'])
    def test_unbuilt_recorded_not_pass(self):
        result=provisional_tests(self.doc)
        self.assertFalse(result['qualification']);self.assertEqual(result['unresolved_commands'],['test'])
    def test_disabled(self):
        self.doc['tests'][0]['properties']=[{'name':'DISABLED','value':True}]
        with self.assertRaises(ValueError):provisional_tests(self.doc)
    def test_duplicate(self):
        self.doc['tests']*=2
        with self.assertRaises(ValueError):provisional_tests(self.doc)
    def test_duplicate_property(self):
        self.doc['tests'][0]['properties']=[{'name':'TIMEOUT','value':1}]*2
        with self.assertRaises(ValueError):provisional_tests(self.doc)
    def test_wrong_command(self):
        self.doc['tests'][0]['command']='run'
        with self.assertRaises(ValueError):provisional_tests(self.doc)
    def test_fresh(self):
        with tempfile.TemporaryDirectory() as root:self.assertEqual(fresh_path(Path(root)/'new'),Path(root)/'new')
    def test_existing_rejected(self):
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaises(ValueError):fresh_path(root)
    def test_unsafe_parent_rejected(self):
        with self.assertRaises(ValueError):fresh_path('/tmp/path/../new')
    def test_workflow_manual_only(self):
        w=Path(__file__).resolve().parents[2]/'.github/workflows/packed-filter-plugin-preflight.yml'
        source=w.read_text()
        self.assertIn('workflow_dispatch:',source)
        self.assertNotIn('  push:',source);self.assertNotIn('  pull_request:',source)
        self.assertIn('contents: read',source);self.assertIn('github.event.repository.private == false',source)
        self.assertIn('github.run_attempt == 1',source);self.assertIn('cancel-in-progress: false',source)
        self.assertIn('test "$EXPECTED_SHA" = "$GITHUB_SHA"',source)
        self.assertNotIn('cmake --build',source);self.assertNotIn('workflow_dispatch:',source.split('jobs:',1)[1])
if __name__=='__main__':unittest.main()
