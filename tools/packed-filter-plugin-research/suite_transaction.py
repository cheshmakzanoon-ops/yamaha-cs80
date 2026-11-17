"""Original-suite transaction: actual CTest orchestration, no CLI or workflow.

Only inert fixtures exercise this coordinator during preparation. A real run
requires the frozen original 569-case contract and source-bound native roster,
reviewed build/seal/load gates and one global Commands ledger.
"""
import hashlib,json,math,re,shutil,stat,os
from pathlib import Path
from identity import require,capture_files
from roster import roster,same_roster,complete_junit,admit_built_tests
from split_ctest import schedule,audit_runtime
from dashboard_exit import require_exit_zero
from capture_roster import bind_contract,frozen,capture_case,compare_case
from capture_seal import seal_case,clear_case,directory_for


def environment_digest(environment):
    require(isinstance(environment,dict) and all(isinstance(k,str) and k and '=' not in k and '\0' not in k and isinstance(v,str) and '\0' not in v for k,v in environment.items()),'invalid frozen environment')
    return hashlib.sha256(json.dumps(environment,sort_keys=True,separators=(',',':'),ensure_ascii=True).encode()).hexdigest()


def owned(path):
    path=Path(path).absolute();require('..' not in path.parts and not any(p.is_symlink() for p in (path,*path.parents)),'canonical nonsymlink path required');return path


def bounded_entries(root,limit=4096):
    """Incremental diagnostic traversal, counting directories as well as files."""
    pending=[root];count=0
    while pending:
        with os.scandir(pending.pop()) as entries:
            for entry in entries:
                count+=1;require(count<=limit,'failure evidence total-entry bound')
                path=Path(entry.path)
                if entry.is_dir(follow_symlinks=False):pending.append(path)
                yield path


def definition_syntax(build,definition_hashes):
    """Observed generated literal CMake subset, never arbitrary CMake execution."""
    from definition_commands import commands_from_definitions
    texts={name:(build/name).read_text() for name in definition_hashes}
    commands_from_definitions(texts)
    for name,text in texts.items():
        for line in text.splitlines():
            if not line.strip() or line.startswith('#') or line.startswith('add_test('):continue
            if re.fullmatch(r'set_tests_properties\(\[=\[[^\]\r\n]+\]=\] PROPERTIES(?: +[A-Za-z_][A-Za-z_0-9]* "[^"\\$\r\n]*")+\)',line):continue
            match=re.fullmatch(r'subdirs\("([^"\\;$\r\n]+)"\)',line)
            require(match is not None,'unsupported generated definition command')
            child=Path(name).parent/match[1]/'CTestTestfile.cmake'
            require(not Path(match[1]).is_absolute() and '..' not in child.parts and str(child) in definition_hashes,'subdirectory definition outside frozen inventory')
    return texts


