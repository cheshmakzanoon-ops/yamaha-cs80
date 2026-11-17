"""Retained descriptors over tiny inert files only, never dlopen."""
import pathlib,shutil,subprocess,tempfile,unittest
import test_native_identity as base
from test_native_identity import write_manifest,ROOT
class NativeIdentityPinTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which('g++')
        if not compiler:raise unittest.SkipTest('Native pin requires C++ compiler and official OpenSSL headers')
        cls.build=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.build.name)/'pin'
        r=subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-pedantic','-I',str(ROOT),str(ROOT/'native_identity_pin_fixture.cpp'),'-lcrypto','-Wl,--wrap=read','-Wl,--wrap=pread','-Wl,--wrap=EVP_DigestUpdate','-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if r.returncode:raise RuntimeError(r.stderr)
    @classmethod
    def tearDownClass(cls):cls.build.cleanup()
    def setUp(self):base.NativeIdentityTests.setUp(self)
    def run_case(self,action,ok=False):
        p=self.base/'expected';write_manifest(p,self.m)
        r=subprocess.run([str(self.exe),str(p),action],capture_output=True,text=True,timeout=5)
        self.assertEqual(r.returncode,0 if ok else 1,r.stderr)
        self.assertNotIn('descriptor leak',r.stderr)
        if action=='second-fail':self.assertIn('input bytes/metadata mismatch',r.stderr)
        if action=='replace':self.assertIn('verified inode identity changed',r.stderr)
        return r
    def test_stamp_independent(self):
        r=self.run_case('stamp',True);s=(self.bundle/'module.bin').stat();e=next(x for x in self.m['entries'] if x['path']=='module.bin')
        self.assertEqual(r.stdout.strip().split(),list(map(str,[s.st_dev,s.st_ino,e['size'],e['mode'],e['uid'],e['gid'],e['sha256']])))
    def test_intermediate_symlink(self):self.m['module']='data/resource';self.run_case('intermediate')
for action in ('check','lifetime','offset'):
    setattr(NativeIdentityPinTests,'test_'+action,lambda self,a=action:(self.run_case(a,True),None)[1])
for action in ('pin-evp','held-writable','replace','different','symlink','second-fail','held-change','held-mode','held-grow','held-truncate','held-evp','held-unlink'):
    setattr(NativeIdentityPinTests,'test_'+action.replace('-','_'),lambda self,a=action:(self.run_case(a),None)[1])
if __name__=='__main__':unittest.main()
