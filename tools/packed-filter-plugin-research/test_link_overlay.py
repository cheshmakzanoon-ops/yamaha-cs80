"""Compile/link tiny inert targets only. Never execute resulting binaries."""
import os,pathlib,re,shlex,shutil,subprocess,tempfile,time,unittest
ROOT=pathlib.Path(__file__).parent
OVERLAY=ROOT/'link_overlay.cmake'

@unittest.skipUnless(os.environ.get('CS80_CMAKE') and os.environ.get('CS80_NINJA'),'explicit CMake/Ninja required for inert overlay controls')
class LinkOverlayTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=pathlib.Path(self.tmp.name)
        self.source=self.root/'source';self.source.mkdir();self.build=self.root/'build'
        (self.source/'tiny.cpp').write_text('int main() { return 0; }\n')
        self.cmake=os.environ['CS80_CMAKE'];self.ninja=os.environ['CS80_NINJA']
        self.base='''cmake_minimum_required(VERSION 3.24)
project(InertMapControl LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library(CS80Reference_VST3 MODULE tiny.cpp)
add_executable(CS80Reference_Standalone tiny.cpp)
add_executable(unaffected tiny.cpp)
'''
    def configure(self,code=None,overlay=True,extra=(),ok=True):
        (self.source/'CMakeLists.txt').write_text(self.base if code is None else code)
        argv=[self.cmake,'-S',str(self.source),'-B',str(self.build),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_MAKE_PROGRAM='+self.ninja]
        if overlay:argv+=['-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES='+str(OVERLAY)]
        r=subprocess.run([*argv,*extra],capture_output=True,text=True,timeout=30)
        self.assertEqual(r.returncode==0,ok,r.stdout+r.stderr)
        return r
    def compile(self):
        r=subprocess.run([self.cmake,'--build',str(self.build),'--verbose','--parallel','1'],capture_output=True,text=True,timeout=30)
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        commands={}
        for line in r.stdout.splitlines():
            if not re.match(r'^\[\d+/\d+\]',line):continue
            argv=shlex.split(re.sub(r'^\[\d+/\d+\] ','',line))
            if '-o' not in argv or '-c' in argv:continue
            output=argv[argv.index('-o')+1]
            commands[output]=argv
        self.assertEqual(set(commands),{'libCS80Reference_VST3.so','CS80Reference_Standalone','unaffected'})
        return commands
    def test_actual_compile_and_link_identity(self):
        self.configure(overlay=False);baseline_compile=(self.build/'compile_commands.json').read_bytes();baseline_link=self.compile()
        self.assertFalse(list(self.build.glob('*.research.map')))
        shutil.rmtree(self.build)
        # Same canonical source/build/overlay path, both passes. No flags normalized away.
        self.configure();self.assertEqual((self.build/'compile_commands.json').read_bytes(),baseline_compile)
        self.assertFalse(list(self.build.glob('*.research.map')));start=time.time_ns();actual=self.compile()
        for output,target in [('libCS80Reference_VST3.so','CS80Reference_VST3'),('CS80Reference_Standalone','CS80Reference_Standalone')]:
            mapfile=self.build/(target+'.research.map');flag='-Wl,-Map,'+str(mapfile)+',--cref'
            self.assertEqual(actual[output].count(flag),1)
            self.assertEqual([x for x in actual[output] if x!=flag],baseline_link[output])
            self.assertGreater(mapfile.stat().st_size,0);self.assertGreaterEqual(mapfile.stat().st_mtime_ns,start)
            self.assertIn('Cross Reference Table',mapfile.read_text())
        self.assertEqual(actual['unaffected'],baseline_link['unaffected'])
        self.assertEqual(len(list(self.build.glob('*.research.map'))),2)
    def test_both_arms_same_overlay_same_compile(self):
        observations=[]
        for arm in ('stock','candidate'):
            if self.build.exists():shutil.rmtree(self.build)
            self.configure();observations.append(((self.build/'compile_commands.json').read_bytes(),self.compile()))
        self.assertEqual(*observations)
    def test_missing_target(self):self.configure(self.base.replace('add_library(CS80Reference_VST3 MODULE tiny.cpp)\n',''),ok=False)
    def test_double_application(self):self.configure(extra=['-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES='+str(OVERLAY)+';'+str(OVERLAY)],ok=False)
    def test_alias_target(self):self.configure(self.base.replace('add_library(CS80Reference_VST3 MODULE tiny.cpp)','add_library(real MODULE tiny.cpp)\nadd_library(CS80Reference_VST3 ALIAS real)'),ok=False)
    def test_imported_target(self):self.configure(self.base.replace('add_library(CS80Reference_VST3 MODULE tiny.cpp)','add_library(CS80Reference_VST3 MODULE IMPORTED)'),ok=False)
    def test_wrong_target_type(self):self.configure(self.base.replace('CS80Reference_VST3 MODULE','CS80Reference_VST3 STATIC'),ok=False)
    def test_symlink_build(self):
        real=self.root/'real';real.mkdir();self.build.symlink_to(real,target_is_directory=True);self.configure(ok=False)
    def test_unsafe_comma_build(self):self.build=self.root/'build,bad';self.configure(ok=False)
    def test_space_build(self):self.build=self.root/'build with space';self.configure();self.compile()
    def test_deferred_subdirectory_targets(self):
        sub=self.source/'sub';sub.mkdir();(sub/'CMakeLists.txt').write_text(self.base[self.base.index('add_library'):].replace('tiny.cpp','../tiny.cpp'))
        self.configure('cmake_minimum_required(VERSION 3.24)\nproject(InertMapControl LANGUAGES CXX)\nadd_subdirectory(sub)\n')
    def test_subdirectory_flags_conflict(self):
        sub=self.source/'sub';sub.mkdir();(sub/'CMakeLists.txt').write_text('set(CMAKE_MODULE_LINKER_FLAGS "-Wl,-Map,hidden.map")\nadd_library(child MODULE ../tiny.cpp)\n')
        self.configure(self.base+'add_subdirectory(sub)\n',ok=False)
    def test_custom_config_conflict(self):self.configure(self.base+'set_property(TARGET CS80Reference_VST3 PROPERTY LINK_FLAGS_RESEARCH "-Wl,-Map,hidden.map")\n',extra=['-DCMAKE_BUILD_TYPE=Research'],ok=False)
    def test_dependency_flags_not_map(self):self.configure(self.base+'set(CMAKE_CXX_FLAGS "-MD -MMD")\n')
    def test_transitive_conflict(self):self.configure(self.base+'add_library(flags INTERFACE)\ntarget_link_options(flags INTERFACE "LINKER:-Map,other.map")\ntarget_link_libraries(CS80Reference_VST3 PRIVATE flags)\n',ok=False)

CONFLICTS={
 'attached_target':'target_link_options(CS80Reference_VST3 PRIVATE "-Wl,-Mapexisting.map")',
 'attached_global':'set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--Mapexisting.map")',
 'attached_compiler':'set(CMAKE_CXX_FLAGS "-Wl,-Mapexisting.map")',
 'target':'target_link_options(CS80Reference_VST3 PRIVATE "-Wl,-Map,other.map")',
 'target_legacy':'set_property(TARGET CS80Reference_VST3 PROPERTY LINK_FLAGS "-Wl,--Map=other.map")',
 'target_release':'set_property(TARGET CS80Reference_VST3 PROPERTY LINK_FLAGS_RELEASE "-Wl,-Map=other.map")',
 'directory':'add_link_options("LINKER:--Map,other.map")',
 'global':'set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Map,other.map")',
 'compiler':'set(CMAKE_CXX_FLAGS "-Wl,-Map,other.map")',
 'print':'target_link_options(CS80Reference_VST3 PRIVATE "-Wl,-M")',
 'cref':'target_link_options(CS80Reference_Standalone PRIVATE "-Wl,--cref")',
 'link_libraries':'target_link_libraries(CS80Reference_VST3 PRIVATE "-Wl,-Map,other.map")',
 'genex':'target_link_options(CS80Reference_VST3 PRIVATE "$<$<CONFIG:Release>:-Wl,-Map,other.map>")',
}
for name,code in CONFLICTS.items():
    setattr(LinkOverlayTests,'test_conflict_'+name,lambda self,c=code:(self.configure(self.base+c+'\n',ok=False),None)[1])
if __name__=='__main__':unittest.main()
