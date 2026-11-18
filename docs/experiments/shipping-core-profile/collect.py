from pathlib import Path
import os,subprocess,time,resource,json,hashlib,signal,sys
if sys.argv[1:] != ['--run-new-study']:
    raise SystemExit('This starts a NEW sampling study. For saved evidence use verify.py --replay. To intentionally acquire new evidence, pass --run-new-study.')
P=Path(__file__).resolve().parent;out=P/'samples';out.mkdir(exist_ok=False)
CPU=min(os.sched_getaffinity(0));order=[(0,1999),(1,5003),(2,1999),(0,5003),(1,1999),(2,5003)]
freeze={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [P/'PLAN.txt',P/'profile.cpp',P/'profile-debug',P/'profile-stock',P/'build-identity.json',P/'collect.py']}
(out/'freeze.json').write_text(json.dumps({'cpu':CPU,'order':order,'inputs':freeze},indent=2)+'\n')
begun=time.monotonic();cpuBefore=resource.getrusage(resource.RUSAGE_CHILDREN);cpu0=cpuBefore.ru_utime+cpuBefore.ru_stime;rows=[]
def cpu_used():
    r=resource.getrusage(resource.RUSAGE_CHILDREN);return r.ru_utime+r.ru_stime-cpu0
def setup():
    os.setsid();os.sched_setaffinity(0,{CPU});resource.setrlimit(resource.RLIMIT_CPU,(30,31))
try:
 for i,(scene,period) in enumerate(order):
    assert time.monotonic()-begun<300 and cpu_used()<150
    stem=out/f'{i+1:02d}-s{scene}-p{period}';cmd=[str(P/'profile-debug'),str(scene),str(period),'20',str(stem)]
    with stem.with_suffix('.json').open('w') as stdout,stem.with_suffix('.stderr').open('w') as stderr:
      started=time.monotonic();p=subprocess.Popen(cmd,stdout=stdout,stderr=stderr,preexec_fn=setup)
      print(json.dumps({'slot_started':i+1,'scene':scene,'period_us':period,'pid':p.pid}),flush=True)
      while p.poll() is None:
        liveCpu=0
        try:
          fields=Path(f'/proc/{p.pid}/stat').read_text().rsplit(')',1)[1].split();liveCpu=(int(fields[11])+int(fields[12]))/os.sysconf('SC_CLK_TCK')
        except FileNotFoundError:pass
        if time.monotonic()-started>45 or time.monotonic()-begun>300 or cpu_used()+liveCpu>150:
          os.killpg(p.pid,signal.SIGKILL);p.wait();raise RuntimeError('predeclared bound exceeded')
        time.sleep(.1)
      assert p.returncode==0,{'slot':i+1,'returncode':p.returncode,'stderr':stem.with_suffix('.stderr').read_text()}
    row=json.loads(stem.with_suffix('.json').read_text());row.update(slot=i+1,wall_seconds=time.monotonic()-started)
    assert row['cpu']==CPU and row['overflow']==0
    rows.append(row);print(json.dumps(row),flush=True)
 for scene in range(3):
    same=[x for x in rows if x['scene']==['factory-held','factory-moving','panel-moving'][scene]]
    assert len(same)==2 and same[0]['prefix4096_hash']==same[1]['prefix4096_hash']
 result={'status':'complete','slots':rows,'measurement_children_cpu_seconds':cpu_used(),'wall_seconds':time.monotonic()-begun,'cpu':CPU}
except BaseException as e:
 result={'status':'incomplete-terminal','error':repr(e),'slots':rows,'measurement_children_cpu_seconds':cpu_used(),'wall_seconds':time.monotonic()-begun}
 (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True);raise
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
