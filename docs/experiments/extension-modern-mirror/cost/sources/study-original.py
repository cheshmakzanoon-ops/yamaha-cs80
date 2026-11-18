#!/usr/bin/env python3
"""Frozen isolated-method collector and offline analyzer. Timing requires authorization."""
from pathlib import Path
import argparse,hashlib,json,math,os,resource,signal,statistics,subprocess,sys,time
ROOT=Path(__file__).resolve().parent
N=131072
EPS=1e-10

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text(),parse_constant=lambda x:(_ for _ in ()).throw(ValueError('nonfinite JSON')))
def write(p,x):
 with Path(p).open('x') as f:json.dump(x,f,sort_keys=True,indent=2,allow_nan=False);f.write('\n');f.flush();os.fsync(f.fileno())
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

def verify_freeze():
 f=read(ROOT/'freeze.json')
 for p,h in f['files'].items():need(sha(ROOT/p)==h,'artifact drift '+p)
 for p,h in f['external_inputs'].items():need(sha(Path(p))==h,'external input drift '+p)
 need(f['untimed_pass'] is True,'untimed gate');return f

def collect(approval):
 f=verify_freeze();plan=read(ROOT/'plan.json');auth=read(approval)
 need(auth=={'action':'isolated-modern-mirror-cost-v1','freeze_sha256':sha(ROOT/'freeze.json'),'plan_sha256':sha(ROOT/'plan.json'),'exclusive_approved':True},'explicit exclusive approval does not bind this study')
 authhash=sha(approval);need(not (ROOT/'campaign').exists(),'existing campaign; no resume')
 write(ROOT/'trial-consumed.json',{'authorization_sha256':authhash,'freeze_sha256':sha(ROOT/'freeze.json'),'one_shot':True});campaign=ROOT/'campaign';campaign.mkdir();write(campaign/'plan.json',plan);write(campaign/'authorization.json',auth)
 def cpu():
  a=resource.getrusage(resource.RUSAGE_SELF);b=resource.getrusage(resource.RUSAGE_CHILDREN);return a.ru_utime+a.ru_stime+b.ru_utime+b.ru_stime
 def stop(s,f):raise InterruptedError('interrupted by signal '+str(s))
 signal.signal(signal.SIGINT,stop);signal.signal(signal.SIGTERM,stop)
 start=time.monotonic_ns();c0=cpu();completed=0;failure=None
 ledger=(campaign/'ledger.jsonl').open('x')
 def log(x):ledger.write(json.dumps(x,sort_keys=True)+'\n');ledger.flush();os.fsync(ledger.fileno())
 log({'event':'start','authorization_sha256':authhash,'wall_ns':start})
 try:
  for slot in plan['slots']:
   need(cpu()-c0+3<=120,'CPU budget reserve exhausted');remain=180-(time.monotonic_ns()-start)/1e9;need(remain>0,'wall budget exhausted');verify_freeze()
   sid=slot['id'];args=[str(ROOT/slot['image']),'--time',slot['scene'],'extra' if slot['extra'] else 'plain',authhash];env=os.environ.copy();env['MIRROR_COST_AUTH']=authhash
   receipt={'slot':slot,'argv':args,'binary_sha256':sha(ROOT/slot['image'])};child=None;before=time.monotonic_ns();u0=resource.getrusage(resource.RUSAGE_CHILDREN);log({'event':'slot-start','slot':sid,'wall_ns':before})
   def limits():resource.setrlimit(resource.RLIMIT_CPU,(3,3));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
   out=err='';code=None;error=None
   try:
    child=subprocess.Popen(args,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,preexec_fn=limits,start_new_session=True)
    out,err=child.communicate(timeout=min(5,remain));code=child.returncode
   except BaseException as e:
    error=str(e)
    if child is not None:
     try:os.killpg(child.pid,signal.SIGKILL)
     except ProcessLookupError:pass
     out,err=child.communicate();code=child.returncode
   end=time.monotonic_ns();u1=resource.getrusage(resource.RUSAGE_CHILDREN);used=u1.ru_utime+u1.ru_stime-u0.ru_utime-u0.ru_stime
   receipt.update(pid=None if child is None else child.pid,exit=code,error=error,wall_begin_ns=before,wall_end_ns=end,process_cpu_ns=round(used*1e9),stdout=out,stderr=err)
   try:
    need(error is None and code==0 and not err,'process failed');need(used<=3 and end-before<=5e9,'process budget');validate_output(json.loads(out),slot);verify_freeze();receipt['integrity']='PASS'
   except Exception as e:receipt['integrity']='FAIL';receipt['integrity_error']=str(e)
   path=campaign/f'slot-{sid:02d}.json';write(path,receipt);log({'event':'slot-end','slot':sid,'receipt_sha256':sha(path),'integrity':receipt['integrity']})
   need(receipt['integrity']=='PASS','slot integrity failed; trial consumed');completed+=1
   need(cpu()-c0<=120 and time.monotonic_ns()-start<=180e9,'whole budget exhausted')
 except BaseException as e:failure=str(e)
 final={'status':'complete' if completed==80 and failure is None else 'failed','completed_slots':completed,'error':failure,'wall_elapsed_ns':time.monotonic_ns()-start,'cpu_elapsed_ns':round((cpu()-c0)*1e9),'no_resume':True};log({'event':'terminal',**final});ledger.close();write(campaign/'terminal.json',final);print(json.dumps(final));return 0 if final['status']=='complete' else 1

def replay():
 verify_freeze();p=ROOT/'campaign';t=read(p/'terminal.json');need(t['status']=='complete' and t['completed_slots']==80 and t['no_resume'] is True,'complete one-shot required');plan=read(ROOT/'plan.json');need(read(p/'plan.json')==plan,'plan copy drift');need(len(list(p.glob('slot-*.json')))==80,'exactly80 slots')
 observations=[]
 for slot in plan['slots']:
  r=read(p/f"slot-{slot['id']:02d}.json");need(r['slot']==slot and r['integrity']=='PASS' and r['binary_sha256']==sha(ROOT/slot['image']),'receipt drift');observations.append(json.loads(r['stdout']))
 result=analyze_data(plan,observations);result.update(terminal=t,freeze_sha256=sha(ROOT/'freeze.json'),plan_sha256=sha(ROOT/'plan.json'));write(p/'analysis.json',result);print(json.dumps({'control_pass':result['control_pass'],'summary':result['summary'],'failures':result['failures']},indent=2))

if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('mode',choices=['ready','collect','analyze']);ap.add_argument('--authorization',type=Path);a=ap.parse_args()
 if a.mode=='ready':verify_freeze();print(json.dumps({'ready_for_exclusive_approval':True,'measurement_clock_reads':0,'freeze_sha256':sha(ROOT/'freeze.json'),'plan_sha256':sha(ROOT/'plan.json')}))
 elif a.mode=='collect':need(a.authorization is not None,'authorization required');sys.exit(collect(a.authorization))
 else:replay()
