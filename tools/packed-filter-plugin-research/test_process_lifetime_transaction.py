"""Supervised tiny inert children. Native SO integration is separately gated."""
import hashlib,json,os,pathlib,shutil,struct,subprocess,sys,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
sys.path.insert(0,str(ROOT.parent/'production-pic-pgo-research'))
from execution import Commands
from process_lifetime_transaction import _process_lifetime,process_lifetime_arm,SCHEMA,LEGACY_SCHEMA,PROFILE,HISTORY,FRAME_CAP
H=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
B=lambda s:struct.pack('<Q',len(s))+s
T=lambda s:B(s.encode())
U=lambda x:struct.pack('<Q',x)
V=lambda x:struct.pack('<I',x)
CHILD=r'''import hashlib,json,os,struct,sys,time
p,mode,nonce,output=sys.argv[1:];m=json.load(open(p));bad=m.get('mutation','')
u=lambda n:struct.pack('<Q',n)
v=lambda n:struct.pack('<I',n)
b=lambda s:u(len(s))+s
t=lambda s:b(s.encode())
h=lambda s:hashlib.sha256(s).hexdigest()
if bad=='missing':sys.exit(0)
if bad=='symlink':os.symlink(p,output);sys.exit(0)
if bad=='fifo':os.mkfifo(output,0o600);sys.exit(0)
if bad=='directory':os.mkdir(output);sys.exit(0)
fd=os.open(output,os.O_CREAT|os.O_EXCL|os.O_WRONLY|os.O_NOFOLLOW,0o600)
case=bytes.fromhex(m['case_identity_hex'])
state=t('cs80-native-state-v2')+v(2 if bad=='policy' else 1)+v(0)+b(b'saved')+v(0)+b(b'ctrl')
connected=t('native-observer-connected-v1')+t('native-observer-created-identity-v1')+bytes.fromhex(('06'*16) if bad=='cid' else m['selected_cid_hex'])+v(1 if m.get('controller_cid_hex') else 0)+(bytes.fromhex(m['controller_cid_hex']) if m.get('controller_cid_hex') else b'')
protocols=[b'changed' if bad=='protocol' else b'initial',b'alter',(b'CS80RPRT'+v(2)+v(2)+b'inert') if m.get('topology',1)==2 else b'CS80RPRT'+v(1)+b'inert']
if m.get('render_override'):protocols[2]=bytes.fromhex(m['render_override'])
payload=t('cs80-instance-history-A-D-v1')+b(b'wrong' if bad=='payload-case' else case)
for x in protocols+[connected,b'metrics',b'initial-flush',b'alter-flush',state,b'A',b'B',b'C',b'D']:payload+=b(x)
for i,kind in enumerate((1,2,1,2,2,2),1):payload+=u(i)+v(kind)
for x in (b'saved',b'saved',b'ctrl'):payload+=v(1)+v(0)+b(x)
rows=[('terminate',0)]
if m.get('topology',1)==2:
 rows=[item for _ in range(3) for item in [('external-input-disable',0),('audio-bus-activate',0),('activate',0)]]+rows
 if bad=='disable-missing':rows=rows[1:]
 if bad=='disable-failed':rows[0]=('external-input-disable',1)
 if bad=='disable-duplicate':rows.insert(0,rows[0])
 if bad=='disable-order':rows[0],rows[1]=rows[1],rows[0]
 if bad=='input-enable':rows.append(('external-input-enable',0))
payload+=u(len(rows))+b''.join(t(name)+u(result) for name,result in rows)+u(1)+t('release')+u(0)+b''.join(u(x) for x in (6,6,6,6,0))
if bad=='payload-tail':payload+=b'extra'
module=m['module'];s=os.stat(module);dev=f'{os.major(s.st_dev):02x}:{os.minor(s.st_dev):02x}'
other='200000-201000 r--p 00000000 00:00 0 [fixture]\n'
target=f'100000-101000 r-xp 00000000 {dev} {s.st_ino} {module}\n'
disposition='provider_retained' if bad in ('retained','bad-retained') else 'provider_removed'
maps=[other,target+other,target+other if disposition=='provider_retained' else other]
if bad=='bad-retained':maps[2]='not a mapping\n'
if bad=='path-inode-mixed':maps[2]=f'100000-101000 r-xp 00000000 {dev} {s.st_ino+1} {module}\n'+other
if bad=='partial-retained':disposition='provider_retained'
body=t('cs80-process-lifetime-child-v3')+t('strict-unmap' if bad=='profile' else 'process-lifetime')+t('0'*64 if bad=='nonce' else nonce)+u(os.getpid()+1 if bad=='pid' else os.getpid())+t('candidate' if bad=='arm' else 'stock')
body+=t(m['expected_manifest_digest'])+t(m['expected_evidence_digest'])+t(m['module_sha256'])+t('cs80-instance-history-A-D-v1')+b(b'other' if bad=='case' else case)+v(2 if bad=='query-policy' else m.get('query_policy',1))+v(1 if bad=='query-result' else m.get('query_result',0))+v(0 if bad=='query-pointer' else m.get('query_pointer',1))+v(2 if bad=='topology' else m.get('topology',1))+v(1)+t(disposition)+t('0'*64 if bad=='payload-hash' else h(payload))+b(payload)
for x in maps:body+=t(x)
body+=u(1)+t('')
objects=['',module] if disposition=='provider_retained' else [''];body+=u(len(objects))+b''.join(t(x) for x in objects)
body+=u(0x100000)+b''.join(u(0x100000+x) for x in (1,2,3))+b''.join(u(x) for x in (1,2,3))
for _ in range(2):body+=u(1)+u(os.getpid())+u(1)
body+=u(3)+t('factory-release')+u(0)+t('ModuleExit')+u(1)+t('loader-close')+u(0)
framed=u(len(body))+body+t('0'*64 if bad=='body-hash' else h(body))
if bad=='truncated':framed=framed[:-10]
if bad=='oversized':framed=u(2**64-1)+framed[8:]
if bad=='trailing':framed+=b'x'
os.write(fd,framed);os.fsync(fd);os.close(fd)
if bad=='mode':os.chmod(output,0o644)
if bad=='extra':open(os.path.join(os.path.dirname(output),'extra'),'w').write('x')
if bad=='module':open(module,'ab').write(b'x')
if bad=='module-inode':
 os.rename(module,module+'.old');open(module,'wb').write(open(module+'.old','rb').read());os.unlink(module+'.old')
if bad=='nonzero':sys.exit(7)
if bad=='live-group':
 if os.fork()==0:time.sleep(10);os._exit(0)
'''

class ProcessLifetimeTransactionTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=pathlib.Path(self.tmp.name);self.work=self.root/'work';self.work.mkdir();self.tools=self.root/'tools';self.tools.mkdir();self.bundle=self.root/'bundle';self.bundle.mkdir();self.module=self.bundle/'module.so';self.module.write_bytes(b'x'*8192)
  self.script=self.tools/'child.py';self.script.write_text(CHILD);self.manifest=self.tools/'manifest.json';self.python=pathlib.Path(sys.executable).resolve();self.out=self.root/'out';self.log=self.root/'logs'
  self.m={'case_identity_hex':b'inert-case'.hex(),'selected_cid_hex':'05'*16,'expected_manifest_digest':'a'*64,'expected_evidence_digest':'b'*64,'module_sha256':H(self.module),'module':str(self.module)};self.manifest.write_text(json.dumps(self.m))
  import process_lifetime_transaction as implementation
  helper=pathlib.Path(implementation.__file__).resolve()
  self.c={**{k:v for k,v in self.m.items() if k!='module'},'controller_cid_hex':None,'controller_query_policy':1,'controller_query_result':0,'controller_query_pointer_present':True,'audio_input_topology':1,'protocol_sha256':[hashlib.sha256(x).hexdigest() for x in (b'initial',b'alter',b'CS80RPRT'+V(1)+b'inert')],'controller_state_policy':1,'controller_state_result':0,'arm':'stock','profile':PROFILE,'schema':SCHEMA,'history_schema':HISTORY,'driver':str(self.python),'driver_script':str(self.script),'driver_mode':'process-lifetime','manifest':str(self.manifest),'manifest_sha256':H(self.manifest),'runtime_hashes':{str(p):H(p) for p in (self.python,self.script)},'helper_hashes':{str(helper):H(helper)},'inputs':[],'module_root':str(self.bundle),'module_relative':'module.so','module_hashes':{'module.so':H(self.module)},'module_directories':[],'module_modes':{'.':self.bundle.stat().st_mode&0o777,'module.so':self.module.stat().st_mode&0o777},'module_entry_cap':16,'module_byte_cap':1024**2,'receipt_cap':1024**2,'timeout':5,'cwd':str(self.work),'output':str(self.out),'environment':{'PATH':'/usr/bin:/bin','HOME':str(self.work),'LANG':'C'}}
  self.commands=Commands(self.log,total_seconds=30)
 def call(self):return _process_lifetime(self.c,self.commands,allow_script=True)
 def mutate(self,name):self.m['mutation']=name;self.manifest.write_text(json.dumps(self.m));self.c['manifest_sha256']=H(self.manifest)
 def failed(self):
  with self.assertRaises(Exception):self.call()
  self.assertTrue(self.commands.terminal_failure);self.assertFalse((self.out/'process-result.json').exists())
 def test_success_removed(self):
  r=self.call();self.assertTrue(r['process_completed']);self.assertTrue(r['provider_removed']);self.assertFalse(r['provider_retained']);self.assertFalse(r['E_transfer_authority']);self.assertEqual(r['actual_pid'],self.commands.ledger[0]['pid']);self.assertEqual(H(r['full_payload_path']),r['payload_sha256']);self.assertRegex(r['nonce'],'^[0-9a-f]{64}$');self.assertEqual(set(p.name for p in self.out.iterdir()),{'child.receipt','A-D.payload','process-result.json'})
 def test_success_retained(self):self.mutate('retained');r=self.call();self.assertTrue(r['provider_retained']);self.assertFalse(r['provider_removed']);self.assertTrue(r['native_dlclose_returned_zero']);self.assertTrue(r['process_completed'])
 def test_existing_output_no_overwrite(self):self.out.mkdir();p=self.out/'keep';p.write_text('untouched');self.failed();self.assertEqual(p.read_text(),'untouched');self.assertFalse(any('argv' in row for row in self.commands.ledger))
 def query_separate(self,policy,result):
  self.c.update(controller_cid_hex='07'*16,controller_query_policy=policy,controller_query_result=result,controller_query_pointer_present=False)
  self.m.update(controller_cid_hex='07'*16,query_policy=policy,query_result=result,query_pointer=0);self.manifest.write_text(json.dumps(self.m));self.c['manifest_sha256']=H(self.manifest)
 def topology_disabled(self):
  self.c['audio_input_topology']=2;self.c['protocol_sha256'][2]=hashlib.sha256(b'CS80RPRT'+V(2)+V(2)+b'inert').hexdigest();self.m['topology']=2;self.manifest.write_text(json.dumps(self.m));self.c['manifest_sha256']=H(self.manifest)
 def test_disabled_input_topology(self):
  self.topology_disabled();r=self.call();self.assertEqual(r['audio_input_topology'],2);self.assertEqual(r['frozen_contract']['audio_input_topology'],2)
 def test_topology_protocol_header_mismatch_even_with_matching_hash(self):
  self.topology_disabled();data=b'CS80RPRT'+V(1)+V(2)+b'inert';self.m['render_override']=data.hex();self.c['protocol_sha256'][2]=hashlib.sha256(data).hexdigest();self.manifest.write_text(json.dumps(self.m));self.c['manifest_sha256']=H(self.manifest);self.failed()
 def test_zero_input_rejects_disabled_protocol_even_with_matching_hash(self):
  data=b'CS80RPRT'+V(2)+V(2)+b'inert';self.m['render_override']=data.hex();self.c['protocol_sha256'][2]=hashlib.sha256(data).hexdigest();self.manifest.write_text(json.dumps(self.m));self.c['manifest_sha256']=H(self.manifest);self.failed()
 def test_topology_implicit_forbidden(self):del self.c['audio_input_topology'];self.failed()
 def test_topology_unknown_forbidden(self):self.c['audio_input_topology']=3;self.failed()
 def test_disabled_missing(self):self.topology_disabled();self.mutate('disable-missing');self.failed()
 def test_disabled_failed(self):self.topology_disabled();self.mutate('disable-failed');self.failed()
 def test_disabled_duplicate(self):self.topology_disabled();self.mutate('disable-duplicate');self.failed()
 def test_external_input_never_enabled(self):self.topology_disabled();self.mutate('input-enable');self.failed()
 def test_disabled_order(self):self.topology_disabled();self.mutate('disable-order');self.failed()
 def test_query_strict_separate(self):
  self.query_separate(1,0xffffffff);r=self.call();self.assertEqual(r['controller_query_result'],0xffffffff);self.assertFalse(r['controller_query_pointer_present'])
 def test_query_pinned_false_null(self):
  self.query_separate(2,1);r=self.call();self.assertEqual(r['controller_query_policy'],2);self.assertEqual(r['controller_query_result'],1)
  for key in ('controller_query_policy','controller_query_result','controller_query_pointer_present'):self.assertEqual(r['frozen_contract'][key],self.c[key])
  actual=hashlib.sha256(json.dumps(r['frozen_contract'],sort_keys=True,separators=(',',':')).encode()).hexdigest();self.assertEqual(r['contract_sha256'],actual)
  for key in ('controller_query_policy','controller_query_result','controller_query_pointer_present'):
   changed=dict(r['frozen_contract']);del changed[key];self.assertNotEqual(actual,hashlib.sha256(json.dumps(changed,sort_keys=True,separators=(',',':')).encode()).hexdigest())
 def test_public_legacy_receipt_schema_rejected(self):
  self.c['schema']=LEGACY_SCHEMA
  with self.assertRaisesRegex(ValueError,'schema/profile'):process_lifetime_arm(self.c,self.commands)
  self.assertFalse(any('argv' in row for row in self.commands.ledger))
 def test_query_strict_rejects_false_null(self):self.query_separate(1,1);self.failed();self.assertFalse(any('argv' in row for row in self.commands.ledger))
 def test_query_unknown_policy(self):self.c['controller_query_policy']=3;self.failed()
 def test_query_implicit_policy_forbidden(self):del self.c['controller_query_policy'];self.failed()
 def test_query_success_null_forbidden(self):self.query_separate(2,0);self.failed()
 def test_query_false_pointer_forbidden(self):self.c.update(controller_query_policy=2,controller_query_result=1);self.failed()
 def test_expected_protocol_mismatch(self):self.c['protocol_sha256'][0]='0'*64;self.failed()
 def test_expected_class_mismatch(self):self.c['selected_cid_hex']='09'*16;self.failed()
 def test_expected_evidence_mismatch(self):self.c['expected_evidence_digest']='c'*64;self.failed()
 def test_stale_manifest_hash(self):self.manifest.write_text('{}');self.failed()
 def test_receipt_bound(self):self.c['receipt_cap']=64;self.failed()
 def test_public_script_rejected(self):
  with self.assertRaisesRegex(ValueError,'native driver'):process_lifetime_arm(self.c,self.commands)
  self.assertTrue(self.commands.terminal_failure)
 def test_deadline(self):self.commands.deadline=0;self.failed()
 def test_actual_pid_ledger_mismatch(self):
  original=self.commands.run
  def changed(*a,**kw):
   result=original(*a,**kw);self.commands.ledger[-1]['pid']+=1;return result
  self.commands.run=changed;self.failed()
 def test_group_fact_not_claimed(self):
  original=self.commands.run
  def changed(*a,**kw):
   result=original(*a,**kw);self.commands.ledger[-1]['owned_process_group_empty']=False;return result
  self.commands.run=changed;self.failed()
 def test_postparse_receipt_mutation(self):
  import process_lifetime_transaction as impl
  original=impl._parse
  def changed(*a,**kw):
   result=original(*a,**kw);p=self.out/'child.receipt';p.write_bytes(p.read_bytes()+b'changed');return result
  from unittest.mock import patch
  with patch.object(impl,'_parse',changed):self.failed()
 def fifo_inner(self):self.mutate('fifo');self.failed()
 def test_fifo_receipt_outer_watchdog(self):
  code="import unittest,test_process_lifetime_transaction as m; r=unittest.TextTestRunner().run(unittest.TestSuite([m.ProcessLifetimeTransactionTests('fifo_inner')])); raise SystemExit(not r.wasSuccessful())"
  result=subprocess.run([sys.executable,'-c',code],cwd=ROOT,capture_output=True,text=True,timeout=8)
  self.assertEqual(result.returncode,0,result.stderr)
 def test_terminal_retry(self):self.mutate('nonce');self.failed();n=len([r for r in self.commands.ledger if 'argv' in r]);self.failed();self.assertEqual(len([r for r in self.commands.ledger if 'argv' in r]),n)

