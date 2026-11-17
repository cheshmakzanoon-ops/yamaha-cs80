"""Real inert CTest transaction integration, never the CS80 suite."""
import hashlib,json,os,pathlib,shutil,subprocess,sys,tempfile,unittest
from unittest.mock import patch
from contextlib import chdir
ROOT=pathlib.Path(__file__).parent
sys.path.insert(0,str(ROOT.parent/'production-pic-pgo-research'))
from execution import Commands
from suite_transaction import _execute_suite,environment_digest,run_original_suite,bounded_entries
H=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()

@unittest.skipUnless(os.environ.get('CS80_CTEST'),'explicit frozen CTest for inert integration')
class SuiteTransactionTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.temp.name);self.build=self.root/'build';self.build.mkdir();self.module=self.root/'module';self.module.mkdir();(self.module/'plugin').write_bytes(b'inert sealed bytes');self.ctest=pathlib.Path(os.environ['CS80_CTEST']);self.python=pathlib.Path(sys.executable).resolve()
  self.env={'PATH':'/usr/bin:/bin','HOME':str(self.root),'LANG':'C','TZ':'UTC'}
 def tearDown(self):self.temp.cleanup()
 def prepare(self,scripts,extra=None):
  lines=[]
  for index,(name,text) in enumerate(scripts.items()):
   script=self.root/(str(index)+'.py');script.write_text(text)
   lines += [f'add_test([=[{name}]=] "{self.python}" "{script}")',f'set_tests_properties([=[{name}]=] PROPERTIES TIMEOUT "2" WORKING_DIRECTORY "{self.build}")']
  if extra:lines+=extra
  definitions=self.build/'CTestTestfile.cmake';definitions.write_text('\n'.join(lines)+'\n')
  p=subprocess.run([str(self.ctest),'--test-dir',str(self.build),'--show-only=json-v1'],capture_output=True,text=True,env=self.env,timeout=10);self.assertEqual(p.returncode,0,p.stderr);self.document=json.loads(p.stdout)
  shutil.rmtree(self.build/'Testing')
  self.commands=Commands(self.root/'commands',total_seconds=60)
  self.args=dict(arm='stock',build=self.build,ctest=self.ctest,document=self.document,definition_hashes={'CTestTestfile.cmake':H(definitions)},executable_hashes={str(self.python):H(self.python)},runtime_hashes={str(self.ctest):H(self.ctest)},module_root=self.module,module_hashes={'plugin':H(self.module/'plugin')},module_directories=[],environment=self.env,output=self.root/'result',commands=self.commands,case_timeouts={name:10 for name in scripts})
 def test_full_order_fresh_archives_environment(self):
  self.prepare({'one':'print("one")','space (x)+':'import os;print(os.getenv("CS80_PARENT_SECRET", "absent"))'})
  with patch.dict(os.environ,{'CS80_PARENT_SECRET':'not-inherited'}):result=_execute_suite(**self.args)
  self.assertTrue(result['complete']);self.assertEqual([x['name'] for x in result['cases']],['one','space (x)+']);self.assertFalse(self.commands.terminal_failure)
  for i in range(2):
   case=self.root/'result/cases'/f'{i:04d}';self.assertTrue((case/'Test.xml').is_file());self.assertTrue((case/'LastTest.log').is_file());self.assertTrue((case/'selection.LastTest.log').is_file())
  self.assertIn('absent',(self.root/'result/cases/0001/LastTest.log').read_text());self.assertFalse((self.build/'Testing/TAG').exists());self.assertFalse(list(self.build.glob('Testing/**/Test.xml')))
  for row in self.commands.ledger:self.assertEqual(row['environment_sha256'],environment_digest(self.env))
 def test_regex_masked_first_exit_rejected(self):
  self.prepare({'cs80_teardown_probe':'print("JUCE shut down");raise SystemExit(42)'},['set_tests_properties([=[cs80_teardown_probe]=] PROPERTIES PASS_REGULAR_EXPRESSION "JUCE shut down; static destructors run")'])
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertTrue(self.commands.terminal_failure);self.assertFalse((self.root/'result/transaction.json').read_text().find('"complete": true')>=0)
 def test_nonzero_stops_later_case(self):
  marker=self.root/'should-not-run';self.prepare({'bad':'raise SystemExit(2)','later':f'open({str(marker)!r},"w").write("bad")'})
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertTrue(self.commands.terminal_failure);self.assertFalse(marker.exists())
 def test_module_drift_terminal(self):
  self.prepare({'one':f'open({str(self.module/"plugin")!r},"w").write("changed")','later':'pass'})
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertTrue(self.commands.terminal_failure);self.assertFalse(any(x.get('label')=='stock-test-0001' for x in self.commands.ledger))
 def test_stale_tag_rejected(self):
  self.prepare({'one':'pass'});(self.build/'Testing').mkdir();(self.build/'Testing/TAG').write_text('20200101-0000\nExperimental\n')
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertEqual(len(self.commands.ledger),1);self.assertTrue(self.commands.terminal_failure)
 def test_stale_log_rejected(self):
  self.prepare({'one':'pass'});p=self.build/'Testing/Temporary';p.mkdir(parents=True);(p/'LastTest.log').write_text('stale')
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertTrue(self.commands.terminal_failure)
 def test_definition_drift_before_execution(self):
  self.prepare({'one':'pass'});(self.build/'CTestTestfile.cmake').write_text('# changed\n')
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertFalse(any('argv' in x for x in self.commands.ledger))
 def test_outer_timeout_cannot_shrink_original(self):
  self.prepare({'one':'pass'});self.args['case_timeouts']={'one':1}
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertTrue(self.commands.terminal_failure)
 def test_existing_output_untouched(self):
  self.prepare({'one':'pass'});out=self.args['output'];out.mkdir();(out/'sentinel').write_text('safe')
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertEqual({p.name for p in out.iterdir()},{'sentinel'})
 def test_repeated_arm_stops(self):
  self.prepare({'one':'pass'});_execute_suite(**self.args)
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertTrue(self.commands.terminal_failure)
 def test_skip77_is_not_admission(self):
  self.prepare({'skip':'raise SystemExit(77)'},['set_tests_properties([=[skip]=] PROPERTIES SKIP_RETURN_CODE "77")'])
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertTrue(self.commands.terminal_failure)
 def test_original_timeout_is_terminal(self):
  self.prepare({'slow':'import time;time.sleep(10)'})
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertTrue(self.commands.terminal_failure);self.assertTrue((self.root/'result/transaction-failure.json').is_file())
 def test_actual_environment_ledger_binding(self):
  self.prepare({'one':'pass'});run=self.commands.run
  def corrupt(*args,**kwargs):
   result=run(*args,**kwargs);self.commands.ledger[-1]['environment_sha256']='0'*64;return result
  self.commands.run=corrupt
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertTrue(self.commands.terminal_failure);self.assertFalse(any(x.get('label')=='stock-test-0000' for x in self.commands.ledger))
 def test_selected_inventory_binding(self):
  self.prepare({'one':'pass'});run=self.commands.run
  def corrupt(*args,**kwargs):
   result=run(*args,**kwargs)
   if '--show-only=json-v1' in args[1]:result[0].write_text('{"tests": []}')
   return result
  self.commands.run=corrupt
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertTrue(self.commands.terminal_failure)
 def test_fifo_is_rejected_before_child(self):
  self.prepare({'one':'pass'});os.mkfifo(self.module/'extra')
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertFalse(any('argv' in x for x in self.commands.ledger))
 def test_post_capture_deadline_is_terminal(self):
  self.prepare({'one':'pass'})
  def after(*args):self.commands.deadline=0
  self.args['capture_handlers']={'before':lambda name:None,'after':after}
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertTrue(self.commands.terminal_failure)
 def test_definition_hook_before_inventory_rejected(self):
  self.prepare({'one':'pass'});marker=self.root/'executed-hook'
  definition=self.build/'CTestTestfile.cmake';definition.write_text(f'execute_process(COMMAND touch "{marker}")\n'+definition.read_text())
  (self.root/'source').write_text('frozen')
  contract={**self.args,'repo':str(self.root),'source_hashes':{'source':H(self.root/'source')},'sealed_captures':str(self.root/'sealed'),'output':str(self.root/'public-result')}
  with self.assertRaisesRegex(ValueError,"raw CTest definitions changed"):run_original_suite(contract,self.commands)
  self.assertFalse(marker.exists());self.assertFalse(any('argv' in x for x in self.commands.ledger))
 def test_frozen_arbitrary_definition_hook_is_rejected(self):
  self.prepare({'one':'pass'});definition=self.build/'CTestTestfile.cmake';definition.write_text('execute_process(COMMAND false)\n'+definition.read_text());self.args['definition_hashes']['CTestTestfile.cmake']=H(definition)
  with self.assertRaisesRegex(ValueError,'unsupported generated definition command'):_execute_suite(**self.args)
  self.assertFalse(any('argv' in x for x in self.commands.ledger))
 def test_failure_evidence_aggregate_bound(self):
  self.prepare({'one':'raise SystemExit(2)'})
  directory=self.build/'Testing/large';directory.mkdir(parents=True)
  for i in range(5):
   with (directory/str(i)).open('wb') as stream:stream.truncate(64*1024*1024)
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  result=json.loads((self.root/'result/transaction-failure.json').read_text())
  self.assertIn('aggregate-size/count',result['failure_capture_error']);self.assertEqual(len(list(directory.iterdir())),5)
 def test_malformed_public_contract_latches(self):
  self.prepare({'one':'pass'})
  with self.assertRaises(KeyError):run_original_suite({},self.commands)
  self.assertTrue(self.commands.terminal_failure)
 def test_relative_nested_output_rejected_before_creation(self):
  self.prepare({'one':'pass'});target=self.build/'nested-output'
  contract={**self.args,'repo':str(self.root),'sealed_captures':str(self.root/'sealed'),'output':'build/nested-output'}
  with chdir(self.root),self.assertRaisesRegex(ValueError,'roots must be disjoint'):run_original_suite(contract,self.commands)
  self.assertFalse(target.exists());self.assertFalse(any('argv' in row for row in self.commands.ledger))
 def test_final_audit_deadline_is_terminal(self):
  import suite_transaction
  self.prepare({'one':'pass'});audit=suite_transaction.audit_runtime;calls=[]
  def expire(**kwargs):
   result=audit(**kwargs);calls.append(1)
   if len(calls)==5:self.commands.deadline=0
   return result
  with patch.object(suite_transaction,'audit_runtime',expire),self.assertRaisesRegex(ValueError,'deadline exhausted'):_execute_suite(**self.args)
  self.assertEqual(len(calls),5);self.assertTrue(self.commands.terminal_failure)
 def test_bad_environment(self):
  self.prepare({'one':'pass'});self.args['environment']={'BAD':'null\0'}
  with self.assertRaises(ValueError):_execute_suite(**self.args)
  self.assertFalse(any('argv' in x for x in self.commands.ledger))

