#!/usr/bin/env python3
"""Reviewed staged qualification. Never retries or overwrites partial stages."""
from pathlib import Path
import hashlib,json,os,shlex,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]; RECOVERY=ROOT.parent
BASE=RECOVERY/'matrix-amount-frozen92'; REFERENCE=RECOVERY/'yamaha-cs80-mseg-rate-automation'
REFBUILD=REFERENCE/'build-native'; BUILD=ROOT/'build-native'; HELPERS=ROOT/'build-matrix-helpers'
OUT=ROOT/'docs/experiments/matrix-amount-automation'
JUCE=RECOVERY/'yamaha-cs80/build/_deps/juce-src'; SDK=JUCE/'modules/juce_audio_processors_headless/format_types/VST3_SDK'
LIBXML=RECOVERY/'yamaha-cs80/build-native/_deps/cs80_libxml2-src'; TOOLS=ROOT.parents[1]/'cs80-build-tools/bin'
ARCHIVE=REFBUILD/'CS80Reference_artefacts/Release/libCS-80 Reference_SharedCode.a'
MODULE=BUILD/'CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
RUNNER=BUILD/'cs80_vst3_matrix_amount_automation_tests'; GENERATOR=HELPERS/'canonical-state'; COMPAT=HELPERS/'compat92'
BASE_SHA='1c407d8164a5a90ba9cb1c910e0d1e2c1f8099407704f7c2cc50814bc4b43485'
ARCHIVE_SHA='7bbe32e6e3ada6edb7a1324a19e146dd2a8da89885834a3e70780d3b7a085198'
ALLOWED={'CMakeLists.txt','src/plugin/HostAutomationBridge.h','tests/Vst3MsegRateAutomationTests.cpp','tests/Vst3MsegCombinedRatesTests.cpp'}
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def hashes(paths):return {str(p):digest(p) for p in paths}
freeze=json.loads((ROOT/'tests/matrix-amount-source-freeze.json').read_text())
inventory=json.loads((BASE/'tracked-identities.json').read_text())
def verify():
    for path,h in freeze.items():assert digest(ROOT/path)==h,('reviewed source changed',path)
    for path,h in inventory.items():
        if path.startswith(('src/','tests/','docs/')) or path=='CMakeLists.txt':
            assert digest(BASE/path)==h,('baseline source changed',path)
            if path not in ALLOWED:assert digest(ROOT/path)==h,('retained candidate changed',path)
        if path.startswith('src/'):assert digest(REFERENCE/path)==h,('archive source differs',path)
    for name in ('Vst3MatrixAmountAutomationTests.cpp','Vst3Frozen92Roster.h'):
        assert digest(BASE/'tests'/name)==digest(ROOT/'tests'/name),('new baseline fixture differs',name)
    assert digest(BASE/'baseline92.so')==BASE_SHA and digest(ARCHIVE)==ARCHIVE_SHA
    if OUT.exists():
        for marker in OUT.glob('*/complete.json'):
            completed=json.loads(marker.read_text())
            assert completed['source_freeze_sha256']==digest(ROOT/'tests/matrix-amount-source-freeze.json'),'stage source freeze differs'
            for path,h in completed.get('evidence_hashes',{}).items():assert digest(Path(path))==h,('completed-stage evidence changed',path)
            for path,h in completed.get('binaries',{}).items():assert digest(Path(path))==h,('completed-stage binary changed',path)
        if (OUT/'generate/complete.json').exists():
            for path,h in json.loads((OUT/'generate/complete.json').read_text())['states'].items():assert digest(Path(path))==h,('canonical state changed',path)
