"""Actual inert wrapper processes; never pluginval, GUI or synth execution."""
import hashlib,json,os,pathlib,shutil,sys,tempfile,unittest
from unittest.mock import patch
ROOT=pathlib.Path(__file__).parent
sys.path.insert(0,str(ROOT.parent/'production-pic-pgo-research'))
from execution import Commands
from validator_runner import run_validations
H=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
class StrictValidatorTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.p=pathlib.Path(self.tmp.name);self.repo=self.p/'repo';(self.repo/'scripts').mkdir(parents=True);self.bin=self.p/'bin';self.bin.mkdir();self.python=pathlib.Path(sys.executable).resolve()
  (self.bin/'python3').symlink_to(self.python)
  self.write(self.bin/'xvfb-run','import os,sys;os.execv(sys.argv[4],sys.argv[4:])')
  self.write(self.bin/'timeout','import os,sys;os.execv(sys.argv[4],sys.argv[4:])')
  self.wrapper=self.repo/'scripts/run-pluginval-with-core.py';self.wrapper.write_text('import pathlib,subprocess,sys\npathlib.Path(sys.argv[sys.argv.index("--core-log")+1]).write_text("inert core receipt")\nraise SystemExit(subprocess.call(sys.argv[sys.argv.index("--")+1:]))\n')
  self.validator=self.bin/'pluginval';self.write(self.validator,'import os,pathlib,sys\np=pathlib.Path(sys.argv[sys.argv.index("--output-dir")+1]);p.mkdir(exist_ok=True)\n(p/sys.argv[sys.argv.index("--output-filename")+1]).write_text("inert report")\nprint(os.getenv("INHERITED_SECRET","absent"));print("SUCCESS")')
  self.bundle=self.p/'bundle';self.bundle.mkdir();(self.bundle/'module').write_bytes(b'inert module bytes')
  self.commands=Commands(self.p/'logs',total_seconds=60)
  self.contract={'environment':{'PATH':str(self.bin),'HOME':str(self.p),'LANG':'C','TZ':'UTC'},'runtime_hashes':{str(p):H(p) for p in (self.python,self.bin/'xvfb-run',self.bin/'timeout',self.validator)},'tool_resolutions':{'python3':str(self.python),'xvfb-run':str(self.bin/'xvfb-run'),'timeout':str(self.bin/'timeout')},'source_hashes':{'scripts/run-pluginval-with-core.py':H(self.wrapper)},'validator_release':{'version':'1.0.4','sha256':H(self.validator)},'module_directories':[],'entry_cap':100,'byte_cap':1048576,'log_byte_cap':1000}
  self.kw={'arm':'stock','repo':self.repo,'validator':self.validator,'bundle':self.bundle,'output':self.p/'output','commands':self.commands,'expected_validator_sha256':H(self.validator),'module_root':self.bundle,'module_hashes':{'module':H(self.bundle/'module')},'case_timeouts':{'headless':10,'gui':10,'gui-seed-regression':10},'contract':self.contract}
 def write(self,path,source):path.write_text('#!'+str(self.python)+'\n'+source+'\n');path.chmod(0o755)
 def tearDown(self):self.tmp.cleanup()
 def run_all(self):return run_validations(**self.kw)
 def no_commands(self):self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_actual_three_workloads_exact_environment(self):
  with patch.dict(os.environ,{'INHERITED_SECRET':'must-not-leak'}):r=self.run_all()
  self.assertEqual(len(r['validators']),3);self.assertEqual(len(r['output_inventory']['sha256']),6)
  self.assertNotIn('must-not-leak',json.dumps(r));self.assertNotIn('environment',r['common_contract'])
  for row in r['validators'].values():self.assertEqual(row['ledger_record']['exit'],0);self.assertTrue(row['ledger_record']['owned_process_group_empty'])
  self.assertIn('absent',(self.p/'logs/stock-headless.stdout').read_text())
  self.assertEqual(r['validators']['gui-seed-regression']['resolved_argv'][1:4],['--signal=TERM','--kill-after=10s','240s'])
 def test_bundle_fifo(self):
  os.mkfifo(self.bundle/'pipe')
  with self.assertRaisesRegex(ValueError,'special node'):self.run_all()
  self.no_commands()
 def test_bundle_symlink(self):
  (self.bundle/'other').symlink_to(self.bundle/'module')
  with self.assertRaisesRegex(ValueError,'symlink'):self.run_all()
  self.no_commands()
 def test_bundle_hardlink(self):
  os.link(self.bundle/'module',self.bundle/'other')
  with self.assertRaisesRegex(ValueError,'hardlink'):self.run_all()
  self.no_commands()
 def test_changed_wrapper(self):
  self.wrapper.write_text('raise SystemExit(0)')
  with self.assertRaisesRegex(ValueError,'source drift'):self.run_all()
  self.no_commands()
 def test_changed_runtime(self):
  self.write(self.bin/'xvfb-run','pass')
  with self.assertRaisesRegex(ValueError,'runtime drift'):self.run_all()
  self.no_commands()
 def test_resolution_drift(self):
  (self.bin/'python3').unlink();(self.bin/'python3').symlink_to(self.bin/'timeout')
  with self.assertRaisesRegex(ValueError,'resolution drift'):self.run_all()
  self.no_commands()
 def test_stale_output(self):
  self.kw['output'].mkdir();(self.kw['output']/'keep').write_text('safe')
  with self.assertRaisesRegex(ValueError,'fresh validator output'):self.run_all()
  self.no_commands();self.assertEqual((self.kw['output']/'keep').read_text(),'safe')
 def test_dangling_output_root(self):
  self.kw['output'].symlink_to(self.p/'missing')
  with self.assertRaisesRegex(ValueError,'nonsymlink'):self.run_all()
  self.no_commands();self.assertFalse((self.p/'missing').exists())
 def test_output_entry_bound(self):
  self.contract['entry_cap']=3
  with self.assertRaisesRegex(ValueError,'aggregate validator evidence bound'):self.run_all()
  self.assertEqual(len([r for r in self.commands.ledger if 'argv' in r]),1)
 def test_environment_object_drift_stops_before_next_case(self):
  run=self.commands.run
  def mutate(*a,**k):
   result=run(*a,**k);k['env']['HOME']='changed';return result
  self.commands.run=mutate
  with self.assertRaisesRegex(ValueError,'frozen validator environment changed'):self.run_all()
  self.assertEqual(len([r for r in self.commands.ledger if 'argv' in r]),1)
 def test_environment_ledger_drift(self):
  run=self.commands.run
  def mutate(*a,**k):
   result=run(*a,**k);self.commands.ledger[-1]['environment_sha256']='0'*64;return result
  self.commands.run=mutate
  with self.assertRaisesRegex(ValueError,'environment binding'):self.run_all()
  self.assertEqual(len([r for r in self.commands.ledger if 'argv' in r]),1)
 def test_lingering_group(self):
  self.write(self.validator,'import subprocess,sys\nsubprocess.Popen([sys.executable,"-c","import time;time.sleep(30)"])\nprint("SUCCESS")')
  self.kw['expected_validator_sha256']=H(self.validator);self.contract['runtime_hashes'][str(self.validator)]=H(self.validator);self.contract['validator_release']['sha256']=H(self.validator)
  with self.assertRaisesRegex(ValueError,'process group remains'):self.run_all()
  self.assertTrue(self.commands.terminal_failure)
 def test_output_byte_bound(self):
  self.write(self.validator,'import pathlib,sys\np=pathlib.Path(sys.argv[sys.argv.index("--output-dir")+1]);p.mkdir(exist_ok=True);(p/"large").write_bytes(b"x"*4096);print("SUCCESS")')
  self.kw['expected_validator_sha256']=H(self.validator);self.contract['runtime_hashes'][str(self.validator)]=H(self.validator);self.contract['validator_release']['sha256']=H(self.validator);self.contract['byte_cap']=1000
  with self.assertRaisesRegex(ValueError,'byte cap'):self.run_all()
  self.assertEqual(len([r for r in self.commands.ledger if 'argv' in r]),1)
 def change_prior_report(self,operation):
  source='import pathlib,sys\np=pathlib.Path(sys.argv[sys.argv.index("--output-dir")+1]);p.mkdir(exist_ok=True);name=sys.argv[sys.argv.index("--output-filename")+1]\n(p/name).write_text("report")\nif name=="cs80-gui":\n '+operation+'\nprint("SUCCESS")'
  self.write(self.validator,source);self.kw['expected_validator_sha256']=H(self.validator);self.contract['runtime_hashes'][str(self.validator)]=H(self.validator);self.contract['validator_release']['sha256']=H(self.validator)
 def test_later_case_cannot_overwrite_prior_report(self):
  self.change_prior_report('(p/"cs80-headless").write_text("changed")')
  with self.assertRaisesRegex(ValueError,'prior generated validator evidence changed'):self.run_all()
  self.assertEqual(len([r for r in self.commands.ledger if 'argv' in r]),2)
 def test_later_case_cannot_delete_prior_report(self):
  self.change_prior_report('(p/"cs80-headless").unlink()')
  with self.assertRaisesRegex(ValueError,'prior generated validator evidence changed'):self.run_all()
  self.assertEqual(len([r for r in self.commands.ledger if 'argv' in r]),2)
 def test_cross_arm_environment_cannot_change(self):
  stock=self.run_all();self.kw.update(arm='candidate',output=self.p/'candidate');self.contract['stock_common_sha256']=stock['common_sha256'];self.contract['environment']['NEW']='different'
  with self.assertRaisesRegex(ValueError,'cross-arm common'):self.run_all()
  self.assertFalse(any(r.get('label','').startswith('candidate-') and 'argv' in r for r in self.commands.ledger))
 def test_cross_arm_same_contract(self):
  stock=self.run_all();self.kw.update(arm='candidate',output=self.p/'candidate');self.contract['stock_common_sha256']=stock['common_sha256'];self.assertEqual(self.run_all()['common_sha256'],stock['common_sha256'])
if __name__=='__main__':unittest.main()
