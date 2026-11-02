from pathlib import Path
import hashlib,json,os,subprocess,signal
B=Path('/workspace/scratch/aa102d573ba3');R=B/'cs80-ui-integration';D=B/'cs80-ui-composed-evidence/green';BUILD=B/'cs80-panel-s2/build-native-red';PIN='265cf3a94cd40d0019ea8008f84167cb416c7e61';BASE='b59b42d734dc5f0c4b3684b671f9cf76c6dc33a1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(['git','-C',str(R),*a])
j=json.loads((D/'receipt.json').read_text());assert j['head']==PIN and j['source_tree']=='32ae37fe643056f66f1004066c52b61b463e695b';assert git('rev-parse','HEAD').decode().strip()=='261246fd52610b40baf96d3142ba4e077fb0fe11';assert not git('status','--porcelain')
assert all(p.startswith('docs/evidence/composed-ui-20261102/') for p in git('diff','--name-only',PIN,'HEAD').decode().splitlines())
archive=R/'docs/evidence/composed-ui-20261102';entries=[]
for line in (archive/'SHA256SUMS').read_text().splitlines():
 digest,name=line.split(None,1);name=name.lstrip('*');assert sha(archive/name)==digest;entries.append(name)
assert len(entries)==33 and len(set(entries))==33
assert set(entries)==set(str(p.relative_to(archive)) for p in archive.rglob('*') if p.is_file() and p.name!='SHA256SUMS')
assert (archive/'receipt.json').read_bytes()==(D/'receipt.json').read_bytes()
print('ARCHIVE_HASHES_PASS',len(entries),flush=True)
expected_paths=set(p for p in git('ls-files','src','deps','tests').decode().splitlines() if Path(p).suffix in ('.h','.hpp','.cpp'))
assert set(j['inputs'])==expected_paths
for p,h in j['inputs'].items():
 assert sha(R/p)==h and hashlib.sha256(git('show',PIN+':'+p)).hexdigest()==h,p
changes=git('diff','--name-only',BASE,PIN,'--','src').decode().splitlines()
assert set(changes)=={'src/plugin/'+p for p in ['AdvancedPanel.cpp','CentreRow.cpp','PluginEditor.cpp','UiComponents.cpp','UiComponents.h']}
for p in changes:
 origin='1eb4d9924db1f0ef159cac4c784895c0d9324a81' if p.endswith('AdvancedPanel.cpp') else '439036a33efcaf4cf7a7327530d9152f487d08e7'
 assert git('ls-tree',PIN,'--',p)==git('ls-tree',origin,'--',p),p
panel=git('show','439036a:tests/PanelEditorTests.h');actual=git('show',PIN+':tests/PanelEditorTests.h')
assert actual.replace(b'#include "FactoryToneRevisionUiTests.h"\n',b'').replace(b'    testFactoryToneRevisionEditor(check, flush);\n',b'')==panel
assert git('show',PIN+':tests/FactoryToneRevisionUiTests.h')==git('show','d04c20a:tests/FactoryToneRevisionUiTests.h')
assert git('show',PIN+':.github/workflows/screenshots.yml')==git('show','f3913a4:.github/workflows/screenshots.yml')
prevpath=B/'cs80-integration/docs/evidence/composed-processor-20261102/build-receipt.json';assert sha(prevpath)==j['prior_recipe_sha256'];prev=json.loads(prevpath.read_text());assert prev['src_tree']=='0f8a26e733644ac9f5985164e0175d4143c5d917'
assert len(j['compile_commands'])==26 and len(j['linked_objects'])==45 and len(j['reused_module_and_font_objects'])==19
assert j['reused_module_and_font_objects']==prev['reused_module_and_font_objects']
replacements={}
for prior,now in zip(prev['compile_commands'],j['compile_commands']):
 p=prior['file'];target='tests/ComposedUiComponentDriver.cpp' if p=='src/tools/ui_snapshot.cpp' else p;obj=D/(target.replace('/','_')+'.o')
 argv=[a.replace(str(B/'cs80-integration/src'),str(R/'src')).replace(str(B/'cs80-integration/tests'),str(R/'tests')) for a in prior['argv']]
 argv[argv.index('-o')+1]=str(obj);argv[argv.index('-c')+1]=str(R/target)
 assert now['argv']==argv and now['object']==str(obj) and now['file']==target and now['original_file']==p
 assert sha(obj)==now['sha256'];replacements[prior['fresh_object']]=str(obj)
link=[replacements.get(a,a) for a in prev['link_argv']];binary=D/'ui-components';link[link.index('-o')+1]=str(binary);link=['-Wl,--dependency-file='+str(D/'link.d') if a.startswith('-Wl,--dependency-file=') else a for a in link];assert link==j['link_argv']
linked=[]
for a in link:
 p=Path(a);p=p if p.is_absolute() else BUILD/p
 if p.is_file() and p.suffix in ('.o','.a'):linked.append(str(p))
assert linked==[x['path'] for x in j['linked_objects']]
for x in j['linked_objects']:assert sha(x['path'])==x['sha256']
for x in j['reused_module_and_font_objects']:assert sha(x['file'])==x['sha256']
assert set(linked)==set(x['object'] for x in j['compile_commands'])|set(x['file'] for x in j['reused_module_and_font_objects'])
assert sha(binary)==j['binary_sha256'];assert j['test_exit_code']==0
old=(D/'test.log').read_text();needle='Composed UI component: 822 checks, 0 failures; desktop components0';assert old.count(needle)==1 and 'NOT RUN' not in old and 'FAIL:' not in old
print('SOURCE_INPUT_OBJECT_ARGV_PROOF_PASS',len(j['inputs']),26,19,45,j['binary_sha256'],flush=True)
env=os.environ.copy();env.pop('DISPLAY',None);env['XDG_CACHE_HOME']='/tmp/cs80-independent-composed-ui-265-cache';env['XDG_CONFIG_HOME']='/tmp/cs80-independent-composed-ui-265-config'
p=subprocess.Popen([str(binary)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,start_new_session=True)
try:out,_=p.communicate(timeout=90)
except BaseException:
 try:os.killpg(p.pid,signal.SIGTERM)
 except ProcessLookupError:pass
 try:p.communicate(timeout=3)
 except subprocess.TimeoutExpired:
  try:os.killpg(p.pid,signal.SIGKILL)
  except ProcessLookupError:pass
  p.communicate()
 raise
print(out,end='');assert p.returncode==0 and out.count(needle)==1 and 'NOT RUN' not in out and 'FAIL:' not in out
assert sha(binary)==j['binary_sha256'];assert not git('status','--porcelain');print('INDEPENDENT_COMPOSED_UI_REPLAY_PASS; no independent recompilation or native window claim')
