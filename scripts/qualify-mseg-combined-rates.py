#!/usr/bin/env python3
"""Bounded combined-rate public-API canonical sessions and supported native queues. No broader suites or fallback."""
from pathlib import Path
import hashlib,json,os,shlex,subprocess
ROOT=Path(__file__).resolve().parents[1]; RECOVERY=ROOT.parent
BASE=RECOVERY/'yamaha-cs80-mseg-rate-automation'; BASEBUILD=BASE/'build-native'
SDK=RECOVERY/'yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK'
ARCHIVE=BASEBUILD/'CS80Reference_artefacts/Release/libCS-80 Reference_SharedCode.a'
MODULE=BASEBUILD/'CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
ARCHIVE_SHA='7bbe32e6e3ada6edb7a1324a19e146dd2a8da89885834a3e70780d3b7a085198'
MODULE_SHA='1c407d8164a5a90ba9cb1c910e0d1e2c1f8099407704f7c2cc50814bc4b43485'
OUT=ROOT/'docs/experiments/mseg-combined-rates'; BUILD=ROOT/'build-mseg-combined-rates'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
retained=json.loads((ROOT/'tests/mseg-combined-retained-freeze.json').read_text())
freeze=json.loads((ROOT/'tests/mseg-combined-source-freeze.json').read_text())
def verify():
    for p,h in {**retained,**freeze}.items():assert digest(ROOT/p)==h,p
    for p,h in retained.items():
        if p.startswith('src/'):assert digest(BASE/p)==h,'generator library source differs: '+p
    assert digest(ARCHIVE)==ARCHIVE_SHA and digest(MODULE)==MODULE_SHA
verify()
OUT.mkdir(parents=True,exist_ok=False);BUILD.mkdir(exist_ok=False)
ENV=os.environ.copy()
raw=subprocess.check_output(['bash','-c','source "$1"; env -0','bash',str(RECOVERY/'yamaha-cs80-tuning-ref-automation/build/recovery-env.sh')])
ENV.update(dict(x.decode().split('=',1) for x in raw.split(b'\0') if x))
def run(args,log,cwd=ROOT):
    with log.open('w') as f:
        f.write('COMMAND '+repr([str(x) for x in args])+'\n');f.flush()
        subprocess.run([str(x) for x in args],cwd=cwd,env=ENV,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=1200)
# Reuse the exact official project's compiler definitions/include paths for a
# standalone processor-linked target, but compile ONLY this new generator.
commands=json.loads((BASEBUILD/'compile_commands.json').read_text())
entry=next(x for x in commands if x['file'].endswith('/tests/MainVst3MultiRecallStateTests.cpp'))
args=shlex.split(entry['command']);obj=BUILD/'generator.o';generator=BUILD/'canonical-state';runner=BUILD/'native-proof'
args[args.index('-o')+1]=str(obj);args[args.index('-c')+1]=str(ROOT/'tests/MsegCombinedCanonicalGenerator.cpp')
run(args,OUT/'generator-compile.log',BASEBUILD)
# Existing SharedCode linkage only; no aggregate target or new library build.
ninja=(BASEBUILD/'build.ninja').read_text();start=ninja.index('build cs80_main_vst3_multirecall_state_tests:')
libs=next(line.split(' = ',1)[1] for line in ninja[start:].splitlines() if line.startswith('  LINK_LIBRARIES = '))
run(['/usr/bin/c++','-o',generator,obj,*shlex.split(libs)],OUT/'generator-link.log',BASEBUILD)
run(['/usr/bin/c++','-std=c++20','-O3','-DNDEBUG','-I'+str(ROOT/'src'),'-I'+str(SDK),ROOT/'tests/Vst3MsegCombinedRatesTests.cpp',SDK/'pluginterfaces/base/coreiids.cpp',SDK/'pluginterfaces/base/funknown.cpp',SDK/'public.sdk/source/vst/vstinitiids.cpp','-ldl','-o',runner],OUT/'native-compile.log')
before={str(p):digest(p) for p in [ARCHIVE,MODULE,generator,runner]}
(OUT/'binary-before.json').write_text(json.dumps(before,indent=2)+'\n')
(OUT/'build-provenance.json').write_text(json.dumps({'retained_source_commit':'d0d72188','baseline_comparison':False,'archive_sha256':ARCHIVE_SHA,'module_sha256':MODULE_SHA,'compiler_database_sha256':digest(BASEBUILD/'compile_commands.json'),'build_ninja_sha256':digest(BASEBUILD/'build.ninja'),'all_production_source_matches':True},indent=2)+'\n')
# Standard JUCE library initialization only. Stop on missing display or any error;
# do not introduce display, privilege, tracing or other workaround.
run([generator,'generate',OUT/'canonical'],OUT/'generate.log')
assert 'canonical_sessions=2 config_roundtrips=2 failures=0' in (OUT/'generate.log').read_text()
captures=[];state_hashes={}
for rate in ('44100','48000','96000'):
    for mode in ('realtime','offline'):
        identity=rate+'-'+mode; native=BUILD/identity;log=OUT/(identity+'.log')
        run([runner,MODULE,OUT/'canonical',native,ROOT/'tests/parity/tolerances-v2/host-automation.json',
             ROOT/'tests/parity/tolerances-v2/mseg-combined-rates-v1.json',rate,mode],log)
        assert 'cases=1 renders=19 captures=19 readbacks=2 failures=0' in log.read_text()
        for enabled in (0,1):
            name='all-on' if enabled else 'all-off';path=native/(name+'.state')
            verify_log=OUT/(identity+'-'+name+'-verify.log')
            run([generator,'verify',path,str(enabled)],verify_log)
            assert 'canonical_native_readback=1 failures=0' in verify_log.read_text()
            state_hashes[str(path)]=digest(path)
        files=sorted(native.glob('*.pcm'));assert len(files)==19
        for p in files:
            assert p.stat().st_size==131072
            captures.append({'case':identity+'/'+p.name,'sha256':digest(p),'bytes':p.stat().st_size})
assert len(captures)==114 and len(state_hashes)==12
for p in sorted((OUT/'canonical').glob('*.state')):state_hashes[str(p)]=digest(p)
assert len(state_hashes)==14
(OUT/'capture-hashes.json').write_text(json.dumps(captures,indent=2)+'\n')
(OUT/'state-hashes.json').write_text(json.dumps(state_hashes,indent=2)+'\n')
verify()
for p,h in before.items():assert digest(Path(p))==h,p
(OUT/'summary.json').write_text(json.dumps({'cases':6,'renders':114,'captures':114,'canonical_sessions':2,
    'native_config_readbacks':12,'scalar_block_comparisons':24,'queue_order_comparisons':24,
    'positive_per_lane_witnesses':18,'roster':92,'production_changes':False,
    'baseline_comparison':False,'historical_suites':'not rerun','cmake_targets':'source reviewed only'},indent=2)+'\n')
files=[ARCHIVE,MODULE,generator,runner,*sorted(OUT.rglob('*'))]
(OUT/'sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p):digest(p) for p in files if p.is_file()},indent=2)+'\n')
print('PASS:6cases114renders114captures12native-readbacks; no baseline comparison')
