#!/usr/bin/env python3
"""Inert fixtures only: never a native or release acceptance."""
import copy, hashlib, importlib.util, io, json, subprocess, sys, tempfile, unittest, zipfile
from pathlib import Path
from xml.sax.saxutils import escape
HERE=Path(__file__).resolve().parent
REPO=HERE.parents[4]
SOURCE='fcc3c8e055223fe4f786ca162ff46076c33471c8'
RUN=45366603800

def module(name,file):
 s=importlib.util.spec_from_file_location(name,HERE/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
P=module('a52_parser','parse-native-evidence.py');B=module('a52_binder','bind-parser-archives.py')

def native_files():
 body="test_optional (fixture) ... skipped 'synthetic'\n\nRan 4 tests in 0.001s\n\nOK (skipped=1)\n"
 xml='<testsuite tests="3" failures="0" skipped="1" disabled="0" time="0">'
 last=''
 for i in range(3):
  name=f'fixture_{i}';out=body if i==0 else 'inert fixture\n';skip='<skipped message="platform fixture" />' if i==1 else ''
  xml+=f'<testcase name="{name}" status="run" time="0">{skip}<system-out>{escape(out)}</system-out></testcase>'
  last+=f'{i+1}/3 Test: {name}\nOutput:\n----\n{out}<end of output>\nTest Passed.\n'
 return {'ctest-results.xml':xml+'</testsuite>','Testing/Temporary/LastTest.log':last,'ci-juce-provenance.txt':'JUCE fixture only\n'}

def lane_log(gui,seed):
 return 'Validation started\nStrictness level: 10\nRandom seed: '+seed+'\nNum programs: 523\n'+('Starting tests in: pluginval / Editor...\n' if gui else '')+'Completed tests in: pluginval\nSUCCESS\n'

def validator_files(target):
 files={}
 for lane,step,console,report,gui,seed in P.LANES[target]:
  text=lane_log(gui,seed or '0xabcdef');files[console]=text;files['pluginval-reports/'+report]=text
  if target=='linux-x86_64':files[console[:-4]+'-core.log']='Original validator termination: PID=123; raw returncode=0; kind=exit; exit_code=0\n'
 if target=='linux-x86_64':files['pluginval-version.txt']='pluginval - 1.0.4\n'
 else:files['pluginval-download.json']=json.dumps({'version':'1.0.4','verified':True})
 return files

def write_zip(path,files):
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
  for n,b in files.items():z.writestr(n,b)
 return path.read_bytes()

def api(raw,name,number):
 route=f'https://api.github.com/repos/{B.REPOSITORY}/actions/artifacts/{number}'
 return dict(id=number,name=name,url=route,archive_download_url=route+'/zip',size_in_bytes=len(raw),digest='sha256:'+hashlib.sha256(raw).hexdigest(),expired=False,workflow_run=dict(id=RUN,head_sha=SOURCE))

class EvidenceTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
 def tearDown(self):self.temp.cleanup()
 def parse_native(self,files=None):
  p=self.root/'native.zip';write_zip(p,files or native_files());return P.native(dict(target='macos-arm64',archive=str(p)),self.root)
 def parse_validator(self,target='macos-arm64',files=None):
  p=self.root/'validator.zip';write_zip(p,files or validator_files(target));return P.validators(dict(target=target,archive=str(p)),self.root)
 def test_full_roster_and_unknown_exits(self):
  total=0
  for target in P.TARGETS:
   n=self.parse_native();self.assertEqual(n['counts'],dict(registered=3,passed=2,failed=0,formal_skipped=1,disabled=0,unknown=0))
   self.assertEqual(n['nested_python_skips']['total'],1);self.assertIsNone(n['ctest_process_exit'])
   v=self.parse_validator(target);self.assertEqual(v['status'],'observed_logs_green_unbound');total+=v['required_lanes']
   self.assertTrue(all(x['original_process_exit'] is None for x in v['lanes']));self.assertFalse(v['qualification'])
  self.assertEqual(total,15)
 def test_missing_corrupt_and_disagreeing_native(self):
  mutations=[]
  f=native_files();del f['Testing/Temporary/LastTest.log'];mutations.append(f)
  f=native_files();f['ctest-results.xml']='<broken';mutations.append(f)
  f=native_files();f['ctest-results.xml']='<!DOCTYPE bad>'+f['ctest-results.xml'];mutations.append(f)
  f=native_files();f['ctest-results.xml']=f['ctest-results.xml'].replace('tests="3"','tests="4"');mutations.append(f)
  f=native_files();f['Testing/Temporary/LastTest.log']=f['Testing/Temporary/LastTest.log'].replace('Test Passed.','Test Failed.',1);mutations.append(f)
  f=native_files();f['Testing/Temporary/LastTest.log']*=2;mutations.append(f)
  f=native_files();f['ctest-results.xml']=f['ctest-results.xml'].replace('skipped="1"','skipped="0"');mutations.append(f)
  for i,f in enumerate(mutations):
   with self.subTest(mutation=i),self.assertRaises((ValueError,P.ET.ParseError)):self.parse_native(f)
 def test_failure_is_observed_not_accepted(self):
  f=native_files();f['ctest-results.xml']=f['ctest-results.xml'].replace('failures="0"','failures="1"').replace('<system-out>','<failure message="fixture"/><system-out>',1)
  f['Testing/Temporary/LastTest.log']=f['Testing/Temporary/LastTest.log'].replace('Test Passed.','Test Failed.',1)
  n=self.parse_native(f);self.assertEqual(n['status'],'observed_test_failure_or_unknown');self.assertEqual(n['counts']['failed'],1)
 def test_fifth_lane_and_start_markers(self):
  for target in ('macos-arm64','macos-x86_64'):
   f=validator_files(target);del f['pluginval-au-seed-c76-regression.log']
   with self.assertRaises(ValueError):self.parse_validator(target,f)
   for change in (lambda s:s.replace('0x3c99748','0x1'),lambda s:s.replace('Validation started\n',''),lambda s:'Validation started\n'+s,lambda s:s+'Abort trap: 6\n',lambda s:s.replace('level: 10','level: 9'),lambda s:s.replace('programs: 523','programs: 522'),lambda s:s.replace('Starting tests in: pluginval / Editor...\n','')):
    f=validator_files(target)
    for key in ('pluginval-au-seed-c76-regression.log','pluginval-reports/cs80-au-seed-c76-regression'):f[key]=change(f[key])
    self.assertEqual(self.parse_validator(target,f)['status'],'observed_log_failure_or_incomplete')
 def test_linux_original_child_crash_and_retry_gap(self):
  f=validator_files('linux-x86_64');f['pluginval-headless-core.log']='Original validator termination: PID=123; raw returncode=-11; kind=signal; signal=SIGSEGV\n'
  self.assertEqual(self.parse_validator('linux-x86_64',f)['status'],'observed_log_failure_or_incomplete')
  f=validator_files('linux-x86_64');f['pluginval-headless-attempt2.log']='FAILED\n';f['pluginval-headless-core.log']='Original validator termination: PID=122; raw returncode=-11; kind=signal; signal=SIGSEGV\n'+f['pluginval-headless-core.log']
  self.assertEqual(self.parse_validator('linux-x86_64',f)['status'],'observed_log_failure_or_incomplete')
 def test_parser_and_exact_archives_members_api_bind(self):
  n=self.parse_native();v=self.parse_validator();nr=(self.root/'native.zip').read_bytes();vr=(self.root/'validator.zip').read_bytes()
  obs=dict(release_qualified=False,parser=P.record((HERE/'parse-native-evidence.py').read_bytes(),HERE/'parse-native-evidence.py'),native=[n],validators=[v])
  na=api(nr,B.ARTIFACTS['macos-arm64'][0],1);va=api(vr,B.ARTIFACTS['macos-arm64'][1],2)
  def bind(o=obs,native_raw=nr,native_api=na,validator_raw=vr,validator_api=va):return B.bind(o,'macos-arm64',native_raw,native_api,validator_raw,validator_api,SOURCE,RUN)
  self.assertEqual(len(bind()),2)
  for key in ('sha256','bytes'):
   bad=copy.deepcopy(obs);bad['parser'][key]='wrong'
   with self.assertRaises(ValueError):bind(o=bad)
  bad=copy.deepcopy(obs);del bad['native'][0]['juce_provenance']
  with self.assertRaises(ValueError):bind(o=bad)
  bad=copy.deepcopy(obs);bad['native'][0]['archive']['sha256']='0'*64
  with self.assertRaises(ValueError):bind(o=bad)
  bad=copy.deepcopy(obs);bad['validators'][0]['lanes'][-1]['console']['sha256']='0'*64
  with self.assertRaises(ValueError):bind(o=bad)
  bad=copy.deepcopy(va);bad['workflow_run']['head_sha']='0'*40
  with self.assertRaises(ValueError):bind(validator_api=bad)
  with self.assertRaises(ValueError):bind(validator_raw=vr+b'changed')
  bad=copy.deepcopy(na);bad['digest']='sha256:'+'0'*64
  with self.assertRaises(ValueError):bind(native_api=bad)
 def test_cli_scope_and_binder_source_identity(self):
  n=self.parse_native();v=self.parse_validator();nr=(self.root/'native.zip').read_bytes();vr=(self.root/'validator.zip').read_bytes()
  cfg=dict(native=[dict(target='macos-arm64',archive='native.zip')],validators=[dict(target='macos-arm64',archive='validator.zip')])
  ip=self.root/'input.json';op=self.root/'observations.json';ip.write_text(json.dumps(cfg))
  command=[sys.executable,str(HERE/'parse-native-evidence.py'),'--inputs',str(ip),'--out',str(op)]
  self.assertEqual(subprocess.run(command,capture_output=True).returncode,0)
  o=json.loads(op.read_text());self.assertFalse(o['release_qualified']);self.assertEqual(o['validator_totals']['required_lanes'],5)
  self.assertNotEqual(subprocess.run(command,capture_output=True).returncode,0)
  self.assertNotEqual(subprocess.run(command[:-1]+[str(self.root/'complete.json'),'--require-complete'],capture_output=True).returncode,0)
  np=self.root/'native-api.json';vp=self.root/'validator-api.json';np.write_text(json.dumps(api(nr,B.ARTIFACTS['macos-arm64'][0],1)));vp.write_text(json.dumps(api(vr,B.ARTIFACTS['macos-arm64'][1],2)))
  bp=self.root/'binding.json'
  cmd=[sys.executable,str(HERE/'bind-parser-archives.py'),'--repo',str(REPO),'--source',SOURCE,'--run-id',str(RUN),'--attempt','1','--target','macos-arm64','--observations',str(op),'--native-zip',str(self.root/'native.zip'),'--native-api',str(np),'--validator-zip',str(self.root/'validator.zip'),'--validator-api',str(vp),'--out',str(bp)]
  result=subprocess.run(cmd,capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stderr)
  altered=self.root/'altered-parser.py';altered.write_bytes((HERE/'parse-native-evidence.py').read_bytes()+b'\n# mutation\n')
  badcmd=cmd[:-1]+[str(self.root/'bad-binding.json'),'--parser',str(altered)]
  self.assertNotEqual(subprocess.run(badcmd,capture_output=True).returncode,0)
  self.assertFalse((self.root/'bad-binding.json').exists())
  b=json.loads(bp.read_text());self.assertTrue(b['parser_archive_binding_verified'])
  for key in ('native_results_accepted','original_attempt_binding_verified','platform_qualified','release_qualified'):self.assertFalse(b[key])

if __name__=='__main__':unittest.main(verbosity=2)
