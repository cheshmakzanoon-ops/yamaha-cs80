"""Non-admitting exit24 metadata captured from actual pre-failure native calls."""
import hashlib,json,os,pathlib,shutil,signal,struct,subprocess,unittest
import test_native_observer_driver as base
ROOT=pathlib.Path(__file__).parent
EXTRA_ID=0x7ffffffe
NONCE='7a'*32
H=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
class Reader:
 def __init__(self,data):self.data=data;self.at=0
 def raw(self,n):
  if n<0 or n>len(self.data)-self.at:raise ValueError('truncated capture')
  b=self.data[self.at:self.at+n];self.at+=n;return b
 def u64(self):return struct.unpack('<Q',self.raw(8))[0]
 def u32(self):return struct.unpack('<I',self.raw(4))[0]
 def blob(self):return self.raw(self.u64())
 def text(self):return self.blob().decode()
 def utf16(self):return self.raw(self.u64()*2).decode('utf-16-le')
 def end(self):
  if self.at!=len(self.data):raise ValueError('trailing capture')
class FailureMetadataTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  base.DriverTests.setUpClass.__func__(cls)
  cls.normalLib=cls.root/'normal-fixture.so';shutil.copy2(cls.lib,cls.normalLib)
  source=(ROOT/'native_observer_module_history_library.cpp').read_text();needle='realComponent.parameterCount=static_cast<int>(fullIds.size());'
  if source.count(needle)!=1:raise RuntimeError('extra-param fixture seam drift')
  source=source.replace(needle,f'fullIds.push_back({EXTRA_ID}u);'+needle)
  p=cls.root/'extra-library.cpp';p.write_text(source)
  flags=[shutil.which('g++'),'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-ffp-contract=off','-I',os.environ['CS80_OBSERVER_SDK'],'-I',str(ROOT),'-I',str(ROOT.parents[1]/'src')]
  commands=[[*flags,'-shared','-fPIC',str(p),'-o',str(cls.lib)]]
  # Private link-interposition control only. The production driver is untouched.
  wrapper=cls.root/'partial-write.cpp';wrapper.write_text(r'''
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
extern "C" ssize_t __real_write(int,const void*,size_t);
extern "C" ssize_t __wrap_write(int fd,const void* data,size_t count){
 struct stat s{};if(fd>2 && ::fstat(fd,&s)==0 && S_ISREG(s.st_mode)){
  const char marker[]="PRIVATE_METADATA_WRITE\n";__real_write(2,marker,sizeof(marker)-1);
  static bool began=false;if(began){errno=ENOSPC;return -1;}began=true;
  return __real_write(fd,data,count<7?count:7);
 }return __real_write(fd,data,count);
}
''');cls.partial=cls.root/'partial-driver'
  commands.append([*flags,str(ROOT/'native_observer_driver.cpp'),str(wrapper),'-Wl,--wrap=write','-ldl','-lcrypto','-o',str(cls.partial)])
  fsync_wrapper=cls.root/'fsync-failure.cpp';fsync_wrapper.write_text('#include <cerrno>\nextern "C" int __wrap_fsync(int){errno=EIO;return -1;}\n');cls.fsync=cls.root/'fsync-driver'
  commands.append([*flags,str(ROOT/'native_observer_driver.cpp'),str(fsync_wrapper),'-Wl,--wrap=fsync','-ldl','-lcrypto','-o',str(cls.fsync)])
  for cmd in commands:
   p=subprocess.run(cmd,capture_output=True,text=True,timeout=60)
   if p.returncode:raise RuntimeError(p.stderr)
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 setUp=base.DriverTests.setUp
 encoded=base.DriverTests.encoded
 def launch(self,mode='valid',partial=False,fsync=False):
  self.req.write_bytes(self.encoded());self.req.chmod(0o600)
  p=subprocess.Popen([str(self.partial if partial else self.fsync if fsync else self.exe),str(self.req),H(self.req),NONCE,str(self.out)],env={'PATH':'/usr/bin:/bin','LANG':'C','HOME':str(self.work),'CS80_JOIN_MODE':mode},stdout=subprocess.PIPE,stderr=subprocess.PIPE,start_new_session=True)
  try:stdout,stderr=p.communicate(timeout=15)
  except subprocess.TimeoutExpired:
   os.killpg(p.pid,signal.SIGKILL);p.communicate();raise
  return p.returncode,p.pid,stdout,stderr
 def decode(self,pid):
  framed=self.out.read_bytes();r=Reader(framed);body=r.blob();self.assertLessEqual(len(body),16*1024*1024+4096);self.assertEqual(r.text(),hashlib.sha256(body).hexdigest());r.end();r=Reader(body)
  self.assertEqual(r.text(),'cs80-process-lifetime-failure-metadata-v1');self.assertEqual(r.text(),'rejected');self.assertEqual(r.u32(),24);self.assertEqual(r.text(),NONCE);self.assertEqual(r.u64(),pid);self.assertEqual(r.text(),'stock')
  for _ in range(2):self.assertRegex(r.text(),r'^[0-9a-f]{64}$')
  self.assertEqual(r.text(),H(self.bundle/'module.so'));self.assertEqual(r.text(),'preload-identities-not-postfailure-verification');self.assertEqual(r.text(),'module-history');self.assertEqual(r.text(),'instance-history');self.assertEqual(r.text(),'initial-flush');self.assertEqual(r.text(),'full flush complete ID-set binding')
  metadata_hash=r.text();metadata=r.blob();r.end();self.assertEqual(hashlib.sha256(metadata).hexdigest(),metadata_hash)
  m=Reader(metadata);self.assertEqual(m.text(),'native-observer-connected-v1');self.assertEqual(m.text(),'native-observer-created-identity-v1');self.assertEqual(m.raw(16),bytes.fromhex('05'*16));self.assertEqual(m.u32(),0)
  for media in (0,1):
   for direction in (0,1):
    self.assertEqual(m.u32(),media);self.assertEqual(m.u32(),direction);n=m.u32();self.assertEqual(n,1 if (media,direction) in ((0,1),(1,0)) else 0)
    for _ in range(n):
     self.assertEqual(m.u32(),media);self.assertEqual(m.u32(),direction);self.assertEqual(m.u32(),2);self.assertEqual(m.utf16(),'O');m.u32();m.u32()
     if media==0:self.assertEqual(m.u64(),3)
  count=m.u32();self.assertEqual(count,193);ids=[]
  for _ in range(count):
   ids.append(m.u32());self.assertEqual(m.utf16(),'P');self.assertEqual(m.utf16(),'');self.assertEqual(m.utf16(),'');m.u32();self.assertEqual(struct.unpack('<d',m.raw(8))[0],.5);m.u32();m.u32();self.assertEqual(struct.unpack('<d',m.raw(8))[0],.25)
  m.end();self.assertEqual(len(set(ids)),193);self.assertEqual(ids[-1],EXTRA_ID)
 def test_complete_failed_native_metadata(self):
  code,pid,out,err=self.launch();self.assertEqual(code,24,err);self.assertIn(b'cleanup_retried=0 exit=24',err);self.decode(pid)
 def test_supervisor_never_admits_failure_frame(self):
  from process_lifetime_transaction import process_lifetime_arm
  c,commands=base.DriverTests.public_contract(self)
  with self.assertRaises(Exception):process_lifetime_arm(c,commands)
  self.assertTrue(commands.terminal_failure);self.assertFalse((pathlib.Path(c['output'])/'process-result.json').exists());self.out=pathlib.Path(c['output'])/'child.receipt';self.assertIn(b'cs80-process-lifetime-failure-metadata-v1',self.out.read_bytes())
 def test_pre_metadata_failure_stays_empty(self):
  code,pid,out,err=self.launch('component-init-false');self.assertEqual(code,24,err);self.assertEqual(self.out.read_bytes(),b'')
 def test_existing_output_not_overwritten(self):
  self.out.write_bytes(b'prior receipt');code,pid,out,err=self.launch();self.assertEqual(code,24,err);self.assertEqual(self.out.read_bytes(),b'prior receipt')
 def test_partial_failure_frame_not_restarted(self):
  code,pid,out,err=self.launch(partial=True);self.assertEqual(code,24,err);self.assertEqual(len(self.out.read_bytes()),7);self.assertEqual(err.count(b'PRIVATE_METADATA_WRITE'),2)
 def test_partial_success_receipt_not_overwritten(self):
  shutil.copy2(self.normalLib,self.bundle/'module.so');code,pid,out,err=self.launch(partial=True);self.assertEqual(code,24,err);self.assertEqual(len(self.out.read_bytes()),7);self.assertEqual(err.count(b'PRIVATE_METADATA_WRITE'),2);self.assertIn(b'stage=receipt-write',err);self.assertIn(b'primary=terminal receipt write failed',err)
 def test_complete_failure_frame_fsync_failure_still_rejected(self):
  code,pid,out,err=self.launch(fsync=True);self.assertEqual(code,24,err);self.decode(pid)
if __name__=='__main__':unittest.main()