def require_done(name):assert (OUT/name/'complete.json').is_file(),('prerequisite not completed',name)
ENV=os.environ.copy()
raw=subprocess.check_output(['bash','-c','source "$1"; env -0','bash',str(RECOVERY/'yamaha-cs80-tuning-ref-automation/build/recovery-env.sh')])
ENV.update(dict(x.decode().split('=',1) for x in raw.split(b'\0') if x))
def run(args,log,cwd=ROOT):
    with log.open('x') as f:
        f.write('COMMAND '+repr([str(x) for x in args])+'\n');f.flush()
        subprocess.run([str(x) for x in args],cwd=cwd,env=ENV,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=1800)
args=sys.argv[1:]
assert args and args[0] in ('build-candidate','build-helpers','generate','run','collect'),'explicit stage required'
stage=args[0]
if stage=='run':
    assert len(args)==3 and args[1] in ('44100','48000','96000') and args[2] in ('realtime','offline')
    name='run-'+args[1]+'-'+args[2]
else:assert len(args)==1;name=stage
verify()
OUT.mkdir(parents=True,exist_ok=True)
if stage=='generate':require_done('build-candidate');require_done('build-helpers')
if stage=='run':require_done('generate')
if stage=='collect':
    for rate in ('44100','48000','96000'):
        for mode in ('realtime','offline'):require_done('run-'+rate+'-'+mode)
# A started directory without complete.json means uncertain/partial work. Never retry it.
D=OUT/name;D.mkdir(exist_ok=False)
result={'stage':name,'source_freeze_sha256':digest(ROOT/'tests/matrix-amount-source-freeze.json')}
if stage=='build-candidate':
    assert not BUILD.exists(),'fresh candidate build required'
    run([TOOLS/'cmake','-S',ROOT,'-B',BUILD,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCS80_BUILD_PLUGIN=ON','-DCS80_BUILD_TESTS=ON',
         '-DFETCHCONTENT_FULLY_DISCONNECTED=ON',f'-DFETCHCONTENT_SOURCE_DIR_JUCE={JUCE}',f'-DFETCHCONTENT_SOURCE_DIR_CS80_LIBXML2={LIBXML}'],D/'configure.log')
    run([TOOLS/'cmake','--build',BUILD,'--target','cs80_vst3_matrix_amount_automation_tests','-j2'],D/'build.log')
    result['binaries']=hashes([MODULE,RUNNER])
elif stage=='build-helpers':
    HELPERS.mkdir(exist_ok=False)
    commands=json.loads((REFBUILD/'compile_commands.json').read_text())
    entry=next(x for x in commands if x['file'].endswith('/tests/MainVst3MultiRecallStateTests.cpp'))
    command=shlex.split(entry['command']);obj=HELPERS/'generator.o'
    command[command.index('-o')+1]=str(obj);command[command.index('-c')+1]=str(ROOT/'tests/MatrixAmountCanonicalGenerator.cpp')
    run(command,D/'generator-compile.log',REFBUILD)
    ninja=(REFBUILD/'build.ninja').read_text();start=ninja.index('build cs80_main_vst3_multirecall_state_tests:')
    libs=next(line.split(' = ',1)[1] for line in ninja[start:].splitlines() if line.startswith('  LINK_LIBRARIES = '))
    run(['/usr/bin/c++','-o',GENERATOR,obj,*shlex.split(libs)],D/'generator-link.log',REFBUILD)
    run(['/usr/bin/c++','-std=c++20','-O3','-DNDEBUG','-I'+str(BASE/'src'),'-I'+str(SDK),BASE/'tests/Vst3MatrixAmountAutomationTests.cpp',
         SDK/'pluginterfaces/base/coreiids.cpp',SDK/'pluginterfaces/base/funknown.cpp',SDK/'public.sdk/source/vst/vstinitiids.cpp','-ldl','-o',COMPAT],D/'baseline-build.log')
    result['binaries']=hashes([GENERATOR,COMPAT,ARCHIVE,BASE/'baseline92.so'])
    result['provenance']={'archive_source':'frozen92 production sources verified','compiler_database':digest(REFBUILD/'compile_commands.json'),'build_ninja':digest(REFBUILD/'build.ninja')}
