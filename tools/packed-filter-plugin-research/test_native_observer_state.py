"""Inert explicit state-contract controls; never loads a real plugin."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('unsupported', 'supported', 'supported-empty', 'component-empty', 'component-failure', 'component-throw', 'component-stream', 'controller-other-failure', 'controller-ok-unexpected', 'controller-bytes', 'controller-stream', 'controller-throw', 'wrong-policy', 'retry', 'encoding-tamper', 'restore-component-failure', 'restore-component-throw', 'restore-mutate', 'restore-stream', 'restore-sync-failure', 'restore-unadmitted', 'restore-retry', 'restore-controller-failure', 'restore-controller-throw', 'restore-controller-mutate')
class StateContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('No render runtime evidence: explicit SDK absent')
        sdk=pathlib.Path(value)
        for manifest in ('native_observer_sdk_provenance.json',):
            for name,wanted in json.loads((ROOT/manifest).read_text())['headers'].items():
                p=sdk/name
                if p.is_symlink() or hashlib.sha256(p.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Pinned SDK bytes differ: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Compiler required for selected SDK controls')
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'state-fake'
        result=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',str(sdk),str(ROOT/'native_observer_state_fixture.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError(result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,name):
        result=subprocess.run([str(self.exe),name],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
for case in CASES:
    setattr(StateContractTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
