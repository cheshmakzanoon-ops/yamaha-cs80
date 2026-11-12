from pathlib import Path
import hashlib,json,subprocess
root=Path.cwd();host=root/'build/cs80_vst3_kbd_contour_combined_automation_tests'
module=root/'../yamaha-cs80-kbd-brilliance-automation/build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert Path('build/compile.exit').read_text().strip()=='0'
assert sha(module)=='0a7fe428463fca164b8dd2f561e1c0d6afaafd571c701098665da39aa9ae0c4a'
inputs={str(p):sha(p) for p in [host,module,Path('tests/Vst3KbdContourCombinedAutomationTests.cpp'),Path('tests/Vst3AutomationTests.cpp'),Path('src/plugin/HostAutomationBridge.h')]}
Path('build/runtime-inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
runs=[]
for rate in [44100,48000,96000]:
 for mode in ['realtime','offline']:
  out=Path(f'build/matrix/{rate}-{mode}');out.mkdir(parents=True,exist_ok=False)
  command=[str(host),str(module),'tests/parity/tolerances-v2/host-automation.json','tests/parity/tolerances-v2/host-automation-kbd-contour-combined-v1.json',str(out),str(rate),mode]
  with (out/'run.log').open('w') as log:status=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
  runs.append(dict(rate=rate,mode=mode,command=command,exit=status,log=str(out/'run.log')))
  Path('build/runs.json').write_text(json.dumps(runs,indent=2)+'\n');print(rate,mode,'exit',status,flush=True)
  if status:raise SystemExit('Failure retained; remaining runs not started.')
captures=[]
for run in runs:
 files=sorted(Path(run['log']).parent.glob('*.pcm'));assert len(files)==36
 for p in files:captures.append(dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p)))
assert len(captures)==216
Path('build/capture-inventory.json').write_text(json.dumps(dict(count=216,baseline_comparisons=0,captures=captures),indent=2)+'\n')
assert all(sha(Path(p))==h for p,h in inputs.items())
print('PASS:6 runs,216 capture identities, no baseline comparison',flush=True)