def _execute_suite(*, arm, build, ctest, document, definition_hashes, executable_hashes,
                   runtime_hashes, module_root, module_hashes, module_directories,
                   environment, output, commands, case_timeouts, capture_handlers=None,source_root=None,source_hashes=None):
    """Internal transaction shared by the full entry and inert CTest controls.

    The full entry must independently bind all original sources/definitions and
    native cases. There is no partial-suite production admission through this API.
    """
    result={'arm':arm,'cases':[],'complete':False,'production_qualified':False}
    output=Path(output);output_owned=False
    try:
        require(arm in ('stock','candidate'),'explicit arm required')
        require(not commands.terminal_failure,'terminal transaction cannot restart')
        build,ctest,module_root,output=map(owned,(build,ctest,module_root,output))
        require(not output.exists() and not output.is_relative_to(build) and not build.is_relative_to(output),'fresh independent suite output required')
        output.mkdir(parents=True);output_owned=True;env_hash=environment_digest(environment)
        expected=roster(document);require(set(case_timeouts)==set(expected),'complete outer timeout roster required')
        for name,t in case_timeouts.items():
            require(isinstance(t,(int,float)) and not isinstance(t,bool) and math.isfinite(t) and t>0,'finite outer timeout required')
            inner=expected[name]['properties'].get('TIMEOUT',1500)
            require(t>=inner,'outer timeout may not shorten original CTest limit')
        require(str(ctest) in runtime_hashes,'CTest bytes must be frozen with runtime')
        require(capture_files(build,list(definition_hashes))==definition_hashes,'initial definition identity differs')
        require(all(runtime_hashes[k]==executable_hashes[k] for k in set(runtime_hashes)&set(executable_hashes)),'conflicting runtime/executable seals')
        texts=definition_syntax(build,definition_hashes)
        plan=schedule(document=document,build=build,output=output/'junit',ctest=ctest,arm=arm,environment=environment,definition_texts=texts)
        (output/'junit').mkdir();(output/'cases').mkdir()
        require(not (build/'Testing/TAG').exists(),'preexisting dashboard tag forbidden')
        require(not list(build.glob('Testing/**/Test.xml')) and not list(build.glob('Testing/Temporary/LastTest*.log')),'preexisting dashboard/log evidence forbidden')
        previous=[]
        def persist():
            (output/'transaction.json').write_text(json.dumps(result,indent=2)+'\n')
        def audit():
            commands.remaining()
            if source_root is not None:require(capture_files(owned(source_root),list(source_hashes))==source_hashes,'source/script changed')
            require({str(p.relative_to(build)) for p in build.rglob('CTestTestfile.cmake')}==set(definition_hashes),'complete definition inventory drift')
            audit_runtime(build=build,ctest=ctest,expected_ctest_sha256=runtime_hashes[str(ctest)],definition_hashes=definition_hashes,environment=environment)
            for path,wanted in {**runtime_hashes,**executable_hashes}.items():
                p=owned(path);require(capture_files(p.parent,[p.name])[p.name]==wanted,'runtime/executable changed')
            require(module_root.is_dir(),'bundle root must be directory')
            paths=list(module_root.rglob('*'));require(all(stat.S_ISREG(p.lstat().st_mode) or stat.S_ISDIR(p.lstat().st_mode) for p in paths),'bundle special node forbidden');require(not any(p.is_symlink() for p in paths),'bundle symlink')
            require({str(p.relative_to(module_root)) for p in paths if p.is_file()}==set(module_hashes),'bundle file inventory drift')
            require({str(p.relative_to(module_root)) for p in paths if p.is_dir()}==set(module_directories),'bundle directory inventory drift')
            require(capture_files(module_root,list(module_hashes))==module_hashes,'sealed module changed')
            commands.remaining()
        def run(label,argv,timeout):
            audit();stdout,stderr=commands.run(label,argv,cwd=build,timeout=timeout,env=environment,replace_env=True,require_group_empty=True)
            r=commands.ledger[-1]
            require(r.get('label')==label and r.get('argv')==argv and r.get('executed_argv')==argv and r.get('cwd')==str(build) and r.get('exit')==0 and not r.get('error') and r.get('owned_process_group_empty') is True and r.get('replace_environment') is True and r.get('environment_sha256')==env_hash,'actual command/environment ledger mismatch')
            audit();return Path(stdout),Path(stderr),dict(r)
        def retain(path,destination):
            commands.remaining()
            path=owned(path);require(path.is_file() and not destination.exists(),'fresh evidence file required')
            before=capture_files(path.parent,[path.name])[path.name];shutil.copy2(path,destination)
            require(capture_files(destination.parent,[destination.name])[destination.name]==capture_files(path.parent,[path.name])[path.name]==before,'evidence changed during retention')
            return before
        def retire():
            commands.remaining()
            for source,copy,wanted in previous:
                require(capture_files(source.parent,[source.name])[source.name]==capture_files(copy.parent,[copy.name])[copy.name]==wanted,'retained equivalent evidence required before retirement')
                source.unlink()
            previous.clear()
        persist()
        for index,case in enumerate(plan['cases']):
            retire();require(not (build/'Testing/TAG').exists() and not list(build.glob('Testing/**/Test.xml')) and not list(build.glob('Testing/Temporary/LastTest*.log')),'stale per-case dashboard evidence');name=case['name'];case_dir=output/'cases'/f'{index:04d}';case_dir.mkdir()
            before_capture=None
            if capture_handlers:before_capture=capture_handlers['before'](name)
            selection=[str(ctest),'--test-dir',str(build),'--show-only=json-v1','--no-tests=error','-R','^'+re.escape(name)+'$']
            stdout,stderr,selection_record=run(case['label']+'-select',selection,60)
            selected=json.loads(stdout.read_bytes());single={'tests':[next(t for t in document['tests'] if t['name']==name)]}
            same_roster(single,selected)
            selection_logs={key:retain(path,case_dir/('selection.'+key)) for key,path in [('stdout',stdout),('stderr',stderr)]}
            # show-only itself writes LastTest.log. Retain and retire it before
            # the real test, rather than accepting an old log as fresh evidence.
            selection_log=build/'Testing/Temporary/LastTest.log'
            require(selection_log.is_file(),'missing selection log')
            digest=retain(selection_log,case_dir/'selection.LastTest.log');previous.append((selection_log,case_dir/'selection.LastTest.log',digest));retire()
            junit=Path(case['argv'][-1]);require(not junit.exists(),'stale JUnit output')
            stdout,stderr,record=run(case['label'],case['argv'],case_timeouts[name])
            complete_junit(junit.read_text(),single)
            tag_path=build/'Testing/TAG';tag_text=tag_path.read_text();tag=tag_text.splitlines()[0]
            require(re.fullmatch(r'[0-9]{8}-[0-9]{4}',tag) is not None,'unexpected dashboard tag')
            dashboard=build/'Testing'/tag/'Test.xml';last=build/'Testing/Temporary'/('LastTest_'+tag+'.log')
            exit_receipt=require_exit_zero(dashboard.read_text(),name=name,expected_argv=expected[name]['command'])
            files={'junit.xml':junit,'Test.xml':dashboard,'LastTest.log':last,'TAG':tag_path,'stdout':stdout,'stderr':stderr}
            cost=build/'Testing/Temporary/CTestCostData.txt'
            if cost.exists():files['CTestCostData.txt']=cost
            hashes={key:retain(path,case_dir/key) for key,path in files.items()}
            for key in ('Test.xml','LastTest.log','TAG','CTestCostData.txt'):
                if key in files:previous.append((files[key],case_dir/key,hashes[key]))
            commands.remaining()
            after_capture=capture_handlers['after'](name,before_capture) if capture_handlers else None
            commands.remaining()
            result['cases'].append({'name':name,'selection':selection_record,'selection_logs':selection_logs,'command':record,'files_sha256':hashes,'exit':exit_receipt,'capture':after_capture})
            persist()
        retire();audit();require([x['name'] for x in result['cases']]==list(expected),'full original order incomplete')
        result['complete']=True;result['complete_test_count']=len(expected);persist();return result
    except BaseException as error:
        commands.terminal_failure=True;commands.ledger.append({'label':str(arm)+'-suite-transaction-failure','error':repr(error),'terminal_failure':True});commands.persist()
        result['error']=repr(error)
        if output_owned and output.is_dir() and not output.is_symlink():
            # On failure preserve generated same-attempt diagnostics as evidence,
            # never reinterpret them as admission or remove originals.
            try:
                failure=output/'failure-evidence';failure.mkdir()
                testing=owned(build/'Testing')
                retained={};result["retained_failure_testing"]=retained;total=0;count=0
                if testing.exists():
                    for source in bounded_entries(testing):
                        require(not source.is_symlink(),'failure evidence symlink')
                        if source.is_dir():continue
                        size=source.stat().st_size;count+=1;total+=size
                        require(source.is_file() and size<=64*1024*1024 and count<=4096 and total<=256*1024*1024,'failure evidence type/aggregate-size/count bound')
                        relative=source.relative_to(testing);target=failure/'Testing'/relative;target.parent.mkdir(parents=True,exist_ok=True)
                        wanted=capture_files(source.parent,[source.name])[source.name];shutil.copy2(source,target)
                        require(capture_files(target.parent,[target.name])[target.name]==capture_files(source.parent,[source.name])[source.name]==wanted,'failure evidence changed')
                        retained[str(relative)]=wanted
                result['retained_failure_testing']=retained
            except BaseException as capture_error:
                result['failure_capture_error']=repr(capture_error)
                result['failure_evidence_incomplete']=True
            (output/'transaction-failure.json').write_text(json.dumps(result,indent=2)+'\n')
        raise


