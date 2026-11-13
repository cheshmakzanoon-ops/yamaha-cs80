#!/usr/bin/env python3
"""Reviewed staged drift qualification; never repeats or overwrites partial stages."""
from pathlib import Path
import hashlib,json,os,subprocess,sys,struct,math
ROOT=Path(__file__).resolve().parents[1]; RECOVERY=ROOT.parent
BASE=RECOVERY/'drift-frozen117'; BUILD=ROOT/'build-native'; HELPERS=ROOT/'build-drift-helpers'
OUT=ROOT/'docs/experiments/drift-automation'
JUCE=RECOVERY/'yamaha-cs80/build/_deps/juce-src'; SDK=JUCE/'modules/juce_audio_processors_headless/format_types/VST3_SDK'
LIBXML=RECOVERY/'yamaha-cs80/build-native/_deps/cs80_libxml2-src'; TOOLS=ROOT.parents[1]/'cs80-build-tools/bin'
MODULE=BUILD/'CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
RUNNER=BUILD/'cs80_vst3_drift_automation_tests'; COMPAT=HELPERS/'compat117'
BASE_SHA='1cd4bd75a6450d2649102a4669eceaeb9e0c1f84234e22081d0373d6e7c390a9'
ALLOWED={'CMakeLists.txt','src/plugin/HostAutomationBridge.h','tests/Vst3NoiseAutomationTests.cpp'}
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def hashes(paths):return {str(p):digest(p) for p in paths}
freeze=json.loads((ROOT/'tests/drift-source-freeze.json').read_text())
inventory=json.loads((BASE/'tracked-identities.json').read_text())
INVENTORY_SHA='bc9d8261c71c0f9aade26e9611ebb9104f7f8c357f0b304690f6879460e4ac75'
def verify():
    assert digest(BASE/'tracked-identities.json')==INVENTORY_SHA and len(inventory)==5673
    for path,h in freeze.items():assert digest(ROOT/path)==h,('reviewed source changed',path)
    for path,h in inventory.items():
        assert digest(BASE/path)==h,('baseline source changed',path)
        if path not in ALLOWED:assert digest(ROOT/path)==h,('retained candidate changed',path)
    for name in ('Vst3DriftAutomationTests.cpp','Vst3Frozen117Roster.h'):
        assert digest(BASE/'tests'/name)==digest(ROOT/'tests'/name),('baseline fixture differs',name)
    assert digest(BASE/'baseline117.so')==BASE_SHA
    if OUT.exists():
        for marker in OUT.glob('*/complete.json'):
            completed=json.loads(marker.read_text())
            assert completed['source_freeze_sha256']==digest(ROOT/'tests/drift-source-freeze.json'),'stage source freeze differs'
            for key in ('evidence_hashes','binaries','capture_hashes'):
                for path,h in completed.get(key,{}).items():assert digest(Path(path))==h,('completed-stage identity changed',path)
def require_done(name):assert (OUT/name/'complete.json').is_file(),('prerequisite incomplete',name)
ENV=os.environ.copy()
raw=subprocess.check_output(['bash','-c','source "$1"; env -0','bash',str(RECOVERY/'yamaha-cs80-tuning-ref-automation/build/recovery-env.sh')])
ENV.update(dict(x.decode().split('=',1) for x in raw.split(b'\0') if x))
def run(args,log):
    with log.open('x') as f:
        f.write('COMMAND '+repr([str(x) for x in args])+'\n');f.flush()
        subprocess.run([str(x) for x in args],cwd=ROOT,env=ENV,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=1800)
args=sys.argv[1:];assert args and args[0] in ('build-candidate','build-baseline','run','collect'),'explicit stage required'
stage=args[0]
if stage=='run':
    assert len(args)==3 and args[1] in ('44100','48000','96000') and args[2] in ('realtime','offline')
    name='run-'+args[1]+'-'+args[2]
