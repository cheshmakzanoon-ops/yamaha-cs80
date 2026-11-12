from pathlib import Path
import hashlib,json,subprocess,sys
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sdk=Path('../yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK')
files=subprocess.check_output(['git','diff','--name-only'],text=True).splitlines()+['tests/Vst3ReleaseFloorInteractionTests.cpp','tests/Vst3AutomationTests.cpp','tests/parity/tolerances-v2/host-automation-release-floor-interaction-v1.json','tests/parity/tolerances-v2/host-automation.json']
files += subprocess.check_output(['git','ls-files','--others','--exclude-standard'],text=True).splitlines()
files=sorted(set(x for x in files if x!='docs/STATUS.md'))
compiler=Path(subprocess.check_output(['which','g++'],text=True).strip()).resolve();python=Path(sys.executable).resolve()
v=dict(timing='Before compilation',sources={p:sha(Path(p)) for p in files},sdk={str(p):sha(p) for p in sorted(sdk.rglob('*')) if p.is_file()},compiler=dict(path=str(compiler),sha256=sha(compiler),version=subprocess.check_output(['g++','--version'],text=True)),python=dict(path=str(python),sha256=sha(python),version=sys.version))
Path('build/provenance-before.json').write_text(json.dumps(v,indent=2)+'\n')
