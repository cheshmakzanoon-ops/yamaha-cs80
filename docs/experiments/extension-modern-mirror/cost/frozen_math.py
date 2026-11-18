"""Exact mathematical functions from the frozen study; no clock/process/DSP operations."""
from pathlib import Path
import json,math,statistics
ROOT=Path(__file__).resolve().parent
N=131072
EPS=1e-10

def read(p):return json.loads(Path(p).read_text(),parse_constant=lambda x:(_ for _ in ()).throw(ValueError('nonfinite JSON')))

def need(x,why):
 if not x:raise ValueError(why)

def expected(scene,extra):
 d=read(ROOT/'untimed'/f'{scene}-{"extra" if extra else "plain"}-stock.json');x=json.loads(d['stdout'])
 x.update(timed=True,main_calls=N,shadow_calls=N//8 if extra else 0,clock_reads=16);return x

def validate_output(o,slot):
 ref=expected(slot['scene'],slot['extra'])
 need(set(o)==set(ref),'wrong output schema')
 for k in ref:
  if k not in ('timing','empty_clocks'):need(o[k]==ref[k],'identity/count/FP mismatch '+k)
 need(len(o['empty_clocks'])==3,'wrong overhead count')
 previous_cpu=previous_wall=0
 for t in o['empty_clocks']+[o['timing']]:
  need(set(t)=={'cpu_begin_ns','cpu_end_ns','wall_begin_ns','wall_end_ns'},'wrong clock schema')
  need(all(type(x) is int and x>0 for x in t.values()),'noninteger/nonpositive raw clock')
  need(previous_cpu<=t['cpu_begin_ns']<t['cpu_end_ns'] and previous_wall<=t['wall_begin_ns']<t['wall_end_ns'],'clock ordering')
  previous_cpu=t['cpu_end_ns'];previous_wall=t['wall_end_ns']
 return o

def metrics(o):
 t=o['timing'];cpu=t['cpu_end_ns']-t['cpu_begin_ns'];wall=t['wall_end_ns']-t['wall_begin_ns'];empty=[x['cpu_end_ns']-x['cpu_begin_ns'] for x in o['empty_clocks']]
 return {'cpu_ns':cpu,'wall_ns':wall,'ns_per_main_call':cpu/N,'wall_ns_per_main_call':wall/N,'empty_cpu_ns':empty,'overhead_pass':max(empty)*200<cpu and sum(empty)*1000<len(empty)*cpu}

def analyze_data(plan,observations):
 need(len(observations)==80,'complete eighty observations required')
 ps=[metrics(validate_output(o,s)) for o,s in zip(observations,plan['slots'])];batches=[];failures=[]
 for b in plan['batches']:
  ids=b['slots'];a=[ps[i]['ns_per_main_call'] for i in ids if plan['slots'][i]['label']=='A'];z=[ps[i]['ns_per_main_call'] for i in ids if plan['slots'][i]['label']=='B'];w=[ps[i]['wall_ns_per_main_call'] for i in ids if plan['slots'][i]['label']=='A'];v=[ps[i]['wall_ns_per_main_call'] for i in ids if plan['slots'][i]['label']=='B']
  need(len(a)==len(z)==2,'ABBA labels');l=statistics.mean(map(math.log,z))-statistics.mean(map(math.log,a));effect=100*math.expm1(l)
  entry={**b,'log_B_over_A':l,'effect_percent':effect,'A_ns_per_call':statistics.mean(a),'B_ns_per_call':statistics.mean(z),'wall_A_ns_per_call':statistics.mean(w),'wall_B_ns_per_call':statistics.mean(v)}
  if b['kind']=='sham' and abs(effect)>3+EPS:failures.append('sham batch '+str(b['id']))
  if b['kind']=='positive':
   ratio=math.exp(l if b['extra_label']=='B' else -l);entry['extra_over_ordinary']=ratio
   if not 1.08-EPS<=ratio<=1.18+EPS:failures.append('positive batch '+str(b['id']))
  if b['kind']=='compare':
   d=statistics.mean(a)-statistics.mean(z);entry.update(stock_minus_candidate_ns=d,hot_assumption_ms_per_audio_second=d*750/1e6)
  batches.append(entry)
 for scene in ('default','dense'):
  sh=[b['log_B_over_A'] for b in batches if b['scene']==scene and b['kind']=='sham'];need(len(sh)==3,'three shams')
  if abs(100*math.expm1(statistics.mean(sh)))>1.5+EPS:failures.append('mean sham '+scene)
 for i,p in enumerate(ps):
  if not p['overhead_pass']:failures.append('clock overhead slot '+str(i))
 summary={}
 for scene in ('default','dense'):
  bs=[b for b in batches if b['scene']==scene and b['kind']=='compare'];ds=[b['stock_minus_candidate_ns'] for b in bs]
  summary[scene]={'independent_batches':len(bs),'stock_ns_per_mirror_batch_means':[b['A_ns_per_call'] for b in bs],'candidate_ns_per_mirror_batch_means':[b['B_ns_per_call'] for b in bs],'stock_minus_candidate_ns_all':ds,'median_difference_ns':statistics.median(ds),'range_difference_ns':[min(ds),max(ds)],'median_hot_assumption_ms_per_audio_second':statistics.median(ds)*750/1e6}
 return {'scope':'Settled-hot genuine production mirror method plus loop/call overhead ONLY; not whole-plugin, callback-separated, DAW or real-time performance. 750 calls/s conversion is an assumption-based arithmetic scale.','control_pass':not failures,'failures':failures,'processes':ps,'batches':batches,'summary':summary,'no_exclusions':True}