else:assert len(args)==1;name=stage
verify();OUT.mkdir(parents=True,exist_ok=True)
if stage=='run':require_done('build-candidate');require_done('build-baseline')
if stage=='collect':
    for rate in ('44100','48000','96000'):
        for mode in ('realtime','offline'):require_done('run-'+rate+'-'+mode)
D=OUT/name;D.mkdir(exist_ok=False) # An incomplete stage is uncertain: never retry or overwrite.
result={'stage':name,'source_freeze_sha256':digest(ROOT/'tests/drift-source-freeze.json')}
if stage=='build-candidate':
    assert not BUILD.exists(),'fresh candidate build required'
    run([TOOLS/'cmake','-S',ROOT,'-B',BUILD,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCS80_BUILD_PLUGIN=ON','-DCS80_BUILD_TESTS=ON',
         '-DFETCHCONTENT_FULLY_DISCONNECTED=ON',f'-DFETCHCONTENT_SOURCE_DIR_JUCE={JUCE}',f'-DFETCHCONTENT_SOURCE_DIR_CS80_LIBXML2={LIBXML}'],D/'configure.log')
    run([TOOLS/'cmake','--build',BUILD,'--target','cs80_vst3_drift_automation_tests','-j2'],D/'build.log')
    result['binaries']=hashes([MODULE,RUNNER])
elif stage=='build-baseline':
    HELPERS.mkdir(exist_ok=False)
    run(['/usr/bin/c++','-std=c++20','-O3','-DNDEBUG','-I'+str(BASE/'src'),'-I'+str(SDK),BASE/'tests/Vst3DriftAutomationTests.cpp',
         SDK/'pluginterfaces/base/coreiids.cpp',SDK/'pluginterfaces/base/funknown.cpp',SDK/'public.sdk/source/vst/vstinitiids.cpp','-ldl','-o',COMPAT],D/'baseline-build.log')
    result['binaries']=hashes([COMPAT,BASE/'baseline117.so'])
elif stage=='run':
    rate,mode=args[1:];candidate=BUILD/('drift-candidate-'+rate+'-'+mode);baseline=HELPERS/('drift-baseline-'+rate+'-'+mode)
    assert not candidate.exists() and not baseline.exists()
    host=ROOT/'tests/parity/tolerances-v2/host-automation.json';contract=ROOT/'tests/parity/tolerances-v2/host-automation-drift-v1.json'
    run([RUNNER,MODULE,candidate,host,contract,rate,mode],D/'candidate.log')
    run([COMPAT,BASE/'baseline117.so',baseline,host,contract,rate,mode,'--compat-only'],D/'baseline.log')
    assert 'compiled_lanes=118\n' in (D/'candidate.log').read_text() and 'compiled_lanes=117\n' in (D/'baseline.log').read_text()
    assert 'scenarios=4 renders=72 captures=72 failures=0 mode=full' in (D/'candidate.log').read_text()
    assert 'scenarios=4 renders=20 captures=20 failures=0 mode=compat-only' in (D/'baseline.log').read_text()
    candidate_files=sorted(candidate.glob('*.pcm'));baseline_files=sorted(baseline.glob('*.pcm'))
    assert len(candidate_files)==72 and len(baseline_files)==20
    for p in candidate_files+baseline_files:assert p.stat().st_size==262144
    pairs=[]
    for b in baseline_files:
        a=candidate/b.name;assert a.read_bytes()==b.read_bytes(),b.name
        pairs.append({'case':rate+'-'+mode+'/'+b.name,'candidate':str(a),'baseline':str(b),'candidate_sha256':digest(a),'baseline_sha256':digest(b),'byte_equal':True})
    save(D/'pairs.json',pairs);save(D/'captures.json',hashes(candidate_files+baseline_files))
    result.update(actual_pcm_pairs=20,candidate_renders=72,baseline_renders=20,capture_hashes=hashes(candidate_files+baseline_files))
