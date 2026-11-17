from pathlib import Path
import tempfile,unittest,os
from suite_transaction import environment_digest
from validator_runner import run_validations
from identity import capture_files
class FakeCommands:
    def __init__(self,root):self.root=root;self.out=root;self.ledger=[];self.terminal_failure=False;self.calls=0;self.mutate=None
    def persist(self):self.persisted=True
    def remaining(self):return 3600
    def run(self,label,argv,cwd,timeout,env,replace_env,require_group_empty):
        self.calls+=1;self.ledger.append({'label':label,'argv':argv,'cwd':str(cwd),'exit':0,'timeout_seconds':timeout,'executed_argv':argv,'environment_sha256':environment_digest(env),'replace_environment':replace_env,'owned_process_group_empty':require_group_empty})
        a=self.root/(label+'.stdout');b=self.root/(label+'.stderr');a.write_text('SUCCESS\n');b.write_text('')
        if self.mutate:self.mutate(self,a,b)
        return a,b
class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.repo=self.root/'repo';self.repo.mkdir();(self.repo/'scripts').mkdir();(self.repo/'scripts/run-pluginval-with-core.py').write_text('# inert wrapper')
        self.bin=self.root/'bin';self.bin.mkdir()
        for name in ('python3','timeout','xvfb-run'):
            p=self.bin/name;p.write_text('#!/bin/sh\n');p.chmod(0o755)
        self.logs=self.root/'logs';self.logs.mkdir()
        self.v=self.root/'pluginval';self.v.write_bytes(b'validator')
        self.bundle=self.root/'bundle';self.bundle.mkdir();(self.bundle/'module').write_bytes(b'module')
        self.commands=FakeCommands(self.logs)
        self.kw=dict(arm='stock',repo=self.repo,validator=self.v,bundle=self.bundle,output=self.root/'out',commands=self.commands,
                     expected_validator_sha256=capture_files(self.root,['pluginval'])['pluginval'],module_root=self.bundle,
                     module_hashes=capture_files(self.bundle,['module']),case_timeouts={'headless':300,'gui':300,'gui-seed-regression':250})
        runtime={str(self.v):self.kw['expected_validator_sha256'],**{str(self.bin/name):capture_files(self.bin,[name])[name] for name in ('python3','timeout','xvfb-run')}}
        self.kw['contract']={'environment':{'PATH':str(self.bin)},'runtime_hashes':runtime,'tool_resolutions':{name:str(self.bin/name) for name in ('python3','timeout','xvfb-run')},'source_hashes':capture_files(self.repo,['scripts/run-pluginval-with-core.py']),'validator_release':{'version':'1.0.4','sha256':self.kw['expected_validator_sha256']},'module_directories':[],'entry_cap':1000,'byte_cap':1048576,'log_byte_cap':1000}
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
    def test_prior_log_drift(self):
        def mutate(c,*_):
            if c.calls==2:(self.logs/'stock-headless.stdout').write_text('changed')
        self.commands.mutate=mutate
        with self.assertRaisesRegex(ValueError,'retained validator log changed'):self.call()
        self.assertEqual(self.commands.calls,2)
    def test_postrun_output_symlink(self):
        self.commands.mutate=lambda *_:(self.root/'out'/'link').symlink_to(self.bundle/'module')
        with self.assertRaisesRegex(ValueError,'symlink/special'):self.call()
        self.assertEqual(self.commands.calls,1)
    def test_postrun_deadline(self):
        self.commands.mutate=lambda c,*_:setattr(c,'remaining',lambda:(_ for _ in ()).throw(ValueError('deadline exhausted')))
        with self.assertRaisesRegex(ValueError,'deadline exhausted'):self.call()
        self.assertEqual(self.commands.calls,1)
    def test_infinite_deadline(self):
        self.kw['case_timeouts']['gui']=float('inf')
        with self.assertRaises(ValueError):self.call()
        self.assertEqual(self.commands.calls,0);self.assertTrue(self.commands.terminal_failure);self.assertTrue(self.commands.persisted)
if __name__=='__main__':unittest.main()
