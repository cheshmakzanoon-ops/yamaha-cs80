from pathlib import Path
import subprocess,json,hashlib,shutil,sys
r=Path('/workspace/scratch/ee447c520988'); s=r/'release-followup'; out=r/'mpe-released-target-evidence/native'; run=out/'candidate-native-c0219bd-attempt-002';run.mkdir()
b=r/'build-mpe-released-target-native'; binary=b/'CS80Snapshots_artefacts/Release/CS80Snapshots'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
old=out/'candidate-native-c0219bd-attempt-001/CS80Snapshots-initial-header';shutil.copy2(binary,old)
paths=[s/'src/core/Engine.cpp',s/'src/core/Engine.h',s/'src/tools/ui_snapshot.cpp',s/'tests/MpeReleasedTargetHostTests.h',s/'CMakeLists.txt',r/'mpe-released-target-evidence/review-inputs/native-fixture-initial.h',old]
(run/'inputs-before.json').write_text(json.dumps({str(p):sha(p) for p in paths},indent=2)+'\n')
(run/'source-status-before.txt').write_text(subprocess.check_output(['git','-C',str(s),'status','--short'],text=True))
log=run/'build.log';argv=['bash',str(out/'build-candidate.sh')]
with log.open('w') as f:p=subprocess.run(argv,cwd=r,stdout=f,stderr=subprocess.STDOUT)
(run/'commands-statuses.json').write_text(json.dumps([{'stage':'rebuild-stable-native-header','argv':argv,'cwd':str(r),'status':p.returncode,'log':str(log),'sha256':sha(log)}],indent=2)+'\n')
print(log.read_text());print('build status:',p.returncode)
if p.returncode:sys.exit(p.returncode)
(run/'binary.sha256').write_text(sha(binary)+'  '+str(binary)+'\n')
(run/'inputs-after.json').write_text(json.dumps({str(p):sha(p) for p in paths},indent=2)+'\n')