def run_original_suite(contract,commands):
    """Full fixed original cohort, never a subset. No execution on import.

    Build/link admission, complete expected inventories and canonical sealed-arm
    restoration must already have succeeded in the parent Stage A transaction.
    This entry executes all original cases once; it does not authorize a run.
    """
    import copy
    arm=None;out=None;owned_out=False
    try:
        c=copy.deepcopy(contract);arm=c.get('arm');out=Path(c['output'])
        require(arm in ('stock','candidate'),'explicit original-suite arm required')
        require(not commands.terminal_failure,'terminal ledger');out=owned(out)
        require(not out.exists(),'fresh arm suite root')
        build=owned(c['build']);ctest=owned(c['ctest']);repo=owned(c['repo'])
        sealed=owned(c['sealed_captures'])
        roots=[build,out,sealed]
        if arm=='candidate':roots.append(owned(c['stock_sealed_captures']))
        require(all(a!=b and not a.is_relative_to(b) and not b.is_relative_to(a) for i,a in enumerate(roots) for b in roots[i+1:]),'suite build/output/capture roots must be disjoint')
        out.mkdir(parents=True);owned_out=True
        env=c['environment'];digest=environment_digest(env)
        require('PATH' in env and all(Path(p).is_absolute() and p for p in env['PATH'].split(':')),'explicit absolute PATH required')
        require(capture_files(repo,list(c['source_hashes']))==c['source_hashes'],'frozen source identity differs')
        require(not (build/'Testing/TAG').exists() and not list(build.glob('Testing/**/Test.xml')) and not list(build.glob('Testing/Temporary/LastTest*.log')),'preexisting inventory/dashboard evidence')
        tool_hash=c['runtime_hashes'][str(ctest)]
        require(capture_files(ctest.parent,[ctest.name])[ctest.name]==tool_hash,'CTest bytes differ')
        def inventory_audit():
            commands.remaining()
            require(capture_files(repo,list(c['source_hashes']))==c['source_hashes'],'frozen source/script identity differs')
            for path,wanted in {**c['runtime_hashes'],**c['executable_hashes']}.items():
                p=owned(path);require(capture_files(p.parent,[p.name])[p.name]==wanted,'runtime/executable changed')
            module=owned(c['module_root']);require(module.is_dir(),'bundle root must be directory')
            entries=list(module.rglob('*'))
            require(all(stat.S_ISREG(p.lstat().st_mode) or stat.S_ISDIR(p.lstat().st_mode) for p in entries),'bundle special node/symlink forbidden')
            require({str(p.relative_to(module)) for p in entries if p.is_file()}==set(c['module_hashes']) and {str(p.relative_to(module)) for p in entries if p.is_dir()}==set(c['module_directories']),'bundle inventory drift')
            require(capture_files(module,list(c['module_hashes']))==c['module_hashes'],'sealed module changed')
            require({str(p.relative_to(build)) for p in build.rglob('CTestTestfile.cmake')}==set(c['definition_hashes']),'complete definition inventory drift')
            audit_runtime(build=build,ctest=ctest,expected_ctest_sha256=tool_hash,definition_hashes=c['definition_hashes'],environment=env)
            definition_syntax(build,c['definition_hashes'])
        def invoke(label,argv):
            inventory_audit()
            stdout,stderr=commands.run(label,argv,cwd=build,timeout=60,env=env,replace_env=True,require_group_empty=True)
            record=commands.ledger[-1]
            require(record['argv']==argv and record['executed_argv']==argv and record['exit']==0 and record['cwd']==str(build) and record['environment_sha256']==digest and record['replace_environment'] is True and record.get('owned_process_group_empty') is True,'inventory command binding')
            require(capture_files(ctest.parent,[ctest.name])[ctest.name]==tool_hash,'CTest bytes changed')
            inventory_audit()
            return Path(stdout),Path(stderr),record
        version,_,_=invoke(arm+'-ctest-version',[str(ctest),'--version'])
        require(version.read_text().splitlines()[0]=='ctest version 3.31.6','unreviewed dashboard producer')
        stdout,stderr,record=invoke(arm+'-full-inventory',[str(ctest),'--test-dir',str(build),'--show-only=json-v1'])
        observed=json.loads(stdout.read_bytes());require(len(roster(observed))==569,'complete frozen original 569-case cohort required')
        admission=admit_built_tests(configured=c['configured'],built=observed,build_root=build,definition_hashes=c['definition_hashes'],executable_hashes=c['executable_hashes'],ctest_record=record,ctest_stdout=stdout.read_bytes(),executable_resolutions=c['executable_resolutions'],ctest_executable=str(ctest))
        raw={n:r['command'] for n,r in roster(observed).items()}
        binding=bind_contract(repo=repo,raw_commands=raw,source_root=repo,build_root=build)
        modules={arg for argv in raw.values() for arg in argv if '.vst3/' in arg}
        require(modules=={c['module_path']} and owned(c['module_path']).is_relative_to(owned(c['module_root'])),'executed native module differs from sealed bundle')
        require(str(Path(c['module_path']).relative_to(c['module_root'])) in c['module_hashes'],'module absent from sealed files')
        # Inventory show-only writes a log; verify an equivalent retained copy
        # before removing it so the first real case begins with absent evidence.
        log=build/'Testing/Temporary/LastTest.log';wanted=capture_files(log.parent,[log.name])[log.name]
        retained=out/'inventory.LastTest.log';shutil.copy2(log,retained)
        require(capture_files(out,[retained.name])[retained.name]==capture_files(log.parent,[log.name])[log.name]==wanted,'inventory log retention failed');log.unlink()
        sealed=owned(c['sealed_captures']);require(not sealed.exists(),'fresh arm capture seal');sealed.mkdir(parents=True)
        native=frozen()['cases'];capture_results={}
        if arm=='candidate':require(set(c['stock_capture_results'])==set(native),'complete stock native capture receipt roster required')
        def before(name):
            commands.remaining()
            if name not in native:return None
            if native[name]['capture_directory'] is None:return None
            directory=directory_for(build,name)
            if arm=='stock':require(not directory.exists(),'stale stock capture directory')
            else:
                stock=owned(c['stock_sealed_captures']);expected=c['stock_capture_results'][name]
                require(capture_case(stock,name)==expected,'stock seal changed')
                if directory.exists():clear_case(build=build,sealed=stock,case=name,expected=expected)
            require(not directory.exists(),'canonical capture not cleared')
        def after(name,_):
            commands.remaining()
            if name not in native:return None
            result=seal_case(build=build,sealed=sealed,case=name) if native[name]['capture_directory'] is not None else capture_case(build,name)
            if arm=='candidate':compare_case(owned(c['stock_sealed_captures']),sealed,name)
            capture_results[name]=result;return result
        result=_execute_suite(arm=arm,build=build,ctest=ctest,document=observed,definition_hashes=c['definition_hashes'],executable_hashes=c['executable_hashes'],runtime_hashes=c['runtime_hashes'],module_root=c['module_root'],module_hashes=c['module_hashes'],module_directories=c['module_directories'],environment=env,output=out/'suite',commands=commands,case_timeouts=c['case_timeouts'],capture_handlers={'before':before,'after':after},source_root=repo,source_hashes=c['source_hashes'])
        require(set(capture_results)==set(native),'complete native assertion/capture roster missing')
        require(capture_files(repo,list(c['source_hashes']))==c['source_hashes'],'source changed during suite')
        commands.remaining()
        result.update({'source_roster_binding':binding,'built_inventory_admission':admission,'native_capture_results':capture_results,'environment_sha256':digest,'original_suite_admitted':True,'production_qualified':False})
        (out/'original-suite-result.json').write_text(json.dumps(result,indent=2)+'\n');return result
    except BaseException as error:
        commands.terminal_failure=True;commands.ledger.append({'label':str(arm)+'-original-suite-failure','error':repr(error),'terminal_failure':True});commands.persist()
        if owned_out:(out/'original-suite-failure.json').write_text(json.dumps({'error':repr(error),'production_qualified':False})+'\n')
        raise
