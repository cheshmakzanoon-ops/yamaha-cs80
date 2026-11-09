from pathlib import Path
import subprocess,shlex,json,hashlib
r=Path.cwd(); b=r/'build-mpe-onset-native'; out=r/'sixteen-release-evidence'
commands=subprocess.check_output([str(r/'pitch-power-recovery/tools/bin/ninja'),'-C',str(b),'-t','commands','CS80Snapshots'],text=True).splitlines()
compile=next(shlex.split(c) for c in commands if ' -c ' in c and '/src/tools/ui_snapshot.cpp' in c)
for flag in ['-MF','-MT']:
 if flag in compile:
  i=compile.index(flag);del compile[i:i+2]
compile=[x for x in compile if x!='-MD']
compile[compile.index('-o')+1]=str(out/'baseline-probe.o');compile[compile.index('-c')+1]=str(out/'BaselineReleaseProbe.cpp')
link=shlex.split(commands[-1]);link=link[link.index('/usr/bin/c++'):link.index('&&',2)] if link[-1]==':' else link
link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
link=[str(out/'baseline-probe.o') if x=='CMakeFiles/CS80Snapshots.dir/src/tools/ui_snapshot.cpp.o' else x for x in link]
link[link.index('-o')+1]=str(out/'baseline-probe')
records=[]
for stage,argv in [('compile',compile),('link',link),('run',[str(out/'baseline-probe')])]:
 log=out/f'baseline-{stage}.log'
 with log.open('w') as f:p=subprocess.run(argv,cwd=b,stdout=f,stderr=subprocess.STDOUT)
 records.append(dict(stage=stage,argv=argv,cwd=str(b),status=p.returncode,log=log.name,sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
 (out/'baseline-commands.json').write_text(json.dumps(records,indent=2)+'\n')
 if p.returncode and stage!='run':print(log.read_text());raise SystemExit(p.returncode)
 if stage=='run':print(log.read_text());raise SystemExit(0 if p.returncode==1 else 1)
