from pathlib import Path
import subprocess,json,hashlib,sys
r=Path('/workspace/scratch/ee447c520988'); source=r/'release-followup'; out=r/'mpe-released-target-evidence/native'; run=out/'candidate-native-c0219bd-attempt-001'
run.mkdir()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(source),*args],text=True)
def snapshot(label):
 tracked=git('ls-files','src','tests','CMakeLists.txt').splitlines()
 data={'source':str(source),'head':git('rev-parse','HEAD').strip(),'status':git('status','--porcelain=v1'),'tracked_files':{p:sha(source/p) for p in tracked if (source/p).is_file()},'compiler':subprocess.check_output(['/usr/bin/c++','--version'],text=True),'scripts':{p.name:sha(p) for p in [out/'configure-candidate.sh',out/'build-candidate.sh']}}
 (run/f'provenance-{label}.json').write_text(json.dumps(data,indent=2)+'\n')
snapshot('before')
assert git('rev-parse','HEAD').strip()=='c0219bd0de8559d72fce4194a849414de5ac7e61'
assert not git('diff','c0219bd','--','src'), 'Production/source changed before candidate build'
assert (source/'tests/MpeReleasedTargetTests.cpp').is_file(), 'Core fixture must exist before configuration'
records=[]
for stage,argv in [('configure',['bash',str(out/'configure-candidate.sh')]),('build',['bash',str(out/'build-candidate.sh')])]:
 log=run/f'{stage}.log'
 with log.open('w') as f:p=subprocess.run(argv,cwd=r,stdout=f,stderr=subprocess.STDOUT)
 records.append({'stage':stage,'argv':argv,'cwd':str(r),'status':p.returncode,'log':str(log),'sha256':sha(log)})
 (run/'commands-statuses.json').write_text(json.dumps(records,indent=2)+'\n')
 print(f'{stage}: status={p.returncode}, log={log}',flush=True)
 if p.returncode:print(log.read_text());sys.exit(p.returncode)
snapshot('after')
binary=r/'build-mpe-released-target-native/CS80Snapshots_artefacts/Release/CS80Snapshots'
(run/'binary.sha256').write_text(sha(binary)+'  '+str(binary)+'\n')
print('Native snapshot binary SHA-256: '+sha(binary),flush=True)
