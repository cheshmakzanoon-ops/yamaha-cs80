from pathlib import Path
import hashlib,json,subprocess
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();r=Path.cwd();host=r/'build/tremolo-host';module=Path('../yamaha-cs80-chorus-controls/build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so');assert sha(module)=='cd7b73b9b7217dcca65dc3112569fd6175b61a52540e4d825969ee52b44503d8';assert Path('build/compile.exit').read_text().strip()=='0'
inputs={str(p):sha(p) for p in [host,module,Path('tests/Vst3TremoloAutomationTests.cpp'),Path('tests/Vst3AutomationTests.cpp')]};Path('build/runtime-inputs.json').write_text(json.dumps(inputs,indent=2)+'\n');runs=[]
for rate in [44100,48000,96000]:
 for mode in ['realtime','offline']:
  out=Path(f'build/matrix/{rate}-{mode}');out.mkdir(parents=True,exist_ok=False);cmd=[str(host),str(module),'tests/parity/tolerances-v2/host-automation.json','tests/parity/tolerances-v2/host-automation-tremolo-v1.json',str(out),str(rate),mode]
  with (out/'run.log').open('w') as log:code=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT).returncode
  runs.append(dict(rate=rate,mode=mode,command=cmd,exit=code,log=str(out/'run.log')));Path('build/runs.json').write_text(json.dumps(runs,indent=2)+'\n');print(rate,mode,code,flush=True)
  if code:raise SystemExit('Failure retained; remaining runs not started')
assert all(sha(Path(p))==h for p,h in inputs.items());print('PASS:6runs,inputs unchanged; no baseline comparison',flush=True)
