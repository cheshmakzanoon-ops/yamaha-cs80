#!/usr/bin/env python3
"""Reproduce A13 actual-program captures; PCM stays outside the repository."""
import concurrent.futures,csv,json,pathlib,subprocess,time
ROOT=pathlib.Path(__file__).resolve().parents[5]
TMP=pathlib.Path('/tmp/cs80-a13'); OUT=pathlib.Path(__file__).resolve().parent
TMP.mkdir(parents=True,exist_ok=True)
subprocess.run(['/tmp/cs80-build-1cee5a/cs80_preset_capture','--library','all','--inventory',str(TMP/'current-inventory.json')],check=True)
before=json.loads((ROOT/'docs/preset-evidence/2026.11.7/development/baseline-inventory.json').read_text())
after=json.loads((TMP/'current-inventory.json').read_text()); ids=after['parameter_ids']
keys=[p for p in after['programs'] if p['category']=='Keys']
pianos=[p['name'] for p in keys if any(dict(zip(ids,p['values']))[f'body{r}_model']==1 for r in (1,2))]
assert len(keys)==45 and len(pianos)==10
jobs=[]
for note,hold in [(36,12),(60,6)]:
 jobs.append((f'pianos-n{note}-firm',pianos,note,.85,hold,1.2))
 jobs.append((f'pianos-n{note}-soft',['Stage Keys','Soft Keys'],note,.25,hold,1.2))
 jobs.append((f'pianos-n{note}-early',['Stage Keys','Soft Keys'],note,.85,.2,2))
for vel in (.25,.85):
 jobs.append((f'ep-n60-v{vel}',['Electric Piano I','Electric Piano II','Tine Piano','Reed Piano'],60,vel,3,1))
manifest=[]
for side,exe in [('before','/tmp/cs80-reference-1cee5a/cs80_preset_capture'),('after','/tmp/cs80-build-1cee5a/cs80_preset_capture')]:
 for label,names,note,vel,hold,tail in jobs:
  folder=TMP/side/label
  args=[exe,'--library','all','--out',str(folder),'--wav','--rate','48000','--note',str(note),'--velocity',str(vel),'--hold',str(hold),'--tail',str(tail)]
  for name in names: args+=['--preset',name]
  manifest.append(dict(side=side,label=label,names=names,note=note,velocity=vel,hold=hold,tail=tail,command=args))
def run(j):
 t=time.time();r=subprocess.run(j['command'],capture_output=True,text=True)
 j.update(exit_code=r.returncode,seconds=time.time()-t)
 (TMP/(j['side']+'-'+j['label']+'.log')).write_text(r.stdout+r.stderr)
 print(j['side'],j['label'],r.returncode,round(j['seconds'],2),flush=True)
 return j
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool: finished=list(pool.map(run,manifest))
(OUT/'capture-manifest.json').write_text(json.dumps(finished,indent=2)+'\n')
rows=[]
for j in finished:
 for row in csv.DictReader(open(TMP/j['side']/j['label']/'metrics.csv')): rows.append(dict(side=j['side'],case=j['label'],**row))
with open(OUT/'capture-metrics.csv','w') as f:
 w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
assert all(j['exit_code']==0 for j in finished)
