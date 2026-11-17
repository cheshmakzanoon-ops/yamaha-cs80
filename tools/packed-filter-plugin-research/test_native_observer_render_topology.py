"""Inert SDK renderer controls: disabled input stays described but has no samples."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('valid','legacy','encoding','invalid-zero','invalid-three','input-count','inputs-null','inputs-pointer','channels-zero','channels-two','array-null','array-pointer','sample-nonnull','silence')
class RenderTopologyTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  sdk=os.environ.get('CS80_OBSERVER_SDK')
  if not sdk:raise unittest.SkipTest('explicit pinned SDK required')
  for manifest in ('native_observer_sdk_provenance.json','native_observer_render_provenance.json'):
   for name,wanted in json.loads((ROOT/manifest).read_text())['headers'].items():
    path=pathlib.Path(sdk)/name
    if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=wanted:raise RuntimeError('SDK drift '+name)
  for name,wanted in json.loads((ROOT/'native_observer_render_topology_provenance.json').read_text())['sources'].items():
   path=ROOT/name
   if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Topology source drift '+name)
  cls.tmp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.tmp.name)/'render-topology'
  p=subprocess.run([shutil.which('g++'),'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',sdk,str(ROOT/'native_observer_render_topology_fixture.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
  if p.returncode:raise RuntimeError(p.stderr)
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def case(self,name):
  p=subprocess.run([str(self.exe),name],capture_output=True,text=True,timeout=5);self.assertEqual(p.returncode,0,p.stderr)
for name in CASES:setattr(RenderTopologyTests,'test_'+name.replace('-','_'),lambda self,n=name:self.case(n))
if __name__=='__main__':unittest.main()
