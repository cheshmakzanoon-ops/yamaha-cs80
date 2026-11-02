import concurrent.futures,datetime,hashlib,json,os,shlex,subprocess,sys
from pathlib import Path
base=Path('/workspace/scratch/aa102d573ba3');old=base/'cs80-panel-s2';source=base/'cs80-panel-component';build=old/'build-native-red'
root=base/'cs80-host-tool-evidence/panel-component';phase=sys.argv[1];dest=root/phase;dest.mkdir(parents=True,exist_ok=False)
def git(repo,*args):return subprocess.check_output(['git','-C',str(repo),*args],text=True).strip()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
head=git(source,'rev-parse','HEAD');assert git(source,'status','--porcelain')==''
assert git(base/'cs80-native-tools/JUCE','rev-parse','HEAD')=='c352e2489000ffeb9e397051377d75eef2efe641'
assert git(base/'cs80-native-tools/JUCE','diff')==''
original=json.loads((root/'red/receipt.json').read_text())
reused=[r for r in original['linked_objects'] if '/JUCE/' in r['path'] or r['path'].endswith('/libcs80_panel_fonts.a')]
assert len(reused)==19,len(reused)
for row in reused:assert sha(row['path'])==row['sha256'],row['path']
inputs={str(p.relative_to(source)):sha(p) for top in ['src','tests'] for p in sorted((source/top).rglob('*')) if p.is_file() and p.suffix in ['.h','.hpp','.cpp','.tsv']}
entries=json.loads((build/'compile_commands.json').read_text())
selected=[e for e in entries if 'CMakeFiles/cs80_core.dir/' in e['command'] or ('CMakeFiles/CS80Snapshots.dir/' in e['command'] and str(old/'src')+'/' in e['file'])]
assert len(selected)==26
receipt={'head':head,'source_tree':git(source,'rev-parse','HEAD:src'),'inputs':inputs,'scope':'fresh26 project translation units; only pinned JUCE/fonts reused; unpeered components, not native window/input/render/DAW',
 'started_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'reused_objects':reused,'compiled':[]}
print('LOCKED_FRESH_BUILD',receipt['started_at'],head,flush=True)
def compile_one(entry):
 args=shlex.split(entry['command']);oldobj=args[args.index('-o')+1];relative=Path(entry['file']).relative_to(old)
 args=[a.replace(str(old/'src'),str(source/'src')).replace(str(old/'tests'),str(source/'tests')) for a in args]
 if relative.as_posix()=='src/tools/ui_snapshot.cpp':args[args.index('-c')+1]=str(source/'tests/PanelComponentDriver.cpp')
 obj=dest/(relative.as_posix().replace('/','_')+'.o');args[args.index('-o')+1]=str(obj)
 print('compile',relative,flush=True)
 with (dest/(obj.name+'.log')).open('w') as f:subprocess.run(args,cwd=build,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=240)
 return {'original_source':str(relative),'actual_source':args[args.index('-c')+1],'old_object':oldobj,'object':str(obj),'sha256':sha(obj),'argv':args}
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
 for row in pool.map(compile_one,selected):receipt['compiled'].append(row)
replacements={r['old_object']:r['object'] for r in receipt['compiled']}
commands=subprocess.check_output(['ninja','-C',str(build),'-t','commands','CS80Snapshots'],text=True)
link=shlex.split(commands.splitlines()[-1])
if link[:2]==[':','&&']:link=link[2:]
if link[-2:]==['&&',':']:link=link[:-2]
fresh=[]
for arg in link:
 if arg in replacements:fresh.append(replacements[arg])
 elif arg=='libcs80_core.a':fresh.extend(replacements[a] for a in replacements if a.startswith('CMakeFiles/cs80_core.dir/'))
 elif arg.startswith('-Wl,--dependency-file='):fresh.append('-Wl,--dependency-file='+str(dest/'link.d'))
 else:
  assert not (arg.startswith('CMakeFiles/CS80Snapshots.dir/src/') and arg.endswith('.o')),arg
  fresh.append(arg)
binary=dest/'panel-components';fresh[fresh.index('-o')+1]=str(binary)
with (dest/'link.log').open('w') as f:subprocess.run(fresh,cwd=build,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
receipt['link_argv']=fresh;receipt['binary_sha256']=sha(binary)
env=os.environ.copy();env.pop('DISPLAY',None);env['XDG_CACHE_HOME']=str(dest/'cache');env['XDG_CONFIG_HOME']=str(dest/'config')
with (dest/'test.log').open('w') as f:result=subprocess.run([str(binary)],cwd=source,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
log=(dest/'test.log').read_text();print(log,flush=True)
for row in reused:assert sha(row['path'])==row['sha256'],row['path']
assert all(sha(source/p)==v for p,v in inputs.items());assert git(source,'rev-parse','HEAD')==head
receipt.update({'exit_code':result.returncode,'finished_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'environment_overrides':{'DISPLAY':'unset','XDG_CACHE_HOME':env['XDG_CACHE_HOME'],'XDG_CONFIG_HOME':env['XDG_CONFIG_HOME']}})
(dest/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
assert result.returncode==0 and 'Panel component-only total: 729 checks, 0 failures; desktop components0' in log
