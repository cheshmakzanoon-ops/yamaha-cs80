from pathlib import Path
import subprocess,os,json,hashlib,datetime
r=Path('/workspace/scratch/ee447c520988');s=r/'release-followup';out=r/'mpe-released-target-evidence/strict-compatibility';env=os.environ.copy();env['PATH']=str(r/'pitch-power-recovery/tools/cmake/data/bin')+':'+str(r/'pitch-power-recovery/tools/bin')+':'+env['PATH'];env['CS80_COMPAT_CACHE']=str(r/'pitch-power-recovery/compat-cache');env['PYTHONDONTWRITEBYTECODE']='1';env['CMAKE_BUILD_PARALLEL_LEVEL']='2'
args=['bash','scripts/check-compat-gates.sh','--strict-v2'];log=out/'run.log';start=datetime.datetime.now(datetime.timezone.utc).isoformat()
with log.open('x') as f:p=subprocess.run(args,cwd=s,env=env,stdout=f,stderr=subprocess.STDOUT)
record=dict(argv=args,cwd=str(s),started_utc=start,exit_code=p.returncode,environment={k:env[k] for k in ['PATH','CS80_COMPAT_CACHE','PYTHONDONTWRITEBYTECODE','CMAKE_BUILD_PARALLEL_LEVEL']},log=log.name,log_sha256=hashlib.sha256(log.read_bytes()).hexdigest());(out/'command.json').write_text(json.dumps(record,indent=2)+'\n')
if p.returncode:raise SystemExit(p.returncode)
w=s/'build-compat-gates';files=[p for p in sorted(w.iterdir()) if p.is_file()]+[w/'current/libcs80_core.a'];(out/'artifacts.json').write_text(json.dumps({str(p):dict(sha256=hashlib.sha256(p.read_bytes()).hexdigest(),size=p.stat().st_size) for p in files},indent=2)+'\n');print(log.read_text())
