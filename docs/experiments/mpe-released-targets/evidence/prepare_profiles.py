import datetime,hashlib,json,os,pathlib,subprocess
root=pathlib.Path('/workspace/scratch/ee447c520988');src=root/'release-followup';out=root/'mpe-released-target-evidence/profiles-preparation'
cmake=root/'pitch-power-recovery/tools/cmake/data/bin/cmake';ninja=root/'pitch-power-recovery/tools/bin/ninja'
clang=root/'pitch-power-recovery/clang-root/usr/bin/clang++-19';clanglib=clang.parent.parent/'lib/x86_64-linux-gnu'
records=[]
def run(tag,stage,args,env):
    log=out/f'{tag}-{stage}.log';start=datetime.datetime.now(datetime.timezone.utc).isoformat()
    with log.open('x') as f:p=subprocess.run([str(x) for x in args],cwd=src,env=env,stdout=f,stderr=subprocess.STDOUT)
    records.append(dict(profile=tag,stage=stage,argv=[str(x) for x in args],cwd=str(src),started_utc=start,exit_code=p.returncode,log=log.name,sha256=hashlib.sha256(log.read_bytes()).hexdigest(),env={k:env[k] for k in ['LD_LIBRARY_PATH'] if k in env}))
    (out/'commands.json').write_text(json.dumps(records,indent=2)+'\n');print(tag,stage,p.returncode,flush=True)
    if p.returncode:raise SystemExit(p.returncode)
for compiler in ['gcc','clang']:
    for profile in ['generic','native']:
        tag=f'{compiler}-{profile}';b=root/f'build-mpe-released-target-profile-{tag}';env=os.environ.copy();env['PYTHONDONTWRITEBYTECODE']='1';flags='-O3 -DNDEBUG'+(' -march=native' if profile=='native' else '')
        args=[cmake,'-S',src,'-B',b,'-G','Ninja',f'-DCMAKE_MAKE_PROGRAM={ninja}','-DCMAKE_BUILD_TYPE=Release','-DCS80_BUILD_PLUGIN=OFF','-DCS80_ENABLE_AVX2_FILTER_BATCH=OFF',f'-DCMAKE_CXX_FLAGS_RELEASE={flags}']
        if compiler=='clang':env['LD_LIBRARY_PATH']=str(clanglib);args += [f'-DCMAKE_CXX_COMPILER={clang}',f'-DCMAKE_C_COMPILER={clang.with_name("clang-19")}']
        run(tag,'configure',args,env);run(tag,'build-core',[cmake,'--build',b,'--target','cs80_core','--parallel','2'],env)
        (out/f'{tag}-source-library.json').write_text(json.dumps(dict(head=subprocess.check_output(['git','-C',str(src),'rev-parse','HEAD'],text=True).strip(),production_sha256={str(p.relative_to(src)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((src/'src/core').glob('*')) if p.is_file()},library=str(b/'libcs80_core.a'),library_sha256=hashlib.sha256((b/'libcs80_core.a').read_bytes()).hexdigest()),indent=2)+'\n')
