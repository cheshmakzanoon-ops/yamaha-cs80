"""Joined actual identity/pin/dlopen/maps proof using tiny inert SOs only."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('maps-pre-unreadable','maps-post-malformed','maps-close-truncated','post-resolve-reverify','valid','reload','preloaded','stale-path','post-open-proof','post-resolve-provider','constructor-mutation','before-close-mutation','outside-reference','close-failure','missing-entry','dependency-export')
class SealedLoaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('Explicit SDK absent; joined loader not exercised')
        sdk=pathlib.Path(value)
        for name,key in [('native_observer_sdk_provenance.json','headers'),('native_observer_initializer_provenance.json','additional_headers')]:
            for p,w in json.loads((ROOT/name).read_text())[key].items():
                file=sdk/p
                if file.is_symlink() or hashlib.sha256(file.read_bytes()).hexdigest()!=w:raise RuntimeError('SDK drift '+p)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Compiler required')
        cls.temp=tempfile.TemporaryDirectory();cls.root=pathlib.Path(cls.temp.name);cls.exe=cls.root/'sealed'
        flags=[compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic']
        def build(args):
            p=subprocess.run(args,capture_output=True,text=True,timeout=40)
            if p.returncode:raise RuntimeError(p.stderr)
        build([*flags,'-pthread','-I',str(sdk),str(ROOT/'native_sealed_loader_fixture.cpp'),'-Wl,--wrap=open','-Wl,--wrap=dlinfo','-Wl,--wrap=dlsym','-Wl,--wrap=dlclose','-ldl','-lcrypto','-o',str(cls.exe)])
        for name,extra in [('valid',[]),('missing-entry',['-DOMIT_ENTRY'])]:build([*flags,'-shared','-fPIC',*extra,str(ROOT/'native_sealed_loader_library.cpp'),'-o',str(cls.root/(name+'.so'))])
        dep=cls.root/'dep.cpp';dep.write_text('extern "C" bool ModuleEntry(void*){return true;}\n')
        build([*flags,'-shared','-fPIC',str(dep),'-o',str(cls.root/'libdep.so')])
        build([*flags,'-shared','-fPIC','-DOMIT_ENTRY',str(ROOT/'native_sealed_loader_library.cpp'),'-L'+str(cls.root),'-Wl,--no-as-needed','-ldep','-Wl,-rpath,$ORIGIN','-o',str(cls.root/'dependency-export.so')])
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,case):
        bundle=self.root/case;bundle.mkdir();library=self.root/((case if case in ('missing-entry','dependency-export') else 'valid')+'.so');shutil.copy2(library,bundle/'module.so');(bundle/'resource').write_bytes(b'resource')
        if case=='dependency-export':shutil.copy2(self.root/'libdep.so',bundle/'libdep.so')
        st=bundle.stat();lines=[f'{json.dumps(str(bundle))} {st.st_mode&0o777} {st.st_uid} {st.st_gid}','"module.so"']
        rows=[f'"." 1 {st.st_mode&0o777} {st.st_uid} {st.st_gid} 0 ""']
        for p in sorted(bundle.iterdir()):
            st=p.stat();rows.append(f'{json.dumps(p.name)} 0 {st.st_mode&0o777} {st.st_uid} {st.st_gid} {st.st_size} "{hashlib.sha256(p.read_bytes()).hexdigest()}"')
        lines+=[str(len(rows)),*rows];manifest=self.root/(case+'.manifest');manifest.write_text('\n'.join(lines)+'\n')
        p=subprocess.run([str(self.exe),str(manifest),case],capture_output=True,text=True,timeout=10)
        self.assertEqual(p.returncode,0,p.stderr)
for case in CASES:setattr(SealedLoaderTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
