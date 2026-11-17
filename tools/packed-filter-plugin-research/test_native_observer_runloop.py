"""Scoped UI-only IRunLoop with inert fds/timers, not real plugin integration."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('thread-release','thread-unregister','thread-pump','timer-conflict','closed','snapshot-ref-fail','snapshot-ref-throw','qi','thread','timer-zero','timer-overflow','deadline-overflow','timer-real','timer-order','timer-skips','timer-self','timer-advance-overflow','receipt-cap','ref-fail','ref-throw','ref-reenter','capacity','many-fds','shutdown-live','final-release-live','read','idle','self','other','generation','throw','nested','active-release','budget','round-budget','deadline-snapshot','hangup','invalid-fd','expired')
class NativeRunLoopTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('Runloop SDK fixture not exercised: explicit pinned SDK absent')
        sdk=pathlib.Path(value);files=json.loads((ROOT/'native_observer_sdk_provenance.json').read_text())['headers']
        files.update(json.loads((ROOT/'native_observer_runloop_provenance.json').read_text())['additional_headers'])
        for name,wanted in files.items():
            p=sdk/name
            if p.is_symlink() or hashlib.sha256(p.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Pinned SDK mismatch: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Runloop fixture compiler missing')
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'fixture'
        argv=[compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',str(ROOT),'-I',str(sdk),str(ROOT/'native_observer_runloop_fixture.cpp'),'-o',str(cls.exe)]
        result=subprocess.run(argv,capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError(result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,name):
        result=subprocess.run([str(self.exe),name],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('handler_refs_balanced=1',result.stdout)
    def test_hung_callback_watchdog(self):
        with self.assertRaises(subprocess.TimeoutExpired):subprocess.run([str(self.exe),'hang'],capture_output=True,text=True,timeout=.3)
for case in CASES:
    setattr(NativeRunLoopTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
