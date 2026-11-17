"""Prospective sequential full-roster CTest schedule; never executes commands.

Only the frozen dependency-free Linux roster is supported. Every original test
remains mandatory, in configured order and with original CTest properties. The
orchestrator must bind source/runtime/env/definitions and archive each overwritten
LastTest.log/JUnit before advancing. This does not establish those actions.
"""
from pathlib import Path
import re
from identity import require
from roster import roster

SUPPORTED={'WORKING_DIRECTORY','TIMEOUT','LABELS','SKIP_RETURN_CODE','PASS_REGULAR_EXPRESSION'}


def schedule(*, document, build, output, ctest, arm, environment, definition_texts):
    require(arm in ('stock','candidate'),'explicit arm required')
    build,output,ctest=map(Path,(build,output,ctest))
    require(all(p.is_absolute() and '..' not in p.parts for p in (build,output,ctest)), 'canonical absolute paths required')
    require(build!=output and not build.is_relative_to(output) and not output.is_relative_to(build), 'separate evidence/build roots required')
    require(isinstance(environment,dict) and not any(k.startswith('CTEST_') for k in environment), 'inherited CTEST configuration forbidden')
    require(isinstance(definition_texts,dict) and definition_texts,'frozen raw definitions required')
    for text in definition_texts.values():
        for line in text.splitlines():
            if not line.strip() or line.startswith('#'):continue
            require(line.startswith(('add_test(','set_tests_properties(','subdirs(')) and line.endswith(')'),
                    'unsupported CTest include/control/hook syntax')
    tests=roster(document);result=[]
    for index,(name,test) in enumerate(tests.items()):
        props=test['properties']
        require(set(props)<=SUPPORTED, 'unsupported per-test semantics: '+name)
        require(props.get('WORKING_DIRECTORY')==str(build), 'original common test cwd required')
        if 'SKIP_RETURN_CODE' in props:
            require(name=='cs80_filter_batch_cap_tests' and props['SKIP_RETURN_CODE']==77, 'unexpected skip semantics')
        if 'PASS_REGULAR_EXPRESSION' in props:
            require(name=='cs80_teardown_probe' and props['PASS_REGULAR_EXPRESSION']==['JUCE shut down',' static destructors run'],
                    'unexpected regex-pass semantics')
        result.append({'name':name,'label':f'{arm}-test-{index:04d}',
            'argv':[str(ctest),'--test-dir',str(build),'--output-on-failure','--no-tests=error','-T','Test',
                    '-R','^'+re.escape(name)+'$','--output-junit',str(output/f'{index:04d}.xml')],
            'original_test':test,'same_attempt_dashboard_exit_required':True})
    return {'cases':result,'full_test_count':len(result),'custom_hooks_absence_required':True,
            'per_case_show_only_identity_required':True,'per_case_log_archive_required':True,'per_case_dashboard_archive_required':True,
            'runtime_environment_identity_required':True,'production_qualified':False}


def audit_runtime(*, build, ctest, expected_ctest_sha256, definition_hashes, environment):
    """Verify frozen bytes and absence of dashboard/custom controls before splitting.

    Call again before/after each process; no concurrent writers are supported.
    Caller must freeze the whole environment and CTest version, not just this hash.
    """
    from identity import capture_files,hashes
    root=Path(build).absolute();tool=Path(ctest).absolute()
    require(isinstance(environment,dict) and not any(k.startswith('CTEST_') for k in environment),'inherited CTEST configuration forbidden')
    hashes(definition_hashes)
    require(capture_files(root,list(definition_hashes))==definition_hashes,'raw CTest definitions changed')
    forbidden={'CTestCustom.cmake','CTestCustom.ctest','CTestConfiguration.ini','DartConfiguration.tcl','CTestConfig.cmake'}
    require(not any(p.name in forbidden for p in root.rglob('*')), 'unexpected dashboard/custom hook configuration')
    require(capture_files(tool.parent,[tool.name])[tool.name]==expected_ctest_sha256, 'CTest runtime bytes changed')
    return {'runtime_and_definition_bytes_bound':True,'custom_hooks_absent':True,
            'whole_environment_identity_required':True,'production_qualified':False}
