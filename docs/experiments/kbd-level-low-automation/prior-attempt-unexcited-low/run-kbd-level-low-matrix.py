from pathlib import Path
import hashlib,json,subprocess
root=Path.cwd()
host=root/'build-native/cs80_vst3_kbd_level_low_automation_tests'
module=Path('build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so')
baseline=Path('../yamaha-cs80-initial-bend-automation')/module
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert Path('build/build.exit').read_text().strip()=='0'
assert sha(baseline)=='0f58966509ddb78b14c2acc454c8e01a5ac387cb4cbf4f460d7e1eae08520f4a'
inputs={str(p):sha(p) for p in [host,module,baseline,Path('tests/Vst3KbdLevelLowAutomationTests.cpp'),Path('src/plugin/HostAutomationBridge.h')]}
Path('build/runtime-inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
results=[]
for kind,mod in [('candidate',module),('baseline',baseline)]:
 for rate in [44100,48000,96000]:
  for mode in ['realtime','offline']:
   out=Path(f'build/kbd-level-low-matrix/{kind}/{rate}-{mode}');out.mkdir(parents=True,exist_ok=False)
   command=[str(host),str(mod),'tests/parity/tolerances-v2/host-automation.json','tests/parity/tolerances-v2/host-automation-kbd-level-low-v1.json',str(out),str(rate),mode]+(['--compat-only'] if kind=='baseline' else [])
   with (out/'run.log').open('w') as log:status=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
   results.append(dict(kind=kind,rate=rate,mode=mode,command=command,exit=status,log=str(out/'run.log')))
   Path('build/matrix-runs.json').write_text(json.dumps(results,indent=2)+'\n')
   print(kind,rate,mode,'exit',status,flush=True)
   if status: raise SystemExit('Failure retained; remaining runs not started.')
pairs=[]
for rate in [44100,48000,96000]:
 for mode in ['realtime','offline']:
  a=Path(f'build/kbd-level-low-matrix/candidate/{rate}-{mode}');b=Path(f'build/kbd-level-low-matrix/baseline/{rate}-{mode}')
  names=sorted(x.name for x in a.glob('*.pcm'));assert len(names)==18
  assert names==sorted(x.name for x in b.glob('*.pcm'))
  for name in names:
   x,y=sha(a/name),sha(b/name);pairs.append(dict(rate=rate,mode=mode,file=name,candidate_sha256=x,baseline_sha256=y,bytes=(a/name).stat().st_size,equal=x==y))
Path('build/pcm-comparison.json').write_text(json.dumps(dict(pair_count=len(pairs),all_equal=all(x['equal'] for x in pairs),pairs=pairs),indent=2)+'\n')
assert len(pairs)==108 and all(x['equal'] for x in pairs)
assert all(sha(Path(p))==h for p,h in inputs.items())
print('PASS:12 runs,108 exact PCM pairs, inputs unchanged',flush=True)
