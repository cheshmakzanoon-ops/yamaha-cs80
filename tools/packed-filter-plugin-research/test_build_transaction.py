"""Inert actual compile/archive/link transaction; generated binaries never run."""
import hashlib,json,os,pathlib,shutil,subprocess,sys,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
sys.path.insert(0,str(ROOT.parent/'production-pic-pgo-research'))
from execution import Commands
from build_transaction import _build_arm
from elf_proof import PROCESS
H=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()

@unittest.skipUnless(os.environ.get('CS80_CMAKE') and os.environ.get('CS80_NINJA'),'explicit CMake/Ninja for inert build transaction')
class BuildTransactionTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.p=pathlib.Path(self.temp.name);self.repo=self.p/'repo';self.repo.mkdir();self.build=self.p/'build';self.cmake=pathlib.Path(os.environ['CS80_CMAKE']);self.ninja=pathlib.Path(os.environ['CS80_NINJA']);self.ar=pathlib.Path(shutil.which('ar')).resolve();self.cxx=pathlib.Path(shutil.which('g++')).resolve()
  self.env={'PATH':str(self.ninja.parent)+':/usr/bin:/bin','HOME':str(self.p),'LANG':'C','TZ':'UTC'}
  self.source=self.repo/'src/core/Ig00156Batch.cpp';self.source.parent.mkdir(parents=True)
  self.source.write_text('extern "C" __attribute__((noinline,visibility("hidden"))) double helper(double x){return x+x;}\nextern "C" double process(double) __asm__("'+PROCESS+'");\nextern "C" __attribute__((noinline,visibility("hidden"))) double process(double x){return helper(x)*1.25;}\n')
  (self.repo/'driver.cpp').write_text('extern "C" double process(double) __asm__("'+PROCESS+'");\nextern "C" double entry(double x){return process(x);}\nint main(){return 0;}\n')
  (self.repo/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.25)
