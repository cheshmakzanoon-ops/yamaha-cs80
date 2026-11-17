"""Opt-in, injected-loader SDK fakes. No module is opened and no audio rendered."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=['success','factory3-success','separate-success','duplicate-cid','missing-cid','separate-missing-class','separate-alias','no-retry','live-module-close']
CASES += ['module:'+x for x in ('host-query','loader-open','loader-resolve','ModuleEntry','GetPluginFactory','factory3-query','factory3-host','class-info')]
CASES += ['instance:'+x for x in ('component-create','combined-query','component-initialize','audio-query','handler-attach','component-state','controller-state','audio-bus-info','event-bus-info','bus-arrangements','bus-readback','sample-size','setup','audio-bus-activate','event-bus-activate','activate','start-processing')]
CASES += ['separate:'+x for x in ('controller-class-id','controller-create','component-identity','controller-identity','controller-initialize','handler-attach','component-connection-query','controller-connection-query','component-connect','controller-connect')]
CASES += ['cleanup:'+x for x in ('stop-processing','deactivate','event-bus-deactivate','audio-bus-deactivate','handler-detach','component-terminate')]
class NativeInitializerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('Injected initializer not exercised: explicit pinned CS80_OBSERVER_SDK absent')
        sdk=pathlib.Path(value)
        headers=json.loads((ROOT/'native_observer_sdk_provenance.json').read_text())['headers']
        headers.update(json.loads((ROOT/'native_observer_initializer_provenance.json').read_text())['additional_headers'])
        for name,digest in headers.items():
            path=sdk/name
            if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:raise RuntimeError('Pinned SDK mismatch: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Selected initializer checks require C++ compiler')
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'injected-initializer'
        result=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',str(ROOT),'-I',str(sdk),str(ROOT/'native_observer_initializer_fixture.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError('Injected initializer fixture compilation/link failed: '+result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,case):
        result=subprocess.run([str(self.exe),case],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
for case in CASES:
    setattr(NativeInitializerTests,'test_'+case.replace(':','_').replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
