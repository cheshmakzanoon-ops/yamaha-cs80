import datetime,hashlib,json,os,subprocess
from pathlib import Path
b=Path('/workspace/scratch/aa102d573ba3');r=b/'cs80-tone-editor';out=b/'cs80-host-tool-evidence/factory-tone-editor';d=out/'green-corrected';d.mkdir()
build=b/'cs80-panel-s2/build-native-red'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(r),*args],text=True).strip()
assert git('rev-parse','HEAD')=='59522333955aea4801204ae37c57f40257ac2a08';assert git('status','--porcelain')==''
green=json.loads((out/'green/receipt.json').read_text());red=json.loads((out/'red-fresh-constructor/receipt.json').read_text())
assert git('rev-parse','HEAD:src')==green['source_tree']
assert [p for p,v in green['inputs'].items() if sha(r/p)!=v]==['tests/FactoryToneRevisionUiTests.h']
assert sha(out/'green/AdvancedPanel.o')==green['object_sha256']
args=red['compile_argv'].copy();obj=d/'driver.o';args[args.index('-o')+1]=str(obj)
link=[str(obj) if a==green['reused_test_object'] else a for a in green['link_argv']]
binary=d/'components';link[link.index('-o')+1]=str(binary)
link=[('-Wl,--dependency-file='+str(d/'link.d')) if a.startswith('-Wl,--dependency-file=') else a for a in link]
inputs={p:sha(r/p) for p in green['inputs']}
receipt={'head':git('rev-parse','HEAD'),'source_tree':green['source_tree'],'inputs':inputs,'compile_argv':args,'link_argv':link,'unchanged_advanced_cpp':str(r/'src/plugin/AdvancedPanel.cpp'),
 'unchanged_advanced_object':str(out/'green/AdvancedPanel.o'),'unchanged_advanced_object_sha256':green['object_sha256'],
 'scope':'corrected MIDI fixture preconditions; production unchanged; matching-header unpeered component execution only','started_at':datetime.datetime.now(datetime.timezone.utc).isoformat()}
for row in green['reused_project_objects']:assert sha(row['fresh_object'])==row['sha256'],row['fresh_object']
for row in green['reused_juce_fonts']:assert sha(row['file'])==row['sha256'],row['file']
for name,argv in [('compile',args),('link',link)]:
 with (d/(name+'.log')).open('w') as f:subprocess.run(argv,cwd=build,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180)
linked=[]
for arg in link:
 p=Path(arg)
 if not p.is_absolute():p=build/p
 if p.is_file() and p.suffix in ('.o','.a'):linked.append({'path':str(p),'sha256':sha(p)})
assert len(linked)==45,len(linked)
env=os.environ.copy();env.pop('DISPLAY',None);env['XDG_CACHE_HOME']=str(d/'cache');env['XDG_CONFIG_HOME']=str(d/'config')
with (d/'test.log').open('w') as f:result=subprocess.run([str(binary)],env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
log=(d/'test.log').read_text();print(log,flush=True)
for p,v in inputs.items():assert sha(r/p)==v,p
for row in linked:assert sha(row['path'])==row['sha256'],row['path']
assert git('rev-parse','HEAD')==receipt['head']
matched=result.returncode==0 and 'Factory tone editor component: 56 checks, 0 failures; desktop components0' in log
receipt.update({'linked_objects':linked,'binary_sha256':sha(binary),'object_sha256':sha(obj),'exit_code':result.returncode,'prediction_matched':matched,'finished_at':datetime.datetime.now(datetime.timezone.utc).isoformat()})
(d/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
assert matched,'Corrected fixture positive failed; raw evidence retained'
