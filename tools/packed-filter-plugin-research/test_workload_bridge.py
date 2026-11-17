"""SDK descriptor/fake-owner tests only; no synth or real module is loaded."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parent
REPO=ROOT.parents[1]
class WorkloadBridgeTests(unittest.TestCase):
    def test_local_source_binding(self):
        for name,wanted in json.loads((ROOT/'workload_bridge_provenance.json').read_text())['local_sources'].items():
            p=REPO/name
            self.assertFalse(p.is_symlink());self.assertEqual(hashlib.sha256(p.read_bytes()).hexdigest(),wanted,name)
    def test_inert_sdk_descriptor_bridge(self):
        self.test_local_source_binding()
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:self.skipTest('No SDK bridge evidence: explicit pinned SDK absent')
        sdk=pathlib.Path(value)
        for manifest in ('native_observer_sdk_provenance.json','native_observer_render_provenance.json','native_observer_initializer_provenance.json'):
            recorded=json.loads((ROOT/manifest).read_text())
            headers={**recorded.get('headers',{}),**recorded.get('additional_headers',{})}
            for name,wanted in headers.items():
                p=sdk/name
                self.assertFalse(p.is_symlink());self.assertEqual(hashlib.sha256(p.read_bytes()).hexdigest(),wanted,name)
        compiler=shutil.which('g++');self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as tmp:
            binary=pathlib.Path(tmp)/'workload-bridge'
            argv=[compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-ffp-contract=off',
                  '-I'+str(sdk),'-I'+str(REPO/'src'),str(ROOT/'workload_bridge_fixture.cpp'),'-o',str(binary)]
            result=subprocess.run(argv,capture_output=True,text=True,timeout=60)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('108 metadata-bound descriptor mappings',result.stdout)
if __name__=='__main__':unittest.main()
