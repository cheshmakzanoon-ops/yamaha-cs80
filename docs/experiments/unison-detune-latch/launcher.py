from pathlib import Path
import hashlib,json,subprocess,datetime
r=Path('/workspace/scratch/ee447c520988/cs80-recovery/yamaha-cs80-unison-latch');o=r/'docs/experiments/unison-detune-latch';tools=r.parents[1]/'cs80-build-tools/bin';b='build-unison-latch-v2';name='cs80_unison_detune_latch_tests';freeze=json.loads((o/'source-freeze.json').read_text())
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify():
 for p,h in freeze.items():assert digest(r/p)==h,p
assert not (r/b).exists();verify();result={'base':subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip(),'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_freeze_sha256':digest(o/'source-freeze.json'),'steps':[]}
steps=[('configure',[tools/'cmake','-S','.', '-B',b,'-G','Ninja','-DCS80_BUILD_PLUGIN=OFF','-DCS80_BUILD_TESTS=ON','-DCMAKE_BUILD_TYPE=Release']),('build',[tools/'cmake','--build',b,'--target',name,'--parallel','2']),('list',[tools/'ctest','--test-dir',b,'--show-only=json-v1','-R','^'+name+'$']),('test',[tools/'ctest','--test-dir',b,'--verbose','-j1','-R','^'+name+'$'])]
for stage,cmd in steps:
 verify();cmd=list(map(str,cmd));log=o/(stage+'.log')
 with log.open('x') as f:code=subprocess.run(cmd,cwd=r,stdout=f,stderr=subprocess.STDOUT,timeout=240).returncode
 result['steps'].append({'name':stage,'command':cmd,'exit':code,'log_sha256':digest(log)});(o/'result.json').write_text(json.dumps(result,indent=2)+'\n');assert code==0,(stage,code)
 if stage=='list':assert [t['name'] for t in json.loads(log.read_text())['tests']]==[name]
 if stage=='test':assert 'RESULT checks=40 failures=0' in log.read_text()
verify();result.update(source_unchanged=True,binary_sha256={name:digest(r/b/name)},completed_utc=datetime.datetime.now(datetime.timezone.utc).isoformat());(o/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS scoped CTest 1/1, 40/40 assertions')
