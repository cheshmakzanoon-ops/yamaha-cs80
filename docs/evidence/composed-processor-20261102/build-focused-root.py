import concurrent.futures, hashlib, json, os, shlex, subprocess, sys, time
from pathlib import Path
base=Path('/workspace/scratch/aa102d573ba3')
old=base/'cs80-panel-s2'
source=base/'cs80-integration'
build=old/'build-native-red'
out=base/'cs80-integration-native-evidence'
phase=sys.argv[1] if len(sys.argv)>1 else 'red'
dest=out/phase
dest.mkdir(parents=True,exist_ok=True)
entries=json.loads((build/'compile_commands.json').read_text())
selected=[e for e in entries if 'CMakeFiles/cs80_core.dir/' in e['command']
          or ('CMakeFiles/CS80Snapshots.dir/' in e['command'] and str(old/'src')+'/' in e['file'])]
assert len(selected)==26,len(selected)
receipt={'phase':phase, 'base':'2a8c726e4a055b7733dd8e314f334de4ac042c87',
         'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip(),
         'src_tree':subprocess.check_output(['git','rev-parse','HEAD:src'],cwd=source,text=True).strip(),
         'working_diff':subprocess.check_output(['git','diff'],cwd=source,text=True),
         'reused_juce_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=base/'cs80-native-tools/JUCE',text=True).strip(),
         'scope':'fresh every project TU; unchanged JUCE/font objects reused; no editor/peer/display/DAW',
         'compile_commands':[]}
replacements={}
def compile_one(entry):
    args=shlex.split(entry['command'])
    oldobj=args[args.index('-o')+1]
    rel=Path(entry['file']).relative_to(old)
    obj=dest/(rel.as_posix().replace('/','_')+'.o')
    args=[a.replace(str(old/'src'),str(source/'src')).replace(str(old/'tests'),str(source/'tests')) for a in args]
    args[args.index('-o')+1]=str(obj)
    record={'file':str(rel),'argv':args,'old_object':oldobj,'fresh_object':str(obj)}
    # Incremental only when every source/header byte and exact command match.
    digest=hashlib.sha256()
    digest.update(json.dumps(args).encode())
    for top in ('src','tests'):
        for p in sorted((source/top).rglob('*')):
            if p.is_file() and p.suffix in ('.h','.hpp','.cpp','.tsv'):
                digest.update(str(p.relative_to(source)).encode());digest.update(p.read_bytes())
    key=digest.hexdigest()
    stamp=obj.with_suffix('.key')
    if not obj.exists() or not stamp.exists() or stamp.read_text()!=key:
        print(time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'compile',rel,flush=True)
        subprocess.run(args,cwd=build,check=True)
        stamp.write_text(key)
    record['sha256']=hashlib.sha256(obj.read_bytes()).hexdigest()
    return record
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    for record in pool.map(compile_one,selected):
        receipt['compile_commands'].append(record)
        replacements[record['old_object']]=record['fresh_object']
commands=subprocess.check_output(['ninja','-C',str(build),'-t','commands','CS80Snapshots'],text=True)
link=shlex.split(commands.splitlines()[-1])
if link[:2]==[':','&&']:link=link[2:]
if link[-2:]==['&&',':']:link=link[:-2]
fresh=[]
reused=[]
for arg in link:
    if arg in replacements:
        fresh.append(replacements[arg])
    elif arg=='libcs80_core.a':
        fresh.extend(replacements[a] for a in replacements if a.startswith('CMakeFiles/cs80_core.dir/'))
    else:
        fresh.append(arg)
        p=Path(arg)
        if not p.is_absolute():p=build/p
        if p.exists() and p.is_file() and p.suffix in ('.o','.a'):
            if arg.startswith('CMakeFiles/CS80Snapshots.dir/src/'):
                raise RuntimeError('stale project object left in link '+arg)
            reused.append({'file':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
binary=dest/'CS80Snapshots'
fresh[fresh.index('-o')+1]=str(binary)
receipt['link_argv']=fresh
receipt['reused_module_and_font_objects']=reused
subprocess.run(fresh,cwd=build,check=True)
receipt['binary_sha256']=hashlib.sha256(binary.read_bytes()).hexdigest()
(dest/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
with (dest/'test.log').open('w') as stream:
    result=subprocess.run([str(binary),'--factory-tone-revision-test'],stdout=stream,stderr=subprocess.STDOUT,timeout=120)
receipt['test_exit_code']=result.returncode
(dest/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print((dest/'test.log').read_text(),flush=True)
print('TEST_EXIT',result.returncode,flush=True)
if phase=='red':
    if result.returncode!=1:raise RuntimeError('Expected ordinary RED assertion exit1')
else:
    result.check_returncode()
