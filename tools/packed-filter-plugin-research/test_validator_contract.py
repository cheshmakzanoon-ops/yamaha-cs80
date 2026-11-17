import unittest
from validator_contract import linux_validations,admit_validation
class ValidatorTests(unittest.TestCase):
    def setUp(self):
        self.c=linux_validations(repo='/repo',validator='/tools/pluginval',bundle='/build/CS80.vst3',output='/evidence')
        self.r={'label':'headless','argv':self.c['headless'],'exit':0}
    def call(self,stdout='SUCCESS\n',stderr=''):
        return admit_validation(name='headless',expected_argv=self.c['headless'],record=self.r,stdout=stdout,stderr=stderr)
    def test_three_cases_and_no_retry_wrapper(self):
        self.assertEqual(set(self.c),{'headless','gui','gui-seed-regression'})
        for argv in self.c.values():
            self.assertNotIn('/repo/scripts/run-pluginval-linux.sh',argv)
            self.assertEqual(argv[argv.index('--strictness-level')+1],'10')
            self.assertEqual(argv[argv.index('--timeout-ms')+1],'120000')
            self.assertIn('/repo/scripts/run-pluginval-with-core.py',argv)
        self.assertNotIn('--skip-gui-tests',self.c['gui'])
        self.assertEqual(self.c['gui-seed-regression'][:4],['timeout','--signal=TERM','--kill-after=10s','240s'])
        self.assertIn('0x2da72db',self.c['gui-seed-regression'])
    def test_success_not_qualification(self):self.assertFalse(self.call()['production_qualified'])
    def test_post_success_crash(self):
        self.r['exit']=139
        with self.assertRaises(ValueError):self.call()
    def test_exit_zero_without_marker(self):
        with self.assertRaises(ValueError):self.call('Completed tests')
    def test_failed_in_stderr(self):
        with self.assertRaises(ValueError):self.call(stderr='FAILED')
    def test_drift(self):
        self.r['argv']=self.r['argv']+['--skip-gui-tests']
        with self.assertRaises(ValueError):self.call()
    def test_error_record(self):
        self.r['error']='timed out'
        with self.assertRaises(ValueError):self.call()
    def test_missing_exit(self):
        del self.r['exit']
        with self.assertRaises(ValueError):self.call()
    def test_noncanonical_paths(self):
        with self.assertRaises(ValueError):linux_validations(repo='relative',validator='/v',bundle='/b',output='/o')
if __name__=='__main__':unittest.main()
