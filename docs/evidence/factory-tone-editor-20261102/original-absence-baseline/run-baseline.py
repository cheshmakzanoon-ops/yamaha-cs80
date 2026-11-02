import datetime,hashlib,json,os,subprocess
from pathlib import Path
b=Path('/workspace/scratch/aa102d573ba3');source=b/'cs80-tone-editor';root=b/'cs80-host-tool-evidence/factory-tone-editor';dest=root/'red';dest.mkdir(exist_ok=False)
build=b/'cs80-panel-s2/build-native-red';base='b59b42d734dc5f0c4b3684b671f9cf76c6dc33a1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(source),*args],text=True).strip()
assert git('rev-parse','HEAD')=='0159cb04114cb9da21916c560f9e54b00cf54e09';assert git('status','--porcelain')==''
assert git('rev-parse','HEAD:src')=='0f8a26e733644ac9f5985164e0175d4143c5d917'
previous=json.loads((b/'cs80-integration/docs/evidence/composed-processor-20261102/build-receipt.json').read_text())
assert previous['src_tree']==git('rev-parse','HEAD:src')
assert len(previous['compile_commands'])==26 and len(previous['reused_module_and_font_objects'])==19
for row in previous['compile_commands']:assert sha(row['fresh_object'])==row['sha256'],row['fresh_object']
for row in previous['reused_module_and_font_objects']:assert sha(row['file'])==row['sha256'],row['file']
inputs={str(p.relative_to(source)):sha(p) for top in ['src','tests','deps'] for p in sorted((source/top).rglob('*')) if p.is_file() and p.suffix in ['.h','.hpp','.cpp','.tsv']}
entry=next(r for r in previous['compile_commands'] if r['file']=='src/tools/ui_snapshot.cpp')
args=[a.replace(str(b/'cs80-integration/src'),str(source/'src')).replace(str(b/'cs80-integration/tests'),str(source/'tests')) for a in entry['argv']]
obj=dest/'driver.o';args[args.index('-o')+1]=str(obj);args[args.index('-c')+1]=str(source/'tests/FactoryToneEditorComponentDriver.cpp')
link=[str(obj) if a==entry['fresh_object'] else a for a in previous['link_argv']]
binary=dest/'factory-tone-editor-components';link[link.index('-o')+1]=str(binary)
link=[('-Wl,--dependency-file='+str(dest/'link.d')) if a.startswith('-Wl,--dependency-file=') else a for a in link]
receipt={'head':git('rev-parse','HEAD'),'source_tree':git('rev-parse','HEAD:src'),'base':base,'matching_root_compiled_head':previous['head'],'inputs':inputs,
 'compile_argv':args,'link_argv':link,'reused_project_objects':previous['compile_commands'],'reused_juce_fonts':previous['reused_module_and_font_objects'],
 'scope':'unpeered custom-editor component baseline; no native window/input/render/DAW','started_at':datetime.datetime.now(datetime.timezone.utc).isoformat()}
print('LOCKED_EDITOR_BASELINE',receipt['started_at'],flush=True)
for name,argv in [('compile',args),('link',link)]:
 with (dest/(name+'.log')).open('w') as f:subprocess.run(argv,cwd=build,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180)
env=os.environ.copy();env.pop('DISPLAY',None);env['XDG_CACHE_HOME']=str(dest/'cache');env['XDG_CONFIG_HOME']=str(dest/'config')
with (dest/'test.log').open('w') as f:result=subprocess.run([str(binary)],cwd=source,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
log=(dest/'test.log').read_text();print(log,flush=True)
failures=[s for s in log.splitlines() if s.startswith('FAIL: ')]
matched=result.returncode==1 and failures==['FAIL: custom editor exposes exactly one factory revision choice'] and 'Factory tone editor component: 5 checks, 1 failures; desktop components0' in log
for p,v in inputs.items():assert sha(source/p)==v,p
for row in previous['compile_commands']:assert sha(row['fresh_object'])==row['sha256'],row['fresh_object']
for row in previous['reused_module_and_font_objects']:assert sha(row['file'])==row['sha256'],row['file']
assert git('rev-parse','HEAD')==receipt['head']
receipt.update({'binary_sha256':sha(binary),'driver_object_sha256':sha(obj),'exit_code':result.returncode,'prediction_matched':matched,'finished_at':datetime.datetime.now(datetime.timezone.utc).isoformat()})
(dest/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
assert matched,'Expected selector-absence baseline did not match; raw evidence retained'
