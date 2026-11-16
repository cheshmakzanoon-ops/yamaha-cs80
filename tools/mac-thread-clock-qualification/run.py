#!/usr/bin/env python3
"""Bounded diagnostic; no retries and no production source modifications."""
import csv, hashlib, io, json, math, os, platform, signal, subprocess, sys
from pathlib import Path
BASE='a1e51ee0483ec70ce6e07061936a3777ee52bb1d'
HERE=Path(__file__).resolve().parent
TIME_FIELDS=set('min_us median_us p95_us p99_us max_us mean_rtf median_rtf p99_rtf max_rtf budget_exceedances event_median_us event_max_us process_cpu_seconds_including_checks thread_cpu_seconds_including_checks callback_cpu_status callback_cpu_seconds callback_cpu_rtf callback_cpu_call_count'.split())
META_VARIABLE={'started_unix_seconds','thread_cpu_clock_available','callback_cpu_requested'}
def check(ok, why):
    if not ok: raise ValueError(why)
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def patch(s):
    old='#if defined(__linux__)\n'
    check(s.count(old)==2,'clock guard count changed')
    check(s.count('clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time)')==1,'clock implementation changed')
    return s.replace(old,'#if defined(__linux__) || defined(__APPLE__)\n')
def inventory():
    keys=set()
    for mode in ('0','1'):
        for selector in ('0','1','10'):
            for phase in ('warmed_held','release_tail'): keys.add((mode,'paired_tones',selector,phase))
        for phase in ('warmed_held','release_tail'): keys.add((mode,'moving_tabs_pressure_wah','0',phase))
        keys.add((mode,'idle_quiet','0','warmed_held'))
    keys.add(('0','idle_noise','0','warmed_held'))
    return keys
def compare(dirs):
    metas=[json.loads((d/'metadata.json').read_text()) for d in dirs]
    stripped=[{k:v for k,v in m.items() if k not in META_VARIABLE} for m in metas]
    check(stripped[0]==stripped[1]==stripped[2],'deterministic metadata changed')
    check([m['thread_cpu_clock_available'] for m in metas]==[False,True,True],'clock availability')
    check([m['callback_cpu_requested'] for m in metas]==[False,False,True],'request modes')
    complete=[json.loads((d/'completed.json').read_text()) for d in dirs]
    check(complete[0]==complete[1]==complete[2] and complete[0]=={'completed':True,'cases':11,'rows':19},'completion differs')
    rows=[list(csv.DictReader(io.StringIO((d/'results.csv').read_text()))) for d in dirs]
    check(len(rows[0])==19 and len(rows[0])==len(rows[1])==len(rows[2])==complete[0]['rows'],'row counts')
    for group in rows:
        check({tuple(r[k] for k in ('isolation','scenario','initial_selector','phase')) for r in group}==inventory(),'unexpected or duplicate inventory')
        check(all(r['rate']=='48000' and r['block']=='128' and r['nonfinite_count']=='0' for r in group),'rate/block/nonfinite gate')
    for triple in zip(*rows):
        check(triple[0].keys()==triple[1].keys()==triple[2].keys(),'CSV schema')
        deterministic=[{k:v for k,v in r.items() if k not in TIME_FIELDS} for r in triple]
        check(deterministic[0]==deterministic[1]==deterministic[2],'workload/output/event fields differ')
        for r in triple[:2]:
            check(r['callback_cpu_status']=='disabled' and int(r['callback_cpu_call_count'])==0,'disabled accounting')
        r=triple[2]
        check(r['callback_cpu_status']=='available','enabled clock failure')
        check(int(r['callback_cpu_call_count'])==int(r['callbacks'])>0,'callback count')
        seconds=float(r['callback_cpu_seconds']); ratio=float(r['callback_cpu_rtf']); audio=float(r['audio_seconds'])
        check(all(math.isfinite(v) for v in (seconds,ratio,audio)) and seconds>0 and audio>0 and ratio>0,'invalid CPU values')
        check(math.isclose(ratio,seconds/audio,rel_tol=2e-5,abs_tol=2e-8),'CPU ratio')
    return {'qualified':True,'rows':len(rows[0]),'pcm_exact_claim':False,'timing_benefit_claim':False}
