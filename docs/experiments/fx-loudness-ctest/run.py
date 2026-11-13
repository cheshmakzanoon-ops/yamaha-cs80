from pathlib import Path
import hashlib,json,subprocess,datetime,os
p=Path('/workspace/scratch/ee447c520988/cs80-recovery/yamaha-cs80-loudness-ctest');e=p/'docs/experiments/fx-loudness-ctest';e.mkdir(parents=True)
t=Path('/workspace/scratch/ee447c520988/cs80-build-tools/bin');h=lambda f:hashlib.sha256(f.read_bytes()).hexdigest();env=dict(os.environ);env['PATH']=str(t)+':'+env['PATH'];b='build-fx-loudness-ctest'
files=['CMakeLists.txt','src/core/FxLoudness.h','tests/FxLoudnessCoreTests.cpp','tests/FxLoudnessGateTests.cpp'];m={'base':'c0b17dea57b14436418337a93f55df8176993475','source_sha256':{f:h(p/f)for f in files},'source_review':'Independent configure/dependency/regex review passed; no plugin fetch or Engine target','started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'steps':[]}
(e/'pre-run.json').write_text(json.dumps(m,indent=2)+'\n')
commands=[('configure',[str(t/'cmake'),'-S','.', '-B',b,'-G','Ninja','-DCS80_BUILD_PLUGIN=OFF','-DCS80_BUILD_TESTS=ON','-DCMAKE_BUILD_TYPE=Release']),('build',[str(t/'cmake'),'--build',b,'--target','cs80_fx_loudness_core_tests','cs80_fx_loudness_gate_tests','--parallel','2']),('list',[str(t/'ctest'),'--test-dir',b,'--show-only=json-v1','-R','^cs80_fx_loudness_(core|gate)_tests$']),('test',[str(t/'ctest'),'--test-dir',b,'--verbose','-j1','-R','^cs80_fx_loudness_(core|gate)_tests$'])]
for label,cmd in commands:
 if label=='test':
  names=[x['name']for x in json.loads((e/'list.log').read_text())['tests']];assert sorted(names)==['cs80_fx_loudness_core_tests','cs80_fx_loudness_gate_tests'],names;m['selected_tests']=names
 with(e/(label+'.log')).open('w')as f:r=subprocess.run(cmd,cwd=p,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
 m['steps'].append({'name':label,'command':cmd,'exit':r.returncode,'log_sha256':h(e/(label+'.log'))});(e/'result.json').write_text(json.dumps(m,indent=2)+'\n')
 if r.returncode:raise SystemExit('Stopped after '+label)
m['source_unchanged']=all(h(p/f)==v for f,v in m['source_sha256'].items());m['binary_sha256']={f:h(p/b/f)for f in ['cs80_fx_loudness_core_tests','cs80_fx_loudness_gate_tests']};m['completed_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();(e/'result.json').write_text(json.dumps(m,indent=2)+'\n');print((e/'test.log').read_text())
