"""Actual terminal child: full framed payload, real retained libraries, fail-closed IO."""
import hashlib,json,os,pathlib,shutil,struct,subprocess,unittest
import test_native_observer_module_history as baseline
ROOT=baseline.ROOT
NONCE='0123456789abcdef'*4
class ProcessChildTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  provenance=json.loads((ROOT/'native_observer_process_child_provenance.json').read_text())
  for section in ('sources','dependencies'):
   for name,wanted in provenance[section].items():
    path=ROOT/name
    if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Terminal child drift: '+name)
  baseline.ModuleHistoryTests.setUpClass.__func__(cls)
  sdk=os.environ['CS80_OBSERVER_SDK'];compiler=shutil.which('g++');flags=[compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-ffp-contract=off','-I',sdk,'-I',str(ROOT.parents[1]/'src')]
  cls.child=cls.root/'process-child';cls.unique=cls.root/'actual-unique.so'
  for cmd in ([*flags,str(ROOT/'native_observer_process_child_fixture.cpp'),'-rdynamic',*[f'-Wl,--wrap={name}' for name in ('open','dlinfo','dlsym','dlclose','write','fsync','close')],'-ldl','-lcrypto','-o',str(cls.child)],[*flags,'-shared','-fPIC',str(ROOT/'native_observer_process_unique_library.cpp'),'-o',str(cls.unique)]):
   p=subprocess.run(cmd,capture_output=True,text=True,timeout=60)
   if p.returncode:raise RuntimeError(p.stderr)
  symbols=subprocess.run(['readelf','-Ws',str(cls.unique)],capture_output=True,text=True,check=True).stdout
  if not any('UNIQUE' in line and 'cs80_inert_gnu_unique_anchor' in line for line in symbols.splitlines()):raise RuntimeError('genuine GNU_UNIQUE anchor absent')
  dynamic=subprocess.run(['readelf','-d',str(cls.unique)],capture_output=True,text=True,check=True).stdout
  if 'NODELETE' in dynamic:raise RuntimeError('GNU_UNIQUE control must not use explicit NODELETE flag')
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def invoke(self,mode='valid',variant='normal',nonce=NONCE,output_kind='new',strict=False):
  label=self._testMethodName;bundle=self.root/label;bundle.mkdir();source={'normal':self.lib,'nodelete':self.retained,'unique':self.unique}[variant];shutil.copy2(source,bundle/'module.so');st=bundle.stat();p=bundle/'module.so';f=p.stat();rows=[f'"." 1 {st.st_mode&0o777} {st.st_uid} {st.st_gid} 0 ""',f'"module.so" 0 {f.st_mode&0o777} {f.st_uid} {f.st_gid} {f.st_size} "{hashlib.sha256(p.read_bytes()).hexdigest()}"'];manifest=self.root/(label+'.manifest');manifest.write_text(f'{json.dumps(str(bundle))} {st.st_mode&0o777} {st.st_uid} {st.st_gid}\n"module.so"\n2\n'+'\n'.join(rows)+'\n')
  output=self.root/(label+'.receipt');log=self.root/(label+'.log')
  if output_kind=='existing':output.write_bytes(b'untouched')
  if output_kind=='symlink':output.symlink_to('/dev/null')
  cmd=[str(self.exe),str(manifest),'nodelete-retained'] if strict else [str(self.child),str(manifest),mode,nonce,str(output)]
  process=subprocess.Popen(cmd,env=dict(os.environ,CS80_JOIN_LOG=str(log)),stdout=subprocess.PIPE,stderr=subprocess.PIPE);stdout,stderr=process.communicate(timeout=15)
  return process.returncode,process.pid,output,log.read_text().splitlines() if log.exists() else [],stderr
 def decode(self,p):
  b=p.read_bytes();pos=0
  def u64():
   nonlocal pos
   value=struct.unpack_from('<Q',b,pos)[0];pos+=8;return value
  n=u64();self.assertLessEqual(n,96*1024*1024);body=b[pos:pos+n];pos+=n;digest_size=u64();self.assertEqual(digest_size,64);self.assertEqual(b[pos:pos+64].decode(),hashlib.sha256(body).hexdigest());pos+=64;self.assertEqual(pos,len(b));b=body;pos=0
  def blob():
   nonlocal pos
   n=u64();v=b[pos:pos+n];self.assertEqual(len(v),n);pos+=n;return v
  def text():return blob().decode()
  self.assertEqual(text(),'cs80-process-lifetime-child-v1');self.assertEqual(text(),'process-lifetime');self.assertEqual(text(),NONCE);pid=u64();self.assertEqual(text(),'stock')
  for _ in range(3):self.assertRegex(text(),r'^[0-9a-f]{64}$')
  self.assertEqual(text(),'cs80-instance-history-A-D-v1');case=blob();self.assertTrue(case);self.assertEqual(struct.unpack_from('<I',b,pos)[0],1);pos+=4;disposition=text();phash=text();payload=blob();self.assertEqual(phash,hashlib.sha256(payload).hexdigest());self.assertGreater(len(payload),1000);self.assertLessEqual(len(payload),64*1024*1024)
  maps=[text() for _ in range(3)];self.assertTrue(all(m.endswith('\n') for m in maps))
  for _ in range(2):
   count=u64();self.assertLessEqual(count,4096)
   for _ in range(count):text()
  for _ in range(7):u64()
  threads=[]
  for _ in range(2):
   count=u64();self.assertLessEqual(count,4096);threads.append([(u64(),u64()) for _ in range(count)])
  self.assertEqual(threads[0],threads[1]);count=u64();self.assertLessEqual(count,128);calls=[(text(),u64()) for _ in range(count)];self.assertIn(('ModuleExit',1),calls);self.assertEqual(pos,len(b));return pid,disposition,payload
 def positive(self,variant):
  code,pid,path,events,err=self.invoke(variant=variant);self.assertEqual(code,0,err);actual,disposition,payload=self.decode(path);self.assertEqual(actual,pid);self.assertEqual(disposition,'provider_removed' if variant=='normal' else 'provider_retained');self.assertEqual(events.count('exit'),1);self.assertEqual(events.count('unloaded'),1 if variant=='normal' else 0)
 def test_removed(self):self.positive('normal')
 def test_nodelete(self):self.positive('nodelete')
 def test_genuine_unique(self):self.positive('unique')
 def test_strict_unique_refused(self):
  code,pid,path,events,err=self.invoke(variant='unique',strict=True);self.assertEqual(code,23,err);self.assertNotIn('unloaded',events)
 def negative(self,mode='valid',variant='normal',nonce=NONCE,output_kind='new'):
  code,pid,path,events,err=self.invoke(mode,variant,nonce,output_kind);self.assertEqual(code,24,err);self.assertIn(b'CS80_PROCESS_LIFETIME_FAILURE v1',err);self.assertIn(b'cleanup_retried=0 exit=24',err);return path,events
 def test_maps_malformed(self):self.negative('maps-malformed','unique')
 def test_maps_partial(self):self.negative('maps-partial','unique')
 def test_maps_permissions(self):self.negative('maps-permission','unique')
 def test_maps_path_mixed(self):self.negative('maps-path-mixed','unique')
 def test_native_close_failure(self):
  path,events=self.negative('native-close-failure','unique');self.assertNotIn('unloaded',events)
 def test_identity_drift(self):self.negative('identity-drift')
 def test_write_failure(self):self.negative('output-write','nodelete')
 def test_flush_failure(self):self.negative('output-flush','unique')
 def test_close_failure(self):self.negative('output-close')
 def test_output_exists(self):
  path,events=self.negative(output_kind='existing');self.assertEqual(path.read_bytes(),b'untouched');self.assertEqual(events,[])
 def test_output_symlink(self):
  path,events=self.negative(output_kind='symlink');self.assertTrue(path.is_symlink());self.assertEqual(events,[])
 def test_bad_nonce(self):
  path,events=self.negative(nonce='A'*64);self.assertFalse(path.exists());self.assertEqual(events,[])
 def test_exit_failure(self):
  path,events=self.negative('exit-false','nodelete');self.assertNotIn('unloaded',events)
 def test_retained_callback(self):
  path,events=self.negative('retained-timer','unique');self.assertNotIn('unloaded',events)
 def test_unregistered_thread(self):
  path,events=self.negative('unregistered-thread','unique');self.assertNotIn('unloaded',events)
if __name__=='__main__':unittest.main()
