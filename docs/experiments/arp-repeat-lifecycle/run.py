from pathlib import Path
import hashlib,json,subprocess,datetime,os
p=Path('/workspace/scratch/ee447c520988/cs80-recovery/yamaha-cs80-arp-repeat-lifecycle');e=p/'docs/experiments/arp-repeat-lifecycle';e.mkdir(parents=True)
t=Path('/workspace/scratch/ee447c520988/cs80-build-tools/bin');h=lambda f:hashlib.sha256(f.read_bytes()).hexdigest();env=dict(os.environ);env['PATH']=str(t)+':'+env['PATH'];b='build-arp-repeat-lifecycle'
files=['CMakeLists.txt','tests/ArpRepeatLifecycleTests.cpp','docs/ARP_REPEAT_LIFECYCLE_REGRESSION.md'] + sorted(str(x.relative_to(p)) for x in (p/'src').rglob('*') if x.is_file());m={'base':'f01cbe84257a700a3690b4fac027fab002f46b56','source_sha256':{f:h(p/f)for f in files},'source_review':'Independent configure/dependency/regex review passed; no plugin fetch; only ARP target and linked cs80_core library','started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'steps':[]}
(e/'pre-run.json').write_text(json.dumps(m,indent=2)+'\n')
commands=[('configure',[str(t/'cmake'),'-S','.', '-B',b,'-G','Ninja','-DCS80_BUILD_PLUGIN=OFF','-DCS80_BUILD_TESTS=ON','-DCMAKE_BUILD_TYPE=Release']),('build',[str(t/'cmake'),'--build',b,'--target','cs80_arp_repeat_lifecycle_tests','--parallel','2']),('list',[str(t/'ctest'),'--test-dir',b,'--show-only=json-v1','-R','^cs80_arp_repeat_lifecycle_tests$']),('test',[str(t/'ctest'),'--test-dir',b,'--verbose','-j1','-R','^cs80_arp_repeat_lifecycle_tests$'])]
for label,cmd in commands:
 if label=='test':
  names=[x['name']for x in json.loads((e/'list.log').read_text())['tests']];assert sorted(names)==['cs80_arp_repeat_lifecycle_tests'],names;m['selected_tests']=names
 with(e/(label+'.log')).open('w')as f:r=subprocess.run(cmd,cwd=p,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
 m['steps'].append({'name':label,'command':cmd,'exit':r.returncode,'log_sha256':h(e/(label+'.log'))});(e/'result.json').write_text(json.dumps(m,indent=2)+'\n')
 if r.returncode:raise SystemExit('Stopped after '+label)
m['source_unchanged']=all(h(p/f)==v for f,v in m['source_sha256'].items());m['binary_sha256']={f:h(p/b/f)for f in ['cs80_arp_repeat_lifecycle_tests']};m['completed_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();(e/'result.json').write_text(json.dumps(m,indent=2)+'\n');print((e/'test.log').read_text())
