import unittest
from definition_commands import commands_from_definitions,bind_definition_commands
class DefinitionCommandsTests(unittest.TestCase):
    def call(self,line):return commands_from_definitions({'CTestTestfile.cmake':line})
    def test_literal_spaces(self):self.assertEqual(self.call('add_test([=[one]=] "/build/a b" "arg")'),{'one':['/build/a b','arg']})
    def test_comment_ignored(self):self.assertEqual(self.call('# add_test ignored\nadd_test([=[one]=] "/a")'),{'one':['/a']})
    def test_unsupported_rejected(self):
        for line in ['add_test(one /a)','add_test([=[one]=] "${PATH}")','add_test([=[one]=] "$<TARGET_FILE:x>")','add_test([=[one]=] "a;b")','add_test([=[one]=] "a\\nb")','add_test([=[one]=]\n "/a")',' add_test([=[one]=] "/a")','add_test([=[one]=] "")']:
            with self.subTest(line=line),self.assertRaises(ValueError):self.call(line)
    def test_duplicate(self):
        with self.assertRaises(ValueError):self.call('add_test([=[one]=] "/a")\nadd_test([=[one]=] "/b")')
    def test_empty(self):
        with self.assertRaises(ValueError):self.call('# no tests')
class AssociationTests(unittest.TestCase):
    def setUp(self):self.text={'defs':'add_test([=[one]=] "/build/a" "arg")\nadd_test([=[two]=] "/build/b")'}
    def call(self,commands,resolutions=None):return bind_definition_commands(self.text,commands,executable_resolutions=resolutions or {})
    def test_exact(self):self.assertEqual(self.call({'one':['/build/a','arg'],'two':['/build/b']})['raw_case_commands_bound'],2)
    def test_swapped_known_binaries(self):
        with self.assertRaises(ValueError):self.call({'one':['/build/b','arg'],'two':['/build/a']})
    def test_changed_argument(self):
        with self.assertRaises(ValueError):self.call({'one':['/build/a','changed'],'two':['/build/b']})
    def test_explicit_bash_resolution(self):
        self.text={'defs':'add_test([=[one]=] "bash" "test.sh")'}
        self.assertTrue(self.call({'one':['/usr/bin/bash','test.sh']},{'bash':'/usr/bin/bash'})['executable_resolution_provenance_required'])
    def test_missing_resolution(self):
        self.text={'defs':'add_test([=[one]=] "bash" "test.sh")'}
        with self.assertRaises(ValueError):self.call({'one':['/usr/bin/bash','test.sh']})
if __name__=='__main__':unittest.main()
