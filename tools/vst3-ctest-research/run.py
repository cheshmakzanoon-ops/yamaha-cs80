#!/usr/bin/env python3
"""Full-suite CI-native trial, not a production workflow replacement."""
import argparse, hashlib, importlib.util, json, os, pathlib, re, shutil, subprocess, time
import xml.etree.ElementTree as ET
spec = importlib.util.spec_from_file_location('frozen', pathlib.Path(__file__).resolve().parents[1] / 'vst3-concurrency-research/run.py')
frozen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(frozen)

def read_inventory(ctest, build, selection=()):
    return json.loads(subprocess.check_output([ctest, '--test-dir', str(build), '--show-only=json-v1', *selection], text=True))

def roster(inventory):
    tests = inventory['tests']
    names = [t['name'] for t in tests]
    if len(names) != len(set(names)):
        raise ValueError('duplicate test names')
    return {t['name']: t for t in tests}

def verify_roster(inventory, expected='bf17b9e3f62882b3774f65386700788381251c285afa4ce0340767b4a6739e3b'):
    names = sorted(roster(inventory))
    digest = hashlib.sha256(('\n'.join(names) + '\n').encode()).hexdigest()
    if digest != expected:
        raise ValueError('full name roster changed')
    return digest

def properties(test):
    pairs = test.get('properties', [])
    result = {p['name']: p['value'] for p in pairs}
    if len(result) != len(pairs):
        raise ValueError('duplicate properties')
    return result

def partition(inventory, selected_names, expected_total=569):
    all_tests = roster(inventory)
    selected = set(selected_names)
    if len(selected) != len(selected_names) or not selected or not selected <= all_tests.keys():
        raise ValueError('missing/duplicate selected names')
    if len(all_tests) != expected_total:
        raise ValueError('full inventory count drift')
    for test in all_tests.values():
        if set(properties(test)) & {'DEPENDS', 'FIXTURES_REQUIRED', 'FIXTURES_SETUP', 'FIXTURES_CLEANUP', 'RESOURCE_LOCK', 'RESOURCE_GROUPS', 'RUN_SERIAL', 'DISABLED'}:
            raise ValueError('new dependency/resource/disabled semantics require review')
    remaining = set(all_tests) - selected
    if not remaining or selected & remaining or selected | remaining != set(all_tests):
        raise ValueError('not an exhaustive disjoint partition')
    return all_tests, selected, remaining

def bracket(text):
    marker = '='
    while ']' + marker + ']' in text:
        marker += '='
    return '[' + marker + '[' + text + ']' + marker + ']'

def environment_append(selected, homes):
    lines = ['\n# Temporary diagnostic-only per-case persistence isolation.\n']
    expected = {}
    for name in sorted(selected):
        home = homes / name
        home.mkdir(parents=True, exist_ok=False)
        values = [f'HOME={home}', f'XDG_CONFIG_HOME={home / "config"}', f'XDG_CACHE_HOME={home / "cache"}']
        expected[name] = values
        lines.append(f'set_tests_properties({bracket(name)} PROPERTIES ENVIRONMENT {bracket(";".join(values))})\n')
    return ''.join(lines).encode(), expected

def verify_environment(original, modified, expected):
    before, after = roster(original), roster(modified)
    if set(before) != set(after):
        raise ValueError('inventory changed after environment injection')
    for name in before:
        a, b = before[name], after[name]
        if a.get('command') != b.get('command'):
            raise ValueError('command drift after environment injection')
        pa, pb = properties(a), properties(b)
        if name in expected:
            if 'ENVIRONMENT' in pa or pb.pop('ENVIRONMENT', None) != expected[name]:
                raise ValueError('unexpected per-case environment')
        if pa != pb:
            raise ValueError('original test properties changed')

