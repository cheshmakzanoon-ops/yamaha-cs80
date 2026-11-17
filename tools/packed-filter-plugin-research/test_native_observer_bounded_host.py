"""Inert runtime tests of separately implemented bounded host, no module loading."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('reference-guards','global-keys','zero-allocation-binary','noalloc-qi','key-allocation-rollback','host-qi','unsupported-create','owner-loss','invalid-policy','oom-create','oom-message-child',
       'limit-services','message-basic','message-oom','message-null','message-retained-attrs','limit-message',
       'variants','attributes-qi','utf16','binary','overwrite','concurrent','oom-insert','oom-node','oom-replace',
       'transient-budget','limit-keys','limit-list-keys','limit-list-bytes','limit-value','limit-key')
class NativeBoundedHostTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('Bounded SDK host runtime not exercised: explicit pinned SDK absent')
        sdk=pathlib.Path(value)
        files=json.loads((ROOT/'native_observer_sdk_provenance.json').read_text())['headers']
        files.update(json.loads((ROOT/'native_observer_initializer_provenance.json').read_text())['additional_headers'])
        for name,wanted in files.items():
            path=sdk/name
            if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Pinned SDK mismatch: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Selected bounded host tests require compiler')
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'fixture'
        cls.argv=[compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',str(ROOT),'-I',str(sdk),str(ROOT/'native_observer_bounded_host_fixture.cpp'),'-o',str(cls.exe)]
        result=subprocess.run(cls.argv,capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError('Bounded host fixture compilation failed: '+result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,case):
        result=subprocess.run([str(self.exe),case],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('allocation_balance=0',result.stdout)
for case in CASES:
    setattr(NativeBoundedHostTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
