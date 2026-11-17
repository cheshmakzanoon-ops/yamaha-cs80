#!/usr/bin/env python3
"""Bounded unrelated integer-only GNU13.3/CMake/Ninja filename fixture.

One instrumented integer execution, no synthesizer source, audio or timing study.
Not run locally on GCC14; intended reviewed new workflow prerequisite only.
"""
import argparse
import json
import os
import re
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import time
from profile_identity_v3 import require, sha, make_contract, complete_training_attempt, live_profile_manifest

CMAKE = '''cmake_minimum_required(VERSION 3.22)
project(IntegerOnlyPathFixture LANGUAGES CXX)
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "13.3.0")
 message(FATAL_ERROR "Actual GNU13.3 required")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library(cs80_core STATIC src/core/Engine.cpp)
add_executable(integer_toy main.cpp)
target_link_libraries(integer_toy PRIVATE cs80_core)
if(MODE STREQUAL "generate")
 set_source_files_properties(src/core/Engine.cpp PROPERTIES COMPILE_OPTIONS "-fprofile-generate=${PROFILE_ROOT}")
 target_link_options(integer_toy PRIVATE "-fprofile-generate=${PROFILE_ROOT}")
elseif(MODE STREQUAL "use")
 set_source_files_properties(src/core/Engine.cpp PROPERTIES COMPILE_OPTIONS "-fprofile-use=${PROFILE_ROOT};-Werror=missing-profile;-Werror=coverage-mismatch")
else()
 message(FATAL_ERROR "Explicit mode required")
endif()
'''
ORIGINAL = 'int fixture_value(int x) { return x == 7 ? 4 : 2; }\n'
MISMATCH = 'int fixture_value(int x) { int s=0; for(int i=0;i<x;++i) { if(i%3==0) s+=i; } return s; }\n'


