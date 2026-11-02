import datetime,hashlib,json,os,subprocess
from pathlib import Path
base=Path('/workspace/scratch/aa102d573ba3');old=base/'cs80-panel-s2';source=base/'cs80-panel-component';build=old/'build-native-red';root=base/'cs80-host-tool-evidence/panel-component';dest=root/'existing-layout';dest.mkdir(exist_ok=False)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(r,*args):return subprocess.check_output(['git','-C',str(r),*args],text=True).strip()
assert git(source,'rev-parse','HEAD')=='439036a33efcaf4cf7a7327530d9152f487d08e7'
assert git(source,'status','--porcelain')==''
before=(old/'tests/PanelEditorTests.h').read_text();after=(source/'tests/PanelEditorTests.h').read_text()
def body(s):
 a=s.index('void testMainViewLayout(');b=s.index('\n}\n',a)+3;return s[a:b]
assert body(before)==body(after)
assert before[:before.index('// Independent physical-direction inventory')]==after[:after.index('// Independent physical-direction inventory')]
driver=dest/'driver.cpp';driver.write_text('''#include PANEL_TEST_HEADER
#include <cstdio>
int main() {
 std::setvbuf(stdout,nullptr,_IONBF,0);
 juce::ScopedJuceInitialiser_GUI manager;
 if(juce::Desktop::getInstance().getNumComponents()!=0)return 2;
 int checks=0,failures=0;
 auto check=[&](bool ok,const char* m){++checks;if(!ok){++failures;std::printf("FAIL: %s\\n",m);}};
 auto flush=[](int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);};
 cs80::tests::testMainViewLayout(check,flush);
 if(juce::Desktop::getInstance().getNumComponents()!=0)return 2;
 std::printf("Existing main layout component: %d checks, %d failures; desktop components0\\n",checks,failures);
 return failures?1:0;
}
''')
red=json.loads((root/'red/receipt.json').read_text());green=json.loads((root/'green/receipt.json').read_text())
for row in red['linked_objects']:assert sha(row['path'])==row['sha256'],row['path']
for row in green['compiled']:assert sha(row['object'])==row['sha256'],row['object']
for row in green['reused_objects']:assert sha(row['path'])==row['sha256'],row['path']
receipt={'source_before':git(old,'rev-parse','HEAD:src'),'source_after':git(source,'rev-parse','HEAD:src'),'head_after':git(source,'rev-parse','HEAD'),'function_sha256':hashlib.sha256(body(before).encode()).hexdigest(),'driver_sha256':sha(driver),'scope':'unchanged existing main-view component geometry, not native painted/input/DAW','arms':[]}
for arm,tree in [('before',old),('after',source)]:
 d=dest/arm;d.mkdir()
 if arm=='before':
  args=red['compile_argv'].copy();previous=red['compile_argv'][red['compile_argv'].index('-o')+1];link=red['link_argv'].copy()
 else:
  row=next(r for r in green['compiled'] if r['original_source']=='src/tools/ui_snapshot.cpp')
  args=row['argv'].copy();previous=row['object'];link=green['link_argv'].copy()
 args=[a.replace(str(source/'tests'),str(tree/'tests')) for a in args]
 args.insert(1,'-DPANEL_TEST_HEADER="'+str(tree/'tests/PanelEditorTests.h')+'"')
 obj=d/'driver.o';args[args.index('-c')+1]=str(driver);args[args.index('-o')+1]=str(obj)
 link=[str(obj) if a==previous else a for a in link]
 binary=d/'layout-components';link[link.index('-o')+1]=str(binary);link=[('-Wl,--dependency-file='+str(d/'link.d')) if a.startswith('-Wl,--dependency-file=') else a for a in link]
 for label,argv in [('compile',args),('link',link)]:
  with (d/(label+'.log')).open('w') as f:subprocess.run(argv,cwd=build,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180)
 env=os.environ.copy();env.pop('DISPLAY',None);env['XDG_CACHE_HOME']=str(d/'cache');env['XDG_CONFIG_HOME']=str(d/'config')
 with (d/'test.log').open('w') as f:result=subprocess.run([str(binary)],cwd=tree,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=90)
 log=(d/'test.log').read_text();print(arm,log,flush=True)
 record={'arm':arm,'compile_argv':args,'link_argv':link,'binary_sha256':sha(binary),'object_sha256':sha(obj),'exit_code':result.returncode,'test_sha256':sha(d/'test.log')}
 receipt['arms'].append(record)
 assert result.returncode==0 and 'Existing main layout component: 37 checks, 0 failures; desktop components0' in log
for row in red['linked_objects']:assert sha(row['path'])==row['sha256'],row['path']
for row in green['compiled']:assert sha(row['object'])==row['sha256'],row['object']
receipt['finished_at']=datetime.datetime.now(datetime.timezone.utc).isoformat();(dest/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
