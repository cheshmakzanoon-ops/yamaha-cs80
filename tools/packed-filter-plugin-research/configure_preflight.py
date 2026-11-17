#!/usr/bin/env python3
"""Current configuration-only capture. Never build/run/train/time the synth.

CMake compiler checks and JUCE's configure-time helper are setup, not synth
qualification. No candidate patch is applied and no executable is assessed.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shutil
from identity import require, capture_files
from compile_inventory import compile_inventory
from roster import cmake_false

ROOT=Path(__file__).resolve().parent
EXECUTION=ROOT.parent/'production-pic-pgo-research/execution.py'
SPEC=importlib.util.spec_from_file_location('reviewed_execution',EXECUTION)
MODULE=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(MODULE)

def save(path,value):Path(path).write_text(json.dumps(value,indent=2)+'\n')
def digest(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def configure_argv(repo,build):
    # Same explicit normal Linux configure options as the standard workflow.
    return ['cmake','-S',str(repo),'-B',str(build),'-G','Ninja',
            '-DCMAKE_BUILD_TYPE=Release','-DCS80_BUILD_PLUGIN=ON','-DCS80_BUILD_TESTS=ON']

def provisional_tests(document):
    tests=document.get('tests',[]);require(isinstance(tests,list) and bool(tests),'missing configured tests')
    names=set();unresolved=[]
    for row in tests:
        name=row.get('name');require(isinstance(name,str) and name and name not in names,'duplicate/invalid test')
        names.add(name);props={}
        for prop in row.get('properties',[]):
            key=prop.get('name');require(key and key not in props and 'value' in prop,'duplicate/invalid property')
            props[key]=prop['value']
        require('DISABLED' not in props or cmake_false(props['DISABLED']),'disabled configured test')
        command=row.get('command')
        if not command:unresolved.append(name)
        else:require(isinstance(command,list) and all(isinstance(v,str) and v for v in command),'invalid configured command')
    return {'count':len(names),'unresolved_commands':sorted(unresolved),'qualification':False}

def fresh_path(value):
    path=Path(value).absolute()
    require('..' not in path.parts,'noncanonical work/output path')
    for p in (path,*path.parents):require(not p.is_symlink(),'symlink work/output path')
    require(not path.exists(),'fresh work/output required')
    return path

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--repo',type=Path,required=True)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--expected-sha',required=True)
    args=parser.parse_args()
    require(re.fullmatch('[0-9a-f]{40}',args.expected_sha),'exact expected source SHA required')
    repo=args.repo.absolute();work=fresh_path(args.work);out=fresh_path(args.out)
    require(not work.is_relative_to(repo) and not out.is_relative_to(repo)
            and not work.is_relative_to(out) and not out.is_relative_to(work),'separate external owned roots required')
    work.mkdir(parents=True);out.mkdir(parents=True)
    commands=MODULE.Commands(out/'commands',total_seconds=2400,progress_seconds=1200)
    terminal={'decision':'PREFLIGHT_BLOCKED','project_built':False,'project_executed':False,'production_qualified':False}
    def run(label,argv):
        stdout,_=commands.run(label,argv,cwd=repo)
        return stdout.read_text()
    try:
        require(platform.system()=='Linux' and platform.machine()=='x86_64','Linux x86-64 setup only')
        require(run('head',['git','rev-parse','HEAD']).strip()==args.expected_sha,'source revision mismatch')
        require(not run('clean-before',['git','status','--porcelain','--untracked-files=all']).strip(),'checkout must be clean')
        files=run('tracked-files',['git','ls-files','-z']).split('\0');files=[f for f in files if f]
        source=capture_files(repo,files);save(out/'source-file-sha256.json',source)
        save(out/'metadata.json',{'revision':args.expected_sha,'platform':platform.platform(),
            'runner_image':os.environ.get('ImageVersion'),'run_id':os.environ.get('GITHUB_RUN_ID'),
            'run_attempt':os.environ.get('GITHUB_RUN_ATTEMPT'),'execution_helper_sha256':digest(EXECUTION),
            'scope':'configuration only; not plugin build, tests, speed or sound qualification'})
        build=work/'build'
        run('configure',configure_argv(repo,build))
        run('juce-provenance',['cmake','-DBUILD_DIR='+str(build),
            '-DEXPECTED_JUCE_SHA=c352e2489000ffeb9e397051377d75eef2efe641','-DEXPECTED_JUCE_VERSION=8.0.11',
            '-P',repo/'scripts/verify-ci-juce-source.cmake'])
        database=json.loads((build/'compile_commands.json').read_text())
        inventory=compile_inventory(database,source_root=repo,build_root=build)
        save(out/'compile_commands.json',database);save(out/'compile-inventory.json',inventory)
        shutil.copy2(build/'CMakeCache.txt',out/'CMakeCache.txt')
        cache={}
        for line in (build/'CMakeCache.txt').read_text().splitlines():
            if line and not line.startswith(('#','//')) and '=' in line and ':' in line.split('=',1)[0]:
                key,value=line.split('=',1);cache[key.split(':',1)[0]]=value
        compilers={}
        for lang,frontend_name in [('C','cc1'),('CXX','cc1plus')]:
            driver=cache['CMAKE_'+lang+'_COMPILER'];version=run(lang+'-version',[driver,'-dumpfullversion']).strip()
            require(version=='13.3.0','GNU13.3 baseline required; no implicit compiler change')
            frontend=Path(run(lang+'-frontend',[driver,'-print-prog-name='+frontend_name]).strip())
            require(frontend.is_absolute() and frontend.is_file(),'absolute compiler frontend required')
            compilers[lang]={'driver':driver,'driver_sha256':digest(Path(driver).resolve()),
                             'version':version,'frontend':str(frontend),'frontend_sha256':digest(frontend)}
        save(out/'compiler-identity.json',compilers)
        tests=json.loads(run('ctest-inventory',['ctest','--test-dir',build,'--show-only=json-v1']))
        save(out/'configured-tests.json',tests);save(out/'provisional-test-summary.json',provisional_tests(tests))
        definitions={}
        for path in sorted(build.rglob('CTestTestfile.cmake')):
            require(not path.is_symlink(),'symlink CTest definition')
            definitions[str(path.relative_to(build))]={'sha256':digest(path),'text':path.read_text()}
        require(bool(definitions),'missing CTest definition files');save(out/'CTest-definitions.json',definitions)
        for target in ('cs80_core','CS80Reference_VST3','CS80Reference_Standalone'):
            run('planned-'+target,['ninja','-C',build,'-t','commands',target])
        dependencies={}
        for name in ('juce','cs80_libxml2'):
            root=build/'_deps'/(name+'-src')
            if name=='juce' and (repo/'deps/juce/CMakeLists.txt').is_file():root=repo/'deps/juce'
            require(root.is_dir(),'missing configured dependency: '+name)
            paths=[str(p.relative_to(root)) for p in sorted(root.rglob('*')) if '.git' not in p.relative_to(root).parts and p.is_file()]
            dependencies[name]={'root':str(root),'files':capture_files(root,paths)}
        save(out/'dependency-files.json',dependencies)
        require(capture_files(repo,files)==source,'tracked source changed during configuration')
        require(not run('clean-after',['git','status','--porcelain','--untracked-files=all']).strip(),'configuration modified checkout')
        terminal.update(decision='CONFIGURATION_CAPTURED_NOT_QUALIFIED',compile_entries=len(inventory),
                        configured_tests=len(tests['tests']))
    except BaseException as error:
        terminal['error']=repr(error)
        raise
    finally:
        save(out/'terminal.json',terminal)
        manifest={str(p.relative_to(out)):digest(p) for p in sorted(out.rglob('*')) if p.is_file()}
        save(out/'manifest.json',manifest)
if __name__=='__main__':main()
