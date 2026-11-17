"""Actual inert shared-library factory/A-D/ModuleExit/unload integration."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
CASES=('destroy-active','overlap-refused','task-baseline-unreadable','task-postexit-malformed','task-postexit-missing','retry-after-success','caught-nested','caught-wrong-thread','unregistered-thread','constructor-thread-postopen','constructor-thread-internalproof','constructor-thread-resolve','nodelete-retained','factory-create-allocation','factory-null','component-init-false','setup-false','factory-release-throw','destructor-overrelease','exit-registers','valid','retained-timer','retained-host','retained-service','exit-false','exit-throw')
class ModuleHistoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        value=os.environ.get('CS80_OBSERVER_SDK')
        if not value:raise unittest.SkipTest('Explicit SDK absent; no real-handle inert integration')
        sdk=pathlib.Path(value)
        for name,wanted in json.loads((ROOT/'native_observer_module_history_provenance.json').read_text())['dependencies'].items():
            path=ROOT/name
            if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Module history dependency drift: '+name)
        for name in ('native_observer_sdk_provenance.json','native_observer_initializer_provenance.json','native_observer_render_provenance.json','native_observer_runloop_provenance.json'):
            m=json.loads((ROOT/name).read_text())
            for p,w in {**m.get('headers',{}),**m.get('additional_headers',{})}.items():
                path=sdk/p
                if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=w:raise RuntimeError('SDK drift '+p)
        for name,wanted in json.loads((ROOT/'workload_bridge_provenance.json').read_text())['local_sources'].items():
            path=ROOT.parents[1]/name
            if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=wanted:raise RuntimeError('Frozen workload source drift: '+name)
        compiler=shutil.which('g++')
        if not compiler:raise RuntimeError('Compiler required')
        cls.temp=tempfile.TemporaryDirectory();cls.root=pathlib.Path(cls.temp.name);cls.exe=cls.root/'module-history';cls.lib=cls.root/'fixture.so'
        flags=[compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-ffp-contract=off','-I',str(sdk),'-I',str(ROOT.parents[1]/'src')]
        def build(args):
            p=subprocess.run(args,capture_output=True,text=True,timeout=60)
            if p.returncode:raise RuntimeError(p.stderr)
        build([*flags,str(ROOT/'native_observer_module_history_fixture.cpp'),'-rdynamic','-Wl,--wrap=open','-Wl,--wrap=dlinfo','-Wl,--wrap=dlsym','-Wl,--wrap=dlclose','-ldl','-lcrypto','-o',str(cls.exe)])
        build([*flags,'-fno-gnu-unique','-shared','-fPIC',str(ROOT/'native_observer_module_history_library.cpp'),'-o',str(cls.lib)])
        cls.retained=cls.root/'gnu-unique.so';build([*flags,'-shared','-fPIC','-Wl,-z,nodelete',str(ROOT/'native_observer_module_history_library.cpp'),'-o',str(cls.retained)])
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def run_case(self,case):
        bundle=self.root/case;bundle.mkdir();shutil.copy2(self.retained if case=='nodelete-retained' else self.lib,bundle/'module.so');st=bundle.stat();rows=[f'"." 1 {st.st_mode&0o777} {st.st_uid} {st.st_gid} 0 ""'];p=bundle/'module.so';f=p.stat();rows.append(f'"module.so" 0 {f.st_mode&0o777} {f.st_uid} {f.st_gid} {f.st_size} "{hashlib.sha256(p.read_bytes()).hexdigest()}"')
        manifest=self.root/(case+'.manifest');manifest.write_text(f'{json.dumps(str(bundle))} {st.st_mode&0o777} {st.st_uid} {st.st_gid}\n"module.so"\n{len(rows)}\n'+'\n'.join(rows)+'\n')
        log=self.root/(case+'.log');env=dict(os.environ,CS80_JOIN_LOG=str(log));p=subprocess.run([str(self.exe),str(manifest),case],env=env,capture_output=True,text=True,timeout=15)
        self.assertEqual(p.returncode,29 if case=='destroy-active' else 0 if case in ('valid','retry-after-success','overlap-refused') else 23,p.stderr)
        events=log.read_text().splitlines() if log.exists() else [];self.assertEqual(events.count('loaded'),0 if case=='task-baseline-unreadable' else 1);self.assertEqual(events.count('entry'),0 if case.startswith('constructor-thread') or case=='task-baseline-unreadable' else 1);self.assertEqual(events.count('terminate'),0 if case.startswith('constructor-thread') or case in ('factory-null','component-init-false','factory-create-allocation','caught-nested','destroy-active','task-baseline-unreadable') else 1);self.assertEqual(events.count('exit'),0 if case.startswith('constructor-thread') or case in ('setup-false','component-init-false','factory-create-allocation','caught-nested','destroy-active','caught-wrong-thread','task-baseline-unreadable') else 1)
        if case in ('valid','retry-after-success','overlap-refused','destructor-overrelease'):self.assertEqual(events.count('unloaded'),1)
        else:self.assertNotIn('unloaded',events)
        if case=='caught-wrong-thread':self.assertEqual(events.count('flush'),1);self.assertEqual(events.count('render'),0)
        if case=='destroy-active':self.assertIn('active-owner-destructor blocked',p.stderr)
        elif case not in ('valid','retry-after-success','overlap-refused'):self.assertIn('QUARANTINED',p.stderr)
for case in CASES:setattr(ModuleHistoryTests,'test_'+case.replace('-','_'),lambda self,name=case:self.run_case(name))
if __name__=='__main__':unittest.main()
