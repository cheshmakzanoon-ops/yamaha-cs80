"""Phase-split and bounded history controls against injected inert SDK objects."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=['operation-nested-caught','metadata-duplicate-id','receipt-spoof','operation-caught-reentrant','cleanup-allocation','history','separate-history','flush','fresh-processing','fresh-prepare','fresh-suspend','fresh-resume','fresh-restore','invalid-wrapper-config','duplicate-initialize','connected-processing','invalid-split-config','metadata-allocation','duplicate-metadata','late-metadata','duplicate-prepare','operation-exception','operation-reentrant','flush-invalid','flush-repeat','suspend-unsaved','save-repeat','restore-processing','suspend-repeat','resume-unrestored','suspended-operation','restore-repeat','second-restore','third-suspend','extra-resume','closed-access']
for action,operations in [('suspend',('stop-processing','deactivate','event-bus-deactivate','audio-bus-deactivate')),('resume',('audio-bus-activate','event-bus-activate','activate','start-processing')),('restore',('component-restore','restore-sync','controller-restore'))]:
    for kind in ('fail','throw'):CASES += [action+'-'+kind+':'+op for op in operations]
class PhaseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('Explicit SDK absent; no phase runtime evidence')
        sdk=pathlib.Path(value)
        for manifest,key in [('native_observer_sdk_provenance.json','headers'),('native_observer_initializer_provenance.json','additional_headers'),('native_observer_render_provenance.json','headers')]:
            for name,wanted in json.loads((ROOT/manifest).read_text())[key].items():
                p=sdk/name
                if p.is_symlink() or hashlib.sha256(p.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Pinned SDK mismatch: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Compiler required')
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'phases'
        p=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',str(sdk),str(ROOT/'native_observer_initializer_phase_fixture.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if p.returncode:raise RuntimeError(p.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,case):
        p=subprocess.run([str(self.exe),case],capture_output=True,text=True,timeout=5)
        self.assertEqual(p.returncode,0,p.stderr)
for case in CASES:setattr(PhaseTests,'test_'+case.replace(':','_').replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
