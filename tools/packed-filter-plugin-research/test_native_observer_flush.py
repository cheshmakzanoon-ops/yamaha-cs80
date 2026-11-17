"""Inert zero-audio flush controls only; no plugin loaded."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('valid', 'offline', 'retry', 'encode-drift', 'encode-output', 'phase', 'missing', 'duplicate', 'wrong-id', 'input-offset', 'input-value', 'mode-mismatch', 'offset', 'value', 'quota', 'input-mutation', 'samples', 'mode', 'format', 'inputs', 'outputs', 'pointers', 'failure', 'exception', 'audio-pointer', 'event-pointer', 'context-pointer', 'invalid-output-id')
class FlushFakeTests(unittest.TestCase):
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
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'flush-fake'
        result=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',str(sdk),str(ROOT/'native_observer_flush_fixture.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError(result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,name):
        result=subprocess.run([str(self.exe),name],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
    def test_golden(self):
        import struct
        u32=lambda x:struct.pack('<I',x)
        u64=lambda x:struct.pack('<Q',x)
        f64=lambda x:struct.pack('<d',x)
        blob=lambda x:u64(len(x))+x
        controls=u32(3)+f64(.25)+u32(5)+f64(.5)
        protocol=blob(b'cs80-control-input-v1')+u32(0)+u64(2)+controls
        expected=blob(b'cs80-control-flush-v1')+blob(protocol)+u32(0)*4+u64(2)+controls+u32(0)+u64(1)+u32(7)+f64(.75)
        result=subprocess.run([str(self.exe),'golden'],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(bytes.fromhex(result.stdout),expected)

for case in CASES:
    setattr(FlushFakeTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
