from pathlib import Path
import subprocess,json,hashlib,os,datetime
r=Path('/workspace/scratch/ee447c520988');s=r/'release-followup';b=r/'build-mpe-released-target-core';out=r/'mpe-released-target-evidence/full-suite';cmake=r/'pitch-power-recovery/tools/cmake/data/bin/cmake';ctest=cmake.with_name('ctest');env=os.environ.copy();env['PYTHONDONTWRITEBYTECODE']='1';records=[]
for name,args in [('build',[cmake,'--build',b,'--parallel','2']),('ctest',[ctest,'--test-dir',b,'--output-on-failure','--parallel','2'])]:
    log=out/f'{name}.log';start=datetime.datetime.now(datetime.timezone.utc).isoformat()
    with log.open('x') as f:p=subprocess.run([str(x) for x in args],cwd=s,env=env,stdout=f,stderr=subprocess.STDOUT)
    records.append(dict(stage=name,argv=[str(x) for x in args],cwd=str(s),start=start,exit_code=p.returncode,log=log.name,sha256=hashlib.sha256(log.read_bytes()).hexdigest()));(out/'commands.json').write_text(json.dumps(records,indent=2)+'\n');print(name,p.returncode,flush=True)
    if p.returncode:raise SystemExit(p.returncode)