project(inert LANGUAGES CXX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library(cs80_core STATIC src/core/Ig00156Batch.cpp)
add_library(cap STATIC src/core/Ig00156Batch.cpp)
target_compile_definitions(cap PRIVATE CS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY=1)
add_library(CS80Reference_VST3 SHARED driver.cpp)
add_executable(CS80Reference_Standalone driver.cpp)
foreach(t CS80Reference_VST3 CS80Reference_Standalone)
 target_link_libraries(${t} PRIVATE cs80_core)
endforeach()
''')
  shutil.copy2(ROOT/'link_overlay.cmake',self.repo/'link_overlay.cmake')
  subprocess.run([str(self.cmake),'-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES='+str(self.repo/'link_overlay.cmake'),'-S',str(self.repo),'-B',str(self.build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_COMPILER='+str(self.cxx),'-DCMAKE_MAKE_PROGRAM='+str(self.ninja)],env=self.env,check=True,capture_output=True)
  db=json.loads((self.build/'compile_commands.json').read_bytes());ninja=subprocess.check_output([str(self.ninja),'-C',str(self.build),'-t','commands'],env=self.env,text=True).splitlines()
  pick=lambda text:next(line for line in ninja if text in line)
  actions={'core_compile':pick('-o CMakeFiles/cs80_core.dir/src/core/Ig00156Batch.cpp.o'),'cap_compile':pick('-o CMakeFiles/cap.dir/src/core/Ig00156Batch.cpp.o'),'archive':pick('libcs80_core.a '),'CS80Reference_VST3':pick('-o libCS80Reference_VST3.so'),'CS80Reference_Standalone':pick('-o CS80Reference_Standalone ')}
  self.c={'arm':'stock','build_tree_entry_cap':10000,'repo':str(self.repo),'build':str(self.build),'output':str(self.p/'stock'),'cmake':str(self.cmake),'ar':str(self.ar),'ninja':str(self.ninja),'environment':self.env,'runtime_hashes':{str(p):H(p) for p in (self.cmake,self.ninja,self.ar,self.cxx)},'compile_database':db,'ninja_commands':ninja,'required_actions':actions,'source_hashes':{str(p.relative_to(self.repo)):H(p) for p in self.repo.rglob('*') if p.is_file()},'input_manifests':[{'root':str(self.build),'sha256':{n:H(self.build/n) for n in ('build.ninja','CMakeCache.txt','compile_commands.json')}}],'archive':'libcs80_core.a','finals':{t:{'binary':('libCS80Reference_VST3.so' if t.endswith('VST3') else t),'map':t+'.research.map'} for t in ('CS80Reference_VST3','CS80Reference_Standalone')}}
  self.commands=Commands(self.p/'logs',total_seconds=60)
 def tearDown(self):self.temp.cleanup()
 def run_arm(self):return _build_arm(self.c,self.commands,expected_count=4)
 def test_stock_and_candidate_actual_links(self):
  stock=self.run_arm();self.assertTrue(stock['build_admitted']);self.assertEqual(len(stock['bindings']),2)
  self.source.write_text(self.source.read_text().replace('*1.25','*1.5'))
  self.c.update(arm='candidate',output=str(self.p/'candidate'),stock_seal=stock['sealed'],stock_objects=stock['objects'],stock_link_outputs=stock['link_outputs'],stock_source_hashes=stock['source_hashes'],stock_contract_sha256=stock['contract_sha256'],candidate_source_sha256=H(self.source))
  self.c['source_hashes']['src/core/Ig00156Batch.cpp']=H(self.source)
  candidate=self.run_arm();self.assertTrue(candidate['build_admitted']);self.assertNotEqual(candidate['objects'],stock['objects'])
 def candidate_contract(self,stock):
  self.source.write_text(self.source.read_text().replace('*1.25','*1.5'))
  self.c.update(arm='candidate',output=str(self.p/'candidate'),stock_seal=stock['sealed'],stock_objects=stock['objects'],stock_link_outputs=stock['link_outputs'],stock_source_hashes=stock['source_hashes'],stock_contract_sha256=stock['contract_sha256'],candidate_source_sha256=H(self.source))
  self.c['source_hashes']['src/core/Ig00156Batch.cpp']=H(self.source)
 def test_corrupt_stock_seal_prevents_removal(self):
  stock=self.run_arm();self.candidate_contract(stock)
  (pathlib.Path(stock['sealed'])/self.c['archive']).write_bytes(b'corrupt')
  with self.assertRaisesRegex(ValueError,'equivalent stock copies'):self.run_arm()
  self.assertTrue((self.build/self.c['archive']).is_file())
  self.assertFalse(any(r.get('label')=='candidate-normal-all-build' for r in self.commands.ledger))
 def test_second_source_change_rejected(self):
  stock=self.run_arm();self.candidate_contract(stock);p=self.repo/'driver.cpp';p.write_text(p.read_text()+'\n');self.c['source_hashes']['driver.cpp']=H(p)
  with self.assertRaisesRegex(ValueError,'only exact filter source'):self.run_arm()
  self.assertFalse(any(r.get('label')=='candidate-normal-all-build' for r in self.commands.ledger))
 def test_candidate_common_environment_cannot_change(self):
  stock=self.run_arm();self.candidate_contract(stock);self.c['environment']['EXTRA']='changed'
  with self.assertRaisesRegex(ValueError,'cross-arm build contract changed'):self.run_arm()
  self.assertFalse(any(r.get('label')=='candidate-normal-all-build' for r in self.commands.ledger))
 def test_output_must_not_nest_in_stock_seal(self):
  stock=self.run_arm();self.candidate_contract(stock);out=pathlib.Path(stock['sealed'])/'new-output';self.c['output']=str(out)
  with self.assertRaisesRegex(ValueError,'separate stock seal'):self.run_arm()
  self.assertFalse(out.exists())
 def test_unrelated_object_mutation_is_terminal(self):
  stock=self.run_arm();self.candidate_contract(stock);run=self.commands.run
  def mutate(label,*args,**kwargs):
   result=run(label,*args,**kwargs)
   if label=='candidate-normal-all-build':
    path=self.build/'CMakeFiles/CS80Reference_VST3.dir/driver.cpp.o';path.write_bytes(path.read_bytes()+b'drift')
   return result
  self.commands.run=mutate
  with self.assertRaisesRegex(ValueError,'only exact two filter objects'):self.run_arm()
  self.assertFalse((self.p/'candidate/build-result.json').exists())
 def test_postbuild_deadline_cannot_admit(self):
  run=self.commands.run
  def expire(label,*args,**kwargs):
   result=run(label,*args,**kwargs)
   if label=='stock-normal-all-build':self.commands.deadline=0
   return result
  self.commands.run=expire
  with self.assertRaisesRegex(ValueError,'deadline exhausted'):self.run_arm()
  self.assertTrue(self.commands.terminal_failure)
 def test_archive_action_requires_exact_member_roster(self):
  old=self.c['required_actions']['archive'];new=old.replace('CMakeFiles/cs80_core.dir/src/core/Ig00156Batch.cpp.o','wrong.o');self.c['required_actions']['archive']=new;self.c['ninja_commands'].append(new)
  with self.assertRaisesRegex(ValueError,'core archive object roster'):self.run_arm()
  self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_dangling_final_output_rejected_before_build(self):
  outside=self.p/'outside-new';(self.build/'libCS80Reference_VST3.so').symlink_to(outside)
  with self.assertRaisesRegex(ValueError,'nonsymlink path'):self.run_arm()
  self.assertFalse(outside.exists());self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_dangling_core_object_rejected_before_build(self):
  outside=self.p/'outside-object';(self.build/'CMakeFiles/cs80_core.dir/src/core/Ig00156Batch.cpp.o').symlink_to(outside)
  with self.assertRaisesRegex(ValueError,'nonsymlink path'):self.run_arm()
  self.assertFalse(outside.exists());self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_output_parent_symlink_rejected_before_build(self):
  outside=self.p/'external';outside.mkdir();parent=self.build/'CMakeFiles/cs80_core.dir/src/core';shutil.rmtree(parent);parent.symlink_to(outside,target_is_directory=True)
  with self.assertRaisesRegex(ValueError,'nonsymlink path'):self.run_arm()
  self.assertEqual(list(outside.iterdir()),[]);self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_persisted_contract_contains_environment_digest_only(self):
  self.c['environment']['INERT_PRIVATE_VALUE']='not-a-real-secret-marker'
  result=self.run_arm();self.assertIn('environment_sha256',result['frozen_contract']);self.assertNotIn('environment',result['frozen_contract'])
  for p in (self.p/'stock/build-result.json',self.p/'logs/execution-ledger.json'):
   self.assertNotIn('not-a-real-secret-marker',p.read_text())
 def test_dangling_depfile_rejected_before_command(self):
  outside=self.p/'outside-dependency';(self.build/'CMakeFiles/cs80_core.dir/src/core/Ig00156Batch.cpp.o.d').symlink_to(outside)
  with self.assertRaisesRegex(ValueError,'build tree symlink'):self.run_arm()
  self.assertFalse(outside.exists());self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_dangling_ninja_state_rejected_before_command(self):
  outside=self.p/'outside-ninja';(self.build/'.ninja_log').symlink_to(outside)
  with self.assertRaisesRegex(ValueError,'build tree symlink'):self.run_arm()
  self.assertFalse(outside.exists());self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_build_tree_entry_cap_rejected_before_command(self):
  self.c['build_tree_entry_cap']=1
  with self.assertRaisesRegex(ValueError,'total-entry bound'):self.run_arm()
  self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_hardlinked_depfile_rejected_before_command(self):
  outside=self.p/'outside-hardlink';outside.write_text('unchanged');os.link(outside,self.build/'CMakeFiles/cs80_core.dir/src/core/Ig00156Batch.cpp.o.d')
  with self.assertRaisesRegex(ValueError,'hardlinked file'):self.run_arm()
  self.assertEqual(outside.read_text(),'unchanged');self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_special_build_node_rejected_before_command(self):
  os.mkfifo(self.build/'unexpected-fifo')
  with self.assertRaisesRegex(ValueError,'build tree symlink/special'):self.run_arm()
  self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_missing_action_is_terminal(self):
  self.c['required_actions']['archive']='not an actual command'
  with self.assertRaises(ValueError):self.run_arm()
  self.assertTrue(self.commands.terminal_failure);self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_wrong_filter_compile_flags_rejected(self):
  old=self.c['required_actions']['core_compile'];new=old.replace('-O3','-O0');self.c['required_actions']['core_compile']=new;self.c['ninja_commands'].append(new)
  with self.assertRaisesRegex(ValueError,'compile action differs'):self.run_arm()
  self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_wrong_map_option_rejected(self):
  key='CS80Reference_VST3';old=self.c['required_actions'][key];new=old.replace('.research.map','.wrong.map');self.c['required_actions'][key]=new;self.c['ninja_commands'].append(new)
  with self.assertRaisesRegex(ValueError,'map-only link option'):self.run_arm()
  self.assertFalse(any('argv' in r for r in self.commands.ledger))
 def test_stock_preexisting_object_rejected(self):
  p=self.build/'CMakeFiles/cs80_core.dir/src/core/Ig00156Batch.cpp.o';p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'stale')
  with self.assertRaisesRegex(ValueError,'must be absent'):self.run_arm()
  self.assertFalse(any(r.get('label')=='stock-normal-all-build' for r in self.commands.ledger))
 def test_input_drift_rejected_before_build(self):
  (self.build/'build.ninja').write_text('# drift\n')
  with self.assertRaises(ValueError):self.run_arm()
  self.assertFalse(any('argv' in r for r in self.commands.ledger))
if __name__=='__main__':unittest.main()
