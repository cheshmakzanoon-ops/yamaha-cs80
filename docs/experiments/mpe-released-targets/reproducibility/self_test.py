#!/usr/bin/env python3
"""Mutation-test the archive verifier in normal and optimized Python.

Changes are made only in temporary bundle copies. No product tests are run.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[2]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def write(path, obj):
    path.write_text(json.dumps(obj, indent=2, sort_keys=True) + '\n')


def load(path):
    return json.loads(path.read_text())


def rehash(root, rel):
    p = root/rel
    manifest = load(root/'manifest.json')
    manifest['files'][rel] = {'bytes': p.stat().st_size, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
    write(root/'manifest.json', manifest)


def json_change(root, rel, edit):
    p = root/rel
    obj = load(p)
    edit(obj)
    write(p, obj)
    rehash(root, rel)


def apply(root, case):
    rel = 'evidence/core-second-run.log'
    if case == 'changed-byte':
        (root/rel).write_text('wrong bytes\n')
    elif case == 'missing-file':
        (root/rel).unlink()
    elif case == 'extra-file':
        (root/'unexpected.txt').write_text('not inventoried')
    elif case == 'unsafe-manifest-path':
        j = load(root/'manifest.json'); j['files']['../escape'] = {'bytes': 0, 'sha256': '0'*64}; write(root/'manifest.json', j)
    elif case == 'duplicate-core-summary':
        p = root/rel; p.write_text(p.read_text()*2); rehash(root, rel)
    elif case == 'truncated-core-summary':
        (root/rel).write_text(''); rehash(root, rel)
    elif case == 'wrong-core-count':
        p = root/rel; p.write_text(p.read_text().replace('7488', '7487')); rehash(root, rel)
    elif case == 'missing-compiler-profile':
        json_change(root, 'evidence/compatibility/comparison.json', lambda j: j['profiles'].pop())
    elif case == 'failed-compatibility-run':
        json_change(root, 'evidence/compatibility/comparison.json', lambda j: j['profiles'][0]['run_exits'].__setitem__(0, 1))
    elif case == 'failed-native-command':
        json_change(root, 'evidence/native/final-native-3003569-attempt-002/commands-statuses.json', lambda j: j[0].__setitem__('status', 1))
    elif case == 'changed-repository-source-pin':
        json_change(root, 'evidence/source-pins.json', lambda j: j['source_inputs']['src/core/Engine.cpp'].__setitem__('sha256', '0'*64))
    elif case == 'missing-preserved-path':
        json_change(root, 'evidence/source-pins.json', lambda j: j['preserved_tree_entries'].pop(next(iter(j['preserved_tree_entries']))))
    elif case == 'misattributed-source-freeze':
        json_change(root, 'evidence/source-pins.json', lambda j: j.__setitem__('focused_source_test_freeze', '0'*40))
    elif case == 'missing-fault-case':
        json_change(root, 'evidence/fault-controls/run-3003569/summary.json', lambda j: j['mutations'].pop('omit-bend-held-guard'))
    elif case == 'crashing-fault-counted':
        json_change(root, 'evidence/fault-controls/run-3003569/summary.json', lambda j: j['mutations']['omit-bend-held-guard'].__setitem__('exit_code', -11))
    elif case == 'missing-final-profile-stage':
        json_change(root, 'evidence/profiles-final/commands.json', lambda j: j.pop())
    elif case == 'disabled-sanitizer-instrumentation':
        def remove_flags(j):
            row = next(r for r in j if r['profile'] == 'sanitizer' and r['stage'] == 'configure')
            row['argv'] = [x.replace('-fsanitize=address,undefined,float-cast-overflow', '') for x in row['argv']]
        json_change(root, 'evidence/profiles-final/commands.json', remove_flags)
    elif case in {'missing-ctest-case', 'duplicate-ctest-case'}:
        rel = 'evidence/native/final-native-3003569-attempt-002/ctest.log'
        p = root/rel
        lines = p.read_text().splitlines()
        row = next(x for x in lines if '1/1 Test' in x)
        if case == 'missing-ctest-case':
            lines.remove(row)
        else:
            lines.insert(1, row)
        p.write_text('\n'.join(lines) + '\n')
        rehash(root, rel)
        sha = hashlib.sha256(p.read_bytes()).hexdigest()
        def update_ctest(j):
            next(r for r in j if r['stage'] == 'ctest')['sha256'] = sha
        json_change(root, 'evidence/native/final-native-3003569-attempt-002/commands-statuses.json', update_ctest)
    elif case != 'valid-bundle':
        raise ValueError('Unknown test case')
    if case in {'duplicate-core-summary', 'truncated-core-summary', 'wrong-core-count'}:
        # Repair both hash indexes so rejection must come from result parsing.
        sha = hashlib.sha256((root/'evidence/core-second-run.log').read_bytes()).hexdigest()
        def update_core(j):
            next(r for r in j['results'] if r['log'] == 'core-second-run.log')['log_sha256'] = sha
        json_change(root, 'evidence/core-focused-manifest.json', update_core)



def integration_case(case):
    spec = importlib.util.spec_from_file_location('bundle_verifier', ROOT/'verify.py')
    verifier = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(verifier)
    pins = load(ROOT/'evidence/source-pins.json')
    original = {n: r for n,r in pins['preserved_tree_entries'].items() if n.startswith('docs/screenshots/')}
    newer = json.loads(json.dumps(original))
    changes = sorted(original)[:24]
    # Synthetic schema fixture only; no claim these bytes represent the real child.
    for name in changes:
        newer[name]['sha256'] = hashlib.sha256((name+'synthetic').encode()).hexdigest()
    record = {'historical_baseline': verifier.BASE,
              'final_source_test_freeze': verifier.FINAL, 'runtime_repeated': False,
              'snapshot_parent_parent_tree_equals_baseline': True,
              'screenshot_parent': '4e01dcda8f0e6a721c492ef5e2ae4d952e7230c5',
              'screenshot_tree': newer, 'changed_screenshot_paths': list(changes),
              'screenshot_only_parent_delta': list(changes)}
    if case == 'integration-outside-screenshots':
        record['screenshot_tree']['src/core/Engine.cpp'] = pins['source_inputs']['src/core/Engine.cpp']
    elif case == 'integration-missing-screenshot':
        record['screenshot_tree'].pop(changes[0])
    elif case == 'integration-extra-screenshot':
        record['screenshot_tree']['docs/screenshots/extra.png'] = next(iter(newer.values()))
    elif case == 'integration-wrong-historical-delta':
        record['changed_screenshot_paths'][0] = sorted(original)[-1]
    elif case == 'integration-parent-runtime-change':
        record['screenshot_only_parent_delta'].append('src/core/Engine.cpp')
    elif case == 'integration-different-baseline-tree':
        record['snapshot_parent_parent_tree_equals_baseline'] = False
    elif case == 'integration-modified-runtime-input':
        # Exercise the exact inventory check used before/after applying the overlay.
        with tempfile.TemporaryDirectory(prefix='mpe-runtime-integrity-') as tmp:
            root = Path(tmp)
            name = 'src/core/Engine.cpp'
            p = root/name; p.parent.mkdir(parents=True)
            p.write_bytes((REPO/name).read_bytes() + b'\n// unexpected runtime change\n')
            verifier.inventory(root, {name: pins['source_inputs'][name]})
    elif case != 'integration-valid-schema':
        raise ValueError('Unknown integration test')
    verifier.integrated_preservation(pins, record)


def main():
    cases = ['valid-bundle', 'changed-byte', 'missing-file', 'extra-file', 'unsafe-manifest-path',
             'duplicate-core-summary', 'truncated-core-summary', 'wrong-core-count',
             'missing-compiler-profile', 'failed-compatibility-run', 'failed-native-command',
             'changed-repository-source-pin', 'missing-preserved-path', 'misattributed-source-freeze',
             'missing-fault-case', 'crashing-fault-counted', 'missing-final-profile-stage',
             'disabled-sanitizer-instrumentation', 'missing-ctest-case', 'duplicate-ctest-case']
    outcomes = []
    with tempfile.TemporaryDirectory(prefix='mpe-bundle-self-test-') as temp:
        for case in cases:
            root = Path(temp)/case
            shutil.copytree(ROOT, root, ignore=shutil.ignore_patterns('__pycache__'))
            apply(root, case)
            codes = []
            for mode in [[], ['-O']]:
                result = subprocess.run([sys.executable, *mode, str(ROOT/'verify.py'), '--root', str(root), '--repo', str(REPO)], text=True, capture_output=True)
                codes.append(result.returncode)
            expected = 0 if case == 'valid-bundle' else 1
            require(codes == [expected, expected], f'Incorrect verifier behavior for {case}: {codes}')
            outcomes.append({'case': case, 'normal_exit': codes[0], 'optimized_exit': codes[1], 'expected_exit': expected})
    integration_cases = ['integration-valid-schema', 'integration-outside-screenshots',
                         'integration-missing-screenshot', 'integration-extra-screenshot',
                         'integration-wrong-historical-delta', 'integration-parent-runtime-change',
                         'integration-different-baseline-tree', 'integration-modified-runtime-input']
    for case in integration_cases:
        codes = []
        for mode in [[], ['-O']]:
            result = subprocess.run([sys.executable, *mode, str(Path(__file__).resolve()), '--integration-case', case], capture_output=True, text=True)
            codes.append(result.returncode)
        expected = 0 if case == 'integration-valid-schema' else 1
        require(codes == [expected, expected], f'Incorrect integration behavior for {case}: {codes}')
        outcomes.append({'case': case, 'normal_exit': codes[0], 'optimized_exit': codes[1], 'expected_exit': expected})
    print(json.dumps({'product_tests_repeated': False, 'integration_schema_fixture': 'synthetic; no screenshot rendering or real integration claim', 'cases': outcomes, 'all_passed': True}, indent=2, sort_keys=True))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--integration-case':
        try:
            integration_case(sys.argv[2])
        except ValueError as error:
            print(str(error), file=sys.stderr)
            sys.exit(1)
    else:
        main()
