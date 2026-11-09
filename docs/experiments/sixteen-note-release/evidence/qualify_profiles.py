import datetime, hashlib, json, os, pathlib, subprocess, sys
root=pathlib.Path('/workspace/scratch/ee447c520988')
src=root/'sixteen-note-release-impl'; out=root/'sixteen-release-evidence'/'profiles'; out.mkdir(exist_ok=False)
cmake=str(root/'pitch-power-recovery/tools/cmake/data/bin/cmake'); ninja=str(root/'pitch-power-recovery/tools/bin/ninja')
clang=root/'pitch-power-recovery/clang-root/usr/bin/clang++-19'; clanglib=root/'pitch-power-recovery/clang-root/usr/lib/x86_64-linux-gnu'
records=[]
def run(tag, stage, cmd, env):
    log=out/f'{tag}-{stage}.log'; start=datetime.datetime.now(datetime.timezone.utc).isoformat()
    with log.open('w') as f: result=subprocess.run(cmd,cwd=src,env=env,stdout=f,stderr=subprocess.STDOUT)
    records.append(dict(profile=tag,stage=stage,argv=cmd,cwd=str(src),started_utc=start,exit_code=result.returncode,log=str(log),sha256=hashlib.sha256(log.read_bytes()).hexdigest(),env={k:env[k] for k in ['LD_LIBRARY_PATH','ASAN_OPTIONS','UBSAN_OPTIONS'] if k in env}))
    (out/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
    print(tag,stage,result.returncode,flush=True)
    if result.returncode: raise SystemExit(result.returncode)
for compiler in ['gcc','clang']:
    for profile in ['generic','native']:
        tag=f'{compiler}-{profile}'; build=root/f'build-sixteen-release-profile-{tag}'; env=os.environ.copy(); env['PYTHONDONTWRITEBYTECODE']='1'
        flags='-O3 -DNDEBUG'+(' -march=native' if profile=='native' else '')
        args=[cmake,'-S',str(src),'-B',str(build),'-G','Ninja',f'-DCMAKE_MAKE_PROGRAM={ninja}','-DCMAKE_BUILD_TYPE=Release','-DCS80_BUILD_PLUGIN=OFF','-DCS80_ENABLE_AVX2_FILTER_BATCH=OFF',f'-DCMAKE_CXX_FLAGS_RELEASE={flags}']
        if compiler=='clang':
            env['LD_LIBRARY_PATH']=str(clanglib); args += [f'-DCMAKE_CXX_COMPILER={clang}',f'-DCMAKE_C_COMPILER={clang.with_name("clang-19")}']
        run(tag,'configure',args,env)
        run(tag,'build',[cmake,'--build',str(build),'--target','cs80_sixteen_voice_release_tests','cs80_tests','cs80_tuning_midi_tests','--parallel','2'],env)
        for name in ['cs80_sixteen_voice_release_tests','cs80_tests','cs80_tuning_midi_tests']: run(tag,name,[str(build/name)],env)
tag='sanitizer'; build=root/'build-sixteen-release-profile-sanitizer'; env=os.environ.copy(); env['ASAN_OPTIONS']='detect_leaks=0'; env['UBSAN_OPTIONS']='halt_on_error=1'; env['PYTHONDONTWRITEBYTECODE']='1'
flags='-O1 -g1 -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS'
run(tag,'configure',[cmake,'-S',str(src),'-B',str(build),'-G','Ninja',f'-DCMAKE_MAKE_PROGRAM={ninja}','-DCMAKE_BUILD_TYPE=Release','-DCS80_BUILD_PLUGIN=OFF','-DCS80_ENABLE_AVX2_FILTER_BATCH=OFF',f'-DCMAKE_CXX_FLAGS_RELEASE={flags}'],env)
run(tag,'build',[cmake,'--build',str(build),'--target','cs80_sixteen_voice_release_tests','cs80_tests','cs80_tuning_midi_tests','--parallel','2'],env)
for name in ['cs80_sixteen_voice_release_tests','cs80_tests','cs80_tuning_midi_tests']: run(tag,name,[str(build/name)],env)