for mutation in ('nonce','pid','arm','profile','query-policy','query-result','query-pointer','topology','case','cid','protocol','payload-case','policy','payload-tail','payload-hash','body-hash','bad-retained','path-inode-mixed','partial-retained','truncated','oversized','trailing','mode','extra','symlink','directory','missing','module','module-inode','nonzero','live-group'):
 setattr(ProcessLifetimeTransactionTests,'test_reject_'+mutation.replace('-','_'),lambda self,m=mutation:(self.mutate(m),self.failed(),None)[2])
class RequestPreselectionTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=pathlib.Path(self.temp.name);self.request=self.root/'request';self.commands=Commands(self.root/'logs',total_seconds=10)
  text32=lambda s:struct.pack('<I',len(s))+s.encode()
  case=text32('packed-filter-workload-draft-v1')+text32('a'*64)+text32('b'*64)+b''.join(V(x) for x in (0,48000,0,8,64))
  self.c={'arm':'stock','selected_cid_hex':'05'*16,'case_identity_hex':case.hex(),'controller_state_policy':1,'controller_query_policy':1,'audio_input_topology':1}
 def check(self,version=2,query=1,topology=1,selectors=(0,48000,0,8,64),state=1,arm='stock',cid='05'*16):
  data=T('cs80-process-lifetime-request-v'+str(version))+T(PROFILE)+T(arm)+T(cid)+b''.join(U(x) for x in selectors)+U(state)+(U(query) if version>=2 else b'')+(U(topology) if version>=3 else b'')
  self.request.write_bytes(data);self.request.chmod(0o600);self.c['manifest_sha256']=H(self.request)
  from process_lifetime_transaction import _request_preselection
  _request_preselection(self.c,self.request,self.commands)
 def test_v3_disabled(self):self.c['audio_input_topology']=2;self.check(version=3,topology=2)
 def test_v3_topology_mismatch(self):
  with self.assertRaisesRegex(ValueError,'topology mismatch'):self.check(version=3,topology=2)
 def test_old_request_cannot_enable_input(self):
  self.c['audio_input_topology']=2
  with self.assertRaisesRegex(ValueError,'topology mismatch'):self.check(version=2)
 def test_v1_strict(self):self.check(version=1)
 def test_v2_strict(self):self.check()
 def test_v2_pinned(self):self.c['controller_query_policy']=2;self.check(query=2)
 def test_v1_cannot_enable_compatibility(self):
  self.c['controller_query_policy']=2
  with self.assertRaisesRegex(ValueError,'query policy'):self.check(version=1)
 def test_v2_policy_mismatch(self):
  with self.assertRaisesRegex(ValueError,'query policy'):self.check(query=2)
 def test_selectors_mismatch(self):
  with self.assertRaisesRegex(ValueError,'selectors'):self.check(selectors=(1,48000,0,8,64))
 def test_state_mismatch(self):
  with self.assertRaisesRegex(ValueError,'state policy'):self.check(state=2)
 def test_arm_mismatch(self):
  with self.assertRaisesRegex(ValueError,'arm/CID'):self.check(arm='candidate')
 def test_cid_mismatch(self):
  with self.assertRaisesRegex(ValueError,'arm/CID'):self.check(cid='07'*16)
 def test_case_text_bound(self):
  self.c['case_identity_hex']=V(2**32-1).hex()
  with self.assertRaisesRegex(ValueError,'case text bound'):self.check()

