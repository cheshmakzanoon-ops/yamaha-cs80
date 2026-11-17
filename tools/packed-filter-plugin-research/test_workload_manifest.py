"""Compile/run inert data-only fixture, never a synth/plugin executable."""
import hashlib,json,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parent
REPO=ROOT.parents[1]
class WorkloadManifestTests(unittest.TestCase):
    def test_source_binding(self):
        frozen=json.loads((ROOT/'workload_manifest_sources.json').read_text())
        self.assertFalse(frozen['production_qualified'])
        text=(ROOT/'workload_manifest.hpp').read_text()
        for name,digest in frozen['source_sha256'].items():
            self.assertEqual(hashlib.sha256((REPO/name).read_bytes()).hexdigest(),digest)
            self.assertIn(digest,text)
    def test_header_only_fixture(self):
        self.test_source_binding()
        compiler=shutil.which('c++')
        self.assertIsNotNone(compiler,'C++20 compiler required for inert fixture')
        with tempfile.TemporaryDirectory() as tmp:
            binary=pathlib.Path(tmp)/'fixture'
            command=[compiler,'-std=c++20','-O2','-Wall','-Wextra','-Werror','-ffp-contract=off',
                     '-I'+str(ROOT),'-I'+str(REPO/'src'),str(ROOT/'test_workload_manifest.cpp'),'-o',str(binary)]
            subprocess.run(command,check=True,capture_output=True,text=True,timeout=60)
            result=subprocess.run([str(binary)],check=True,capture_output=True,text=True,timeout=10)
            self.assertIn('PASS: 108 unique draft cases',result.stdout)
            self.assertIn('no native runtime',result.stdout)
if __name__=='__main__':unittest.main()
