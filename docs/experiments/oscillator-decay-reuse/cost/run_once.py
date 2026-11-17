import pathlib,json,hashlib,subprocess,resource,os,time,signal
r=pathlib.Path(__file__).resolve().parent;plan=json.loads((r/'plan.json').read_text());H=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def verify():
 for p,h in plan['immutable_sha256'].items():assert H(pathlib.Path(p))==h,p
 assert sorted(os.sched_getaffinity(0))==plan['allowed_cpus']
verify()
for arm,rounds in [('aa',4),('ab',12)]:
 verify();start=time.monotonic();reason=None
 def limits():
  os.sched_setaffinity(0,{0});resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_FSIZE,(64*1024**2,64*1024**2))
 with (r/(arm+'.csv')).open('x')as out,(r/(arm+'.stderr')).open('x')as err:
  p=subprocess.Popen([str(r/('probe-'+arm)),'8192',str(rounds)],env=plan['environment'],cwd=r,stdout=out,stderr=err,start_new_session=True,preexec_fn=limits)
  while p.poll() is None:
   if time.monotonic()-start>=600:reason='wall_cap'
   if sum(q.stat().st_size for q in r.rglob('*') if q.is_file())>64*1024**2:reason='retained_cap'
   if reason:os.killpg(p.pid,signal.SIGKILL);break
   time.sleep(.05)
  code=p.wait(timeout=3)
 try:os.killpg(p.pid,0);empty=False
 except ProcessLookupError:empty=True
 if not empty:os.killpg(p.pid,signal.SIGKILL);reason=reason or 'remaining_group'
 (r/(arm+'.execution.json')).write_text(json.dumps({'returncode':code,'watchdog_reason':reason,'group_empty':empty,'wall_seconds':time.monotonic()-start,'pid':p.pid},indent=2));verify()
 if code or reason or not empty:raise RuntimeError('invalid cost process; retain no retry')
print('Both phases completed; analyze separately')
