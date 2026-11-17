"""Tests real dlopen only with tiny inert shared objects compiled in this test."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('direct','wrong-close','wrong-resolve','no-reopen','invalid-elf','noncanonical','missing','symlink',
       'factory-null','missing-entry','missing-factory','missing-exit','data-export','entry-failure','exit-failure','dependency-export')
class NativeLoaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('Inert loader fixtures not exercised: explicit pinned SDK absent')
        sdk=pathlib.Path(value);files=json.loads((ROOT/'native_observer_sdk_provenance.json').read_text())['headers'];files.update(json.loads((ROOT/'native_observer_initializer_provenance.json').read_text())['additional_headers'])
        for name,wanted in files.items():
            path=sdk/name
            if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Pinned SDK mismatch: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Selected loader checks require compiler')
        cls.temp=tempfile.TemporaryDirectory();cls.directory=pathlib.Path(cls.temp.name);cls.exe=cls.directory/'loader-fixture'
        flags=[compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic']
        def build(argv):
            result=subprocess.run(argv,capture_output=True,text=True,timeout=30)
            if result.returncode:raise RuntimeError('Inert fixture compilation failed: '+result.stderr)
        build([*flags,'-pthread','-I',str(ROOT),'-I',str(sdk),str(ROOT/'native_observer_loader_fixture.cpp'),'-ldl','-o',str(cls.exe)])
        variants={'valid':[],'missing-entry':['-DOMIT_ENTRY'],'missing-factory':['-DOMIT_FACTORY'],'missing-exit':['-DOMIT_EXIT'],'data-export':['-DDATA_ENTRY'],'entry-failure':['-DENTRY_FAIL=1'],'exit-failure':['-DEXIT_FAIL=1']}
        cls.libraries={}
        for name,extra in variants.items():
            path=cls.directory/(name+'.so');build([*flags,'-fPIC','-shared',*extra,str(ROOT/'native_observer_loader_library.cpp'),'-o',str(path)]);cls.libraries[name]=path
        dep=cls.directory/'dependency.cpp';dep.write_text('extern "C" bool ModuleEntry(void*) {return true;}\n')
        build([*flags,'-fPIC','-shared',str(dep),'-o',str(cls.directory/'libdependency.so')])
        path=cls.directory/'dependency-export.so'
        build([*flags,'-fPIC','-shared','-DOMIT_ENTRY',str(ROOT/'native_observer_loader_library.cpp'),'-L'+str(cls.directory),'-Wl,--no-as-needed','-ldependency','-Wl,-rpath,$ORIGIN','-o',str(path)])
        cls.libraries['dependency-export']=path
        cls.invalid=cls.directory/'invalid.so';cls.invalid.write_text('not an ELF image')
        cls.link=cls.directory/'link.so';cls.link.symlink_to(cls.libraries['valid'])
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,case):
        path=self.libraries.get(case,self.libraries['valid'])
        if case=='invalid-elf':path=self.invalid
        elif case=='noncanonical':path=str(self.directory)+'/./valid.so'
        elif case=='missing':path=self.directory/'missing.so'
        elif case=='symlink':path=self.link
        log=self.directory/(case+'.log');env=dict(os.environ,CS80_LOADER_FIXTURE_LOG=str(log))
        result=subprocess.run([str(self.exe),case,str(path)],env=env,capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
        observed=log.read_text().splitlines() if log.exists() else []
        if case in ('invalid-elf','noncanonical','missing','symlink'):expected=[]
        elif case in ('missing-entry','missing-factory','missing-exit','data-export','dependency-export'):expected=['open','unload']
        elif case=='entry-failure':expected=['open','entry','unload']
        else:expected=['open','entry','exit','unload']
        self.assertEqual(observed,expected)
for case in CASES:
    setattr(NativeLoaderTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
