"""Pinned official host semantics and explicit defect reproduction, no plugin."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('host-identity','unsupported-create','message','int-float','utf16','binary-copy','missing-invalid',
       'allocation-int-control','allocation-string-control','allocation-binary-control','allocation-string-overwrite',
       'allocation-string-to-int','allocation-binary-overwrite','allocation-binary-to-float','allocation-empty-binary')
class NativeOfficialHostTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('Official host SDK tests not exercised: explicit pinned SDK absent')
        sdk=pathlib.Path(value);files={}
        for receipt,key in [('native_observer_sdk_provenance.json','headers'),('native_observer_initializer_provenance.json','additional_headers'),('native_observer_host_provenance.json','sdk_files')]:
            files.update(json.loads((ROOT/receipt).read_text())[key])
        for name,wanted in files.items():
            path=sdk/name
            if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Pinned SDK mismatch: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Selected host checks require compiler')
        cls.temp=tempfile.TemporaryDirectory();directory=pathlib.Path(cls.temp.name);cls.exe=directory/'host-fixture';obj=directory/'official-sdk.o'
        flags=[compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',str(ROOT),'-I',str(sdk)]
        # Exact upstream funknown.cpp line488 has a known %x/int* warning. Keep
        # the diagnostic visible in the receipt, downgrade only SDK TU format.
        result=subprocess.run([*flags,'-Wno-error=format','-c',str(ROOT/'native_observer_host_sdk.cpp'),'-o',str(obj)],capture_output=True,text=True,timeout=45)
        cls.sdk_diagnostics=result.stderr
        if result.returncode:raise RuntimeError('Official SDK helper compilation failed: '+result.stderr)
        result=subprocess.run([*flags,str(ROOT/'native_observer_host_fixture.cpp'),str(obj),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError('Host fixture compile/link failed: '+result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,case):
        result=subprocess.run([str(self.exe),case],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
        if case.endswith('-control'):self.assertIn('outstanding_bytes=0 outstanding_blocks=0',result.stdout)
for case in CASES:
    setattr(NativeOfficialHostTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
