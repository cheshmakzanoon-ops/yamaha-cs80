from pathlib import Path
import hashlib,json,subprocess
r=Path.cwd();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
module=Path('../yamaha-cs80-ring-times/build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so')
frozen=Path('../yamaha-cs80-ring-times/build-native/cs80_vst3_ring_times_automation_tests')
assert sha(module)=='2e536c42f7cfe221127501471bbe091fc1e2e5554b2938215c0e21e8c9bf342e'
assert sha(frozen)=='0819d4a0ccbeff57f47d7b322ae6ff7f489ef21afff3ab0372f9e10fee27e99a'
assert Path('build/compile.exit').read_text().strip()=='0'
paths=[module,frozen,Path('build/retrigger-host'),Path('build/historical-times-host'),Path('tests/Vst3AutomationTests.cpp'),Path('tests/Vst3RingRetriggerAutomationTests.cpp'),Path('tests/Vst3RingTimesAutomationTests.cpp')]
inputs={str(p):sha(p) for p in paths};Path('build/runtime-inputs.json').write_text(json.dumps(inputs,indent=2)+'\n');runs=[]
for kind,host,contract,compat in [('retrigger',r/'build/retrigger-host','ring-retrigger',False),('historical-new',r/'build/historical-times-host','ring-times',True),('historical-frozen',frozen,'ring-times',True)]:
 for rate in [44100,48000,96000]:
  for mode in ['realtime','offline']:
   out=Path(f'build/matrix/{kind}/{rate}-{mode}');out.mkdir(parents=True,exist_ok=False)
   cmd=[str(host),str(module),'tests/parity/tolerances-v2/host-automation.json',f'tests/parity/tolerances-v2/host-automation-{contract}-v1.json',str(out),str(rate),mode]+(['--compat-only'] if compat else [])
   with (out/'run.log').open('w') as log:code=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT).returncode
   runs.append(dict(kind=kind,rate=rate,mode=mode,command=cmd,exit=code,log=str(out/'run.log')));Path('build/runs.json').write_text(json.dumps(runs,indent=2)+'\n');print(kind,rate,mode,code,flush=True)
   if code:raise SystemExit('Failure retained; remaining runs not started')
pairs=[]
for rate in [44100,48000,96000]:
 for mode in ['realtime','offline']:
  a=Path(f'build/matrix/historical-new/{rate}-{mode}');b=Path(f'build/matrix/historical-frozen/{rate}-{mode}');names=sorted(p.name for p in a.glob('*.pcm'));assert len(names)==36 and names==sorted(p.name for p in b.glob('*.pcm'))
  for name in names:pairs.append(dict(rate=rate,mode=mode,file=name,new_sha256=sha(a/name),frozen_sha256=sha(b/name),equal=(a/name).read_bytes()==(b/name).read_bytes()))
Path('build/capture-comparison.json').write_text(json.dumps(dict(count=len(pairs),all_equal=all(x['equal'] for x in pairs),pairs=pairs),indent=2)+'\n');assert len(pairs)==216 and all(x['equal'] for x in pairs)
assert all(sha(Path(p))==h for p,h in inputs.items());print('PASS:18runs,216default-helper PCM pairs, inputs unchanged',flush=True)