class ExactEnvironmentTests(unittest.TestCase):
 def test_legacy_merge_and_exact_replacement(self):
  with tempfile.TemporaryDirectory() as d,patch.dict(os.environ,{'CS80_PARENT_SENTINEL':'present'}):
   commands=Commands(pathlib.Path(d)/'logs',total_seconds=10)
   argv=[sys.executable,'-c','import os;print(os.getenv("CS80_PARENT_SENTINEL","absent"))']
   first,_=commands.run('legacy',argv,cwd=d,env={'A':'B'});second,_=commands.run('exact',argv,cwd=d,env={},replace_env=True)
   self.assertEqual(first.read_text().strip(),'present');self.assertEqual(second.read_text().strip(),'absent');self.assertEqual(commands.ledger[-1]['environment_sha256'],environment_digest({}))
 def test_live_descendant_rejects_successful_leader(self):
  with tempfile.TemporaryDirectory() as d:
   commands=Commands(pathlib.Path(d)/'logs',total_seconds=10)
   argv=[sys.executable,'-c','import subprocess,sys;subprocess.Popen([sys.executable,"-c","import time;time.sleep(30)"])']
   with self.assertRaises(ValueError):commands.run('descendant',argv,cwd=d,require_group_empty=True)
   self.assertTrue(commands.terminal_failure)
 def test_diagnostic_directories_count_toward_cap(self):
  with tempfile.TemporaryDirectory() as d:
   root=pathlib.Path(d)
   for i in range(5):(root/str(i)).mkdir()
   with self.assertRaisesRegex(ValueError,'total-entry bound'):list(bounded_entries(root,limit=4))
 def test_invalid_environment_rejected(self):
  for value in ({'BAD=KEY':'x'},{'X':'a\0b'},{1:'x'},{'X':1}):
   with tempfile.TemporaryDirectory() as d:
    commands=Commands(pathlib.Path(d)/'logs')
    with self.assertRaises(ValueError):commands.run('bad',[sys.executable,'-c','pass'],cwd=d,env=value,replace_env=True)
if __name__=='__main__':unittest.main()