elif stage=='collect':
    pairs=[];all_captures={};metrics=[];model_metrics=[]
    host=json.loads((ROOT/'tests/parity/tolerances-v2/host-automation.json').read_text())['host_automation']
    limits=json.loads((ROOT/'tests/parity/tolerances-v2/host-automation-drift-v1.json').read_text())['host_automation_drift']
    def pcm(p):
        raw=p.read_bytes();assert len(raw)==262144
        values=struct.unpack('<65536f',raw);assert all(map(math.isfinite,values));return raw,values
    def delta(a,b):return max(abs(x-y) for x,y in zip(a,b))
    for d in sorted(OUT.glob('run-*')):
        require_done(d.name);part=json.loads((d/'pairs.json').read_text());assert len(part)==20
        captures=json.loads((d/'captures.json').read_text());assert len(captures)==92
        for p,h in captures.items():assert digest(Path(p))==h;pcm(Path(p))
        all_captures.update(captures)
        for pair in part:
            a,b=Path(pair['candidate']),Path(pair['baseline'])
            assert digest(a)==pair['candidate_sha256'] and digest(b)==pair['baseline_sha256'] and a.read_bytes()==b.read_bytes()
        candidate=Path(part[0]['candidate']).parent;expected=set();model_scalars=[]
        for scenario in range(4):
            prefix=f'drift-case{scenario}-'
            suffixes=['noqueue','null','preceding117','legacy-prior','pure-prior','legacy','legacy-flat','pure','pure-drift-held','scalar','constant',*[f'block{x}' for x in (7,64,256,1024)]]
            if scenario%2==1:suffixes += [f'reverse{x}' for x in (7,64,256,1024)]+[f'held-lane{x}' for x in range(2)]
            expected.update(prefix+x+'.pcm' for x in suffixes);data={x:pcm(candidate/(prefix+x+'.pcm')) for x in suffixes}
            for x,y in [('noqueue','null'),('legacy','legacy-flat'),('pure','pure-drift-held')]:assert data[x][0]==data[y][0]
            scalar=data['scalar'];model_scalars.append(scalar);constant=data['constant'];assert scalar[0][:8192]==constant[0][:8192]
            errors=[delta(scalar[1],data[f'block{x}'][1]) for x in (7,64,256,1024)];assert max(errors)<=min(host['audio_max_abs_error'],limits['audio_max_abs_error'])
            effect=delta(scalar[1][2048:57344],constant[1][2048:57344]);assert effect>limits['minimum_drift_effect']
            lane_effects=[]
            if scenario%2==1:
                for x in (7,64,256,1024):assert data[f'block{x}'][0]==data[f'reverse{x}'][0]
                lane_effects=[delta(scalar[1][2048:57344],data[f'held-lane{x}'][1][2048:57344]) for x in range(2)];assert min(lane_effects)>limits['minimum_drift_effect']
            metrics.append({'config':d.name,'scenario':scenario,'max_error':max(errors),'held_effect':effect,'per_lane_effects':lane_effects})
        for i in range(2):
            difference=delta(model_scalars[i][1][2048:57344],model_scalars[i+2][1][2048:57344]);assert difference>limits['minimum_drift_effect']
            model_metrics.append({'config':d.name,'paired_scenario':i,'model_effect':difference})
        assert {p.name for p in candidate.glob('*.pcm')}==expected;pairs+=part
    assert len(pairs)==120 and len(all_captures)==552;save(D/'compatibility-pairs.json',pairs);save(D/'captures.json',all_captures)
    result.update(full_cases=24,compat_cases=24,candidate_renders=432,baseline_renders=120,actual_pcm_pairs=120,captures=552,
        scalar_block_comparisons=96,queue_order_comparisons=48,overall_witnesses=24,combined_per_lane_witnesses=24,
        initial_holds=24,model_witnesses=12,model_metrics=model_metrics,recomputed_metrics=metrics,roster=118,historical_suites='not rerun',forward_guards='source reviewed only',state_readbacks=False)
verify();result['evidence_hashes']=hashes(sorted(p for p in D.rglob('*') if p.is_file()));save(D/'complete.json',result)
print('PASS stage '+name)