def junit_report(path, expected, all_tests):
    root = ET.parse(path).getroot()
    cases = list(root.iter('testcase'))
    names = [c.attrib.get('name') for c in cases]
    if len(names) != len(set(names)) or set(names) != set(expected):
        raise ValueError('JUnit membership mismatch')
    failures, skipped = [], []
    for c in cases:
        name = c.attrib['name']
        if c.find('failure') is not None or c.find('error') is not None:
            failures.append(name)
        if c.find('skipped') is not None or c.attrib.get('status') == 'notrun':
            skipped.append(name)
            skip = c.find('skipped')
            if (name != 'cs80_filter_batch_cap_tests' or properties(all_tests[name]).get('SKIP_RETURN_CODE') != 77
                    or skip is None or skip.attrib.get('message') != 'SKIP_RETURN_CODE=77'):
                raise ValueError('unexpected skipped/not-run test: ' + name)
    return {'count': len(cases), 'names': names, 'failures': failures, 'skipped_by_existing_policy': skipped}

def run_group(ctest, build, out, name, flags, jobs, expected, all_tests):
    folder = out / name
    folder.mkdir()
    temp = build / 'Testing/Temporary'
    last = temp / 'LastTest.log'
    if last.exists():
        last.rename(folder / 'prior-LastTest.log')
    command = [ctest, '--test-dir', str(build), '--output-on-failure', '--no-tests=error', '--parallel', str(jobs),
               '--output-junit', str(folder / 'junit.xml'), '--output-log', str(folder / 'ctest.log'), *flags]
    record = {'name': name, 'command': command, 'expected': sorted(expected), 'exit': None, 'accepted': False}
    start = time.monotonic()
    try:
        with (folder / 'console.log').open('wb') as log:
            record['exit'] = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
    except Exception as error:
        record['launch_error'] = str(error)
    record['seconds'] = time.monotonic() - start
    # Report errors never prevent the remaining group from running.
    try:
        for filename in ('LastTest.log', 'LastTestsFailed.log', 'CTestCostData.txt'):
            source = temp / filename
            if source.exists():
                shutil.copy2(source, folder / filename)
        for filename in ('LastTest.log', 'junit.xml', 'ctest.log', 'console.log'):
            if not (folder / filename).is_file() or (folder / filename).stat().st_size == 0:
                raise ValueError('missing/empty report: ' + filename)
        record['junit'] = junit_report(folder / 'junit.xml', expected, all_tests)
        record['accepted'] = record['exit'] == 0 and not record['junit']['failures']
    except Exception as error:
        record['report_error'] = str(error)
    (folder / 'result.json').write_text(json.dumps(record, indent=2))
    return record

