#!/usr/bin/env python3
"""Finite standalone waveform law qualification; not a performance measurement."""
from pathlib import Path
import hashlib,json,subprocess,struct,math
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/experiments/lfo-random-switching'
BUILD=ROOT/'build-lfo-random-switching'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
freeze=json.loads((ROOT/'tests/lfo-random-switch-source-freeze.json').read_text())
retained=json.loads((ROOT/'tests/lfo-random-switch-retained-freeze.json').read_text())
def verify():
    for name,expected in {**retained,**freeze}.items():assert digest(ROOT/name)==expected,f'source changed: {name}'
verify()
contract=json.loads((ROOT/'tests/parity/tolerances-v2/lfo-random-switching-v1.json').read_text())['lfo_random_switching']
assert contract['cases']==24 and contract['samples']==1140300 and contract['shape_abs_tolerance']==1e-6 and contract['minimum_boundary_clearance']==1e-10 and contract['phase_abs_tolerance']==1e-10 and contract['switches_per_case']==6
OUT.mkdir(parents=True,exist_ok=False);BUILD.mkdir(exist_ok=False)
def run(args,log):
    with log.open('w') as f:
        f.write('COMMAND '+repr([str(x) for x in args])+'\n');f.flush()
        subprocess.run([str(x) for x in args],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
runner=BUILD/'lfo-random-switching'
run(['/usr/bin/c++','-std=c++20','-O3','-DNDEBUG','-I'+str(ROOT/'src'),'-I'+str(ROOT/'tests'),ROOT/'tests/LfoRandomSwitchingTests.cpp','-o',runner],OUT/'build.log')
before=digest(runner);(OUT/'binary-before.json').write_text(json.dumps({str(runner):before},indent=2)+'\n')
captures=BUILD/'captures'
run([runner,captures],OUT/'run.log')
assert 'cases=24 samples=1140300 reset_samples=1140300 sequence_captures=48 rate_captures=4 waveform_captures=8 failures=0' in (OUT/'run.log').read_text()
files=sorted(captures.glob('*.f32'));assert len(files)==60
identities=[]
for p in files:
    rate=int(p.stem.split('-')[1] if p.name.startswith(('rates-','waves-')) else p.stem.split('-')[0]);assert p.stat().st_size==rate
    data=list(x[0] for x in struct.iter_unpack('f',p.read_bytes()));assert all(math.isfinite(v) for v in data)
    if p.name.startswith('rates-'):assert all(16<=v<=20 for v in data)
    elif p.name.startswith('waves-'):assert all(v in [5,6] for v in data)
    else:assert all(-1<=v<=1 for v in data)
    identities.append({'capture':p.name,'sha256':digest(p),'bytes':p.stat().st_size})
verify();assert digest(runner)==before
(OUT/'capture-hashes.json').write_text(json.dumps(identities,indent=2)+'\n')
(OUT/'summary.json').write_text(json.dumps({'cases':24,'compared_samples':1140300,'reset_replay_samples':1140300,'sequence_captures':48,'rate_input_captures':4,'waveform_input_captures':8,'switches':144,'production_changes':False,'scope':'standalone oscillator live selection, not host waveform automation/Engine smoothing/randomness quality'},indent=2)+'\n')
(OUT/'sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)):digest(p) for p in [runner,*sorted(OUT.iterdir())] if p.is_file()},indent=2)+'\n')
print('PASS:24cases,1140300comparedsamples,1140300resetreplay,60captures')
