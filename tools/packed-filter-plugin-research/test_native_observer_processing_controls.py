"""Full vs subset control authority and native-owned snapshot copies."""
import test_native_observer_processing as bootstrap
ROOT=bootstrap.ROOT
import pathlib,subprocess,unittest
CASES=('restore-copy','restore-early','restore-failed','restore-post-close','full-order-golden','full-reordered-golden','full-missing','full-extra','full-duplicate','full-positional','subset-before-full','failed-full-subset','subset-success','subset-unknown','subset-duplicate','subset-all','subset-empty','snapshot-copy','snapshot-fresh','snapshot-unprepared','snapshot-unsaved','snapshot-no-metrics','snapshot-copy-oom','snapshot-rejected-save','snapshot-wrong-thread')
class NativeProcessingControlsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Reuse exact SDK verification, not inherited test methods.
        bootstrap.NativeProcessingOwnerTests.setUpClass();cls.temp=bootstrap.NativeProcessingOwnerTests.temp;cls.exe=pathlib.Path(cls.temp.name)/'controls'
        import os,shutil
        sdk=os.environ['CS80_OBSERVER_SDK'];compiler=shutil.which('g++')
        result=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',str(ROOT),'-I',sdk,str(ROOT/'native_observer_processing_controls_fixture.cpp'),'-ldl','-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if result.returncode:raise RuntimeError(result.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,name):
        result=subprocess.run([str(self.exe),name],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
for case in CASES:
    setattr(NativeProcessingControlsTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
