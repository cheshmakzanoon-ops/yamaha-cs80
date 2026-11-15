import json,re,hashlib
from pathlib import Path
p=Path(__file__).parent;ts=json.load(open(p/'nonwindow-inventory.json'))['tests'];out=[]
for t in ts:
 command=t.get('command',[])
 for name in command:
  f=Path(name)
  if f.suffix not in ('.py','.sh','.cmake') or not f.exists():continue
  text=f.read_text();marks=[{'line':i,'text':line.strip()} for i,line in enumerate(text.splitlines(),1) if re.search(r'TemporaryDirectory|mkdtemp|mktemp|exist_ok|\.exists\(|mkdir\(|rmtree|FileExists|--out|--output|remove_all',line)]
  if 'TemporaryDirectory' in text or 'mktemp -d' in text:kind='unique temporary root present; inspected registered arguments'
  elif f.name=='ExpectFixtureInputRejection.cmake':kind='read-only fixed missing/truncated input rejection; no output path'
  elif f.name in ('test_memory_verified_diode.py','test_memory_centered_log_rational.py','test_memory_interval_extrema.py'):kind='read-only numeric oracle, child stdin/stdout; no generated output directory'
  elif f.name=='run-pluginval-linux.sh':kind='argument to retry test, whose outer shell creates unique mktemp root'
  else:kind='manual review required'
  out.append({'test':t['name'],'script':str(f),'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'classification':kind,'markers':marks,'command':command})
(p/'wrapper-path-audit.json').write_text(json.dumps({'selected_inventory_tests':len(ts),'script_entries':len(out),'unique_scripts':len({x['script'] for x in out}),'entries':out},indent=2)+'\n')
from collections import Counter
print(len(ts),len(out),len({x['script'] for x in out}));print(Counter(x['classification'] for x in out))
