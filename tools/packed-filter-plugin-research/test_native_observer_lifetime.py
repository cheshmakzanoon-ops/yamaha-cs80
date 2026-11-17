"""SDK adapter runtime tests against in-process fakes, never a real module.

Explicit SDK opt-in required. A skip is no adapter runtime evidence. The fixture
interposes dlclose with an in-process function and never calls dlopen/process.
"""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('stream-roundtrip','stream-invalid','query-interface','handler','handler-nonfinite','handler-threads','state','state-invalid',
       'metadata','metadata-range','metadata-empty','metadata-no-class','metadata-default-range','partial','partial-uninitialized','combined','combined-double-init',
       'phase-correct-sequence','phase-late-identity','phase-late-identity-declared','phase-early-metrics','phase-early-metrics-declared','phase-combined-cache','identity-other-component',
       'full-close','stop-failure','stop-exception','deactivate-failure','disconnect-failure','terminate-failure','exit-failure','dlclose-failure','retry')
class NativeFakeLifetimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('SDK adapter fake runtime not exercised: CS80_OBSERVER_SDK absent')
        sdk=pathlib.Path(value);manifest=json.loads((ROOT/'native_observer_sdk_provenance.json').read_text())
        for name,wanted in manifest['headers'].items():
            path=sdk/name
            if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Pinned SDK bytes differ: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Selected SDK fake-runtime checks require a C++ compiler')
        cls.temp=tempfile.TemporaryDirectory();cls.executable=pathlib.Path(cls.temp.name)/'fake-sdk'
        result=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread',
            '-I',str(ROOT),'-I',str(sdk),str(ROOT/'native_observer_fake_fixture.cpp'),'-o',str(cls.executable)],capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError('SDK fake fixture compilation/link failed: '+result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,name):
        result=subprocess.run([str(self.executable),name],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
for case in CASES:
    setattr(NativeFakeLifetimeTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