def main():
    check(platform.system()=='Darwin' and platform.machine()=='arm64','requires native macOS ARM')
    root=Path.cwd(); out=root/'mac-clock-result'; out.mkdir(exist_ok=False)
    commands=[]
    def run(name,args,timeout,cwd=out):
        receipt={'name':name,'command':list(map(str,args)),'timeout_seconds':timeout}
        commands.append(receipt)
        try:
            with (out/(name+'.log')).open('w') as log:
                cp=subprocess.Popen(receipt['command'],cwd=cwd,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                try: cp.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    try: os.killpg(cp.pid,signal.SIGKILL)
                    except ProcessLookupError: pass
                    cp.wait(); raise
            receipt['returncode']=cp.returncode
            check(cp.returncode==0,name+' failed')
        except subprocess.TimeoutExpired:
            receipt['timeout']=True; raise
        finally: (out/'commands.json').write_text(json.dumps(commands,indent=2))
    try:
        run('sdk',['xcrun','--show-sdk-version'],10); run('sdk-path',['xcrun','--show-sdk-path'],10); run('platform',['sw_vers'],10); run('compiler',['clang++','--version'],10)
        (out/'environment.json').write_text(json.dumps({k:os.environ.get(k) for k in ('ImageOS','ImageVersion','RUNNER_ARCH','GITHUB_SHA','GITHUB_RUN_ATTEMPT')},indent=2))
        run('archive',['git','archive','--format=tar','-o',out/'source.tar',BASE],30,root)
        source=out/'source'; source.mkdir(); run('extract',['tar','-xf',out/'source.tar','-C',source],30)
        original=source/'src/tools/source_factory_benchmark.cpp'; candidate=out/'candidate.cpp'; candidate.write_text(patch(original.read_text()))
        generated=out/'generated'; generated.mkdir()
        for name,path in [('original',original),('candidate',candidate)]:
            (generated/(name+'.cpp')).write_text('#include "'+str(HERE/'fp.h')+'"\n#define main benchmark_main\n#include "'+str(path)+'"\n#undef main\nint main(int argc,char** argv) { auto before=fp(); int rc=benchmark_main(argc,argv); bool ok=recordFP("benchmark-fp.json",before,fp()); return ok?rc:97; }\n')
        (generated/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.22)
project(MacClockQualification LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CS80_BUILD_PLUGIN OFF CACHE BOOL "" FORCE)
set(CS80_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("{source}" core-build)
foreach(name original candidate)
 add_executable(${{name}} ${{name}}.cpp)
 target_include_directories(${{name}} PRIVATE "{source}/src/tools")
 target_link_libraries(${{name}} PRIVATE cs80_core)
endforeach()
add_executable(probe "{HERE}/probe.cpp")
''')
        build=out/'build'; run('configure',['cmake','-S',generated,'-B',build,'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0','-DCMAKE_OSX_ARCHITECTURES=arm64'],120)
        run('build',['cmake','--build',build,'--target','original','candidate','probe','--parallel','3'],600)
        compile_commands=json.loads((build/'compile_commands.json').read_text())
        core_commands=[c for c in compile_commands if c['file'].endswith('/Ig00156Batch.cpp')]
        check(len(core_commands)==1 and 'CS80_DISABLE_AVX2_FILTER_BATCH=1' in core_commands[0]['command'],'scalar backend not established')
        (out/'diagnostic-override.json').write_text(json.dumps({'clock':'Apple CLOCK_THREAD_CPUTIME_ID enabled only in generated candidate; inherited Linux-only metadata label retained verbatim','backend':'Clang ARM scalar','deployment_target':'15.0','pcm_capture':False},indent=2))
        core=build/'core-build/libcs80_core.a'; binaries={n:build/n for n in ('original','candidate','probe')}
        hashes={n:sha(p) for n,p in {**binaries,'core':core,'original_source':original,'candidate_source':candidate}.items()}
        (out/'hashes.json').write_text(json.dumps(hashes,indent=2))
        run('probe',[binaries['probe']],15)
        dirs=[]; fp_records=[]
        for label,exe,enabled in [('original',binaries['original'],False),('candidate-off',binaries['candidate'],False),('candidate-on',binaries['candidate'],True)]:
            work=out/label; work.mkdir(); result=work/'results'; dirs.append(result)
            check(sha(core)==hashes['core'] and sha(exe)==hashes['original' if label=='original' else 'candidate'],'binary changed')
            (work/'binary-sha256.txt').write_text(sha(exe)+'\n')
            args=[exe,'--out',result,'--source-commit',BASE,'--quick','--rate','48000','--block','128','--callbacks','16']
            if enabled: args+=['--callback-cpu']
            run(label,args,180,work)
            fp=json.loads((work/'benchmark-fp.json').read_text()); fp_records.append(fp); check(fp['fpcr_before']==fp['fpcr_after'],'benchmark FPCR changed')
            check(sha(core)==hashes['core'] and sha(exe)==(work/'binary-sha256.txt').read_text().strip(),'post-run binary changed')
        check(len({f['fpcr_before'] for f in fp_records})==1,'initial benchmark FPCR differs')
        report=compare(dirs); report['base']=BASE; report['benchmark_fp']=fp_records
        (out/'result.json').write_text(json.dumps(report,indent=2))
    except Exception as e:
        (out/'result.json').write_text(json.dumps({'qualified':False,'error':str(e)},indent=2)); raise
if __name__=='__main__': main()
