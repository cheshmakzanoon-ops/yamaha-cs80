"""Pure complete-suite evidence checks, without historical hard-coded test counts."""
import math
import xml.etree.ElementTree as ET
from identity import require

_FALSE = ('0', 'OFF', 'NO', 'FALSE', 'N', 'IGNORE', 'NOTFOUND', '')

def cmake_false(value):
    if value is False or value == 0:
        return True
    return isinstance(value, str) and (value.upper() in _FALSE or value.upper().endswith('-NOTFOUND'))

def roster(document):
    require(isinstance(document, dict) and isinstance(document.get('tests'), list)
            and bool(document['tests']), 'nonempty CTest roster required')
    result = {}
    for test in document['tests']:
        name = test.get('name')
        require(isinstance(name, str) and name and name not in result, 'missing/duplicate test name')
        command = test.get('command')
        require(isinstance(command, list) and command
                and all(isinstance(x, str) and x for x in command), 'unresolved test command')
        properties = test.get('properties', [])
        require(isinstance(properties, list), 'invalid test properties')
        indexed = {}
        for prop in properties:
            require(isinstance(prop, dict) and isinstance(prop.get('name'), str)
                    and prop['name'] and prop['name'] not in indexed and 'value' in prop,
                    'duplicate/malformed property')
            indexed[prop['name']] = prop['value']
        require('DISABLED' not in indexed or cmake_false(indexed['DISABLED']), 'disabled test forbidden')
        result[name] = {'command': command, 'properties': indexed}
    return result

def same_roster(frozen, observed):
    require(roster(frozen) == roster(observed), 'complete current test inventory/command/property drift')

def complete_junit(text, frozen):
    expected = roster(frozen)
    root = ET.fromstring(text)
    require(root.tag in ('testsuite', 'testsuites'), 'unexpected JUnit root')
    cases = list(root.iter('testcase'))
    names = [case.get('name') for case in cases]
    require(len(names) == len(expected) and len(set(names)) == len(names)
            and set(names) == set(expected), 'incomplete/duplicate JUnit test inventory')
    for suite in root.iter():
        if suite.tag not in ('testsuite', 'testsuites'):
            continue
        if 'tests' in suite.attrib:
            require(suite.attrib['tests'].isdigit()
                    and int(suite.attrib['tests']) == len(list(suite.iter('testcase'))),
                    'inconsistent suite test count')
        for key in ('failures', 'errors', 'skipped', 'disabled'):
            if key in suite.attrib:
                require(suite.attrib[key].isdigit() and int(suite.attrib[key]) == 0,
                        'nonzero/invalid suite status')
    for case in cases:
        require(not any(node.tag in ('failure', 'error', 'skipped') for node in case.iter()),
                'failed/errored/skipped test')
        require(case.get('status') in (None, 'run', 'passed'), 'non-running test status')
        try:
            duration = float(case.attrib['time'])
        except (ValueError, KeyError):
            raise ValueError('missing/invalid test duration')
        require(math.isfinite(duration) and duration >= 0, 'invalid test duration')
    return {'complete_test_count': len(cases), 'production_qualified': False}


def admit_resolved_roster(configured, built):
    """Bind every prebuild case/property to its resolved postbuild command.

    The raw CTest definitions must independently remain byte-identical. Missing
    commands are permitted only in the prebuild receipt, never in built evidence.
    """
    observed = roster(built)
    require(isinstance(configured,dict) and isinstance(configured.get('tests'),list),
            'configured test receipt required')
    projected = {'tests':[]}
    names=[]
    for row in configured['tests']:
        require(isinstance(row,dict), 'invalid configured case')
        name=row.get('name');require(isinstance(name,str) and name in observed,
                                     'configured case missing from build')
        names.append(name)
        # Fill only an absent command; an existing empty/malformed command fails.
        new=dict(row)
        if 'command' not in new:new['command']=observed[name]['command']
        projected['tests'].append(new)
    require(len(names)==len(set(names)) and set(names)==set(observed),
            'complete configured/built case roster differs')
    same_roster(projected,built)
    return {'resolved_test_count':len(observed),'raw_definition_identity_required':True,
            'resolved_executable_provenance_required':True,'production_qualified':False}


def admit_built_tests(*, configured, built, build_root, definition_hashes,
                      executable_hashes, ctest_record, ctest_stdout):
    """Join roster admission to raw definitions and actual owned executable bytes.

    expected hashes must be independently captured/frozen, not synthesized from
    the submitted CTest JSON. Command provenance uses the shared trusted ledger.
    The case-to-command association depends on trusted actual CTest execution
    against these unchanged definitions, not independent CMake-language parsing.
    A forged stdout/ledger could swap existing binaries or alter unresolved argv.
    The orchestrator must freeze the CTest runtime, source/scripts and dependencies
    and capture stdout directly via its shared ledger; this is a mandatory gate.
    """
    from pathlib import Path
    import json,hashlib
    from identity import capture_files,hashes
    build=Path(build_root).absolute()
    expected=['ctest','--test-dir',str(build),'--show-only=json-v1']
    require(ctest_record.get('argv')==expected and ctest_record.get('exit')==0
            and not ctest_record.get('error'), 'trusted CTest inventory command required')
    require(isinstance(ctest_stdout,bytes) and json.loads(ctest_stdout)==built,
            'built roster not bound to captured CTest stdout')
    hashes(definition_hashes);hashes(executable_hashes)
    require(capture_files(build,list(definition_hashes))==definition_hashes,
            'raw CTest definitions changed')
    observed=roster(built)
    paths={r['command'][0] for r in observed.values()}
    require(paths==set(executable_hashes), 'complete independently frozen executable roster required')
    for name,wanted in executable_hashes.items():
        path=Path(name)
        require(path.is_absolute() and '..' not in path.parts,'canonical executable required')
        require(capture_files(path.parent,[path.name])[path.name]==wanted, 'test executable bytes changed')
    receipt=admit_resolved_roster(configured,built)
    return {**receipt,'raw_definition_identity_required':False,
            'resolved_executable_provenance_required':False,
            'ctest_runtime_and_case_association_required':True,
            'ctest_stdout_sha256':hashlib.sha256(ctest_stdout).hexdigest()}
