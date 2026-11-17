"""Real ProcessingWorker + actual observer helpers + inert SDK instance only."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('worker-startup-oom','wrong-thread-idle','process-and-cleanup-fail','synchronous-parity','initialize-failure','prepare-failure','capture-failure','destructor','normal-render','normal-flush','render-then-flush','process-fail','process-throw','flush-throw','worker-misuse','nested-close','nested-suspend','pump-throw','timeout','setup-mismatch','flush-invalid','submit-oom','expired')
class NativeProcessingOwnerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('Native worker lease SDK controls not exercised: explicit pinned SDK absent')
        sdk=pathlib.Path(value);headers={}
        for file,key in [('native_observer_sdk_provenance.json','headers'),('native_observer_initializer_provenance.json','additional_headers'),('native_observer_render_provenance.json','headers')]:
            headers.update(json.loads((ROOT/file).read_text())[key])
        for name,wanted in headers.items():
            p=sdk/name
            if p.is_symlink() or hashlib.sha256(p.read_bytes()).hexdigest()!=wanted:raise RuntimeError('SDK mismatch: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Selected native worker lease tests require compiler')
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'fixture'
        result=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-ffp-contract=off','-I',str(ROOT.parents[1]/'src'),'-I',str(ROOT),'-I',str(sdk),str(ROOT/'native_observer_processing_fixture.cpp'),'-ldl','-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError(result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,name):
        result=subprocess.run([str(self.exe),name],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
    def test_hang_never_unloads(self):
        with self.assertRaises(subprocess.TimeoutExpired):subprocess.run([str(self.exe),'hang'],capture_output=True,text=True,timeout=.3)
    def test_wrong_thread_destructor_terminates(self):
        result=subprocess.run([str(self.exe),'destructor-wrong-thread'],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,77,result.stderr)
    def test_active_pump_destructor_terminates(self):
        result=subprocess.run([str(self.exe),'destructor-in-pump'],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,77,result.stderr)
for case in CASES:
    setattr(NativeProcessingOwnerTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
