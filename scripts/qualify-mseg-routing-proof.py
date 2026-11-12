#!/usr/bin/env python3
"""Single bounded public-API canonical state proof. No broader suites or fallback."""
from pathlib import Path
import hashlib,json,os,shlex,subprocess
ROOT=Path(__file__).resolve().parents[1]; RECOVERY=ROOT.parent
BASE=RECOVERY/'yamaha-cs80-lfo-rate-automation'; BASEBUILD=BASE/'build-native'
SDK=RECOVERY/'yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK'
ARCHIVE=BASEBUILD/'CS80Reference_artefacts/Release/libCS-80 Reference_SharedCode.a'
MODULE=BASEBUILD/'CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
ARCHIVE_SHA='f5b8ed1d2ca740b521d2fca7d3b4f83c2dd1cf0b9a572d1212a21324f48933a3'
MODULE_SHA='59ed49c6b64d28847b7ebe83ec60811c819c06d827679a15a72992818c93de4f'
OUT=ROOT/'docs/experiments/mseg-routing-proof'; BUILD=ROOT/'build-mseg-routing-proof'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
retained=json.loads((ROOT/'tests/mseg-routing-retained-freeze.json').read_text())
freeze=json.loads((ROOT/'tests/mseg-routing-source-freeze.json').read_text())
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
args[args.index('-o')+1]=str(obj);args[args.index('-c')+1]=str(ROOT/'tests/MsegCanonicalStateGenerator.cpp')
run(args,OUT/'generator-compile.log',BASEBUILD)
# Existing SharedCode linkage only; no aggregate target or new library build.
ninja=(BASEBUILD/'build.ninja').read_text();start=ninja.index('build cs80_main_vst3_multirecall_state_tests:')
libs=next(line.split(' = ',1)[1] for line in ninja[start:].splitlines() if line.startswith('  LINK_LIBRARIES = '))
run(['/usr/bin/c++','-o',generator,obj,*shlex.split(libs)],OUT/'generator-link.log',BASEBUILD)
run(['/usr/bin/c++','-std=c++20','-O3','-DNDEBUG','-I'+str(ROOT/'src'),'-I'+str(SDK),ROOT/'tests/Vst3MsegRoutingProofTests.cpp',SDK/'pluginterfaces/base/coreiids.cpp',SDK/'pluginterfaces/base/funknown.cpp',SDK/'public.sdk/source/vst/vstinitiids.cpp','-ldl','-o',runner],OUT/'native-compile.log')
before={str(p):digest(p) for p in [ARCHIVE,MODULE,generator,runner]}
(OUT/'binary-before.json').write_text(json.dumps(before,indent=2)+'\n')
(OUT/'build-provenance.json').write_text(json.dumps({'published_source':'244016b22e290a450707947b221f777f18e22c01','archive_sha256':ARCHIVE_SHA,'module_sha256':MODULE_SHA,'compiler_database_sha256':digest(BASEBUILD/'compile_commands.json'),'build_ninja_sha256':digest(BASEBUILD/'build.ninja'),'all_production_source_matches':True},indent=2)+'\n')
# Standard JUCE library initialization only. Stop on missing display or any error;
# do not introduce display, privilege, tracing or other workaround.
run([generator,'generate',OUT/'canonical'],OUT/'generate.log')
assert 'canonical_sessions=6 config_roundtrips=6 failures=0' in (OUT/'generate.log').read_text()
run([runner,MODULE,OUT/'canonical',BUILD/'native',ROOT/'tests/parity/tolerances-v2/mseg-native-routing-proof-v1.json'],OUT/'native.log')
assert 'routes=3 renders=15 captures=15 readbacks=15 failures=0' in (OUT/'native.log').read_text()
for route in range(3):
    for suffix,enabled in [('active',1),('zero',1),('off',0),('pure',1),('pure-off',0)]:
        name=f'route{route}-{suffix}';path=BUILD/'native'/(name+'.state')
        run([generator,'verify',path,str(route),str(enabled)],OUT/(name+'-verify.log'))
        assert 'canonical_native_readback=1 failures=0' in (OUT/(name+'-verify.log')).read_text()
captures=[]
for p in sorted((BUILD/'native').glob('*.pcm')):
    assert p.stat().st_size==131072;captures.append({'file':p.name,'sha256':digest(p),'bytes':p.stat().st_size})
assert len(captures)==15
states=[*sorted((OUT/'canonical').glob('*.state')),*sorted((BUILD/'native').glob('*.state'))];assert len(states)==21
(OUT/'capture-hashes.json').write_text(json.dumps(captures,indent=2)+'\n')
(OUT/'state-hashes.json').write_text(json.dumps({str(p):digest(p) for p in states},indent=2)+'\n')
verify()
for p,h in before.items():assert digest(Path(p))==h,p
(OUT/'summary.json').write_text(json.dumps({'routes':3,'renders':15,'captures':15,'canonical_sessions':6,'native_config_readbacks':15,'sample_rate':48000,'mode':'realtime','roster':89,'production_changes':False,'historical_suites':'not rerun','cmake_targets':'source reviewed only'},indent=2)+'\n')
files=[ARCHIVE,MODULE,generator,runner,*sorted(OUT.rglob('*'))]
(OUT/'sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p):digest(p) for p in files if p.is_file()},indent=2)+'\n')
print('PASS: 3 routes,15 finite renders,6 canonical sessions,15 native config readbacks')
