from pathlib import Path
import json,hashlib,subprocess
r=Path('/workspace/scratch/aa102d573ba3/cs80-ui-integration');b=r.parent;d=b/'cs80-native-key-fixture-evidence';d.mkdir(exist_ok=False)
prior=json.loads((b/'cs80-ui-composed-evidence/green/receipt.json').read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(['git','-C',str(r),*a],text=True).strip()
head=git('rev-parse','HEAD');assert head.startswith('8839621');assert git('diff','--name-only','265cf3a',head,'--','src')=='src/tools/ui_snapshot.cpp'
row=next(x for x in prior['compile_commands'] if x['original_file']=='src/tools/ui_snapshot.cpp')
args=row['argv'][:];obj=d/'ui_snapshot.o';args[args.index('-o')+1]=str(obj);args[args.index('-c')+1]=str(r/'src/tools/ui_snapshot.cpp')
for x in prior['linked_objects']:assert sha(x['path'])==x['sha256']
with (d/'compile.log').open('w') as f:subprocess.run(args,cwd=b/'cs80-panel-s2/build-native-red',stdout=f,stderr=subprocess.STDOUT,check=True,timeout=240)
link=[str(obj) if a==row['object'] else a for a in prior['link_argv']];binary=d/'native-ui-snapshot';link[link.index('-o')+1]=str(binary);link=[a for a in link if not a.startswith('-Wl,--dependency-file=')]
with (d/'link.log').open('w') as f:subprocess.run(link,cwd=b/'cs80-panel-s2/build-native-red',stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
for x in prior['linked_objects']:assert sha(x['path'])==x['sha256']
assert git('rev-parse','HEAD')==head
(d/'receipt.json').write_text(json.dumps({'head':head,'src':git('rev-parse','HEAD:src'),'compile_argv':args,'link_argv':link,'reused_inputs':prior['linked_objects'],'new_object_sha256':sha(obj),'binary_sha256':sha(binary),'scope':'compile/link actual native renderer only; not executed without native display; 25 prior fresh project objects and19 unchanged dependencies reused after identity check'},indent=2)+'\n')
print('ACTUAL_NATIVE_RENDERER_COMPILE_LINK_PASS; NOT RUN; '+sha(binary))
