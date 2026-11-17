"""Pure structural provider checks; no plugin loads or verified binding claim."""
import pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('elf-section-extended','elf-section-entry','elf-section-overflow','elf-load-flags','maps-empty','maps-offset-overflow','maps-unpaged','maps-inode-suffix','valid','aslr','stale-path-not-covered','self-maps','elf-short','elf-class','elf-endian','elf-ident-version','elf-version','elf-machine','elf-type','elf-header-size','elf-entry-size','elf-count','elf-ph-overflow','elf-filesz','elf-memory','elf-address-overflow','elf-align','elf-congruence','elf-nonexec','elf-overlap','maps-truncated','maps-perms','maps-device','maps-overflow','maps-overlap','maps-reverse','maps-line-bound','maps-byte-bound','wrong-inode','wrong-device','wrong-size','wrong-base','wrong-offset','nonexec-map','bss-export','before-base','preexisting')
class ProviderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Compiler required for structural controls')
        cls.temp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.temp.name)/'provider'
        p=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic',str(ROOT/'native_provider_fixture.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
        if p.returncode:raise RuntimeError(p.stderr)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,case):
        p=subprocess.run([str(self.exe),case],capture_output=True,text=True,timeout=5)
        self.assertEqual(p.returncode,0,p.stderr)
for case in CASES:setattr(ProviderTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
