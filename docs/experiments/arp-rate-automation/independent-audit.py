from pathlib import Path
import json,hashlib,struct,math
R=Path('/workspace/scratch/ee447c520988/cs80-recovery/yamaha-cs80-arp-rate-automation')
E=R/'docs/experiments/arp-rate-automation'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):
 raw=p.read_bytes();assert len(raw)==262144,p
 v=struct.unpack('<65536f',raw);assert all(math.isfinite(x) for x in v),p
 return raw,v
def difference(a,b):assert len(a)==len(b);return max(abs(x-y) for x,y in zip(a,b))
def effect(a,b):return difference(a[1][2048:57344],b[1][2048:57344])
freeze=R/'tests/arp-rate-source-freeze.json'
for path,h in json.loads(freeze.read_text()).items():assert digest(R/path)==h,path
stages=['build-candidate','build-baseline']+[f'run-{sr}-{mode}' for sr in (44100,48000,96000) for mode in ('realtime','offline')]+['collect']
for name in stages:
 j=json.loads((E/name/'complete.json').read_text());assert j['source_freeze_sha256']==digest(freeze)
 for key in ('evidence_hashes','binaries','capture_hashes'):
  for path,h in j.get(key,{}).items():assert digest(Path(path))==h,path
rows=[];seen=set();pairs=scalar=order=holds=inert=0;effects=[];rate_effects=[]
common=['noqueue','null','preceding115','legacy-prior','pure-prior']
baseSuffix=common+['legacy','legacy-flat','pure','pure-rate-held','arp-off','arp-off-rate-held','scalar','constant']+[f'block{x}' for x in (7,64,256,1024)]
for sr in (44100,48000,96000):
 for mode in ('realtime','offline'):
  a=R/'build-native'/f'arp-rate-candidate-{sr}-{mode}'
  b=R/'build-arp-rate-helpers'/f'arp-rate-baseline-{sr}-{mode}'
  names=set();bnames=set()
  for case in (0,1):
   stem=f'arp-case{case}-'
   suffix=baseSuffix+([f'reverse{x}' for x in (7,64,256,1024)]+['held-lane0','held-lane1'] if case else [])
   data={}
   for name in suffix:
    p=a/(stem+name+'.pcm');names.add(p.name);data[name]=read(p);seen.add(str(p))
   for name in common:
    p=b/(stem+name+'.pcm');bnames.add(p.name);raw,v=read(p);assert raw==data[name][0];seen.add(str(p));pairs+=1
   for x,y in [('noqueue','null'),('legacy','legacy-flat'),('pure','pure-rate-held'),('arp-off','arp-off-rate-held')]:assert data[x][0]==data[y][0];inert+=1
   assert data['scalar'][0][:8192]==data['constant'][0][:8192];holds+=1
   errors=[]
   for block in (7,64,256,1024):
    err=difference(data['scalar'][1],data[f'block{block}'][1]);assert err<=2e-6;errors.append(err);scalar+=1
    if case:assert data[f'block{block}'][0]==data[f'reverse{block}'][0];order+=1
   overall=effect(data['scalar'],data['constant']);assert overall>1e-5;effects.append(overall)
   lanes=[]
   if case:
    for i in (0,1):
     val=effect(data['scalar'],data[f'held-lane{i}']);assert val>1e-5;lanes.append(val)
    rate_effects.append(lanes[0])
   else:rate_effects.append(overall)
   rows.append(dict(config=f'run-{sr}-{mode}',scenario=case,max_error=max(errors),held_effect=overall,per_lane_effects=lanes))
  assert {p.name for p in a.glob('*.pcm')}==names
  assert {p.name for p in b.glob('*.pcm')}==bnames
j=json.loads((E/'collect/complete.json').read_text())
assert sorted(rows,key=lambda x:(x['config'],x['scenario']))==sorted(j['recomputed_metrics'],key=lambda x:(x['config'],x['scenario']))
assert (len(seen),pairs,scalar,order,holds)==(300,60,48,24,12)
manifest=json.loads((E/'collect/captures.json').read_text());assert set(manifest)==seen
for path,h in manifest.items():assert digest(Path(path))==h
print(json.dumps(dict(captures=len(seen),pairs=pairs,scalar=scalar,order=order,holds=holds,control_pairs=inert,max_scalar=max(x['max_error']for x in rows),effect_range=[min(effects),max(effects)],rate_effect_range=[min(rate_effects),max(rate_effects)],stage_count=len(stages),collector_metrics_exact=True),indent=2))
