from pathlib import Path
import json,hashlib,struct,math
R=Path('/workspace/scratch/ee447c520988/cs80-recovery/yamaha-cs80-drift-automation')
E=R/'docs/experiments/drift-automation'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):
 raw=p.read_bytes();assert len(raw)==262144,p
 v=struct.unpack('<65536f',raw);assert all(math.isfinite(x) for x in v),p
 return raw,v
def difference(a,b):assert len(a)==len(b);return max(abs(x-y) for x,y in zip(a,b))
def effect(a,b):return difference(a[1][2048:57344],b[1][2048:57344])
B=R.parent/'drift-frozen117'
assert digest(B/'baseline117.so')=='1cd4bd75a6450d2649102a4669eceaeb9e0c1f84234e22081d0373d6e7c390a9'
assert digest(B/'tracked-identities.json')=='bc9d8261c71c0f9aade26e9611ebb9104f7f8c357f0b304690f6879460e4ac75'
inventory=json.loads((B/'tracked-identities.json').read_text());assert len(inventory)==5673
changed=[]
for path,h in inventory.items():
 assert digest(B/path)==h,path
 if digest(R/path)!=h:changed.append(path)
assert set(changed)=={'CMakeLists.txt','src/plugin/HostAutomationBridge.h','tests/Vst3NoiseAutomationTests.cpp'}
for name in ('Vst3DriftAutomationTests.cpp','Vst3Frozen117Roster.h'):assert digest(B/'tests'/name)==digest(R/'tests'/name)
freeze=R/'tests/drift-source-freeze.json'
assert len(json.loads(freeze.read_text()))==11
for path,h in json.loads(freeze.read_text()).items():assert digest(R/path)==h,path
stages=['build-candidate','build-baseline']+[f'run-{sr}-{mode}' for sr in (44100,48000,96000) for mode in ('realtime','offline')]+['collect']
for name in stages:
 j=json.loads((E/name/'complete.json').read_text());assert j['source_freeze_sha256']==digest(freeze)
 for key in ('evidence_hashes','binaries','capture_hashes'):
  for path,h in j.get(key,{}).items():assert digest(Path(path))==h,path
model_rows=[]
rows=[];seen=set();pairs=scalar=order=holds=inert=0;effects=[];drift_effects=[]
common=['noqueue','null','preceding117','legacy-prior','pure-prior']
baseSuffix=common+['legacy','legacy-flat','pure','pure-drift-held','scalar','constant']+[f'block{x}' for x in (7,64,256,1024)]
for sr in (44100,48000,96000):
 for mode in ('realtime','offline'):
  a=R/'build-native'/f'drift-candidate-{sr}-{mode}'
  b=R/'build-drift-helpers'/f'drift-baseline-{sr}-{mode}'
  names=set();bnames=set();model_scalars=[]
  for case in range(4):
   stem=f'drift-case{case}-'
   suffix=baseSuffix+([f'reverse{x}' for x in (7,64,256,1024)]+['held-lane0','held-lane1'] if case%2 else [])
   data={}
   for name in suffix:
    p=a/(stem+name+'.pcm');names.add(p.name);data[name]=read(p);seen.add(str(p))
   for name in common:
    p=b/(stem+name+'.pcm');bnames.add(p.name);raw,v=read(p);assert raw==data[name][0];seen.add(str(p));pairs+=1
   for x,y in [('noqueue','null'),('legacy','legacy-flat'),('pure','pure-drift-held')]:assert data[x][0]==data[y][0];inert+=1
   assert data['scalar'][0][:8192]==data['constant'][0][:8192];holds+=1
   errors=[]
   for block in (7,64,256,1024):
    err=difference(data['scalar'][1],data[f'block{block}'][1]);assert err<=1e-8;errors.append(err);scalar+=1
    if case%2:assert data[f'block{block}'][0]==data[f'reverse{block}'][0];order+=1
   model_scalars.append(data['scalar'])
   overall=effect(data['scalar'],data['constant']);assert overall>1e-6;effects.append(overall)
   lanes=[]
   if case%2:
    for i in (0,1):
     val=effect(data['scalar'],data[f'held-lane{i}']);assert val>1e-6;lanes.append(val)
    drift_effects.append(lanes[0])
   else:drift_effects.append(overall)
   rows.append(dict(config=f'run-{sr}-{mode}',scenario=case,max_error=max(errors),held_effect=overall,per_lane_effects=lanes))
  for i in range(2):
   model_delta=effect(model_scalars[i],model_scalars[i+2]);assert model_delta>1e-6
   model_rows.append(dict(config=f'run-{sr}-{mode}',paired_scenario=i,model_effect=model_delta))
  assert {p.name for p in a.glob('*.pcm')}==names
  assert {p.name for p in b.glob('*.pcm')}==bnames
assert inert==72 and len(model_rows)==12
j=json.loads((E/'collect/complete.json').read_text())
assert sorted(model_rows,key=lambda x:(x['config'],x['paired_scenario']))==sorted(j['model_metrics'],key=lambda x:(x['config'],x['paired_scenario']))
assert sorted(rows,key=lambda x:(x['config'],x['scenario']))==sorted(j['recomputed_metrics'],key=lambda x:(x['config'],x['scenario']))
assert (len(seen),pairs,scalar,order,holds)==(552,120,96,48,24)
manifest=json.loads((E/'collect/captures.json').read_text());assert set(manifest)==seen
for path,h in manifest.items():assert digest(Path(path))==h
print(json.dumps(dict(captures=len(seen),pairs=pairs,scalar=scalar,order=order,holds=holds,control_pairs=inert,max_scalar=max(x['max_error']for x in rows),effect_range=[min(effects),max(effects)],drift_effect_range=[min(drift_effects),max(drift_effects)],model_witnesses=len(model_rows),model_effect_range=[min(x["model_effect"] for x in model_rows),max(x["model_effect"] for x in model_rows)],baseline_inventory=len(inventory),source_hashes=11,stage_count=len(stages),collector_metrics_exact=True),indent=2))
