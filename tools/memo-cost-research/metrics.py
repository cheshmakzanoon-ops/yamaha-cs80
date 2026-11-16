import csv,json,math,statistics,hashlib
from pathlib import Path
KEY=('mode','scenario','initial_selector','phase')
ORDERS=('ABC','ACB','BAC','BCA','CAB','CBA')
def inventory(low):
 r=set()
 for sel in ('0','1','10'):
  for mode in ('legacy','isolation'):
   for phase in ('warmed_held','release_tail'):r.add((mode,'paired_tones',sel,phase))
  if not low:
   for scenario in ('paired_tones','mpe_chorus_reverb','mpe_phaser_delay_drive'):
    for phase in ('warmed_held','release_tail')+(() if scenario=='paired_tones' else ('sustain_pedal_hold',)):r.add(('source_performance',scenario,sel,phase))
 if low:
  for mode in ('legacy','isolation'):
   for phase in ('warmed_held','release_tail'):r.add((mode,'moving_tabs_pressure_wah','0',phase))
   r.add((mode,'idle_quiet','0','warmed_held'))
  r.add(('legacy','idle_noise','0','warmed_held'))
 return r
def read(path,low):
 with (path/'results.csv').open() as f:rows=list(csv.DictReader(f))
 r={tuple(x[k] for k in KEY):x for x in rows};assert len(rows)==len(r) and set(r)==inventory(low)
 assert json.loads((path/'completed.json').read_text())==dict(completed=True,cases=11 if low else 15,rows=19 if low else 36)
 for x in rows:
  assert float(x['rate'])==48000 and int(x['block'])==128 and int(x['nonfinite_count'])==0
  assert x['callback_cpu_status']=='available' and int(x['callback_cpu_call_count'])==int(x['callbacks'])>0
  if x['phase']=='warmed_held':assert int(x['callbacks'])==(512 if low else 256)
  for k in ('callback_cpu_seconds','callback_cpu_rtf','audio_seconds'):assert math.isfinite(float(x[k])) and float(x[k])>0
 return r
def check(a,b):
 assert set(a)==set(b)
 for k in a:
  for field in ('rate','block','checksum','callbacks','audio_seconds','callback_cpu_call_count','nonfinite_count'):assert a[k][field]==b[k][field],(k,field)
def groups(keys,low):
 if low:
  r={'isolation_held':[k for k in keys if k[0]=='isolation' and k[3]=='warmed_held' and not k[1].startswith('idle_')]};assert len(r['isolation_held'])==4
 else:
  r={mode+'_dry_held':[k for k in keys if k[0]==mode and k[1]=='paired_tones' and k[3]=='warmed_held'] for mode in ('source_performance','isolation')};assert all(len(x)==3 for x in r.values())
 return r
def cpu(rows):return sum(float(x['callback_cpu_seconds']) for x in rows.values())
def audio(rows):return sum(float(x['audio_seconds']) for x in rows.values())
def ratios(a,b,low):
 check(a,b);r={'total':(cpu(b)/audio(b))/(cpu(a)/audio(a))}
 for name,keys in groups(a,low).items():r[name]=statistics.median(float(b[k]['callback_cpu_seconds'])/float(a[k]['callback_cpu_seconds']) for k in keys)
 return r
def stat(values,first=None):
 r={'ratios':values,'median':statistics.median(values),'minimum':min(values),'maximum':max(values),'below_one':sum(x<1 for x in values)}
 if first is not None:r['relative_order_medians']={n:statistics.median(v for v,f in zip(values,first) if f==b) for n,b in [('numerator_first',True),('denominator_first',False)]}
 return r
def calibration(p,low):
 suite='lowload' if low else 'active';samples=[];reference=None
 for i in range(1,5):
  a,b=[read(p/f'calibration-{suite}-{i}-{arm}',low) for arm in ('left','right')]
  for x in (a,b):
   if reference is None:reference=x
   else:check(reference,x)
  samples.append((a,b))
 agg={n:stat([ratios(a,b,low)[n] for a,b in samples]) for n in ratios(*samples[0],low)}
 cases={str(k):stat([float(b[k]['callback_cpu_seconds'])/float(a[k]['callback_cpu_seconds']) for a,b in samples]) for k in reference}
 usable=all(.98<=v<=1.02 for s in agg.values() for v in s['ratios']) and all(.9<=v<=1.1 for s in cases.values() for v in s['ratios'])
 return {'suite':suite,'usable':usable,'aggregates':agg,'E':{n:max(abs(x-1) for x in s['ratios']) for n,s in agg.items()},'cases':cases}
def loss(s,row=False):
 m=s['median'];v=s['ratios'];limit=1.10 if row else 1.05
 if m>limit and sum(x>(1.05 if row else 1) for x in v)>=5:return 'hard'
 if (m>(1.05 if row else 1.03) and sum(x>1 for x in v)>=5) or m>limit:return 'caution'
 return None
def analyze(p,low,cal):
 assert cal['usable'];suite='lowload' if low else 'active';samples=[];ref=None;hashes={}
 for i,order in enumerate(ORDERS,1):
  run={a:read(p/f'{suite}-{i}-{a}',low) for a in 'ABC'}
  for arm,x in run.items():
   if ref is None:ref=x
   else:check(ref,x)
   f=p/f'{suite}-{i}-{arm}/results.csv';hashes[str(f.relative_to(p))]=hashlib.sha256(f.read_bytes()).hexdigest()
  samples.append(run)
 out={'suite':suite,'orders':ORDERS,'contrasts':{},'csv_hashes':hashes,'material_signal':False,'hard_losses':[],'cautions':[]}
 for numerator,denominator in (('C','A'),('C','B'),('B','A')):
  label=numerator+'/'+denominator;first=[o.index(numerator)<o.index(denominator) for o in ORDERS]
  aggs={n:stat([ratios(r[denominator],r[numerator],low)[n] for r in samples],first) for n in ratios(samples[0][denominator],samples[0][numerator],low)}
  cases={str(k):stat([float(r[numerator][k]['callback_cpu_seconds'])/float(r[denominator][k]['callback_cpu_seconds']) for r in samples],first) for k in ref}
  absolute={arm:[{'summed_cpu':cpu(r[arm]),'summed_audio':audio(r[arm]),'cpu_audio':cpu(r[arm])/audio(r[arm])} for r in samples] for arm in 'ABC'}
  out['contrasts'][label]={'aggregates':aggs,'cases':cases,'absolute':absolute}
  for name,s in aggs.items():
   l=loss(s)
   if l:out['hard_losses' if l=='hard' else 'cautions'].append([label,name])
  for name,s in cases.items():
   l=loss(s,True)
   if l:out['hard_losses' if l=='hard' else 'cautions'].append([label,name])
 if not low:
  out['material_signal']=not out['hard_losses'] and not out['cautions'] and all(s['median']<=.98 and 1-s['median']>2*cal['E'][name] and s['below_one']>=5 and all(x<1 for x in s['relative_order_medians'].values()) for label in ('C/A','C/B') for name,s in out['contrasts'][label]['aggregates'].items() if name!='total')
 return out
