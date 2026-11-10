from pathlib import Path
import subprocess,shlex,json,hashlib,sys
r=Path('/workspace/scratch/ee447c520988'); b=r/'build-mpe-released-target-native'; out=r/'mpe-released-target-evidence/native'; run=out/'candidate-probe-c0219bd-attempt-001'
run.mkdir()
commands=subprocess.check_output([str(r/'pitch-power-recovery/tools/bin/ninja'),'-C',str(b),'-t','commands','CS80Snapshots'],text=True).splitlines()
(run/'ninja-CS80Snapshots-commands.txt').write_text('\n'.join(commands)+'\n')
compile=next(shlex.split(c) for c in commands if ' -c ' in c and '/src/tools/ui_snapshot.cpp' in c)
for flag in ['-MF','-MT']:
 if flag in compile:
  i=compile.index(flag);del compile[i:i+2]
compile=[x for x in compile if x!='-MD']
compile[compile.index('-o')+1]=str(run/'baseline-probe.o');compile[compile.index('-c')+1]=str(out/'BaselineReleasedTargetsProbe.cpp')
link=shlex.split(commands[-1]);link=link[link.index('/usr/bin/c++'):link.index('&&',2)] if link[-1]==':' else link
link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
link=[str(run/'baseline-probe.o') if x=='CMakeFiles/CS80Snapshots.dir/src/tools/ui_snapshot.cpp.o' else x for x in link]
link[link.index('-o')+1]=str(run/'baseline-probe')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
source=r/'release-followup'
tracked=subprocess.check_output(['git','-C',str(source),'ls-files','src','tests','CMakeLists.txt'],text=True).splitlines()
manifest={str(source/p):sha(source/p) for p in tracked if (source/p).is_file()}
for x in link:
 p=b/x
 if p.is_file():manifest[str(p.resolve())]=sha(p)
manifest[str(out/'BaselineReleasedTargetsProbe.cpp')]=sha(out/'BaselineReleasedTargetsProbe.cpp')
binary=b/'CS80Snapshots_artefacts/Release/CS80Snapshots';manifest[str(binary)]=sha(binary)
(run/'inputs-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
provenance={'source':str(source),'head':subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip(),'status':subprocess.check_output(['git','-C',str(source),'status','--porcelain=v1'],text=True),'source_diff_vs_c0219bd':subprocess.check_output(['git','-C',str(source),'diff','--stat','c0219bd','HEAD','--','src','tests','CMakeLists.txt'],text=True),'build':str(b),'binary_sha256':sha(binary)}
(run/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
records=[]
for stage,argv in [('compile',compile),('link',link),('run',[str(run/'baseline-probe')])]:
 log=run/f'{stage}.log'
 with log.open('w') as f:p=subprocess.run(argv,cwd=b,stdout=f,stderr=subprocess.STDOUT)
 records.append(dict(stage=stage,argv=argv,cwd=str(b),status=p.returncode,log=str(log),sha256=sha(log)))
 (run/'commands-statuses.json').write_text(json.dumps(records,indent=2)+'\n')
 print(f'{stage}: status={p.returncode}, log={log}')
 if p.returncode and stage!='run':print(log.read_text());sys.exit(p.returncode)
 if stage=='run':
  print(log.read_text());(run/'binary.sha256').write_text(sha(run/'baseline-probe')+'  '+str(run/'baseline-probe')+'\n');sys.exit(p.returncode)
