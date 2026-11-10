#!/usr/bin/env python3
"""Compile this external fixture with each revision's own headers and archive."""
import argparse, hashlib, json, pathlib, shlex, subprocess, time
ROOT=pathlib.Path('/workspace/scratch/ee447c520988')
HERE=ROOT/'mpe-released-target-evidence/compatibility'
p=argparse.ArgumentParser();p.add_argument('side',choices=['baseline','candidate']);p.add_argument('--source',default='CompatibilityCapture-v1.cpp');p.add_argument('--profiles',nargs='+',default=['gcc-generic','gcc-native','clang-generic','clang-native']);args=p.parse_args()
source=HERE/args.source
revision=ROOT/('sixteen-note-release' if args.side=='baseline' else 'release-followup')
prefix='build-sixteen-release-profile-' if args.side=='baseline' else 'build-mpe-released-target-profile-'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
for profile in args.profiles:
    build=ROOT/(prefix+profile); output=HERE/args.source.removesuffix('.cpp')/(args.side+'-'+profile); output.mkdir(parents=True,exist_ok=False)
    commands=json.loads((build/'compile_commands.json').read_text())
    core=next(x for x in commands if x['file'].endswith('/core/Engine.cpp'))
    tokens=shlex.split(core['command']); flags=[]; i=1
    while i<len(tokens):
        t=tokens[i]
        if t in ['-o','-c']:i+=2;continue
        if t.startswith('-I'):i+=1;continue
        flags.append(t);i+=1
    compiler=tokens[0]; archive=build/'libcs80_core.a'; binary=output/'capture'
    cmd=[compiler,*flags,'-I'+str(revision/'src'),str(source),str(archive),'-o',str(binary)]
    record={'side':args.side,'profile':profile,'source':str(source),'source_sha256':sha(source),'header_root':str(revision/'src'),'revision':subprocess.check_output(['git','-C',str(revision),'rev-parse','HEAD'],text=True).strip(),'archive':str(archive),'archive_sha256':sha(archive),'engine_compile_command':core,'compile_command':cmd,'compiler_version':subprocess.check_output([compiler,'--version'],text=True),'core_tree_sha256':{str(f.relative_to(revision)):sha(f) for f in sorted((revision/'src/core').rglob('*')) if f.is_file()},'runs':[]}
    (output/'record.json').write_text(json.dumps(record,indent=2)+'\n')
    result=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(output/'compile.log').write_text(result.stdout)
    record['compile_exit']=result.returncode
    if result.returncode==0:
        record['binary_sha256']=sha(binary)
        for partition in [0,1]:
            raw=output/f'partition-{partition}.bin'; cmd=[str(binary),str(raw),str(partition)]
            start=time.time();result=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            (output/f'partition-{partition}.log').write_text(result.stdout)
            r={'command':cmd,'exit':result.returncode,'seconds_for_provenance_only':time.time()-start,'capture_sha256':sha(raw),'bytes':raw.stat().st_size}
            record['runs'].append(r)
            print(args.side,profile,partition,'exit',result.returncode,result.stdout[-600:],flush=True)
        record['partition_equal']=record['runs'][0]['capture_sha256']==record['runs'][1]['capture_sha256']
    (output/'record.json').write_text(json.dumps(record,indent=2)+'\n')
    print('completed',output,flush=True)
