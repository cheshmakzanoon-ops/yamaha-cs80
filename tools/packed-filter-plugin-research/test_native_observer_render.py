"""Inert IAudioProcessor adapter controls only; never loads a real plugin."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('note-off','context-mutation','interface-mutation','sample-size-mutation','interface-bounds','output-invalid-id','output-negative','output-infinite','queue-quota','event-ppq','event-flags','note-length','note-channel','note-id','note-tuning','boundaries','empty','output','cc','cc-no-opt-in','cc-invalid','event-count','param-count','rate','mode','block','frames','event-order','event-end','event-type','event-bus','event-offset','note-pitch','note-value','param-order','param-end','param-id','param-value','process-failure','process-throw','nan','silence','descriptor','input-event-mutation','input-param-mutation','input-point-mutation','output-offset','output-value','output-order','output-quota')
class RenderFakeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('No render runtime evidence: explicit SDK absent')
        sdk=pathlib.Path(value)
        for manifest in ('native_observer_sdk_provenance.json','native_observer_render_provenance.json'):
            for name,wanted in json.loads((ROOT/manifest).read_text())['headers'].items():
                p=sdk/name
                if p.is_symlink() or hashlib.sha256(p.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Pinned SDK bytes differ: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Compiler required for selected SDK controls')
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'render-fake'
        result=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',str(sdk),str(ROOT/'native_observer_render_fixture.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError(result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,name):
        result=subprocess.run([str(self.exe),name],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
for case in CASES:
    setattr(RenderFakeTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
