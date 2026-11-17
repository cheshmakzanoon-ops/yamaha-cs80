"""Independent fieldwise Python bytes, not C++ encoder roundtrip tautology."""
import hashlib,json,os,pathlib,shutil,struct,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
NEGATIVES=('callback-short','callback-extra','callback-start','callback-count','callback-result','callback-mask','pcm-short','pcm-extra','pcm-nan','output-end','output-negative','output-id','output-value','output-nan','output-block-order','output-queue-order','output-queue-repeat','output-block-bound','output-total-bound','protocol-invalid')
def expected_protocol():
    b=b'CS80RPRT'+struct.pack('<IdiiqIQ',1,48000,0,2,3,1,3)
    b+=struct.pack('<qiidII',0,0,0,0,0,0)+struct.pack('<iiffii',2,60,-0.,.5,2,-1)
    b+=struct.pack('<qiidII',1,0,0,0,0,1)+struct.pack('<iiffi',2,60,1.5,.25,-1)
    b+=struct.pack('<qiidII',2,0,0,0,0,65535)+struct.pack('<Iiii',1,2,64,0)
    b+=struct.pack('<QqIdqId',2,0,7,-0.,2,8,1.)
    return b
def expected_observation():
    protocol=expected_protocol()
    b=b'CS80ROBS'+struct.pack('<IQ',1,len(protocol))+protocol
    b+=struct.pack('<Q',2)+struct.pack('<qiiQ',0,2,0,1)+struct.pack('<qiiQ',2,1,0,3)
    b+=struct.pack('<Q6f',6,0.,-0.,1.,-1.,.25,-.25)
    b+=struct.pack('<Q',4)
    for point in ((0,9,.25),(1,9,.5),(0,10,.75),(2,9,1.)):
        b+=struct.pack('<qId',*point)
    return b
class RenderEncodingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('No encoding runtime evidence: explicit SDK absent')
        sdk=pathlib.Path(value)
        for manifest in ('native_observer_sdk_provenance.json','native_observer_render_provenance.json'):
            for name,wanted in json.loads((ROOT/manifest).read_text())['headers'].items():
                p=sdk/name
                if p.is_symlink() or hashlib.sha256(p.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Pinned SDK bytes differ: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Compiler required')
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'render-encoding'
        p=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',str(sdk),str(ROOT/'native_observer_render_encoding_fixture.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if p.returncode:raise RuntimeError(p.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,name):
        p=subprocess.run([str(self.exe),name],capture_output=True,text=True,timeout=5)
        self.assertEqual(p.returncode,0,p.stderr)
        return bytes.fromhex(p.stdout)
    def test_protocol_exact(self):self.assertEqual(self.run_case('protocol'),expected_protocol())
    def test_observation_exact(self):self.assertEqual(self.run_case('observation'),expected_observation())
    def test_repeat_identity(self):self.assertEqual(self.run_case('observation'),self.run_case('observation'))
    def test_domain_separation(self):self.assertNotEqual(hashlib.sha256(self.run_case('protocol')).digest(),hashlib.sha256(self.run_case('observation')).digest())
for case in NEGATIVES:
    setattr(RenderEncodingTests,'test_'+case.replace('-','_'),lambda self,name=case:self.assertEqual(self.run_case(name),b''))
if __name__=='__main__':unittest.main()
