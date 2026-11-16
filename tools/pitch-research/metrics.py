import csv,json,math,statistics,hashlib
from pathlib import Path
KEY=('mode','scenario','initial_selector','phase')
def read(path,inventory):
 d=json.loads((path/'completed.json').read_text());assert d==dict(completed=True,cases=inventory[0],rows=inventory[1]),d
 with (path/'results.csv').open() as f:a=list(csv.DictReader(f))
 r={tuple(x[k] for k in KEY):x for x in a};assert len(a)==len(r)==inventory[1]
 for x in a:
  assert float(x['rate'])==48000 and int(x['block'])==128
  assert int(x['nonfinite_count'])==0 and x['callback_cpu_status']=='available'
  for k in ['callback_cpu_seconds','callback_cpu_rtf','audio_seconds']:assert math.isfinite(float(x[k])) and float(x[k])>0
  assert int(x['callback_cpu_call_count'])==int(x['callbacks'])>0
 return r
def check(a,b):
 assert a.keys()==b.keys()
 for k in a:
  for f in ['rate','block','checksum','callbacks','audio_seconds','callback_cpu_call_count','nonfinite_count']:assert a[k][f]==b[k][f],(k,f)
def stat(x):return dict(ratios=x,median=statistics.median(x),minimum=min(x),maximum=max(x),below_one=sum(v<1 for v in x),above={str(t):sum(v>t for v in x) for t in [1,1.03,1.05,1.10]},order_medians={'AB':statistics.median(x[::2]),'BA':statistics.median(x[1::2])})
def analyze(p,stage,count,calibration=None):
 inv=(11,19) if 'lowload' in stage else (15,36);pairs=[];hashes={}
 for i in range(1,count+1):
  a,b=[read(p/f'{stage}-{i}-{v}',inv) for v in ['baseline','candidate']];check(a,b)
  if pairs:check(pairs[0][0],a);check(pairs[0][0],b)
  pairs.append((a,b))
  for v in ['baseline','candidate']:
   f=p/f'{stage}-{i}-{v}/results.csv';hashes[str(f.relative_to(p))]=hashlib.sha256(f.read_bytes()).hexdigest()
 cases=[]
 for k in pairs[0][0]:
  ratios=[float(b[k]['callback_cpu_seconds'])/float(a[k]['callback_cpu_seconds']) for a,b in pairs]
  absolute={v:[{f:float(pair[j][k][f]) for f in ['callback_cpu_seconds','callback_cpu_rtf','audio_seconds']} for pair in pairs] for j,v in enumerate(['baseline','candidate'])}
  cases.append(dict(case=k,ratio=stat(ratios),absolute=absolute))
 total=stat([sum(float(x['callback_cpu_seconds']) for x in b.values())/sum(float(x['callback_cpu_seconds']) for x in a.values()) for a,b in pairs])
 subset=[k for k in pairs[0][0] if k[0]==('isolation' if 'lowload' in stage else 'source_performance') and k[3]=='warmed_held' and not k[1].startswith('idle_')]
 assert len(subset)==(4 if 'lowload' in stage else 9)
 source=stat([statistics.median(float(b[k]['callback_cpu_seconds'])/float(a[k]['callback_cpu_seconds']) for k in subset) for a,b in pairs])
 result=dict(subset_definition='four isolation nonidle held' if 'lowload' in stage else 'nine source_performance held',stage=stage,pairs=count,total=total,source_held=source,cases=cases,csv_hashes=hashes)
 if stage.startswith('calibration'):
  E=max(abs(v-1) for s in [total,source] for v in s['ratios']);ok=all(.98<=v<=1.02 for s in [total,source] for v in s['ratios']) and all(.90<=v<=1.10 for c in cases for v in c['ratio']['ratios'])
  result.update(empirical_aggregate_deviation=E,usable=ok,decision='usable empirical screening guard, not precision proof' if ok else 'calibration unusable; stop without candidate timing')
 else:
  hard=[];caution=[]
  if stage in ['performance','lowload']:
   for name,s in [('total',total),('source_held',source)]:
    if s['median']>1.05 and s['above']['1']>=(3 if stage=='lowload' else 5):hard.append(name)
    elif s['median']>1.03 and s['above']['1']>=(3 if stage=='lowload' else 5) or s['median']>1.05:caution.append(name)
  n=3 if stage=='lowload' else 5
  for c in cases:
   s=c['ratio']
   if s['median']>1.10 and s['above']['1.05']>=n:hard.append(c['case'])
   elif s['median']>1.05 and s['above']['1']>=n or s['median']>1.10:caution.append(c['case'])
  signal=False
  if stage=='performance':
   assert calibration and calibration['usable'];E=calibration['empirical_aggregate_deviation']
   signal=not hard and not caution and all(s['median']<=.98 and 1-s['median']>2*E and s['below_one']>=5 and all(v<1 for v in s['order_medians'].values()) for s in [total,source])
  result.update(hard_losses=hard,cautions=caution,material_signal=signal,decision='manual dispersion review required' if signal else 'no promotion; benefit unestablished or adverse' if stage=='performance' else 'manual low-load review required')
 (p/f'{stage}-analysis.json').write_text(json.dumps(result,indent=2)+'\n');return result
