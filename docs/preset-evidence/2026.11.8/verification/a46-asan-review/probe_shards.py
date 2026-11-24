#!/usr/bin/env python3
"""Independent synthetic CTest/schema controls; never a product/ASan result."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
CMAKE = '/tmp/cs80-614c8ce056b7-deps/cmake/data/bin/cmake'
CTEST = '/tmp/cs80-614c8ce056b7-deps/cmake/data/bin/ctest'


def main():
    cli = argparse.ArgumentParser()
    cli.add_argument('--helper', type=Path, required=True)
    cli.add_argument('--label', required=True)
    args = cli.parse_args()
    raw = args.helper.read_bytes()
    snapshot = HERE / (args.label + '-helper.py')
    snapshot.write_bytes(raw)
    spec = importlib.util.spec_from_file_location('a46_shards', snapshot)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    env = dict(os.environ, GITHUB_SHA='a' * 40, GITHUB_RUN_ID='1', GITHUB_RUN_ATTEMPT='1', **module.SANITIZER_ENV)
    results = []
    with tempfile.TemporaryDirectory(prefix=args.label + '-', dir=HERE) as temporary:
        work = Path(temporary)

        def call(arguments):
            return subprocess.run(arguments, env=env, capture_output=True, text=True, timeout=45)

        def fixture(label, failing=False, fixtures=False):
            source, build = work / (label + '-source'), work / (label + '-build')
            source.mkdir()
            (source / 'skip.py').write_text('raise SystemExit(77)\n')
            lines = ['cmake_minimum_required(VERSION 3.22)', 'project(A46InertFixture NONE)', 'enable_testing()']
            for i in range(1, 233):
                command = ('"' + sys.executable + '" "' + str(source / 'skip.py') + '"' if i == 80
                           else '"${CMAKE_COMMAND}" -E ' + ('false' if failing and i == 1 else 'true'))
                lines += [f'add_test(NAME a46_{i:03} COMMAND {command})',
                          f'set_tests_properties(a46_{i:03} PROPERTIES RUN_SERIAL TRUE TIMEOUT 10)']
            lines += ['set_tests_properties(a46_080 PROPERTIES SKIP_RETURN_CODE 77)']
            if fixtures:
                lines += ['set_tests_properties(a46_001 PROPERTIES FIXTURES_SETUP state)',
                          'set_tests_properties(a46_002 PROPERTIES FIXTURES_REQUIRED state)',
                          'set_tests_properties(a46_003 PROPERTIES FIXTURES_CLEANUP state)']
            (source / 'CMakeLists.txt').write_text('\n'.join(lines) + '\n')
            configured = call([CMAKE, '-S', str(source), '-B', str(build),
                               *[f'-D{k}={v}' for k, v in module.BUILD_FLAGS.items()]])
            if configured.returncode:
                raise RuntimeError(configured.stdout + configured.stderr)
            return build

        def helper(operation, build, output, extra=()):
            return call([sys.executable, str(snapshot), operation, '--ctest', CTEST,
                         '--build', str(build), '--output', str(output), *extra])

        def verify(case):
            p = call([sys.executable, str(snapshot), 'verify', '--input', str(case),
                      '--output', str(case / 'summary.json'), '--jobs-result', 'success'])
            summary = json.loads((case / 'summary.json').read_bytes())
            return p.returncode, summary

        build = fixture('positive')
        originals = work / 'originals'
        for shard in (1, 2, 3):
            output = originals / str(shard)
            for operation, extra in [('plan', ('--shard', str(shard))), ('run', ())]:
                p = helper(operation, build, output, extra)
                if p.returncode:
                    raise RuntimeError(operation + ': ' + p.stdout + p.stderr)
        code, summary = verify(originals)
        if code != 0 or summary.get('tests') != 232 or summary.get('passed') != 231:
            raise RuntimeError('Positive complete union failed: ' + repr(summary))
        results.append({'case': 'real_ctest_232_inert_cases_with_declared_skip', 'expected_accept': True,
                        'accepted': True, 'summary': summary})
        sample_log = (originals / '1/LastTest.log').read_text()
        (HERE / (args.label + '-lasttest-framing.txt')).write_text('\n'.join(sample_log.splitlines()[:28]) + '\n...\n' + '\n'.join(sample_log.splitlines()[-8:]) + '\n')

        def mutate(label, action, shard=1):
            with tempfile.TemporaryDirectory(prefix='mutation-', dir=work) as d:
                case = Path(d) / 'artifacts'
                shutil.copytree(originals, case)
                action(case / str(shard), case)
                receipt_path = case / str(shard) / 'result.json'
                if receipt_path.exists():
                    receipt = json.loads(receipt_path.read_text())
                    receipt['sha256'] = {name: hashlib.sha256((case / str(shard) / name).read_bytes()).hexdigest()
                                         for name in module.EVIDENCE if (case / str(shard) / name).exists()}
                    receipt_path.write_text(json.dumps(receipt))
                code, summary = verify(case)
                results.append({'case': label, 'expected_accept': False, 'accepted': code == 0,
                                'returncode': code, 'error': summary.get('error')})

        def xml_change(folder, function):
            p = folder / 'ctest-results.xml'
            tree = ET.parse(p)
            function(tree.getroot())
            tree.write(p)

        def status_change(value):
            def apply(root):
                case = next(root.iter('testcase'))
                if value is None:
                    case.attrib.pop('status', None)
                else:
                    case.set('status', value)
            return apply

        mutate('junit_missing_run_status', lambda p, _: xml_change(p, status_change(None)))
        mutate('junit_unknown_status', lambda p, _: xml_change(p, status_change('mystery')))
        for value in (None, 'run', 'disabled'):
            def change_skip(root, value=value):
                case = next(c for c in root.iter('testcase') if c.get('name') == 'a46_080')
                if value is None: case.attrib.pop('status', None)
                else: case.set('status', value)
            mutate('junit_declared_skip_status_' + str(value), lambda p, _, fn=change_skip: xml_change(p, fn), 2)
        mutate('junit_wrong_total', lambda p, _: xml_change(p, lambda root: root.set('tests', '1')))
        mutate('junit_failure', lambda p, _: xml_change(p, lambda root: ET.SubElement(next(root.iter('testcase')), 'failure')))
        mutate('junit_missing_case', lambda p, _: xml_change(p, lambda root: root.remove(next(root.iter('testcase')))))
        mutate('junit_duplicate_case', lambda p, _: xml_change(p, lambda root: root.append(copy.deepcopy(next(root.iter('testcase'))))))
        def nested(root):
            cases = list(root.findall('testcase'))
            wrapper = ET.SubElement(root, 'unknown-wrapper')
            for case in cases:
                root.remove(case)
                wrapper.append(case)
        mutate('junit_cases_nested_in_unknown_wrapper', lambda p, _: xml_change(p, nested))
        def hidden_marker(root, marker):
            case = next(root.iter('testcase'))
            wrapper = ET.SubElement(case, 'unknown-wrapper')
            ET.SubElement(wrapper, marker)
        mutate('junit_nested_skip_in_running_case', lambda p, _: xml_change(p, lambda r: hidden_marker(r, 'skipped')))
        mutate('junit_nested_failure_in_running_case', lambda p, _: xml_change(p, lambda r: hidden_marker(r, 'failure')))
        mutate('lasttest_names_and_footer_only', lambda p, _: (p / 'LastTest.log').write_text('\n'.join(
            line for line in (p / 'LastTest.log').read_text().splitlines() if ' Testing: ' in line or line.startswith('End testing:')) + '\n'))
        mutate('lasttest_wrong_global_ordinals', lambda p, _: (p / 'LastTest.log').write_text(__import__('re').sub(
            r'^\d+/\d+ (Testing|Test):', r'999/999 \1:', (p / 'LastTest.log').read_text(), flags=__import__('re').M)))
        mutate('lasttest_duplicate_start', lambda p, _: (p / 'LastTest.log').write_text('Start testing: duplicated\n' + (p / 'LastTest.log').read_text()))
        mutate('lasttest_duplicate_full_start_frame', lambda p, _: (p / 'LastTest.log').write_text(
            '\n'.join((p / 'LastTest.log').read_text().splitlines()[:2]) + '\n' + (p / 'LastTest.log').read_text()))
        mutate('lasttest_missing_output_opening', lambda p, _: (p / 'LastTest.log').write_text(
            (p / 'LastTest.log').read_text().replace('Output:\n----------------------------------------------------------\n', '', 1)))
        mutate('lasttest_end_before_output', lambda p, _: (p / 'LastTest.log').write_text('End testing: premature\n' + '\n'.join(
            line for line in (p / 'LastTest.log').read_text().splitlines() if not line.startswith('End testing:')) + '\n'))
        mutate('lasttest_missing_last_output_terminator', lambda p, _: (p / 'LastTest.log').write_text(
            (p / 'LastTest.log').read_text().rsplit('<end of output>', 1)[0] + (p / 'LastTest.log').read_text().rsplit('<end of output>', 1)[1]))
        mutate('lasttest_reported_failure', lambda p, _: (p / 'LastTest.log').write_text((p / 'LastTest.log').read_text().replace('Test Passed.', 'Test Failed.', 1)))
        def change_receipt(folder, field, value):
            p = folder / 'result.json'; v = json.loads(p.read_text()); v[field] = value; p.write_text(json.dumps(v))
        mutate('original_ctest_exit8', lambda p, _: change_receipt(p, 'ctest_returncode', 8))
        mutate('original_ctest_signal', lambda p, _: change_receipt(p, 'ctest_returncode', -15))
        mutate('missing_junit', lambda p, _: (p / 'ctest-results.xml').unlink())
        for shard in (0, 4, -1):
            p = helper('plan', build, work / ('invalid-' + str(shard)), ('--shard', str(shard)))
            results.append({'case': 'out_of_range_' + str(shard), 'expected_accept': False, 'accepted': p.returncode == 0,
                            'returncode': p.returncode})
        failed_build = fixture('real-failure', failing=True)
        failed_output = work / 'real-failure-output'
        p = helper('plan', failed_build, failed_output, ('--shard', '1'))
        if p.returncode: raise RuntimeError(p.stderr)
        p = helper('run', failed_build, failed_output)
        receipt = json.loads((failed_output / 'result.json').read_text())
        results.append({'case': 'real_ctest_failed_test_propagation', 'expected_accept': False,
                        'accepted': p.returncode == 0, 'returncode': p.returncode,
                        'original_ctest_returncode': receipt['ctest_returncode'], 'completed': receipt['completed']})
        fixture_build = fixture('dependency', fixtures=True)
        expanded = call([CTEST, '--test-dir', str(fixture_build), '--show-only=json-v1', '-I', '2,232,3'])
        actual = [test['name'] for test in json.loads(expanded.stdout)['tests']]
        intended = [f'a46_{i:03}' for i in range(2, 233, 3)]
        p = helper('plan', fixture_build, work / 'dependency-output', ('--shard', '2'))
        results.append({'case': 'actual_fixture_setup_cleanup_cross_shard', 'expected_accept': False,
                        'accepted': p.returncode == 0, 'returncode': p.returncode,
                        'planned_count': len(intended), 'actual_ctest_selected_count': len(actual),
                        'auto_added': sorted(set(actual) - set(intended)), 'error': p.stderr.strip()})
    output = {'scope': 'Synthetic CTest helper review only; no product or sanitizer qualification.',
              'helper_sha256': hashlib.sha256(raw).hexdigest(), 'helper_bytes': len(raw),
              'cases': results, 'unexpected_acceptances': [r['case'] for r in results if r['accepted'] != r['expected_accept']]}
    (HERE / (args.label + '-probe.json')).write_text(json.dumps(output, indent=2) + '\n')
    print(json.dumps({'cases': len(results), 'helper_sha256': output['helper_sha256'],
                      'unexpected_acceptances': output['unexpected_acceptances']}))


if __name__ == '__main__':
    main()
