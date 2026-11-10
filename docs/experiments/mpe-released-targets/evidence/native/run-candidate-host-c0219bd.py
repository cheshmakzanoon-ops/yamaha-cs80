from pathlib import Path
import subprocess,json,hashlib,sys
r=Path('/workspace/scratch/ee447c520988'); source=r/'release-followup'; out=r/'mpe-released-target-evidence/native'; run=out/'candidate-host-c0219bd-attempt-001'; run.mkdir()
build=r/'build-mpe-released-target-native'; binary=build/'CS80Snapshots_artefacts/Release/CS80Snapshots'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(source),*args],text=True)
paths=['src/core/Engine.cpp','src/core/Engine.h','src/plugin/PluginProcessor.cpp','src/plugin/PluginProcessor.h','src/tools/ui_snapshot.cpp','tests/MpeReleasedTargetHostTests.h','CMakeLists.txt']
manifest={str(source/p):sha(source/p) for p in paths}; manifest[str(binary)]=sha(binary)
(run/'provenance.json').write_text(json.dumps({'source':str(source),'head':git('rev-parse','HEAD').strip(),'status':git('status','--porcelain=v1'),'production_diff_vs_c0219bd':git('diff','c0219bd','--','src/core','src/plugin'),'inputs':manifest},indent=2)+'\n')
records=[]
for stage,argv in [('dry-run',[str(r/'pitch-power-recovery/tools/bin/ninja'),'-C',str(build),'-n','CS80Snapshots']),('direct',[str(binary),'--mpe-released-target-tests'])]:
 log=run/f'{stage}.log'
 with log.open('w') as f:
  try:p=subprocess.run(argv,cwd=build,stdout=f,stderr=subprocess.STDOUT,timeout=240);status=p.returncode
  except subprocess.TimeoutExpired:status=124
 records.append({'stage':stage,'argv':argv,'cwd':str(build),'timeout_seconds':240,'status':status,'log':str(log),'sha256':sha(log)})
 (run/'commands-statuses.json').write_text(json.dumps(records,indent=2)+'\n')
 print(f'{stage}: status={status}, log={log}',flush=True)
 if stage=='dry-run' and (status!=0 or 'no work to do.' not in log.read_text()):
  print(log.read_text());raise SystemExit('Build not fully current; direct test not run.')
 if stage=='direct':print(log.read_text());sys.exit(status)