def trial(ctest, build, out, inventory, selected_names, expected_total=569):
    all_tests, selected, remaining = partition(inventory, selected_names, expected_total)
    regex = '^(' + '|'.join(re.escape(n) for n in sorted(selected)) + ')$'
    testfile = build / 'CTestTestfile.cmake'
    if testfile.is_symlink() or not testfile.is_file():
        raise ValueError('expected regular generated CTest file')
    original = testfile.read_bytes()
    (out / 'original-CTestTestfile.cmake').write_bytes(original)
    result = {'accepted': False, 'selected': sorted(selected), 'remaining': sorted(remaining), 'groups': []}
    patched = None
    try:
        addition, environments = environment_append(selected, out / 'homes')
        patched = original + addition
        testfile.write_bytes(patched)
        modified = read_inventory(ctest, build)
        verify_environment(inventory, modified, environments)
        (out / 'modified-inventory.json').write_text(json.dumps(modified, indent=2))
        for flags, expected in ((['-R', regex], selected), (['-E', regex], remaining)):
            observed = read_inventory(ctest, build, flags)
            if set(roster(observed)) != expected:
                raise ValueError('actual CTest selection is not the promised partition')
        for name, flags, jobs, expected in [('selected', ['-R', regex], 2, selected), ('remaining', ['-E', regex], 1, remaining)]:
            try:
                record = run_group(ctest, build, out, name, flags, jobs, expected, all_tests)
            except Exception as error:
                record = {'name': name, 'accepted': False, 'error': str(error)}
            result['groups'].append(record)
        result['home_entries'] = [str(p.relative_to(out / 'homes')) for name in selected for p in (out / 'homes' / name).rglob('*')]
        result['accepted'] = all(g['accepted'] for g in result['groups']) and not result['home_entries']
    except Exception as error:
        result['error'] = str(error)
    finally:
        # Do not overwrite an unexpected concurrent modification.
        if patched is not None and testfile.read_bytes() == patched:
            testfile.write_bytes(original)
        result['generated_file_restored'] = testfile.read_bytes() == original
        if not result['generated_file_restored']:
            result['accepted'] = False
        (out / 'scheduler-result.json').write_text(json.dumps(result, indent=2))
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=pathlib.Path, required=True)
    parser.add_argument('--build', type=pathlib.Path, required=True)
    parser.add_argument('--out', type=pathlib.Path, required=True)
    parser.add_argument('--ctest', default='ctest')
    args = parser.parse_args()
    repo, build, out = args.repo.resolve(), args.build.resolve(), args.out.resolve()
    if any(c in str(p) for p in (repo, build, out) for c in ';\r\n'):
        raise ValueError('unsafe CMake environment path')
    out.mkdir(parents=True, exist_ok=False)
    result = {'accepted': False, 'baseline': frozen.BASELINE}
    try:
        result['frozen_tool_sha256'] = frozen.digest(spec.origin)
        if result['frozen_tool_sha256'] != 'c0f08f082abc0ea2eae94f6b7b1667c473e0b1cdbe707408b0a20d34eccd8cc1':
            raise ValueError('frozen helper changed')
        subprocess.run(['git', '-C', str(repo), 'diff', '--exit-code', frozen.BASELINE, 'HEAD', '--', 'src', 'tests', 'CMakeLists.txt', 'cmake'], check=True)
        if subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain', '--', 'src', 'tests', 'CMakeLists.txt', 'cmake'], text=True).strip():
            raise ValueError('dirty frozen source')
        for p in ('/usr/local/lib/libMTS.so', '/Library/Application Support/MTS-ESP/libMTS.dylib'):
            if pathlib.Path(p).exists():
                raise ValueError('external MTS library present')
        result['commit'] = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
        inventory = read_inventory(args.ctest, build)
        (out / 'original-inventory.json').write_text(json.dumps(inventory, indent=2))
        result['full_roster_sha256'] = verify_roster(inventory)
        selected = frozen.validate_inventory(inventory, repo, build)
        inputs = {str(build / 'CTestTestfile.cmake')} | {c['command'][0] for c in selected} | {c['command'][1] for c in selected}
        result['input_hashes'] = {p: frozen.digest(p) for p in sorted(inputs)}
        result['trial'] = trial(args.ctest, build, out, inventory, [c['name'] for c in selected])
        result['inputs_unchanged'] = all(frozen.digest(p) == h for p, h in result['input_hashes'].items())
        # Keep original PWM directories for production-equivalent consumers.
        result['captures'] = {}
        for case in selected:
            if case['capture']:
                capture = pathlib.Path(case['capture'])
                records = {p.name: {'bytes': p.stat().st_size, 'sha256': frozen.digest(p)} for p in capture.iterdir() if p.is_file()}
                if set(records) != frozen.expected_pcm_names() or any(r['bytes'] != 32768 for r in records.values()):
                    raise ValueError('unexpected preserved PWM outputs')
                result['captures'][case['name']] = records
        result['accepted'] = result['trial']['accepted'] and result['inputs_unchanged']
    except Exception as error:
        result['error'] = str(error)
    (out / 'results.json').write_text(json.dumps(result, indent=2))
    print('PASS' if result['accepted'] else 'FAIL', 'CI-native full-suite diagnostic; no production workflow change')
    return 0 if result['accepted'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
