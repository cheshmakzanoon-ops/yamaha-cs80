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
