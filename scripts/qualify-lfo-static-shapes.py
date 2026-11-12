#!/usr/bin/env python3
"""Finite normal-input interaction fixture against exact published89 native binary."""
from pathlib import Path
import os,json,hashlib,subprocess
ROOT=Path(__file__).resolve().parents[1]
RECOVERY=ROOT.parent
SDK=RECOVERY/'yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK'
MODULE=RECOVERY/'yamaha-cs80-lfo-rate-automation/build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
EXPECTED_MODULE='59ed49c6b64d28847b7ebe83ec60811c819c06d827679a15a72992818c93de4f'
OUT=ROOT/'docs/experiments/lfo-static-shapes'
BUILD=ROOT/'build-lfo-static-shapes'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
freeze=json.loads((ROOT/'tests/lfo-static-shapes-source-freeze.json').read_text())
retained=json.loads((ROOT/'tests/lfo-static-shapes-retained-freeze.json').read_text())
def verify_sources():
    for path,expected in {**retained,**freeze}.items():
        assert digest(ROOT/path)==expected,f'source changed: {path}'
verify_sources()
assert digest(MODULE)==EXPECTED_MODULE
OUT.mkdir(parents=True,exist_ok=False)
BUILD.mkdir(exist_ok=False)
env=os.environ.copy()
raw=subprocess.check_output(['bash','-c','source "$1"; env -0','bash',str(RECOVERY/'yamaha-cs80-tuning-ref-automation/build/recovery-env.sh')])
env.update(dict(item.decode().split('=',1) for item in raw.split(b'\0') if item))
def run(args,path):
    with path.open('w') as f:
        f.write('COMMAND '+repr([str(x) for x in args])+'\n');f.flush()
        subprocess.run([str(x) for x in args],cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=1200)
runner=BUILD/'lfo-static-shapes'
run(['/usr/bin/c++','-std=c++20','-O3','-DNDEBUG','-I'+str(ROOT/'src'),'-I'+str(SDK),
    ROOT/'tests/Vst3LfoStaticShapesTests.cpp',SDK/'pluginterfaces/base/coreiids.cpp',SDK/'pluginterfaces/base/funknown.cpp',SDK/'public.sdk/source/vst/vstinitiids.cpp','-ldl','-o',runner],OUT/'build.log')
before={str(p):digest(p) for p in [MODULE,runner]}
(OUT/'binary-before.json').write_text(json.dumps(before,indent=2)+'\n')
captures=[]
for rate in ['44100','48000','96000']:
    for mode in ['realtime','offline']:
        identity=rate+'-'+mode;dest=BUILD/identity;log=OUT/(identity+'.log')
        assert not dest.exists()
        run([runner,MODULE,ROOT/'tests/parity/tolerances-v2/host-automation.json',ROOT/'tests/parity/tolerances-v2/host-automation-lfo-static-shapes-v1.json',dest,rate,mode],log)
        assert 'compiled_lanes=89\n' in log.read_text()
        assert 'scenarios=16 renders=256 captures=64 failures=0 mode=full' in log.read_text()
        files=sorted(dest.glob('*.pcm'));assert len(files)==64
        for p in files:
            assert p.stat().st_size==32768
            captures.append({'capture':identity+'/'+p.name,'sha256':digest(p),'bytes':32768})
verify_sources()
for p in [MODULE,runner]:assert digest(p)==before[str(p)],f'binary changed: {p}'
assert len(captures)==384
(OUT/'capture-hashes.json').write_text(json.dumps(captures,indent=2)+'\n')
(OUT/'summary.json').write_text(json.dumps({'cases':96,'renders':1536,'captures':384,'scalar_block_comparisons':288,'queue_order_checks':96,'cross_build_pairs':0,'historical_suites':'not rerun','baseline_published_commit':'244016b22e290a450707947b221f777f18e22c01'},indent=2)+'\n')
files=[MODULE,runner,*sorted(OUT.iterdir())]
(OUT/'sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p):digest(p) for p in files if p.is_file()},indent=2)+'\n')
print('PASS: 96 cases,1536 renders,384 captures,288 scalar/block comparisons,96 queue-order checks')
