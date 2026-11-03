#!/usr/bin/env python3
"""Reproduce bounded graph/legacy checks; never patch the repository or relax gates."""
from pathlib import Path
import argparse, hashlib, json, platform, re, subprocess
BASE='db24a47170610f81236e8829ef5536af69dfc94d'
BASE_SHA='559adda6ec5da6334ba19592e05189cb5f8dc953608937a483a3da9e4557c4c1'
p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--output',type=Path,required=True);p.add_argument('--compiler',default='c++');p.add_argument('--audio',action='store_true');args=p.parse_args()
repo=args.repo.resolve();out=args.output.resolve()
if out==repo or repo in out.parents: raise SystemExit('Output must be outside the source repository')
if out.exists() and any(out.iterdir()): raise SystemExit('Use a new or empty output directory to avoid stale receipts')
out.mkdir(parents=True,exist_ok=True)
flags=['-std=c++20','-O2','-ffp-contract=off']
records=[]
def run(label,cmd,expected=0):
    r=subprocess.run([str(x) for x in cmd],capture_output=True,text=True)
    (out/(label+'.log')).write_text(r.stdout+r.stderr)
    records.append({'label':label,'command':[str(x) for x in cmd],'exit':r.returncode,'expected_exit':expected,
        'log_sha256':hashlib.sha256((out/(label+'.log')).read_bytes()).hexdigest()})
    print(label,'exit',r.returncode,'expected',expected,flush=True)
    if r.returncode!=expected: raise RuntimeError(f'{label}: unexpected exit {r.returncode}; see log')
    return r

def exact_replace(text,old,new,label):
    if text.count(old)!=1: raise RuntimeError(f'{label}: expected exactly one boundary')
    return text.replace(old,new)

dsp=(repo/'src/core/Dsp.h').read_bytes()
if dsp!=subprocess.check_output(['git','-C',str(repo),'show',f'{BASE}:src/core/Dsp.h']):
    raise RuntimeError('Shared Dsp.h dependency differs from the frozen baseline')
raw=subprocess.check_output(['git','-C',str(repo),'show',f'{BASE}:src/core/BbdChorus.h'])
if hashlib.sha256(raw).hexdigest()!=BASE_SHA: raise RuntimeError('Frozen baseline hash differs')
frozen=exact_replace(raw.decode(),'namespace cs80\n{','namespace cs80_oe2_frozen\n{\nusing namespace cs80;','frozen namespace')
(out/'FrozenBbdChorus.h').write_text(frozen)
current=(repo/'src/core/BbdChorus.h').read_text()
current=exact_replace(current,'namespace cs80\n{','namespace topology_probe {\ninline double lo1=0, lo2=0;\ninline std::array<float,2> wet {};\n}\nnamespace cs80\n{','injection namespace')
for n in (1,2):
    current,count=re.subn(r'^        const double do'+str(n)+r' = .*?;$','        const double do'+str(n)+' = topology_probe::lo'+str(n)+';',current,flags=re.M)
    if count!=1: raise RuntimeError(f'DO{n} boundary changed')
current=exact_replace(current,'        if (! on)\n            return { inL, inR };','        wet = topology_probe::wet; // OFFLINE injection only\n        if (! on)\n            return { inL, inR };','wet injection')
observed=(repo/'src/core/BbdChorus.h').read_text()
observed=exact_replace(observed,'namespace cs80\n{','namespace topology_observe {\ninline double phase1=0,phase2=0,clockHz=0;\ninline std::array<float,2> wet {};\n}\nnamespace cs80\n{','observer namespace')
observed=exact_replace(observed,'        clockPeriods += clockHz / (double) sr;','        topology_observe::phase1=do1;topology_observe::phase2=do2;topology_observe::clockHz=clockHz;\n        clockPeriods += clockHz / (double) sr;','clock observer')
observed=exact_replace(observed,'        if (! on)\n            return { inL, inR };','        topology_observe::wet=wet; // observation only\n        if (! on)\n            return { inL, inR };','wet observer')
helper=(repo/'src/experimental/Oe2Topology.h').read_text()
mutations={
    'candidate':helper,
    'same-chain-grouping':exact_replace(helper,'gains[0] * wet[0] + gains[1] * wet[1],\n             gains[2] * wet[1] + gains[3] * wet[0]','(gains[0] + gains[1]) * wet[0],\n             (gains[2] + gains[3]) * wet[1]','same-chain mutation'),
    'crossed-right-phase':exact_replace(helper,'return { x, y, -x, -y };','return { x, y, -y, -x };','phase mutation'),
    'swapped-O3-O4-routes':exact_replace(helper,'gains[2] * wet[1] + gains[3] * wet[0]','gains[2] * wet[0] + gains[3] * wet[1]','O3/O4 mutation')}
