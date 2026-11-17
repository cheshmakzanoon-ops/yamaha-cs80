from pathlib import Path
import tempfile,unittest
from validator_runner import run_validations
from identity import capture_files
class FakeCommands:
    def __init__(self,root):self.root=root;self.ledger=[];self.terminal_failure=False;self.calls=0;self.mutate=None
    def persist(self):self.persisted=True
    def run(self,label,argv,cwd,timeout):
        self.calls+=1;self.ledger.append({'label':label,'argv':argv,'cwd':str(cwd),'exit':0,'timeout_seconds':timeout})
        a=self.root/(label+'.stdout');b=self.root/(label+'.stderr');a.write_text('SUCCESS\n');b.write_text('')
        if self.mutate:self.mutate(self,a,b)
        return a,b
class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.v=self.root/'pluginval';self.v.write_bytes(b'validator')
        self.bundle=self.root/'bundle';self.bundle.mkdir();(self.bundle/'module').write_bytes(b'module')
        self.commands=FakeCommands(self.root)
        self.kw=dict(arm='stock',repo=self.root,validator=self.v,bundle=self.bundle,output=self.root/'out',commands=self.commands,
                     expected_validator_sha256=capture_files(self.root,['pluginval'])['pluginval'],module_root=self.bundle,
                     module_hashes=capture_files(self.bundle,['module']),case_timeouts={'headless':300,'gui':300,'gui-seed-regression':250})
    def tearDown(self):self.tmp.cleanup()
    def call(self):return run_validations(**self.kw)
    def test_three_bound_cases(self):
        result=self.call();self.assertEqual(len(result['validators']),3);self.assertFalse(result['production_qualified'])
        self.assertEqual([r['timeout_seconds'] for r in self.commands.ledger],[300,300,250])
        for r in result['validators'].values():self.assertEqual(set(r['log_sha256']),{'stdout','stderr'})
    def test_repeated_arm(self):
        self.call()
        with self.assertRaises(ValueError):self.call()
        self.assertEqual(self.commands.calls,3)
    def test_wrong_validator(self):
        self.v.write_bytes(b'changed')
        with self.assertRaises(ValueError):self.call()
        self.assertEqual(self.commands.calls,0);self.assertTrue(self.commands.terminal_failure)
    def test_postrun_module_drift(self):
        self.commands.mutate=lambda *_:(self.bundle/'module').write_bytes(b'changed')
        with self.assertRaises(ValueError):self.call()
        self.assertEqual(self.commands.calls,1);self.assertTrue(self.commands.terminal_failure)
    def test_post_success_crash(self):
        self.commands.mutate=lambda c,*_:c.ledger[-1].update(exit=139)
        with self.assertRaises(ValueError):self.call()
        self.assertEqual(self.commands.calls,1)
    def test_failed_log(self):
        self.commands.mutate=lambda c,a,b:b.write_text('FAILED')
        with self.assertRaises(ValueError):self.call()
        self.assertTrue(self.commands.terminal_failure)
    def test_missing_log(self):
        self.commands.mutate=lambda c,a,b:b.unlink()
        with self.assertRaises(ValueError):self.call()
        self.assertTrue(self.commands.terminal_failure)
    def test_other_bundle(self):
        self.kw['bundle']=self.root/'elsewhere'
        with self.assertRaises(ValueError):self.call()
        self.assertEqual(self.commands.calls,0)
    def test_omitted_bundle_resource(self):
        (self.bundle/'resource').write_bytes(b'resource')
        with self.assertRaises(ValueError):self.call()
        self.assertEqual(self.commands.calls,0)
    def test_added_resource_during_validation(self):
        self.commands.mutate=lambda *_:(self.bundle/'extra').write_bytes(b'extra')
        with self.assertRaises(ValueError):self.call()
        self.assertEqual(self.commands.calls,1)
    def test_infinite_deadline(self):
        self.kw['case_timeouts']['gui']=float('inf')
        with self.assertRaises(ValueError):self.call()
        self.assertEqual(self.commands.calls,0);self.assertTrue(self.commands.terminal_failure);self.assertTrue(self.commands.persisted)
if __name__=='__main__':unittest.main()
