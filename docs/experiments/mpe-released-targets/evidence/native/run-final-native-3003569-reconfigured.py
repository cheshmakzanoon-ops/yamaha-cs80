from pathlib import Path
import subprocess,json,hashlib,sys
r=Path('/workspace/scratch/ee447c520988'); s=r/'release-followup'; out=r/'mpe-released-target-evidence/native'; run=out/'final-native-3003569-attempt-002';run.mkdir()
b=r/'build-mpe-released-target-native'; binary=b/'CS80Snapshots_artefacts/Release/CS80Snapshots'; ninja=r/'pitch-power-recovery/tools/bin/ninja'; ctest=r/'pitch-power-recovery/tools/cmake/data/bin/ctest'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(s),*args],text=True)
assert git('rev-parse','HEAD').strip()=='300356958b3cde471f8dc14a7b505070a4cffc8c'
assert not git('status','--porcelain=v1')
assert not git('diff','c0219bd','HEAD','--','src/core','src/plugin')
def provenance(label):
 paths=git('ls-files','src','tests','CMakeLists.txt').splitlines()
 data={'source':str(s),'head':git('rev-parse','HEAD').strip(),'tree':git('rev-parse','HEAD^{tree}').strip(),'status':git('status','--porcelain=v1'),'production_diff_vs_c0219bd':git('diff','c0219bd','HEAD','--','src/core','src/plugin'),'files':{str(s/p):sha(s/p) for p in paths if (s/p).is_file()},'binary':{'path':str(binary),'sha256':sha(binary)}}
 (run/f'provenance-{label}.json').write_text(json.dumps(data,indent=2)+'\n')
provenance('before')
records=[]
for stage,argv in [('configure',['bash',str(out/'configure-candidate.sh')]),('build',['bash',str(out/'build-candidate.sh')]),('no-work',[str(ninja),'-C',str(b),'-d','explain','CS80Snapshots']),('direct',[str(binary),'--mpe-released-target-tests']),('ctest',[str(ctest),'--test-dir',str(b),'-R','^cs80_mpe_released_target_host_tests$','--output-on-failure','--no-tests=error'])]:
 log=run/f'{stage}.log'
 with log.open('w') as f:
  try:p=subprocess.run(argv,cwd=b,stdout=f,stderr=subprocess.STDOUT,timeout=240);status=p.returncode
  except subprocess.TimeoutExpired:status=124
 records.append({'stage':stage,'argv':argv,'cwd':str(b),'timeout_seconds':240,'status':status,'log':str(log),'sha256':sha(log),'binary_sha256':sha(binary)})
 (run/'commands-statuses.json').write_text(json.dumps(records,indent=2)+'\n')
 print(f'{stage}: status={status}, log={log}',flush=True)
 print(log.read_text(),flush=True)
 if stage=='no-work' and 'no work to do.' not in log.read_text():raise SystemExit('No-work gate did not establish currency; direct test not run.')
 if status:sys.exit(status)
provenance('after')
(run/'binary.sha256').write_text(sha(binary)+'  '+str(binary)+'\n')
