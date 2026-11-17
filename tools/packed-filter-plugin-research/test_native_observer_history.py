"""Fixed A-D actual owner/worker plus inert SDK integration; no native module."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('fail-A-and-close','final-encoding-allocation','invalid-case','fail-setup','fail-metadata','allocation-after-A','history','reordered-roster','retry','expired','fail-initial','fail-A','fail-state','fail-alter','fail-B','fail-suspend','fail-restore','fail-resume','fail-C','fail-D','fail-close')
class HistoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('Explicit SDK absent; no joined history evidence')
        sdk=pathlib.Path(value)
        for name,wanted in json.loads((ROOT/'native_observer_history_provenance.json').read_text())['dependencies'].items():
            path=ROOT/name
            if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=wanted:raise RuntimeError('History dependency changed: '+name)
        for name in ('native_observer_sdk_provenance.json','native_observer_initializer_provenance.json','native_observer_render_provenance.json'):
            m=json.loads((ROOT/name).read_text())
            for p,w in {**m.get('headers',{}),**m.get('additional_headers',{})}.items():
                path=sdk/p
                if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=w:raise RuntimeError('Pinned SDK changed: '+p)
        for p,w in json.loads((ROOT/'workload_bridge_provenance.json').read_text())['local_sources'].items():
            path=ROOT.parents[1]/p
            if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=w:raise RuntimeError('Frozen workload source changed: '+p)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Compiler required')
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'history'
        p=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-ffp-contract=off','-I',str(sdk),'-I',str(ROOT.parents[1]/'src'),str(ROOT/'native_observer_history_fixture.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,timeout=60)
        if p.returncode:raise RuntimeError(p.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,case):
        p=subprocess.run([str(self.exe),case],capture_output=True,text=True,timeout=10)
        self.assertEqual(p.returncode,0,p.stderr)
for case in CASES:setattr(HistoryTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
