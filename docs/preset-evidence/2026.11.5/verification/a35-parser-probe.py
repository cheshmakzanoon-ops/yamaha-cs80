import importlib.util,json,tempfile,sys,hashlib
from pathlib import Path
from xml.sax.saxutils import escape
repo=Path('/workspace/scratch/c98fb40a9529/yamaha-cs80')
path=repo/'docs/preset-evidence/2026.11.4/ci/source-078c098/parse-native-evidence.py'
spec=importlib.util.spec_from_file_location('historical_native_parser',path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
root=Path(tempfile.mkdtemp(prefix='fixture-',dir='/tmp/cs80-a35'))
checks=0;observations={}
def check(ok,label):
 global checks
 checks+=1
 if not ok:raise AssertionError(label)
def reject(fn,label):
 try:fn()
 except (ValueError,KeyError,OSError):check(True,label);return
 raise AssertionError(label+' was accepted')
def native_fixture(name,n,skip=False):
 d=root/name;d.mkdir();(d/'Testing/Temporary').mkdir(parents=True)
 xml=[];last=[]
 for i in range(n):
  key=f'fixture_{i:04d}';disabled=skip and i==2;formal=skip and i==1
  body="test_optional (fixture) ... skipped 'fixture'\n\nRan 4 tests in 0.001s\n\nOK (skipped=1)\n" if skip and i==0 else 'synthetic fixture only\n'
  st='disabled' if disabled else 'notrun' if formal else 'run'
  marker='<skipped message="disabled" />' if disabled else '<skipped message="optional fixture" />' if formal else ''
  xml.append(f'<testcase name="{key}" status="{st}" time="0">{marker}<system-out>{escape(body)}</system-out></testcase>')
  if not disabled:last.append(f'{i+1}/{n} Test: {key}\nOutput:\n----\n{body}<end of output>\nTest Passed.\n')
 (d/'ctest-results.xml').write_text(f'<testsuite tests="{n}" failures="0" skipped="{int(skip)}" disabled="{int(skip)}" time="0">'+''.join(xml)+'</testsuite>')
 (d/'Testing/Temporary/LastTest.log').write_text(''.join(last));return d
for target,n in [('windows-x86_64',244),('macos-arm64',266),('macos-x86_64',266),('linux-x86_64',602)]:
 d=native_fixture(target,n);r=m.native({'target':target,'directory':str(d)},root)
 check(r['counts']['registered']==n and r['counts']['passed']==n,target+' dynamic count')
 check(r['ctest_process_exit'] is None,target+' exit stays unknown')
 check(r['status']=='observed_tests_green',target+' parsed synthetic count')
 observations[target]=r['counts']
d=native_fixture('skip-classification',5,True);r=m.native({'target':'linux-x86_64','directory':str(d)},root)
check(r['counts']=={'registered':5,'passed':3,'failed':0,'formal_skipped':1,'disabled':1,'unknown':0},'separate formal/disabled counts')
check(r['nested_python_skips']['total']==1,'nested summary counted once despite JUnit and explicit copies')
check(r['status']=='observed_tests_green','scope allows separately recorded skips/disabled')
old=(d/'Testing/Temporary/LastTest.log').read_text();(d/'Testing/Temporary/LastTest.log').write_text(old.replace('Test Passed.','Test Failed.',1))
reject(lambda:m.native({'target':'linux-x86_64','directory':str(d)},root),'passing JUnit disagrees with LastTest')
(d/'Testing/Temporary/LastTest.log').write_text(old+old)
reject(lambda:m.native({'target':'linux-x86_64','directory':str(d)},root),'duplicate LastTest execution')
empty=native_fixture('empty',0);e=m.native({'target':'linux-x86_64','directory':str(empty)},root)
check(e['status']=='observed_tests_green' and e['counts']['registered']==0,'documented expected-roster external gate')
observations['expected_roster_is_external']=True;observations['missing_juce_provenance_is_optional']=('juce_provenance' not in e)

def lane_log(gui,seed):
 return 'Validation started\nStrictness level: 10\nRandom seed: '+seed+'\nNum programs: 523\n'+('Starting tests in: pluginval / Editor...\n' if gui else '')+'Completed tests in: pluginval\nSUCCESS\n'
def validator_fixture(target):
 d=root/('v-'+target);d.mkdir();(d/'pluginval-reports').mkdir()
 for lane,step,console,report,gui,seed in m.LANES[target]:
  text=lane_log(gui,seed or '0xabcdef')
  (d/console).write_text(text);(d/'pluginval-reports'/report).write_text(text)
  if target=='linux-x86_64':(d/(console[:-4]+'-core.log')).write_text('Original validator termination: PID=123; raw returncode=0; kind=exit; exit_code=0\n')
 if target=='linux-x86_64':(d/'pluginval-version.txt').write_text('pluginval - 1.0.4\n')
 else:(d/'pluginval-download.json').write_text(json.dumps({'version':'1.0.4','verified':True}))
 return d
vs={};total=0
for target in m.TARGETS:
 d=validator_fixture(target);vs[target]=d;r=m.validators({'target':target,'directory':str(d)},root)
 check(r['status']=='observed_logs_green_unbound',target+' 13-lane mapping compatibility')
 check(all(x['original_process_exit'] is None for x in r['lanes']),target+' original exits not inferred')
 total+=r['required_lanes']
check(total==13,'13 required lanes across targets')
d=vs['linux-x86_64'];core=d/'pluginval-headless-core.log';zero=core.read_text()
core.write_text('Original validator termination: PID=123; raw returncode=-11; kind=signal; signal=SIGSEGV\n')
r=m.validators({'target':'linux-x86_64','directory':str(d)},root)
check(r['status']=='observed_log_failure_or_incomplete','SUCCESS followed by actual child crash rejected')
core.write_text(zero)
console=d/'pluginval-gui-seed-regression.log';report=d/'pluginval-reports/cs80-gui-seed-regression';original=console.read_text()
for label,bad in [('strictness',original.replace('level: 10','level: 9')),('program count',original.replace('programs: 523','programs: 522')),('fixed seed',original.replace('0x2da72db','0x1')),('GUI coverage',original.replace('Starting tests in: pluginval / Editor...\n','')),('terminal success',original+'post-success failure\n')]:
 console.write_text(bad);report.write_text(bad);r=m.validators({'target':'linux-x86_64','directory':str(d)},root)
 check(r['status']=='observed_log_failure_or_incomplete',label+' mismatch rejected')
console.write_text(original);report.write_text(original)
prior=d/'pluginval-headless-attempt1.log';prior.write_text('Starting tests in: pluginval / fixture...\nCompleted tests in: pluginval / fixture\n')
core.write_text('Original validator termination: PID=122; raw returncode=-11; kind=signal; signal=SIGSEGV\n'+zero)
r=m.validators({'target':'linux-x86_64','directory':str(d)},root);head=r['lanes'][0]
check(head['validation_retries']==1 and head['validation_invocations']==2,'retry counts actual preserved attempt')
check(head['retry_eligibility_requires_manual_job_log_review'] is True,'eligibility remains explicit manual gate')
prior.write_text('FAILED\n')
r=m.validators({'target':'linux-x86_64','directory':str(d)},root)
check(r['status']=='observed_logs_green_unbound','documented prior eligibility external gate')
observations['prior_retry_eligibility_is_external']=True
prior.rename(d/'pluginval-headless-attempt2.log');r=m.validators({'target':'linux-x86_64','directory':str(d)},root)
check(r['status']=='observed_log_failure_or_incomplete','retry gap rejected')
(root/'probe-results.json').write_text(json.dumps({'scope':'Synthetic parser semantics only; no .5 native evidence or release is accepted.','checks':checks,'failures':0,'parser_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'observations':observations},indent=2)+'\n')
print(json.dumps({'synthetic_checks':checks,'failures':0,'result_path':str(root/'probe-results.json'),'observations':observations},indent=2))
