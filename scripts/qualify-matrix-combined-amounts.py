#!/usr/bin/env python3
"""Staged test-only matrix concurrency checks; no retries or production rebuild."""
from pathlib import Path
import hashlib,json,os,shlex,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]; RECOVERY=ROOT.parent
BASE=RECOVERY/'yamaha-cs80-matrix-amount-automation'; BASEBUILD=BASE/'build-native'
SDK=RECOVERY/'yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK'
ARCHIVE=BASEBUILD/'CS80Reference_artefacts/Release/libCS-80 Reference_SharedCode.a'
MODULE=BASEBUILD/'CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
ARCHIVE_SHA='d97b5bf205862626d1fc366eb87e18807cc081d0aed8e7cb5c0e0f02d1a498ca'
MODULE_SHA='f444d571bfebc48b00732670936fd77e34dec1b3066fb4b07ef70ba05cf2e209'
OUT=ROOT/'docs/experiments/matrix-combined-amounts'; BUILD=ROOT/'build-matrix-combined'
GENERATOR=BUILD/'canonical-state'; RUNNER=BUILD/'native-combined'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def hashes(paths):return {str(p):digest(p) for p in paths}
retained=json.loads((ROOT/'tests/matrix-combined-retained-freeze.json').read_text())
freeze=json.loads((ROOT/'tests/matrix-combined-source-freeze.json').read_text())
def verify():
    for p,h in {**retained,**freeze}.items():assert digest(ROOT/p)==h,p
    for p,h in retained.items():
        if p.startswith('src/'):assert digest(BASE/p)==h,('archive source differs',p)
    assert digest(ARCHIVE)==ARCHIVE_SHA and digest(MODULE)==MODULE_SHA
    if OUT.exists():
        for marker in OUT.glob('*/complete.json'):
            data=json.loads(marker.read_text())
            assert data['freeze_sha256']==digest(ROOT/'tests/matrix-combined-source-freeze.json')
            for key in ('binaries','evidence','states','captures'):
                for p,h in data.get(key,{}).items():assert digest(Path(p))==h,('completed stage changed',p)
def require_done(stage):assert (OUT/stage/'complete.json').is_file(),('prerequisite incomplete',stage)
ENV=os.environ.copy()
raw=subprocess.check_output(['bash','-c','source "$1"; env -0','bash',str(RECOVERY/'yamaha-cs80-tuning-ref-automation/build/recovery-env.sh')])
ENV.update(dict(x.decode().split('=',1) for x in raw.split(b'\0') if x))
def run(args,log,cwd=ROOT):
    with log.open('x') as f:
        f.write('COMMAND '+repr([str(x) for x in args])+'\n');f.flush()
        subprocess.run([str(x) for x in args],cwd=cwd,env=ENV,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=1200)
args=sys.argv[1:];assert args and args[0] in ('build','generate','run','collect'),'explicit stage required'
stage=args[0]
if stage=='run':
    assert len(args)==3 and args[1] in ('44100','48000','96000') and args[2] in ('realtime','offline')
    name='run-'+args[1]+'-'+args[2]
else:assert len(args)==1;name=stage
verify();OUT.mkdir(parents=True,exist_ok=True)
if stage=='generate':require_done('build')
if stage=='run':require_done('generate')
if stage=='collect':
    for rate in ('44100','48000','96000'):
        for mode in ('realtime','offline'):require_done('run-'+rate+'-'+mode)
D=OUT/name;D.mkdir(exist_ok=False) # An incomplete prior stage is never overwritten or auto-retried.
result={'stage':name,'freeze_sha256':digest(ROOT/'tests/matrix-combined-source-freeze.json')}
if stage=='build':
    BUILD.mkdir(exist_ok=False)
    commands=json.loads((BASEBUILD/'compile_commands.json').read_text())
    entry=next(x for x in commands if x['file'].endswith('/tests/MainVst3MultiRecallStateTests.cpp'))
    command=shlex.split(entry['command']);obj=BUILD/'generator.o'
    command[command.index('-o')+1]=str(obj);command[command.index('-c')+1]=str(ROOT/'tests/MatrixCombinedCanonicalGenerator.cpp')
    run(command,D/'generator-compile.log',BASEBUILD)
    ninja=(BASEBUILD/'build.ninja').read_text();start=ninja.index('build cs80_main_vst3_multirecall_state_tests:')
    libs=next(line.split(' = ',1)[1] for line in ninja[start:].splitlines() if line.startswith('  LINK_LIBRARIES = '))
    run(['/usr/bin/c++','-o',GENERATOR,obj,*shlex.split(libs)],D/'generator-link.log',BASEBUILD)
    run(['/usr/bin/c++','-std=c++20','-O3','-DNDEBUG','-I'+str(ROOT/'src'),'-I'+str(SDK),ROOT/'tests/Vst3MatrixCombinedAmountsTests.cpp',
         SDK/'pluginterfaces/base/coreiids.cpp',SDK/'pluginterfaces/base/funknown.cpp',SDK/'public.sdk/source/vst/vstinitiids.cpp','-ldl','-o',RUNNER],D/'native-compile.log')
    result['binaries']=hashes([ARCHIVE,MODULE,GENERATOR,RUNNER])
    result['provenance']={'archive_source':'all production matches frozen108','compiler_database':digest(BASEBUILD/'compile_commands.json'),
                          'build_ninja':digest(BASEBUILD/'build.ninja'),'production_rebuild':False,'cmake_targets':'source reviewed only'}
elif stage=='generate':
    # Normal JUCE lifecycle only; stop on display/initialization errors, with no workaround.
    run([GENERATOR,'generate',D/'canonical'],D/'generate.log')
    assert 'canonical_sessions=2 config_roundtrips=2 failures=0' in (D/'generate.log').read_text()
    states=sorted((D/'canonical').glob('*.state'));assert len(states)==2;result['states']=hashes(states)
elif stage=='run':
    rate,mode=args[1:];native=BUILD/(rate+'-'+mode)
    run([RUNNER,MODULE,OUT/'generate/canonical',native,ROOT/'tests/parity/tolerances-v2/host-automation.json',
         ROOT/'tests/parity/tolerances-v2/matrix-combined-amounts-v1.json',rate,mode],D/'native.log')
    assert 'cases=1 renders=36 captures=36 readbacks=2 failures=0' in (D/'native.log').read_text()
    states=[]
    for enabled in (0,1):
        label='all-on' if enabled else 'all-off';path=native/(label+'.state');log=D/(label+'-verify.log')
        run([GENERATOR,'verify',path,str(enabled)],log)
        assert 'canonical_native_readback=1 failures=0' in log.read_text();states.append(path)
    captures=sorted(native.glob('*.pcm'));assert len(captures)==36
    for p in captures:assert p.stat().st_size==131072
    result.update(captures=hashes(captures),states=hashes(states),renders=36,native_readbacks=2)
elif stage=='collect':
    captures={};states={}
    for d in sorted(OUT.glob('run-*')):
        require_done(d.name);data=json.loads((d/'complete.json').read_text())
        captures.update(data['captures']);states.update(data['states'])
    assert len(captures)==216 and len(states)==12
    result.update(captures=captures,states=states,cases=6,renders=216,canonical_sessions=2,native_readbacks=12,
                  scalar_block_comparisons=24,queue_order_comparisons=48,per_slot_witnesses=96,
                  baseline_comparison=False,production_changes=False,historical_suites='not rerun',cmake_targets='source reviewed only')
verify();result['evidence']=hashes(sorted(p for p in D.rglob('*') if p.is_file()));save(D/'complete.json',result)
print('PASS stage '+name)