elif stage=='generate':
    # Standard JUCE lifecycle only; stop on display/initialization errors, never bypass.
    run([GENERATOR,'generate',D/'canonical'],D/'generate.log')
    assert 'canonical_sessions=17 config_roundtrips=17 failures=0' in (D/'generate.log').read_text()
    states=sorted((D/'canonical').glob('*.state'));assert len(states)==17
    result['states']=hashes(states)
elif stage=='run':
    rate,mode=args[1:];canonical=OUT/'generate/canonical'
    candidate=BUILD/('matrix-candidate-'+rate+'-'+mode);baseline=HELPERS/('matrix-baseline-'+rate+'-'+mode)
    assert not candidate.exists() and not baseline.exists()
    host=ROOT/'tests/parity/tolerances-v2/host-automation.json';contract=ROOT/'tests/parity/tolerances-v2/host-automation-matrix-amount-v1.json'
    run([RUNNER,MODULE,canonical,candidate,host,contract,rate,mode],D/'candidate.log')
    run([COMPAT,BASE/'baseline92.so',canonical,baseline,host,contract,rate,mode,'--compat-only'],D/'baseline.log')
    assert 'compiled_lanes=108\n' in (D/'candidate.log').read_text() and 'compiled_lanes=92\n' in (D/'baseline.log').read_text()
    assert 'scenarios=16 renders=240 captures=64 failures=0 mode=full' in (D/'candidate.log').read_text()
    assert 'scenarios=16 renders=112 captures=64 failures=0 mode=compat-only' in (D/'baseline.log').read_text()
    states={}
    for slot in range(16):
        for label,directory in [('candidate',candidate),('baseline',baseline)]:
            path=directory/(f'matrix-slot{slot}.state');log=D/(f'{label}-slot{slot}-verify.log')
            run([GENERATOR,'verify',path,str(slot)],log)
            assert 'canonical_native_readback=1 failures=0' in log.read_text();states[str(path)]=digest(path)
    names=sorted(p.name for p in candidate.glob('*.pcm'))
    assert names==sorted(p.name for p in baseline.glob('*.pcm')) and len(names)==64
    pairs=[]
    for filename in names:
        a,b=candidate/filename,baseline/filename
        assert a.stat().st_size==b.stat().st_size==131072 and a.read_bytes()==b.read_bytes(),filename
        pairs.append({'case':rate+'-'+mode+'/'+filename,'candidate':str(a),'baseline':str(b),'candidate_sha256':digest(a),'baseline_sha256':digest(b),'byte_equal':True})
    save(D/'pairs.json',pairs);save(D/'state-hashes.json',states)
    result.update(actual_pcm_pairs=64,native_readbacks=32,candidate_renders=240,baseline_renders=112)
elif stage=='collect':
    pairs=[];states={}
    for d in sorted(OUT.glob('run-*')):
        require_done(d.name);part=json.loads((d/'pairs.json').read_text());assert len(part)==64
        for pair in part:
            a,b=Path(pair['candidate']),Path(pair['baseline'])
            assert digest(a)==pair['candidate_sha256'] and digest(b)==pair['baseline_sha256'] and a.read_bytes()==b.read_bytes()
        pairs+=part
        state=json.loads((d/'state-hashes.json').read_text())
        for p,h in state.items():assert digest(Path(p))==h
        states.update(state)
    assert len(pairs)==384 and len(states)==192
    save(D/'compatibility-pairs.json',pairs);save(D/'state-hashes.json',states)
    result.update(full_cases=96,compat_cases=96,candidate_renders=1440,baseline_renders=672,actual_pcm_pairs=384,native_readbacks=192,
                  canonical_sessions=17,roster=108,historical_suites='not rerun',forward_guards='source reviewed only')
verify()
result['evidence_hashes']=hashes(sorted(p for p in D.rglob('*') if p.is_file()))
save(D/'complete.json',result)
print('PASS stage '+name)