def require_compiler_error(text, warning):
    require(warning in ('missing-profile', 'coverage-mismatch'), 'unexpected diagnostic control')
    pattern = r'(?m)^.*\berror:\s+[^\n]*\[-Werror=' + re.escape(warning) + r'\]\s*$'
    require(re.search(pattern, text) is not None, 'specific compiler error diagnostic missing: ' + warning)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--compiler-identity',type=Path,required=True)
    args=parser.parse_args()
    work,out=args.work.resolve(),args.out.resolve();work.mkdir(parents=True,exist_ok=False);out.mkdir(parents=True,exist_ok=False)
    deadline=time.monotonic()+300;ledger=[]
    def command(label,argv,*,cwd,success=True):
        require(not any(x['label']==label for x in ledger),'fixture label retry forbidden')
        remaining=deadline-time.monotonic();require(remaining>0,'fixture deadline')
        argv=list(map(str,argv));record={'label':label,'argv':argv,'cwd':str(cwd),'expected_success':success,'exit':None};ledger.append(record)
        (out/'commands.json').write_text(json.dumps(ledger,indent=2)+'\n')
        child=None
        try:
            with (out/(label+'.stdout')).open('wb') as stdout,(out/(label+'.stderr')).open('wb') as stderr:
                child=subprocess.Popen(argv,cwd=cwd,stdout=stdout,stderr=stderr,start_new_session=True)
                try:record['exit']=child.wait(timeout=remaining)
                except BaseException:
                    try:os.killpg(child.pid,signal.SIGKILL)
                    except ProcessLookupError:pass
                    child.wait();raise
            require((record['exit']==0) if success else (record['exit']!=0),'unexpected fixture command result')
            require(time.monotonic()<=deadline,'fixture deadline')
            return (out/(label+'.stdout')).read_text()+'\n'+(out/(label+'.stderr')).read_text()
        finally:(out/'commands.json').write_text(json.dumps(ledger,indent=2)+'\n')
    source=work/'source';build=work/'build';profile=work/'profiles';profile.mkdir()
    (source/'src/core').mkdir(parents=True)
    engine=source/'src/core/Engine.cpp';engine.write_text(ORIGINAL)
    (source/'main.cpp').write_text('int fixture_value(int); int main() { return fixture_value(7)==4 ? 0 : 1; }\n')
    (source/'CMakeLists.txt').write_text(CMAKE)
    shutil.copytree(source,out/'source-initial')
    (out/'Engine-mismatch.cpp').write_text(MISMATCH)
    decision={'decision':'FIXTURE_STOP','scope':'Unrelated integer compiler plumbing; no synth/audio/cost'}
    try:
        def configure(label,mode,profiles):
            command(label,['cmake','-S',source,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
                           '-DMODE='+mode,'-DPROFILE_ROOT='+str(profiles)],cwd=work)
            shutil.copy2(build/'compile_commands.json',out/(label+'-compile_commands.json'))
            shutil.copy2(build/'CMakeCache.txt',out/(label+'-CMakeCache.txt'))
        configure('configure-generate','generate',profile)
        cache=(build/'CMakeCache.txt').read_text()
        (out/'CMakeCache.txt').write_text(cache)
        driver=next(line.split('=',1)[1] for line in cache.splitlines() if line.startswith('CMAKE_CXX_COMPILER:FILEPATH='))
        expected=json.loads(args.compiler_identity.read_text())['CXX']
        actual={'command':driver,'resolved':str(Path(driver).resolve()),'sha256':sha(Path(driver).resolve().read_bytes()),
                'version':command('compiler-version',[driver,'-dumpfullversion'],cwd=work).strip()}
        frontend=command('compiler-frontend',[driver,'-print-prog-name=cc1plus'],cwd=work).strip()
        actual.update(frontend=str(Path(frontend).resolve()),frontend_sha256=sha(Path(frontend).read_bytes()))
        (out/'compiler-identity-observed.json').write_text(json.dumps(actual,indent=2)+'\n')
        (out/'compiler-identity-expected.json').write_text(json.dumps(expected,indent=2)+'\n')
        require(actual==expected,'fixture compiler differs from frozen actual GNU13.3')
        command('build-generate',['cmake','--build',build,'--verbose'],cwd=work)
        database=json.loads((build/'compile_commands.json').read_text())
        (out/'generate-compile_commands.json').write_text(json.dumps(database,indent=2)+'\n')
        row=next(x for x in database if x['file']==str(engine))
        argv=row.get('arguments') or shlex.split(row['command'])
        require(argv[argv.index('-o')+1]=='CMakeFiles/cs80_core.dir/src/core/Engine.cpp.o','real CMake relative output required')
        obj=build/'CMakeFiles/cs80_core.dir/src/core/Engine.cpp.o'
        shutil.copy2(obj,out/'relative-generation-Engine.cpp.o')
        contract=make_contract(argv,row['directory'],str(engine),str(obj),str(profile),obj.read_bytes(),sha(obj.read_bytes()))
        (out/'relative-contract.json').write_text(json.dumps(contract,indent=2)+'\n')
        frozen=complete_training_attempt(profile,out/'relative-profile-evidence',contract,
             lambda:command('integer-generate-execution-once',[build/'integer_toy'],cwd=build),lambda:None)
        configure('configure-use','use',profile);obj.unlink()
        command('build-use',['cmake','--build',build,'--verbose'],cwd=work)
        symbols=command('use-symbols',['nm',obj],cwd=work);require('__gcov' not in symbols,'use object has generation runtime')
        shutil.copy2(obj,out/'relative-use-Engine.cpp.o')
        require(live_profile_manifest(profile,contract)==frozen,'profile mutated on use')
        missing=work/'missing';missing.mkdir();configure('configure-missing','use',missing);obj.unlink()
        text=command('missing-profile-negative',['cmake','--build',build,'--verbose','--target','cs80_core'],cwd=work,success=False)
        require_compiler_error(text,'missing-profile')
        configure('configure-mismatch','use',profile);engine.write_text(MISMATCH)
        text=command('coverage-mismatch-negative',['cmake','--build',build,'--verbose','--target','cs80_core'],cwd=work,success=False)
        require_compiler_error(text,'coverage-mismatch')
        require(live_profile_manifest(profile,contract)==frozen,'profile mutated by negative controls')
        # Absolute -o contrast: compile only, never execute/link another generator.
        engine.write_text(ORIGINAL);absolute_root=work/'absolute-profile';absolute_root.mkdir()
        absolute_obj=work/'absolute-object/Engine.cpp.o';absolute_obj.parent.mkdir()
        absolute_argv=[driver,'-std=gnu++20','-O3','-DNDEBUG','-fPIC','-fprofile-generate='+str(absolute_root),
                       '-c',str(engine),'-o',str(absolute_obj)]
        command('absolute-output-object-only',absolute_argv,cwd=build)
        shutil.copy2(absolute_obj,out/'absolute-generation-Engine.cpp.o')
        absolute_contract=make_contract(absolute_argv,str(build),str(engine),str(absolute_obj),str(absolute_root),
                                        absolute_obj.read_bytes(),sha(absolute_obj.read_bytes()))
        require(not list(absolute_root.iterdir()),'absolute contrast unexpectedly executed')
        (out/'absolute-contract.json').write_text(json.dumps(absolute_contract,indent=2)+'\n')
        require(time.monotonic()<=deadline,'fixture final deadline')
        decision.update(decision='FIXTURE_PASS',instrumented_integer_executions=1,
                        relative_path=contract['expected_gcda'],absolute_path=absolute_contract['expected_gcda'],
                        missing_profile_rejected=True,coverage_mismatch_rejected=True)
    except BaseException as error:decision['error']=repr(error)
    finally:(out/'decision.json').write_text(json.dumps(decision,indent=2)+'\n')
    return 0 if decision['decision']=='FIXTURE_PASS' else 1

if __name__=='__main__':raise SystemExit(main())
