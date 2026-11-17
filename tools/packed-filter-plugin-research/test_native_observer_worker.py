"""Real dedicated-thread controls only; no SDK/native plugin invocation."""
import pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('copy-throw','invalid-kind','task-destructor-lock','cancel-destructor-lock','startup-oom','destructor-joins','basic','cross-receipt','close-before-await','submit-oom','wrong-submit','after-close','nested-construct','double-submit','wrong-sequence','expired','invalid-budget','wrong-await','callback-throw','worker-submit','worker-close','timeout-late','late-observed','on-time-observed-late','pump-throw','pump-close','pump-submit','pump-await','pump-budget','repeat-await')
class ProcessingWorkerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Mandatory processing-worker controls require g++')
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'fixture'
        argv=[compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',str(ROOT),str(ROOT/'native_observer_worker_fixture.cpp'),'-o',str(cls.exe)]
        result=subprocess.run(argv,capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError(result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,name):
        result=subprocess.run([str(self.exe),name],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('worker_control_passed=1',result.stdout)
    def test_hung_close_never_detaches(self):
        with self.assertRaises(subprocess.TimeoutExpired):subprocess.run([str(self.exe),'hang'],capture_output=True,text=True,timeout=.3)
    def test_hung_destructor_never_detaches(self):
        with self.assertRaises(subprocess.TimeoutExpired):subprocess.run([str(self.exe),'destructor-hang'],capture_output=True,text=True,timeout=.3)
for case in CASES:
    setattr(ProcessingWorkerTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
