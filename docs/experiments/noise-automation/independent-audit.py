from pathlib import Path
import json,hashlib,struct,math
R=Path('/workspace/scratch/ee447c520988/cs80-recovery/yamaha-cs80-noise-automation')
E=R/'docs/experiments/noise-automation'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):
 raw=p.read_bytes();assert len(raw)==262144,p
 v=struct.unpack('<65536f',raw);assert all(math.isfinite(x) for x in v),p
 return raw,v
def difference(a,b):assert len(a)==len(b);return max(abs(x-y) for x,y in zip(a,b))
def effect(a,b):return difference(a[1][2048:57344],b[1][2048:57344])
B=R.parent/'noise-frozen116'
assert digest(B/'baseline116.so')=='161c8679b4136d5cbbc65ce1758d0de1c188c7cee9d066608297a97568019098'
assert digest(B/'tracked-identities.json')=='659df23b70f709671723c92d719ac6a5667cf709429b0e45880bde34c6a6f074'
inventory=json.loads((B/'tracked-identities.json').read_text());assert len(inventory)==5625
changed=[]
for path,h in inventory.items():
 assert digest(B/path)==h,path
 if digest(R/path)!=h:changed.append(path)
assert set(changed)=={'CMakeLists.txt','src/plugin/HostAutomationBridge.h','tests/Vst3ArpRateAutomationTests.cpp'}
for name in ('Vst3NoiseAutomationTests.cpp','Vst3Frozen116Roster.h'):assert digest(B/'tests'/name)==digest(R/'tests'/name)
freeze=R/'tests/noise-source-freeze.json'
assert len(json.loads(freeze.read_text()))==11
for path,h in json.loads(freeze.read_text()).items():assert digest(R/path)==h,path
stages=['build-candidate','build-baseline']+[f'run-{sr}-{mode}' for sr in (44100,48000,96000) for mode in ('realtime','offline')]+['collect']
for name in stages:
 j=json.loads((E/name/'complete.json').read_text());assert j['source_freeze_sha256']==digest(freeze)
 for key in ('evidence_hashes','binaries','capture_hashes'):
  for path,h in j.get(key,{}).items():assert digest(Path(path))==h,path
no_note_effects=[]
rows=[];seen=set();pairs=scalar=order=holds=inert=0;effects=[];noise_effects=[]
common=['noqueue','null','preceding116','legacy-prior','pure-prior']
baseSuffix=common+['legacy','legacy-flat','pure','pure-noise-held','no-notes','no-notes-noise-held','scalar','constant']+[f'block{x}' for x in (7,64,256,1024)]
for sr in (44100,48000,96000):
 for mode in ('realtime','offline'):
  a=R/'build-native'/f'noise-candidate-{sr}-{mode}'
  b=R/'build-noise-helpers'/f'noise-baseline-{sr}-{mode}'
  names=set();bnames=set()
  for case in (0,1):
   stem=f'noise-case{case}-'
   suffix=baseSuffix+([f'reverse{x}' for x in (7,64,256,1024)]+['held-lane0','held-lane1'] if case else [])
   data={}
   for name in suffix:
    p=a/(stem+name+'.pcm');names.add(p.name);data[name]=read(p);seen.add(str(p))
   for name in common:
    p=b/(stem+name+'.pcm');bnames.add(p.name);raw,v=read(p);assert raw==data[name][0];seen.add(str(p));pairs+=1
   for x,y in [('noqueue','null'),('legacy','legacy-flat'),('pure','pure-noise-held')]:assert data[x][0]==data[y][0];inert+=1
   assert data['scalar'][0][:8192]==data['constant'][0][:8192];holds+=1
   errors=[]
   for block in (7,64,256,1024):
    err=difference(data['scalar'][1],data[f'block{block}'][1]);assert err<=1e-8;errors.append(err);scalar+=1
    if case:assert data[f'block{block}'][0]==data[f'reverse{block}'][0];order+=1
   quiet=effect(data['no-notes'],data['no-notes-noise-held']);assert quiet>1e-7;no_note_effects.append(quiet)
   overall=effect(data['scalar'],data['constant']);assert overall>1e-7;effects.append(overall)
   lanes=[]
   if case:
    for i in (0,1):
     val=effect(data['scalar'],data[f'held-lane{i}']);assert val>1e-7;lanes.append(val)
    noise_effects.append(lanes[0])
   else:noise_effects.append(overall)
   rows.append(dict(config=f'run-{sr}-{mode}',scenario=case,max_error=max(errors),held_effect=overall,no_note_effect=quiet,per_lane_effects=lanes))
  assert {p.name for p in a.glob('*.pcm')}==names
  assert {p.name for p in b.glob('*.pcm')}==bnames
assert inert==36 and len(no_note_effects)==12
j=json.loads((E/'collect/complete.json').read_text())
assert sorted(rows,key=lambda x:(x['config'],x['scenario']))==sorted(j['recomputed_metrics'],key=lambda x:(x['config'],x['scenario']))
assert (len(seen),pairs,scalar,order,holds)==(300,60,48,24,12)
manifest=json.loads((E/'collect/captures.json').read_text());assert set(manifest)==seen
for path,h in manifest.items():assert digest(Path(path))==h
print(json.dumps(dict(captures=len(seen),pairs=pairs,scalar=scalar,order=order,holds=holds,control_pairs=inert,max_scalar=max(x['max_error']for x in rows),effect_range=[min(effects),max(effects)],noise_effect_range=[min(noise_effects),max(noise_effects)],no_note_witnesses=len(no_note_effects),no_note_effect_range=[min(no_note_effects),max(no_note_effects)],baseline_inventory=len(inventory),source_hashes=11,stage_count=len(stages),collector_metrics_exact=True),indent=2))