try:
    for name,mutated in mutations.items():
        folder=out/name;(folder/'core').mkdir(parents=True,exist_ok=True);(folder/'experimental').mkdir(exist_ok=True)
        (folder/'core/BbdChorus.h').write_text(current);(folder/'experimental/Oe2Topology.h').write_text(mutated)
        exe=folder/'routes'
        run(name+'-build',[args.compiler,*flags,'-I'+str(folder),'-I'+str(repo/'src'),'-I'+str(repo/'src/core'),repo/'tests/diagnostics/Oe2TopologyRouteTests.cpp','-o',exe])
        run(name+'-routes',[exe],0 if name=='candidate' else 1)
        extra=folder/'modulated-routes'
        run(name+'-modulated-build',[args.compiler,*flags,'-I'+str(folder),'-I'+str(repo/'src'),'-I'+str(repo/'src/core'),repo/'tests/diagnostics/Oe2TopologyModulatedRouteTests.cpp','-o',extra])
        run(name+'-modulated-routes',[extra],0 if name=='candidate' else 1)
    path_variants={
        'independent':observed,
        'both-clock-disabled':exact_replace(observed,'        if (chorus)\n            octaves =','        if (chorus && !tremolo)\n            octaves =','clock mutation'),
        'both-vca-disabled':exact_replace(observed,'const double m = tremolo ? d : 0.0;','const double m = tremolo && !chorus ? d : 0.0;','VCA mutation')}
    for name,header in path_variants.items():
        folder=out/name;(folder/'core').mkdir(parents=True,exist_ok=True)
        (folder/'core/BbdChorus.h').write_text(header)
        exe=folder/'paths'
        run(name+'-build',[args.compiler,*flags,'-I'+str(folder),'-I'+str(repo/'src'),'-I'+str(repo/'src/core'),repo/'tests/diagnostics/Oe2IndependentPathTests.cpp','-o',exe])
        run(name+'-paths',[exe],0 if name=='independent' else 1)
    exe=out/'legacy'
    run('legacy-build',[args.compiler,*flags,'-I'+str(out),'-I'+str(repo/'src'),'-I'+str(repo/'src/core'),repo/'tests/diagnostics/Oe2TopologyLegacyTests.cpp','-o',exe])
    run('legacy-comparison',[exe])
    if args.audio:
        exe=out/'audio-renderer'
        run('audio-build',[args.compiler,*flags,'-I'+str(out),'-I'+str(repo/'src'),'-I'+str(repo/'src/core'),repo/'tests/diagnostics/Oe2TopologyAudio.cpp','-o',exe])
        run('audio-first',[exe,out/'audio-first'])
        run('audio-repeat',[exe,out/'audio-repeat'])
        for path in sorted((out/'audio-first').glob('*.f32')):
            if path.read_bytes()!=(out/'audio-repeat'/path.name).read_bytes():raise RuntimeError('Audio repeat differs: '+path.name)
        run('audio-analysis',['python3',repo/'tests/diagnostics/analyze-oe2-topology-audio.py',out/'audio-first',out/'wavs'])
finally:
    (out/'manifest.json').write_text(json.dumps({'base_commit':BASE,'base_header_sha256':BASE_SHA,
        'candidate_head':subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip(),
        'tracked_diff_sha256':hashlib.sha256(subprocess.check_output(['git','-C',str(repo),'diff','--no-ext-diff'])).hexdigest(),
        'candidate_header_sha256':hashlib.sha256((repo/'src/core/BbdChorus.h').read_bytes()).hexdigest(),
        'candidate_helper_sha256':hashlib.sha256((repo/'src/experimental/Oe2Topology.h').read_bytes()).hexdigest(),
        'compiler':subprocess.check_output([args.compiler,'--version'],text=True).splitlines()[0],
        'platform':platform.platform(),'flags':flags,
        'source_sha256':{str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [
            repo/'src/core/BbdChorus.h',repo/'src/core/Dsp.h',repo/'src/experimental/Oe2Topology.h',
            *sorted((repo/'tests/diagnostics').glob('Oe2Topology*.cpp')),repo/'tests/diagnostics/Oe2IndependentPathTests.cpp',
            repo/'tests/diagnostics/run-oe2-topology-experiment.py',repo/'tests/diagnostics/analyze-oe2-topology-audio.py']},
        'artifact_sha256':{str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.rglob('*')) if p.is_file() and p!=out/'manifest.json'},
        'results':records},indent=2)+'\n')
