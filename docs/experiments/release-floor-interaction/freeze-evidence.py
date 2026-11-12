from pathlib import Path
import json,hashlib,subprocess
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
e=Path('docs/experiments/release-floor-interaction'); vpath=e/'verification.json';v=json.loads(vpath.read_text())
v['render_count']=936
v['below_floor_cases']=36;v['crossover_cases']=36
v['source_hashes']={p:sha(Path(p)) for p in v['source_hashes']}
vpath.write_text(json.dumps(v,indent=2)+'\n')
manifest={str(p.relative_to(e)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(e.rglob('*')) if p.is_file() and p.name!='manifest.json'}
(e/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
files=subprocess.check_output(['git','diff','--name-only'],text=True).splitlines()+subprocess.check_output(['git','ls-files','--others','--exclude-standard'],text=True).splitlines()
files=sorted(set(p for p in files if p!='docs/STATUS.md'))
Path('build/frozen-release-floor-interaction-files.json').write_text(json.dumps({p:sha(Path(p)) for p in files},indent=2)+'\n')
for p in [Path('build/frozen-release-floor-interaction-files.json'),vpath,Path('docs/VST3_RELEASE_FLOOR_INTERACTION.md')]:print(p,sha(p))
print('files',len(files))
