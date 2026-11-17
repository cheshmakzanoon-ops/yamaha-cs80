"""Actual tiny configure/inspection only; no project build or binary execution."""
import copy,hashlib,json,os,pathlib,shutil,subprocess,sys,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
sys.path.insert(0,str(ROOT.parent/'production-pic-pgo-research'))
from execution import Commands
from configure_transaction import _configure_arm,_tree,_map_parity,configure_arm,IMPLEMENTATION_INPUTS,_retain,_ninja_graph_parity
from configure_preflight import configure_argv
H=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()

@unittest.skipUnless(os.environ.get('CS80_CMAKE') and os.environ.get('CS80_NINJA'),'explicit tools required for inert configure transaction')
class ConfigureTransactionTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=pathlib.Path(self.temp.name);self.repo=self.root/'repo';self.repo.mkdir();self.build=self.root/'build'
  self.cmake=pathlib.Path(os.environ['CS80_CMAKE']);self.ninja=pathlib.Path(os.environ['CS80_NINJA']);self.ctest=self.cmake.parent/'ctest';self.cxx=pathlib.Path(shutil.which('g++')).resolve();self.ar=pathlib.Path(shutil.which('ar')).resolve();self.ranlib=pathlib.Path(shutil.which('ranlib')).resolve()
  self.env={'PATH':str(self.ninja.parent)+':/usr/bin:/bin','HOME':str(self.root),'LANG':'C','TZ':'UTC','CXX':str(self.cxx)}
  (self.repo/'tiny.cpp').write_text('int main() { return 0; }\n')
  (self.repo/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(inert LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
enable_testing()
add_library(CS80Reference_VST3 MODULE tiny.cpp)
add_executable(CS80Reference_Standalone tiny.cpp)
add_test(NAME inert COMMAND CS80Reference_Standalone)
set_tests_properties(inert PROPERTIES TIMEOUT 10)
''')
  self.overlay=self.repo/'link_overlay.cmake';shutil.copy2(ROOT/'link_overlay.cmake',self.overlay)
  expected={}
  # Independent inert setup freezes exact expected phase bytes, then removes only its temp build.
  for phase in ('bootstrap','baseline','overlay'):
   argv=configure_argv(self.repo,self.build);argv[0]=str(self.cmake)
   if phase=='overlay':argv+=['-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES='+str(self.overlay)]
   subprocess.run(argv,env=self.env,capture_output=True,check=True,timeout=30)
   tree=_tree(self.build,10000,20*1024**2)
   definitions={p:h for p,h in tree['sha256'].items() if pathlib.Path(p).name=='CTestTestfile.cmake'}
   required={'CMakeCache.txt','build.ninja','CMakeFiles/rules.ninja','compile_commands.json',*definitions}
   required.update(p for p in tree['sha256'] if p.endswith(('CMakeCXXCompiler.cmake','CMakeSystem.cmake')))
   expected[phase]={'generated':{p:tree['sha256'][p] for p in sorted(required)},'definitions':definitions,'compile_sha256':tree['sha256']['compile_commands.json'],'ninja_commands':subprocess.check_output([str(self.ninja),'-C',str(self.build),'-t','commands'],env=self.env,text=True).splitlines()}
  # CMake uses canonical driver but /usr/bin/ar spelling; freeze exact observed cache value.
  from configure_transaction import _cache
  cache=_cache(self.build/'CMakeCache.txt');cache_runtime={k:cache[k]['value'] for k in ('CMAKE_CXX_COMPILER','CMAKE_AR','CMAKE_RANLIB','CMAKE_MAKE_PROGRAM')}
  shutil.rmtree(self.build)
  self.c={'arm':'stock','repo':str(self.repo),'build':str(self.build),'output':str(self.root/'evidence'),'cmake':str(self.cmake),'ninja':str(self.ninja),'ctest':str(self.ctest),'overlay':str(self.overlay),'environment':self.env,'helper_hashes':{str(p):H(p) for p in IMPLEMENTATION_INPUTS},'source_hashes':{p.name:H(p) for p in self.repo.iterdir()},'runtime_hashes':{str(p):H(p) for p in (self.cmake,self.ninja,self.ctest,self.cxx,self.ar,self.ranlib,self.overlay)},'cache_runtime':cache_runtime,'cache_runtime_files':{k:str(pathlib.Path(v).resolve()) for k,v in cache_runtime.items()},'seed':{'sha256':{},'directories':[]},'expected':expected,'generated_evidence_mode':False,'dependencies':[],'expected_test_names':['inert'],'retained_entry_cap':40000,'retained_byte_cap':100*1024**2,'tree_entry_cap':10000,'tree_byte_cap':20*1024**2,'finals':{t:{'binary':'libCS80Reference_VST3.so' if t.endswith('VST3') else t,'map':t+'.research.map'} for t in ('CS80Reference_VST3','CS80Reference_Standalone')}}
  self.commands=Commands(self.root/'logs',total_seconds=60)
 def call(self):return _configure_arm(self.c,self.commands,expected_compile_count=2,expected_test_count=1)
 def failure(self,pattern):
  with self.assertRaisesRegex((ValueError,RuntimeError),pattern):self.call()
  self.assertTrue(self.commands.terminal_failure);self.assertFalse((self.root/'evidence/configure-result.json').exists())
 def test_complete_actual_transaction(self):
  result=self.call();self.assertTrue(result['configuration_admitted']);self.assertFalse(result['production_qualified'])
  self.assertEqual(len([r for r in self.commands.ledger if 'argv' in r]),9)
  self.assertTrue(all(r['replace_environment'] and r['owned_process_group_empty'] for r in self.commands.ledger))
  self.assertFalse(list(self.build.glob('*.research.map')))
  self.assertFalse((self.build/'CS80Reference_Standalone').exists())
  self.assertEqual(result['phases']['baseline']['compile_sha256'],result['phases']['overlay']['compile_sha256'])
  for phase in ('bootstrap','baseline','overlay'):
   manifest=result['phases'][phase]['build_tree_manifest']['sha256']
   self.assertEqual({p:H(self.root/'evidence'/phase/p) for p in manifest},manifest)
  raw=(self.root/'evidence/configure-result.json').read_text();self.assertNotIn('"environment":',raw);self.assertNotIn('"HOME":',raw)
 def test_public_requires_full_normal_contract(self):
  with self.assertRaises(ValueError):configure_arm(self.c,self.commands)
  self.assertTrue(self.commands.terminal_failure)
 def test_helper_scope_omitted(self):self.c['helper_hashes'].pop(str(IMPLEMENTATION_INPUTS[0]));self.failure('implementation scope omitted')
 def test_helper_hash_drift(self):self.c['helper_hashes'][str(IMPLEMENTATION_INPUTS[0])]='0'*64;self.failure('runtime/overlay changed')
 def test_authenticated_generated_evidence(self):
  self.c['generated_evidence_mode']=True
  for phase in self.c['expected'].values():
   phase['compile_sha256']=None;phase['ninja_commands']=None
   for group in ('generated','definitions'):phase[group]={p:None for p in phase[group]}
  result=self.call();self.assertEqual(result['generated_evidence_provenance'],'authenticated-configure-observation-not-preauthorized-hashes')
 def test_cumulative_retention_byte_cap(self):self.c['retained_byte_cap']=1024;self.failure('cumulative retained')
 def test_cumulative_retention_entry_cap(self):self.c['retained_entry_cap']=3;self.failure('cumulative retained|entry bound')
 def test_retention_covers_three_snapshots(self):
  result=self.call();usage=result['retained_usage_before_receipt'];self.assertGreater(usage['entries'],sum(len(result['phases'][p]['build_tree_manifest']['sha256']) for p in ('bootstrap','baseline','overlay')))
 def test_observed_definition_script_blocked_before_show(self):
  self.c['generated_evidence_mode']=True
  for phase in self.c['expected'].values():
   phase['compile_sha256']=None;phase['ninja_commands']=None
   for group in ('generated','definitions'):phase[group]={p:None for p in phase[group]}
  original=self.commands.run
  def inject(label,*args,**kwargs):
   result=original(label,*args,**kwargs)
   if label=='stock-configure-bootstrap':(self.build/'CTestTestfile.cmake').write_text('execute_process(COMMAND never)\n')
   return result
  self.commands.run=inject;self.failure('unsupported|definition|empty')
  self.assertFalse(any(r.get('label')=='stock-configured-tests-bootstrap' for r in self.commands.ledger))
 def test_raw_graph_extra_dependency_rejected(self):
  self.call();before=(self.root/'evidence/baseline/build.ninja').read_bytes();after=(self.root/'evidence/overlay/build.ninja').read_bytes()+b'build unrelated: phony\n'
  with self.assertRaisesRegex(ValueError,'Ninja graph differs'):_ninja_graph_parity(before,after,self.build,self.overlay)
 def test_retirement_receipt_points_to_exact_copy(self):
  result=self.call();receipts=result['retained_ctest_artifacts_for_verified_retirement'];self.assertTrue(receipts)
  for row in receipts:self.assertEqual(H(row['path']),row['sha256']);self.assertEqual(H(row['retained_path']),row['sha256'])
 def test_source_drift(self):(self.repo/'tiny.cpp').write_text('changed');self.failure('source changed')
 def test_overlay_drift(self):self.overlay.write_text('# changed');self.failure('source changed|runtime/overlay changed')
 def test_preconfigured_seed(self):
  self.build.mkdir();(self.build/'CMakeCache.txt').write_text('x');self.c['seed']['sha256']['CMakeCache.txt']=H(self.build/'CMakeCache.txt');self.failure('preconfigured')
 def test_extra_seed(self):self.build.mkdir();(self.build/'extra').write_text('x');self.failure('trusted seed')
 def test_symlink_seed(self):self.build.mkdir();(self.build/'link').symlink_to(self.repo/'tiny.cpp');self.failure('symlink/special')
 def test_hardlink_seed(self):self.build.mkdir();os.link(self.repo/'tiny.cpp',self.build/'hard');self.failure('hardlink')
 def test_entry_bound(self):self.c['tree_entry_cap']=1;self.failure('entry bound')
 def test_byte_bound(self):self.c['tree_byte_cap']=1;self.failure('byte cap')
 def test_generated_manifest_wrong(self):self.c['expected']['baseline']['generated']['build.ninja']='0'*64;self.failure('generated input differs')
 def test_definition_roster_missing(self):
  for phase in self.c['expected'].values():phase['definitions']={}
  self.failure('CTest definition identity')
 def test_expected_compile_changed(self):
  for phase in self.c['expected'].values():phase['compile_sha256']='0'*64
  self.failure('compile database identity')
 def test_expected_tests_changed(self):self.c['expected_test_names']=['other'];self.failure('test roster')
 def test_expected_ninja_changed(self):self.c['expected']['baseline']['ninja_commands'][0]+=' wrong';self.failure('Ninja command roster')
 def test_hook_before_show_only(self):
  original=self.commands.run
  def hook(label,*args,**kwargs):
   result=original(label,*args,**kwargs)
   if label=='stock-configure-baseline':(self.build/'CTestCustom.cmake').write_text('message(FATAL_ERROR "not allowed")\n')
   return result
  self.commands.run=hook;self.failure('hook|control|custom|CTestCustom')
  self.assertFalse(any(r.get('label')=='stock-configured-tests-baseline' for r in self.commands.ledger))
 def test_environment_ctest_rejected(self):self.c['environment']['CTEST_PARALLEL_LEVEL']='9';self.failure('CTEST')
 def test_ledger_environment_tamper(self):
  original=self.commands.run
  def tamper(*args,**kwargs):
   result=original(*args,**kwargs);self.commands.ledger[-1]['environment_sha256']='wrong';return result
  self.commands.run=tamper;self.failure('command/environment binding')
 def test_postcommand_deadline(self):
  original=self.commands.run
  def expire(*args,**kwargs):
   result=original(*args,**kwargs);self.commands.deadline=0;return result
  self.commands.run=expire;self.failure('deadline exhausted')
 def test_terminal_no_retry(self):
  self.c['arm']='wrong';self.failure('explicit configure arm');self.c['arm']='stock';self.failure('terminal')
 def test_dependency_seed_exact(self):
  self.build.mkdir();dep=self.build/'_deps';dep.mkdir();(dep/'frozen').write_text('frozen dependency')
  self.c['seed']=_tree(self.build,10000,20*1024**2);self.c['dependencies']=[{'root':str(dep),'sha256':{'frozen':H(dep/'frozen')}}]
  self.assertTrue(self.call()['configuration_admitted'])
 def test_dependency_drift(self):
  dep=self.root/'dep';dep.mkdir();(dep/'frozen').write_text('drift')
  self.c['dependencies']=[{'root':str(dep),'sha256':{'frozen':'0'*64}}];self.failure('dependency changed')
 def test_missing_ranlib_handoff_rejected(self):
  self.c['cache_runtime'].pop('CMAKE_RANLIB');self.c['cache_runtime_files'].pop('CMAKE_RANLIB');self.failure('archive tools required')
 def test_cache_runtime_alias_drift(self):self.c['cache_runtime_files']['CMAKE_AR']=str(self.cxx);self.failure('alias changed')
 def test_retained_baseline_corruption(self):
  original=self.commands.run
  def tamper(label,*args,**kwargs):
   result=original(label,*args,**kwargs)
   if label=='stock-configure-overlay':(self.root/'evidence/baseline/CMakeCache.txt').write_text('corrupt')
   return result
  self.commands.run=tamper;self.failure('retained baseline/overlay')
 def test_log_corruption(self):
  original=self.commands.run
  def tamper(label,*args,**kwargs):
   result=original(label,*args,**kwargs)
   if label=='stock-configured-ninja-overlay':(self.root/'logs/stock-configure-baseline.stdout').write_text('corrupt')
   return result
  self.commands.run=tamper;self.failure('execution logs changed')
 def test_generated_symlink_terminal(self):
  original=self.commands.run
  def tamper(label,*args,**kwargs):
   result=original(label,*args,**kwargs)
   if label=='stock-configure-baseline':(self.build/'bad').symlink_to(self.repo/'tiny.cpp')
   return result
  self.commands.run=tamper;self.failure('symlink/special')
 def test_streamed_retention_preserves_mode(self):
  source=self.root/'large-inert';source.write_bytes(b'x'*(2*1024*1024+7));source.chmod(0o640);dest=self.root/'retained';calls=[]
  class Clock:
   def remaining(self):calls.append(True)
  _retain(source,dest,H(source),Clock());self.assertEqual(H(dest),H(source));self.assertEqual(dest.stat().st_mode&0o777,0o640);self.assertGreaterEqual(len(calls),6)
 def test_overlay_nonmap_change(self):
  a=['cc -o x.o -c x.cpp',': && cc -o a x.o && :',': && cc -o b x.o && :'];b=[*a]
  b[0]+=' -DCHANGED';b[1]=b[1].replace('cc ','cc -Wl,-Map,/build/CS80Reference_VST3.research.map,--cref ');b[2]=b[2].replace('cc ','cc -Wl,-Map,/build/CS80Reference_Standalone.research.map,--cref ')
  with self.assertRaisesRegex(ValueError,'non-final'):_map_parity(a,b,pathlib.Path('/build'),{'CS80Reference_VST3':{'binary':'a'},'CS80Reference_Standalone':{'binary':'b'}})
if __name__=='__main__':unittest.main()
