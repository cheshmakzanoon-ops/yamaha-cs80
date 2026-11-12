from pathlib import Path
import hashlib,json,subprocess
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
host=Path('build/cs80_vst3_ribbon_release_modes_automation_tests').resolve()
module=Path('../yamaha-cs80-ribbon-automation/build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so')
assert Path('build/build.exit').read_text().strip()=='0'
assert sha(module)=='340cea26af567f1757ce908a6bb97203a6c47eb8310d0ab0bcd4196e0f4180a4'
inputs={str(p):sha(p) for p in [host,module,Path('tests/Vst3RibbonReleaseModesAutomationTests.cpp'),Path('tests/Vst3AutomationTests.cpp'),Path('src/plugin/HostAutomationBridge.h')]}
Path('build/runtime-inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
runs=[];captures=[]
for rate in [44100,48000,96000]:
 for mode in ['realtime','offline']:
  out=Path(f'build/ribbon-release-modes-matrix/{rate}-{mode}');out.mkdir(parents=True,exist_ok=False)
  cmd=[str(host),str(module),'tests/parity/tolerances-v2/host-automation.json','tests/parity/tolerances-v2/host-automation-ribbon-release-modes-v1.json',str(out),str(rate),mode]
  with (out/'run.log').open('w') as log:status=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT).returncode
  runs.append(dict(rate=rate,mode=mode,command=cmd,exit=status,log=str(out/'run.log')))
  Path('build/matrix-runs.json').write_text(json.dumps(runs,indent=2)+'\n');print(rate,mode,'exit',status,flush=True)
  if status:raise SystemExit('Failure retained; remaining runs not started.')
  files=sorted(out.glob('*.pcm'));assert len(files)==36
  captures += [dict(rate=rate,mode=mode,path=str(p),bytes=p.stat().st_size,sha256=sha(p)) for p in files]
assert len(captures)==216
Path('build/capture-identities.json').write_text(json.dumps(dict(count=len(captures),baseline_comparison=False,captures=captures),indent=2)+'\n')
assert all(sha(Path(p))==h for p,h in inputs.items())
print('PASS:6 runs,216 capture identities; no baseline comparison;inputs unchanged',flush=True)
