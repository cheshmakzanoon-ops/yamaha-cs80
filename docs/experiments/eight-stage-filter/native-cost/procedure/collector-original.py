#!/usr/bin/env python3
"""One-shot blind collector. --ready/--self-test never access clocks or run DSP."""
from pathlib import Path
import argparse,hashlib,json,os,resource,signal,subprocess,sys,time
ROOT=Path(__file__).resolve().parent
ACTION='native-eight-average-cost-v1-96'

def strict(s):
 def pairs(xs):
  out={}
  for k,v in xs:
   if k in out:raise ValueError('duplicate key '+k)
   out[k]=v
  return out
 return json.loads(s,object_pairs_hook=pairs,parse_constant=lambda x:(_ for _ in ()).throw(ValueError('nonfinite '+x)))
def read(p):return strict(Path(p).read_bytes())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def need(ok,message):
 if not ok:raise ValueError(message)
def write(p,obj):
 with Path(p).open('x') as f:json.dump(obj,f,sort_keys=True,indent=2,allow_nan=False);f.write('\n');f.flush();os.fsync(f.fileno())
def verify_freeze():
 f=read(ROOT/'freeze.json')
 for rel,pin in f['files'].items():need(sha(ROOT/rel)==pin,'frozen artifact drift: '+rel)
 for role,pin in f['roles'].items():
  path=Path(pin['path']);need(path.is_file() and sha(path)==pin['sha256'],'role drift: '+role)
  stat=path.stat();need((stat.st_dev,stat.st_ino,stat.st_size)==(pin['device'],pin['inode'],pin['bytes']),'physical role identity drift: '+role)
 for path,pin in f.get('loader_files',{}).items():need(sha(path)==pin,'loader dependency drift: '+path)
 for path,target in f.get('loader_links',{}).items():need(str(Path(path).resolve())==target,'loader link drift: '+path)
 need(f['untimed_gate']['passed'] is True,'untimed gate not passed')
 need(f['roles']['stock']['sha256']!=f['roles']['candidate']['sha256'],'candidate is stock')
 return f

def validate_output(observed,slot,freeze,**kw):
 # Written and independently tested with the frozen genuine host contract.
 from host_contract import validate
 return validate(observed,slot,freeze,**kw)