# The expected identity and protocols are computed BEFORE the child executes.
# This probe performs file verification and pure workload encoding only, never
# dlopen. It does not bless bytes copied from the receipt under test.
NATIVE_EXPECTED=r'''
#define INIT_CLASS_IID
#include "native_identity.hpp"
#include "workload_bridge.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace cs80_identity;using namespace cs80_observer;
int main(int argc,char** argv){try{
 require(argc==2,"probe arguments");std::ifstream in(argv[1]);ExpectedIdentity e;e.arm="stock";
 in>>std::quoted(e.bundle.path)>>e.bundle.mode>>e.bundle.uid>>e.bundle.gid>>std::quoted(e.module);
 std::size_t n=0;in>>n;require(n<=64,"probe manifest cap");
 for(std::size_t i=0;i<n;++i){Entry row;in>>std::quoted(row.path)>>row.directory>>row.mode>>row.uid>>row.gid>>row.size>>std::quoted(row.sha256);e.bundleEntries.push_back(row);}
 require(bool(in),"probe manifest");auto identity=VerifiedIdentity::verify(e,{64,64,8*1024*1024,4096});
 const packed_filter_workload::Case c{packed_filter_workload::Policy::legacy,48000,packed_filter_workload::Mode::realtime,8,64};
 ConnectedParameterRoster roster;for(const auto& q:packed_filter_workload::controls(c))roster.ids.push_back(q.id);roster.declaredCount=static_cast<int32>(roster.ids.size());
 const auto plan=workload_detail::planFromRoster(c,roster);const auto bytes=packed_filter_workload::encode_draft(c);
 std::cout<<identity->expectedManifestDigest()<<"\n"<<identity->evidenceDigest()<<"\n";
 for(auto b:bytes){std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b);}std::cout<<"\n";
 for(const auto& encoded:{encodeFlushProtocol(plan.initial),encodeFlushProtocol(plan.alter),encodeRenderProtocol(plan.render)})std::cout<<cs80_identity::detail::digest(std::string(encoded.data.begin(),encoded.data.end()))<<"\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
'''

class ProcessLifetimeNativeTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  import test_native_observer_process_child as child
  child.ProcessChildTests.setUpClass.__func__(cls)
  source=cls.root/'expected.cpp';source.write_text(NATIVE_EXPECTED);cls.expected=cls.root/'expected'
  command=[shutil.which('g++'),'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',os.environ['CS80_OBSERVER_SDK'],'-I',str(ROOT),'-I',str(ROOT.parents[1]/'src'),str(source),'-lcrypto','-o',str(cls.expected)]
  result=subprocess.run(command,capture_output=True,text=True,timeout=60)
  if result.returncode:raise RuntimeError(result.stderr)
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def native(self,variant='normal',mode='valid'):
  root=self.root/self._testMethodName;root.mkdir();bundle=root/'bundle';bundle.mkdir();module=bundle/'module.so';shutil.copy2({'normal':self.lib,'unique':self.unique,'nodelete':self.retained}[variant],module)
  manifest=root/'manifest';s=bundle.stat();f=module.stat();manifest.write_text(f'{json.dumps(str(bundle))} {s.st_mode&0o777} {s.st_uid} {s.st_gid}\n"module.so"\n2\n"." 1 {s.st_mode&0o777} {s.st_uid} {s.st_gid} 0 ""\n"module.so" 0 {f.st_mode&0o777} {f.st_uid} {f.st_gid} {f.st_size} "{H(module)}"\n')
  probe=subprocess.run([str(self.expected),str(manifest)],capture_output=True,text=True,check=True,timeout=10).stdout.splitlines();self.assertEqual(len(probe),6)
  import process_lifetime_transaction as impl
  helper=pathlib.Path(impl.__file__).resolve();work=root/'work';work.mkdir()
  c={'arm':'stock','schema':LEGACY_SCHEMA,'profile':PROFILE,'history_schema':HISTORY,'controller_state_policy':1,'controller_state_result':0,'selected_cid_hex':'05'*16,'controller_cid_hex':None,'case_identity_hex':probe[2],'protocol_sha256':probe[3:],'expected_manifest_digest':probe[0],'expected_evidence_digest':probe[1],'manifest':str(manifest),'manifest_sha256':H(manifest),'driver':str(self.child),'driver_mode':mode,'runtime_hashes':{str(self.child):H(self.child)},'helper_hashes':{str(helper):H(helper)},'inputs':[],'module_root':str(bundle),'module_relative':'module.so','module_sha256':H(module),'module_hashes':{'module.so':H(module)},'module_directories':[],'module_modes':{'.':s.st_mode&0o777,'module.so':f.st_mode&0o777},'module_entry_cap':16,'module_byte_cap':8*1024**2,'receipt_cap':8*1024**2,'timeout':15,'cwd':str(work),'output':str(root/'out'),'environment':{'PATH':'/usr/bin:/bin','HOME':str(work),'LANG':'C'}}
  return c,Commands(root/'logs',total_seconds=30)
 def positive(self,variant):
  c,commands=self.native(variant);r=_process_lifetime(c,commands,allow_fixture=True);self.assertTrue(r['process_completed']);self.assertEqual(r['provider_disposition'],'provider_removed' if variant=='normal' else 'provider_retained');self.assertEqual(r['actual_pid'],commands.ledger[0]['pid']);self.assertEqual(H(r['full_payload_path']),r['payload_sha256']);self.assertFalse(r['production_qualified']);self.assertFalse(r['E_transfer_authority'])
 def test_native_removed(self):self.positive('normal')
 def test_native_unique(self):self.positive('unique')
 def test_native_nodelete(self):self.positive('nodelete')
 def test_public_native_fixture_mode_rejected(self):
  c,commands=self.native()
  with self.assertRaisesRegex(ValueError,'schema/profile'):process_lifetime_arm(c,commands)
  self.assertFalse(any('argv' in row for row in commands.ledger))
 def test_native_failed_exit(self):
  c,commands=self.native('unique','exit-false')
  with self.assertRaises(Exception):_process_lifetime(c,commands,allow_fixture=True)
  self.assertTrue(commands.terminal_failure);self.assertFalse((pathlib.Path(c['output'])/'process-result.json').exists())
 def test_native_plausible_receipt_nonzero(self):
  c,commands=self.native('unique','output-close')
  with self.assertRaises(Exception):_process_lifetime(c,commands,allow_fixture=True)
  self.assertTrue((pathlib.Path(c['output'])/'child.receipt').exists());self.assertTrue(commands.terminal_failure);self.assertFalse((pathlib.Path(c['output'])/'process-result.json').exists())

if __name__=='__main__':unittest.main()
