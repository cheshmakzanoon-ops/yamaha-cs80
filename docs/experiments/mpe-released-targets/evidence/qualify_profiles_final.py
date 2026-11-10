import datetime, hashlib, json, os, pathlib, subprocess
root=pathlib.Path('/workspace/scratch/ee447c520988');src=root/'release-followup';out=root/'mpe-released-target-evidence/profiles-final';out.mkdir(exist_ok=False)
cmake=root/'pitch-power-recovery/tools/cmake/data/bin/cmake';ninja=root/'pitch-power-recovery/tools/bin/ninja';clanglib=root/'pitch-power-recovery/clang-root/usr/lib/x86_64-linux-gnu'
records=[]; targets=['cs80_mpe_released_target_tests','cs80_mpe_member_onset_tests','cs80_controller_reset_tests','cs80_sixteen_voice_release_tests','cs80_tests','cs80_tuning_midi_tests','cs80_poly_pressure_routing_tests']
head=subprocess.check_output(['git','-C',str(src),'rev-parse','HEAD'],text=True).strip()
if head!='04597f5ee87da1f6390a8682298a1a0e656f834c':raise RuntimeError('unexpected source freeze')
inputs={p:hashlib.sha256((src/p).read_bytes()).hexdigest() for p in subprocess.check_output(['git','-C',str(src),'ls-files','src','tests','CMakeLists.txt'],text=True).splitlines()}
(out/'inputs.json').write_text(json.dumps(dict(head=head,hashes=inputs),indent=2)+'\n')
def run(tag,stage,cmd,env):
    cmd=[str(x) for x in cmd];log=out/f'{tag}-{stage}.log';start=datetime.datetime.now(datetime.timezone.utc).isoformat()
    with log.open('x') as f:p=subprocess.run(cmd,cwd=src,env=env,stdout=f,stderr=subprocess.STDOUT)
    records.append(dict(profile=tag,stage=stage,argv=cmd,cwd=str(src),started_utc=start,exit_code=p.returncode,log=log.name,sha256=hashlib.sha256(log.read_bytes()).hexdigest(),env={k:env[k] for k in ['LD_LIBRARY_PATH','ASAN_OPTIONS','UBSAN_OPTIONS'] if k in env}));(out/'commands.json').write_text(json.dumps(records,indent=2)+'\n');print(tag,stage,p.returncode,flush=True)
    if p.returncode:raise SystemExit(p.returncode)
for compiler in ['gcc','clang']:
    for profile in ['generic','native']:
        tag=f'{compiler}-{profile}';b=root/f'build-mpe-released-target-profile-{tag}';env=os.environ.copy();env['PYTHONDONTWRITEBYTECODE']='1'
        if compiler=='clang':env['LD_LIBRARY_PATH']=str(clanglib)
        run(tag,'build',[cmake,'--build',b,'--target',*targets,'--parallel','2'],env)
        for name in targets:run(tag,name,[b/name],env)
        (out/f'{tag}-binaries.json').write_text(json.dumps({str(b/name):hashlib.sha256((b/name).read_bytes()).hexdigest() for name in targets+['libcs80_core.a']},indent=2)+'\n')
tag='sanitizer';b=root/'build-mpe-released-target-profile-sanitizer';env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0';env['UBSAN_OPTIONS']='halt_on_error=1';env['PYTHONDONTWRITEBYTECODE']='1'
flags='-O1 -g1 -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS'
run(tag,'configure',[cmake,'-S',src,'-B',b,'-G','Ninja',f'-DCMAKE_MAKE_PROGRAM={ninja}','-DCMAKE_BUILD_TYPE=Release','-DCS80_BUILD_PLUGIN=OFF','-DCS80_ENABLE_AVX2_FILTER_BATCH=OFF',f'-DCMAKE_CXX_FLAGS_RELEASE={flags}'],env)
run(tag,'build',[cmake,'--build',b,'--target',*targets,'--parallel','2'],env)
for name in targets:run(tag,name,[b/name],env)
(out/f'{tag}-binaries.json').write_text(json.dumps({str(b/name):hashlib.sha256((b/name).read_bytes()).hexdigest() for name in targets+['libcs80_core.a']},indent=2)+'\n')
if any(hashlib.sha256((src/p).read_bytes()).hexdigest()!=h for p,h in inputs.items()):raise RuntimeError('source/test changed during execution')
(out/'source-unchanged.json').write_text(json.dumps(dict(unchanged=True,head=head),indent=2)+'\n')