def collection(auth_path):
 freeze=verify_freeze();plan=read(ROOT/'plan.json');auth=read(auth_path)
 need(auth=={'action':ACTION,'plan_sha256':sha(ROOT/'plan.json'),'freeze_sha256':sha(ROOT/'freeze.json'),'exclusive_approved':True},'authorization does not exactly bind this study')
 need(not (ROOT/'campaign').exists(),'campaign exists; no resume')
 auth_sha=sha(auth_path)
 write(ROOT/'trial-consumed.json',{'action':ACTION,'authorization_sha256':auth_sha,'plan_sha256':sha(ROOT/'plan.json'),'freeze_sha256':sha(ROOT/'freeze.json'),'one_shot':True})
 campaign=ROOT/'campaign';campaign.mkdir()
 for name,source in [('plan.json',ROOT/'plan.json'),('freeze.json',ROOT/'freeze.json'),('authorization.json',Path(auth_path))]:
  with (campaign/name).open('xb') as dest:dest.write(source.read_bytes());dest.flush();os.fsync(dest.fileno())
 def cpu_ns():
  a=resource.getrusage(resource.RUSAGE_SELF);b=resource.getrusage(resource.RUSAGE_CHILDREN)
  return round((a.ru_utime+a.ru_stime+b.ru_utime+b.ru_stime)*1e9)
 def stop(sig,frame):raise InterruptedError('signal '+str(sig))
 signal.signal(signal.SIGINT,stop);signal.signal(signal.SIGTERM,stop)
 start=time.monotonic_ns();cpu_start=cpu_ns();completed=0;failure=None
 ledger=(campaign/'ledger.jsonl').open('x')
 def log(obj):ledger.write(json.dumps(obj,sort_keys=True,allow_nan=False)+'\n');ledger.flush();os.fsync(ledger.fileno())
 log({'event':'start','action':ACTION,'authorization_sha256':auth_sha,'plan_sha256':sha(ROOT/'plan.json'),'freeze_sha256':sha(ROOT/'freeze.json'),'wall_ns':start})
 try:
  for slot in plan['schedule']['slots']:
   need(cpu_ns()-cpu_start+5_000_000_000<=240_000_000_000,'global CPU reserve exhausted')
   remaining=360-(time.monotonic_ns()-start)/1e9;need(remaining>=6,'global wall reserve exhausted')
   verify_freeze()
   sid=slot['slot'];role=slot['module_role'];out_dir=campaign/f'data-{sid:02d}'
   from host_contract import command
   args=command(freeze,slot,out_dir,auth_sha)
   home=campaign/f'home-{sid:02d}';home.mkdir();need(not any(home.iterdir()),'fresh empty HOME')
   env=dict(freeze['loader_environment']);env['HOME']=str(home);env['CS80_NATIVE_AVERAGE_AUTH']=auth_sha
   stat=Path(freeze['roles'][role]['path']).stat()
   receipt={'schema':1,'slot':slot,'argv':args,'environment':env,'initial_home_empty':True,'host_sha256':freeze['roles']['host']['sha256'],'module_sha256':freeze['roles'][role]['sha256'],'module_stat':{'device':stat.st_dev,'inode':stat.st_ino,'bytes':stat.st_size},'authorization_sha256':auth_sha,'plan_sha256':sha(ROOT/'plan.json'),'freeze_sha256':sha(ROOT/'freeze.json')}
   child=None;out=err='';code=None;error=None;before=time.monotonic_ns();usage=resource.getrusage(resource.RUSAGE_CHILDREN)
   log({'event':'slot-start','slot':sid,'wall_ns':before})
   def limits():
    os.sched_setaffinity(0,{0});resource.setrlimit(resource.RLIMIT_CPU,(4,4));resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(4*1024**3,4*1024**3));resource.setrlimit(resource.RLIMIT_FSIZE,(32*1024**2,32*1024**2))
   try:
    child=subprocess.Popen(args,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,preexec_fn=limits,start_new_session=True)
    out,err=child.communicate(timeout=6);code=child.returncode
   except BaseException as exc:
    error=type(exc).__name__+': '+str(exc)
    if child is not None:
     try:os.killpg(child.pid,signal.SIGKILL)
     except ProcessLookupError:pass
     out,err=child.communicate();code=child.returncode
   end=time.monotonic_ns();after=resource.getrusage(resource.RUSAGE_CHILDREN)
   user_ns=round((after.ru_utime-usage.ru_utime)*1e9);system_ns=round((after.ru_stime-usage.ru_stime)*1e9)
   receipt.update(pid=None if child is None else child.pid,exit=code,error=error,wall_begin_ns=before,wall_end_ns=end,resource_user_ns=user_ns,resource_system_ns=system_ns,stdout=out,stderr=err)
   try:
    need(error is None and code==0 and not err,'process failure')
    need(user_ns+system_ns<=4_000_000_000 and end-before<=6_000_000_000,'process budget exceeded')
    observed=strict(out);need(observed['cpu_ns_sum']<=user_ns+system_ns,'measured CPU exceeds process CPU');receipt['artifacts']=validate_output(observed,slot,freeze,out_dir=out_dir,auth=auth_sha,pid=child.pid,before=before,after=end);verify_freeze();receipt['integrity']='PASS'
   except Exception as exc:receipt['integrity']='FAIL';receipt['integrity_error']=type(exc).__name__+': '+str(exc)
   path=campaign/f'slot-{sid:02d}.json';write(path,receipt)
   log({'event':'slot-end','slot':sid,'receipt_sha256':sha(path),'integrity':receipt['integrity']})
   need(receipt['integrity']=='PASS','slot integrity failed; no resume');completed+=1
   need(cpu_ns()-cpu_start<=240_000_000_000 and time.monotonic_ns()-start<=360_000_000_000,'global budget exceeded')
 except BaseException as exc:failure=type(exc).__name__+': '+str(exc)
 terminal={'schema':1,'status':'complete' if completed==96 and failure is None else 'failed','completed_slots':completed,'error':failure,'wall_begin_ns':start,'wall_end_ns':time.monotonic_ns(),'cpu_elapsed_ns':cpu_ns()-cpu_start,'no_resume':True,'analysis_performed':False}
 log({'event':'terminal',**terminal});ledger.close();write(campaign/'terminal.json',terminal);print(json.dumps(terminal,sort_keys=True));return 0 if terminal['status']=='complete' else 1

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('mode',choices=['ready','collect']);p.add_argument('--authorization',type=Path);a=p.parse_args()
 if a.mode=='ready':
  f=verify_freeze();print(json.dumps({'ready':True,'action':ACTION,'measurement_clock_reads':0,'plan_sha256':sha(ROOT/'plan.json'),'freeze_sha256':sha(ROOT/'freeze.json')},sort_keys=True))
 else:
  need(a.authorization is not None,'authorization required');sys.exit(collection(a.authorization))
