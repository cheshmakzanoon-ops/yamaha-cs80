from pathlib import Path
import subprocess,json,hashlib,shutil,sys
r=Path('/workspace/scratch/ee447c520988'); s=r/'release-followup'; out=r/'mpe-released-target-evidence/native'; run=out/'final-native-neighbors-3003569-attempt-001';run.mkdir()
b=r/'build-mpe-released-target-native'; binary=b/'CS80Snapshots_artefacts/Release/CS80Snapshots'; ctest=r/'pitch-power-recovery/tools/cmake/data/bin/ctest'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(s),*args],text=True)
def snap(label):
 data={'head':git('rev-parse','HEAD').strip(),'tree':git('rev-parse','HEAD^{tree}').strip(),'status':git('status','--porcelain=v1'),'binary':str(binary),'binary_sha256':sha(binary),'fixture_sha256':sha(s/'tests/MpeReleasedTargetHostTests.h')}
 (run/f'provenance-{label}.json').write_text(json.dumps(data,indent=2)+'\n');return data
before=snap('before')
assert before['head']=='300356958b3cde471f8dc14a7b505070a4cffc8c' and not before['status']
assert before['binary_sha256']=='11fbae61a11d603c50e0f8d915e50d3731c42eacdd19e0378e79d36884130cf4'
regex='^cs80_(mpe_released_target_host|mpe_member_onset_host|sixteen_voice_release_host|controller_reset_host|keyboard_handoff_host|undo_replay_gesture)_tests$'
argv=[str(ctest),'--test-dir',str(b),'-R',regex,'--verbose','--output-on-failure','--no-tests=error']
log=run/'ctest.log'
with log.open('w') as f:p=subprocess.run(argv,cwd=b,stdout=f,stderr=subprocess.STDOUT)
(run/'commands-statuses.json').write_text(json.dumps([{'stage':'focused-native-neighbors','argv':argv,'cwd':str(b),'status':p.returncode,'log':str(log),'sha256':sha(log)}],indent=2)+'\n')
for name in ['LastTest.log','LastTestsFailed.log']:
 path=b/'Testing/Temporary'/name
 if path.exists():shutil.copy2(path,run/name)
after=snap('after')
assert before==after, 'Source or binary changed during qualification'
(run/'binary.sha256').write_text(sha(binary)+'  '+str(binary)+'\n')
print(log.read_text());print('Final neighbor CTest status:',p.returncode)
sys.exit(p.returncode)
