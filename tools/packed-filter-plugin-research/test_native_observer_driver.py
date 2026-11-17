"""Unwrapped standalone driver with real inert library; bounded request rejection."""
import hashlib,json,os,pathlib,shutil,struct,subprocess,sys,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
U=lambda n:struct.pack('<Q',n)
T=lambda s:U(len(s.encode()))+s.encode()
H=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
class DriverTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  sdk=os.environ.get('CS80_OBSERVER_SDK')
  if not sdk:raise unittest.SkipTest('Explicit pinned SDK required')
  for name in ('native_observer_sdk_provenance.json','native_observer_initializer_provenance.json','native_observer_render_provenance.json','native_observer_runloop_provenance.json'):
   m=json.loads((ROOT/name).read_text())
   for path,wanted in {**m.get('headers',{}),**m.get('additional_headers',{})}.items():
    if H(pathlib.Path(sdk)/path)!=wanted:raise RuntimeError('SDK drift '+path)
  for path,wanted in json.loads((ROOT/'workload_bridge_provenance.json').read_text())['local_sources'].items():
   if H(ROOT.parents[1]/path)!=wanted:raise RuntimeError('Workload drift '+path)
  cls.temp=tempfile.TemporaryDirectory();cls.root=pathlib.Path(cls.temp.name);cls.exe=cls.root/'driver';cls.lib=cls.root/'fixture.so';cls.parser=cls.root/'parser'
  parser_source=cls.root/'parse.cpp';parser_source.write_text('#define INIT_CLASS_IID\n#include "native_observer_request.hpp"\nint main(int argc,char** argv){try{if(argc!=3)return 1;auto q=cs80_observer::request::read(argv[1],argv[2]);return q.identity.arm.empty()?1:0;}catch(...){return 2;}}\n')
  from test_process_lifetime_transaction import NATIVE_EXPECTED
  probe_source=cls.root/'expected.cpp';cls.expected=cls.root/'expected'
  probe_source.write_text(NATIVE_EXPECTED)
  flags=['g++','-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-ffp-contract=off','-I',sdk,'-I',str(ROOT.parents[1]/'src')]
  for cmd in ([*flags,'-I',str(ROOT),str(probe_source),'-lcrypto','-o',str(cls.expected)],[*flags,'-I',str(ROOT),str(parser_source),'-ldl','-lcrypto','-o',str(cls.parser)],[*flags,str(ROOT/'native_observer_driver.cpp'),'-ldl','-lcrypto','-o',str(cls.exe)],[*flags,'-shared','-fPIC',str(ROOT/'native_observer_module_history_library.cpp'),'-o',str(cls.lib)]):
   p=subprocess.run(cmd,capture_output=True,text=True,timeout=60)
   if p.returncode:raise RuntimeError(p.stderr)
  cls.sourceLib=cls.root/'source-fixture.so';source=cls.root/'source-fixture.cpp'
  text=(ROOT/'native_observer_module_history_library.cpp').read_text().replace('static std::vector<ParamID> fullIds;','static std::vector<ParamID> fullIds; static std::vector<SourceParameterExpectation> sourceRows;')
  text=text.replace('for(const auto& q:packed_filter_workload::controls(c))fullIds.push_back(q.id);','sourceRows=sourceParameterExpectations();for(const auto& q:sourceRows)fullIds.push_back(q.info.id);(void)c;')
  text=text.replace("p={};p.id=fullIds.at(static_cast<std::size_t>(index));p.title[0]='P';p.defaultNormalizedValue=defaultNormalized;",'p=sourceRows.at(static_cast<std::size_t>(index)).info;')
  text=text.replace('ParamValue PLUGIN_API getParamNormalized(ParamID) override {return normalized;}','ParamValue PLUGIN_API getParamNormalized(ParamID id) override {for(const auto& q:sourceRows)if(q.info.id==id)return q.info.defaultNormalizedValue;throw std::runtime_error("unknown fake param");}')
  source.write_text(text)
  p=subprocess.run([*flags,'-I',str(ROOT),'-shared','-fPIC',str(source),'-o',str(cls.sourceLib)],capture_output=True,text=True,timeout=60)
  if p.returncode:raise RuntimeError(p.stderr)
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def setUp(self):
  self.work=self.root/self._testMethodName;self.work.mkdir();self.bundle=self.work/'bundle';self.bundle.mkdir();shutil.copy2(self.lib,self.bundle/'module.so');self.req=self.work/'request';self.out=self.work/'receipt'
 def encoded(self,**kw):
  def val(k,d):return kw.get(k,d)
  def root(p):
   s=p.stat();return T(str(p))+U(s.st_mode&0o777)+U(s.st_uid)+U(s.st_gid)
  def entry(p,name,directory):
   s=p.stat();return T(name)+U(directory)+U(s.st_mode&0o777)+U(s.st_uid)+U(s.st_gid)+U(0 if directory else s.st_size)+T('' if directory else H(p))
  data=T(val('schema','cs80-process-lifetime-request-v1'))+T(val('profile','process-lifetime'))+T(val('arm','stock'))+T(val('cid','05'*16))
  data+=b''.join(U(val(k,d)) for k,d in [('policy',0),('rate',48000),('mode',0),('voices',8),('block',64),('state',1)]+([('query',1)] if val('schema','cs80-process-lifetime-request-v1')in ('cs80-process-lifetime-request-v2','cs80-process-lifetime-request-v3','cs80-process-lifetime-request-v4') else [])+([('topology',1)] if val('schema','cs80-process-lifetime-request-v1')in ('cs80-process-lifetime-request-v3','cs80-process-lifetime-request-v4') else [])+([('binding',1)] if val('schema','cs80-process-lifetime-request-v1')=='cs80-process-lifetime-request-v4' else [])+[('timeout',5000),('entries',64),('files',64),('bytes',8*1024*1024),('paths',4096)])
  data+=root(self.bundle)+T('module.so')+U(2)+entry(self.bundle,'.',1)+entry(self.bundle/'module.so','module.so',0)+U(0)+U(0)
  return data
 def run_driver(self,data=None,hash_override=None,nonce='1'*64):
  self.req.write_bytes(self.encoded() if data is None else data);self.req.chmod(0o600)
  return subprocess.run([str(self.exe),str(self.req),hash_override or H(self.req),nonce,str(self.out)],capture_output=True,timeout=15)
 def test_actual_inert_success(self):
  p=self.run_driver();self.assertEqual(p.returncode,0,p.stderr);self.assertTrue(self.out.is_file());b=self.out.read_bytes();n=struct.unpack_from('<Q',b)[0];self.assertEqual(len(b),n+80);self.assertEqual(b[-64:].decode(),hashlib.sha256(b[8:8+n]).hexdigest());self.assertIn(b'cs80-process-lifetime-child-v4',b);self.assertTrue(b'provider_retained' in b or b'provider_removed' in b)
 def test_candidate_native_arm(self):
  p=self.run_driver(self.encoded(arm='candidate'));self.assertEqual(p.returncode,0,p.stderr);self.assertIn(b'candidate',self.out.read_bytes())
 def test_parse_all_108_frozen_cases(self):
  for policy in range(3):
   for rate in (44100,48000,96000):
    for mode in (0,1):
     for voices in (8,16):
      for block in (64,256,1024):
       self.req.write_bytes(self.encoded(policy=policy,rate=rate,mode=mode,voices=voices,block=block));self.req.chmod(0o600)
       p=subprocess.run([str(self.parser),str(self.req),H(self.req)],capture_output=True,timeout=5);self.assertEqual(p.returncode,0,(policy,rate,mode,voices,block))
  self.assertFalse(self.out.exists())
 def public_contract(self,source_aware=False):
  from process_lifetime_transaction import process_lifetime_arm,SCHEMA,PROFILE,HISTORY
  import process_lifetime_transaction as impl
  from execution import Commands
  if source_aware:shutil.copy2(self.sourceLib,self.bundle/'module.so')
  self.req.write_bytes(self.encoded(schema='cs80-process-lifetime-request-v4',binding=2) if source_aware else self.encoded());self.req.chmod(0o600)
  module=self.bundle/'module.so';s=self.bundle.stat();f=module.stat();text_manifest=self.work/'probe-manifest'
  text_manifest.write_text(f'{json.dumps(str(self.bundle))} {s.st_mode&0o777} {s.st_uid} {s.st_gid}\n"module.so"\n2\n"." 1 {s.st_mode&0o777} {s.st_uid} {s.st_gid} 0 ""\n"module.so" 0 {f.st_mode&0o777} {f.st_uid} {f.st_gid} {f.st_size} "{H(module)}"\n')
  probe=subprocess.run([str(self.expected),str(text_manifest)],capture_output=True,text=True,check=True,timeout=10).stdout.splitlines();self.assertEqual(len(probe),6)
  helper=pathlib.Path(impl.__file__).resolve();cwd=self.work/'cwd';cwd.mkdir()
  c={'parameter_binding_policy':1,'audio_input_topology':1,'controller_query_policy':1,'controller_query_result':0,'controller_query_pointer_present':True,'arm':'stock','schema':SCHEMA,'profile':PROFILE,'history_schema':HISTORY,'controller_state_policy':1,'controller_state_result':0,'selected_cid_hex':'05'*16,'controller_cid_hex':None,'case_identity_hex':probe[2],'protocol_sha256':probe[3:],'expected_manifest_digest':probe[0],'expected_evidence_digest':probe[1],'manifest':str(self.req),'manifest_sha256':H(self.req),'driver':str(self.exe),'runtime_hashes':{str(self.exe):H(self.exe)},'helper_hashes':{str(helper):H(helper)},'inputs':[],'module_root':str(self.bundle),'module_relative':'module.so','module_sha256':H(module),'module_hashes':{'module.so':H(module)},'module_directories':[],'module_modes':{'.':s.st_mode&0o777,'module.so':f.st_mode&0o777},'module_entry_cap':16,'module_byte_cap':8*1024**2,'receipt_cap':8*1024**2,'timeout':15,'cwd':str(cwd),'output':str(self.work/'output'),'environment':{'PATH':'/usr/bin:/bin','HOME':str(cwd),'LANG':'C'}}
  if source_aware:
   c['parameter_binding_policy']=2;c['history_schema']='cs80-instance-history-A-D-v2'
   c['helper_hashes'].update({str((ROOT/name).resolve()):H(ROOT/name) for name in ('source_parameter_proof.py','source_parameter_roles.py','failure_metadata_decode.py')})
  return c,Commands(self.work/'logs',total_seconds=30)
 def test_public_supervisor_integration(self):
  from process_lifetime_transaction import process_lifetime_arm
  c,commands=self.public_contract();result=process_lifetime_arm(c,commands)
  self.assertTrue(result['process_completed']);self.assertFalse(result['production_qualified']);self.assertEqual(result['actual_pid'],commands.ledger[0]['pid']);self.assertEqual(H(pathlib.Path(result['full_payload_path'])),result['payload_sha256'])
 def test_public_source_aware_supervisor_integration(self):
  from process_lifetime_transaction import process_lifetime_arm
  c,commands=self.public_contract(source_aware=True);result=process_lifetime_arm(c,commands)
  self.assertTrue(result['process_completed']);self.assertFalse(result['production_qualified'])
 def test_public_request_query_policy_mismatch(self):
  from process_lifetime_transaction import process_lifetime_arm
  c,commands=self.public_contract();c['controller_query_policy']=2
  with self.assertRaisesRegex(ValueError,'prelaunch controller query policy mismatch'):process_lifetime_arm(c,commands)
  self.assertFalse(any('argv' in row for row in commands.ledger))
 def test_public_request_topology_mismatch(self):
  from process_lifetime_transaction import process_lifetime_arm
  c,commands=self.public_contract();c['audio_input_topology']=2
  with self.assertRaisesRegex(ValueError,'prelaunch audio input topology mismatch'):process_lifetime_arm(c,commands)
  self.assertFalse(any('argv' in row for row in commands.ledger))
 def test_v4_strict_binding(self):
  p=self.run_driver(self.encoded(schema='cs80-process-lifetime-request-v4',binding=1));self.assertEqual(p.returncode,0,p.stderr)
 def test_v4_bad_binding_zero(self):self.bad(self.encoded(schema='cs80-process-lifetime-request-v4',binding=0))
 def test_v4_bad_binding_three(self):self.bad(self.encoded(schema='cs80-process-lifetime-request-v4',binding=3))
 def test_v4_source_aware_rejects_legacy_fixture(self):
  p=self.run_driver(self.encoded(schema='cs80-process-lifetime-request-v4',binding=2));self.assertEqual(p.returncode,24,p.stderr);self.assertIn(b'source-aware complete native roster',p.stderr);self.assertIn(b'source-aware-roster-bind',self.out.read_bytes())
 def test_public_request_binding_mismatch(self):
  from process_lifetime_transaction import process_lifetime_arm
  c,commands=self.public_contract();c['parameter_binding_policy']=2;c['history_schema']='cs80-instance-history-A-D-v2'
  c['helper_hashes'].update({str((ROOT/name).resolve()):H(ROOT/name) for name in ('source_parameter_proof.py','source_parameter_roles.py','failure_metadata_decode.py')})
  with self.assertRaisesRegex(ValueError,'prelaunch parameter binding policy mismatch'):process_lifetime_arm(c,commands)
  self.assertFalse(any('argv' in row for row in commands.ledger))
 def test_v3_legacy_topology_request(self):
  p=self.run_driver(self.encoded(schema='cs80-process-lifetime-request-v3',topology=1));self.assertEqual(p.returncode,0,p.stderr)
 def test_v3_bad_topology_zero(self):self.bad(self.encoded(schema='cs80-process-lifetime-request-v3',topology=0))
 def test_v3_bad_topology_three(self):self.bad(self.encoded(schema='cs80-process-lifetime-request-v3',topology=3))
 def test_v2_juce_query_request(self):
  p=self.run_driver(self.encoded(schema='cs80-process-lifetime-request-v2',query=2));self.assertEqual(p.returncode,0,p.stderr)
 def test_v2_bad_query_zero(self):self.bad(self.encoded(schema='cs80-process-lifetime-request-v2',query=0))
 def test_v2_bad_query_three(self):self.bad(self.encoded(schema='cs80-process-lifetime-request-v2',query=3))
 def bad(self,data):
  p=self.run_driver(data);self.assertEqual(p.returncode,2,p.stderr);self.assertFalse(self.out.exists());self.assertIn(b'CS80_REQUEST_REJECTED',p.stderr)
 def test_trailing(self):self.bad(self.encoded()+b'x')
 def test_truncated(self):self.bad(self.encoded()[:-1])
 def test_length_overflow(self):self.bad(U(2**64-1))
 def test_nul(self):self.bad(self.encoded(arm='stock\0'))
 def test_bad_hash(self):
  p=self.run_driver(hash_override='0'*64);self.assertEqual(p.returncode,2);self.assertFalse(self.out.exists())
 def test_bad_nonce_no_load(self):
  p=self.run_driver(nonce='A'*64);self.assertEqual(p.returncode,24);self.assertFalse(self.out.exists())
 def test_symlink(self):
  target=self.work/'target';target.write_bytes(self.encoded());target.chmod(0o600);self.req.symlink_to(target)
  p=subprocess.run([str(self.exe),str(self.req),H(target),'1'*64,str(self.out)],capture_output=True,timeout=15);self.assertEqual(p.returncode,2);self.assertFalse(self.out.exists())
 def reject_file(self,path,expected,sha=None):
  p=subprocess.run([str(self.exe),str(path),sha or H(path),'1'*64,str(self.out)],capture_output=True,timeout=5);self.assertEqual(p.returncode,2);self.assertFalse(self.out.exists());self.assertIn(expected.encode(),p.stderr)
 def test_ancestor_symlink(self):
  real=self.work/'real';real.mkdir();target=real/'request';target.write_bytes(self.encoded());target.chmod(0o600);alias=self.work/'alias';alias.symlink_to(real,target_is_directory=True);self.reject_file(alias/'request','root ancestor unavailable/symlink')
 def test_hardlinked_request(self):
  self.req.write_bytes(self.encoded());self.req.chmod(0o600);os.link(self.req,self.work/'other');self.reject_file(self.req,'request regular owned private file')
 def test_fifo_request(self):
  os.mkfifo(self.req,0o600);self.reject_file(self.req,'request regular owned private file','0'*64)
 def test_oversized_request(self):
  with self.req.open('wb') as f:f.truncate(8*1024*1024+1)
  self.req.chmod(0o600);self.reject_file(self.req,'request regular owned private file')
 def test_wrong_permissions(self):
  self.req.write_bytes(self.encoded());self.req.chmod(0o644)
  p=subprocess.run([str(self.exe),str(self.req),H(self.req),'1'*64,str(self.out)],capture_output=True,timeout=15);self.assertEqual(p.returncode,2);self.assertFalse(self.out.exists())
for k,v in [('schema','other'),('profile','strict-unmap'),('arm','any'),('cid','0'*32),('cid','F'*32),('policy',3),('rate',12345),('mode',2),('voices',9),('block',65),('state',0),('timeout',0),('timeout',600001),('entries',16385),('files',0),('bytes',0),('paths',4097)]:
 setattr(DriverTests,'test_bad_'+k+'_'+str(v),lambda self,k=k,v=v:self.bad(self.encoded(**{k:v})))
if __name__=='__main__':unittest.main()
