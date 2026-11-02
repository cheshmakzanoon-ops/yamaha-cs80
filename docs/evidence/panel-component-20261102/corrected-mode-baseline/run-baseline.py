import collections, datetime, hashlib, json, os, subprocess
from pathlib import Path
base=Path('/workspace/scratch/aa102d573ba3')
source=base/'cs80-panel-component';old=base/'cs80-panel-s2';build=old/'build-native-red'
root=base/'cs80-host-tool-evidence/panel-component';dest=root/'red-corrected'
dest.mkdir(parents=True,exist_ok=False)
def git(repo,*args):return subprocess.check_output(['git','-C',str(repo),*args],text=True).strip()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
head=git(source,'rev-parse','HEAD');assert head=='c9c28b53675c149c25790dc92b75c0915f3c2544',head
assert git(source,'diff','--','src','tests')==''
assert git(source,'diff','--cached','--','src','tests')==''
assert git(source,'rev-parse','HEAD:src')=='dbbefcc5ca0eeac5b741a90b916c0e79336246fd'
assert git(old,'rev-parse','HEAD')=='e4f791c95ce64b8d0df9dbfa331512af40a033c9'
assert git(old,'diff','--','src','tests')==''
assert git(old,'rev-parse','HEAD:src')==git(source,'rev-parse','HEAD:src')
assert git(base/'cs80-native-tools/JUCE','rev-parse','HEAD')=='c352e2489000ffeb9e397051377d75eef2efe641'
oldheader=subprocess.check_output(['git','-C',str(old),'show','HEAD:tests/PanelEditorTests.h'],text=True)
newheader=(source/'tests/PanelEditorTests.h').read_text()
def original(x):
 a=x.index('void testOwnerLeverDirections(');b=x.index('\n}\n',a)+3;return x[a:b]
assert original(newheader)==original(oldheader)
original_receipt=json.loads((root/'red/receipt.json').read_text())
objects=original_receipt['linked_objects']
for row in objects:assert sha(row['path'])==row['sha256'],row['path']
inputs={str(p):sha(p) for p in [source/'tests/PanelEditorTests.h',source/'tests/PanelComponentDriver.cpp']}
compile_args=[a.replace(str(root/'red/driver.o'),str(dest/'driver.o')) for a in original_receipt['compile_argv']]
link_args=[a.replace(str(root/'red/driver.o'),str(dest/'driver.o')).replace(str(root/'red/panel-components'),str(dest/'panel-components')) for a in original_receipt['link_argv']]
receipt={'head':head,'source_tree':git(source,'rev-parse','HEAD:src'),'matching_frozen_build_source':git(old,'rev-parse','HEAD'),
 'juce_commit':git(base/'cs80-native-tools/JUCE','rev-parse','HEAD'),'inputs':inputs,'original_owner_function_sha256':hashlib.sha256(original(newheader).encode()).hexdigest(),
 'compile_argv':compile_args,'link_argv':link_args,'linked_objects':objects,
 'scope':'unpeered component geometry/synthetic events; no window/input/render/screenshot/DAW acceptance',
 'started_at':datetime.datetime.now(datetime.timezone.utc).isoformat()}
for name,args in [('compile',compile_args),('link',link_args)]:
 with (dest/(name+'.log')).open('w') as f:subprocess.run(args,cwd=build,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180)
env=os.environ.copy();env.pop('DISPLAY',None);env['XDG_CACHE_HOME']=str(dest/'cache');env['XDG_CONFIG_HOME']=str(dest/'config')
with (dest/'test.log').open('w') as f:result=subprocess.run([str(dest/'panel-components')],cwd=source,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
log=(dest/'test.log').read_text();print(log,flush=True)
expected=[]
oldlog=(root/'red/test.log').read_text().split('Owner lever component:')[0]
expected.extend(line for line in oldlog.splitlines() if line.startswith('FAIL: '))
for label in ['ORIGINAL SUB FUNCTION direct','ORIGINAL SUB SPEED direct','ORIGINAL SUB FUNCTION rebound from MIX','ORIGINAL SUB SPEED rebound from MIX']:
 for family in ['endpoints','physical arrow direction','physical arrow direction','downward pointer drag']:
  expected.append('FAIL: owner lever '+family+': '+label)
for mode in ['legacy','original']:
 for size in ['1600x1000','1120x700']:
  expected.append('FAIL: S2 front-strip order: AT VCF before KBD BRILLIANCE LOW '+mode+' SUB at '+size)
actual=[line for line in log.splitlines() if line.startswith('FAIL: ')]
matched=(result.returncode==1 and collections.Counter(actual)==collections.Counter(expected)
 and 'Owner lever component: 351 checks, 92 failures' in log
 and 'Original SUB component: 54 checks, 16 failures' in log
 and 'S2 order component: 204 checks, 4 failures' in log
 and 'Panel component-only total: 609 checks, 112 failures; desktop components0' in log)
for row in objects:assert sha(row['path'])==row['sha256'],row['path']
assert all(sha(p)==v for p,v in inputs.items())
assert git(source,'rev-parse','HEAD')==head
receipt.update({'binary_sha256':sha(dest/'panel-components'),'driver_object_sha256':sha(dest/'driver.o'),'exit_code':result.returncode,
 'expected_failure_multiset':expected,'prediction_matched':matched,'finished_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'process_environment_overrides':{'DISPLAY':'unset','XDG_CACHE_HOME':env['XDG_CACHE_HOME'],'XDG_CONFIG_HOME':env['XDG_CONFIG_HOME']}})
(dest/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
assert matched,'Corrected prospective failure families/counts did not match; inspect retained raw evidence'
