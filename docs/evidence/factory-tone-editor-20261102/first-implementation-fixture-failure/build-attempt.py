import datetime,hashlib,json,os,subprocess
from pathlib import Path
b=Path('/workspace/scratch/aa102d573ba3');source=b/'cs80-tone-editor';root=b/'cs80-host-tool-evidence/factory-tone-editor';dest=root/'green';dest.mkdir(exist_ok=False)
build=b/'cs80-panel-s2/build-native-red'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(source),*args],text=True).strip()
assert git('rev-parse','HEAD')=='1eb4d9924db1f0ef159cac4c784895c0d9324a81';assert git('status','--porcelain')==''
red=json.loads((root/'red-fresh-constructor/receipt.json').read_text())
changed=[p for p,v in red['inputs'].items() if sha(source/p)!=v]
assert changed==['src/plugin/AdvancedPanel.cpp'],changed
assert git('diff','--name-only','5b26e431a3e1ea5cb0416580edff0580dc9a9575..HEAD','--','src')=='src/plugin/AdvancedPanel.cpp'
for row in red['reused_project_objects']:assert sha(row['fresh_object'])==row['sha256'],row['fresh_object']
for row in red['reused_juce_fonts']:assert sha(row['file'])==row['sha256'],row['file']
assert sha(root/'red-fresh-constructor/driver.o')==red['driver_object_sha256']
entry=next(r for r in red['reused_project_objects'] if r['file']=='src/plugin/AdvancedPanel.cpp')
args=[a.replace(str(b/'cs80-integration/src'),str(source/'src')).replace(str(b/'cs80-integration/tests'),str(source/'tests')) for a in entry['argv']]
obj=dest/'AdvancedPanel.o';args[args.index('-o')+1]=str(obj)
link=[str(obj) if a==entry['fresh_object'] else a for a in red['link_argv']]
binary=dest/'factory-tone-editor-components';link[link.index('-o')+1]=str(binary)
link=[('-Wl,--dependency-file='+str(dest/'link.d')) if a.startswith('-Wl,--dependency-file=') else a for a in link]
inputs={p:sha(source/p) for p in red['inputs']}
receipt={'head':git('rev-parse','HEAD'),'source_tree':git('rev-parse','HEAD:src'),'baseline_head':red['head'],'changed_source_files':changed,'inputs':inputs,
 'compile_argv':args,'link_argv':link,'reused_project_objects':red['reused_project_objects'],'reused_juce_fonts':red['reused_juce_fonts'],
 'reused_test_object':str(root/'red-fresh-constructor/driver.o'),'reused_test_object_sha256':red['driver_object_sha256'],
 'scope':'one changed CPP, all production headers and other project TUs byte-identical to matching root build; same compiled test object; unpeered component only',
 'started_at':datetime.datetime.now(datetime.timezone.utc).isoformat()}
print('LOCKED_EDITOR_IMPLEMENTATION',receipt['started_at'],flush=True)
for name,argv in [('compile',args),('link',link)]:
 with (dest/(name+'.log')).open('w') as f:subprocess.run(argv,cwd=build,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180)
env=os.environ.copy();env.pop('DISPLAY',None);env['XDG_CACHE_HOME']=str(dest/'cache');env['XDG_CONFIG_HOME']=str(dest/'config')
with (dest/'test.log').open('w') as f:result=subprocess.run([str(binary)],cwd=source,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
log=(dest/'test.log').read_text();print(log,flush=True)
for p,v in inputs.items():assert sha(source/p)==v,p
for row in red['reused_project_objects']:assert sha(row['fresh_object'])==row['sha256'],row['fresh_object']
for row in red['reused_juce_fonts']:assert sha(row['file'])==row['sha256'],row['file']
assert sha(root/'red-fresh-constructor/driver.o')==red['driver_object_sha256']
assert git('rev-parse','HEAD')==receipt['head']
matched=result.returncode==0 and 'Factory tone editor component: 54 checks, 0 failures; desktop components0' in log
receipt.update({'object_sha256':sha(obj),'binary_sha256':sha(binary),'exit_code':result.returncode,'prediction_matched':matched,'finished_at':datetime.datetime.now(datetime.timezone.utc).isoformat()})
(dest/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
assert matched,'Registered editor positive did not match; raw evidence retained'
