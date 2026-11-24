#!/usr/bin/env python3
"""Reconcile original native evidence without executing any product binary.

The A40 frozen checker derives the expected roster and separately checks this
review. This script independently parses the original XML, LastTest and producer
console, then joins A42's source map. Missing platforms remain unobserved.
"""
from __future__ import annotations

import argparse
import ast
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

sys.dont_write_bytecode = True
SOURCE = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
TREE = '17115913cbb29157bd00539485bcc5f38b57219c'
CONTEXT = dict(factory_revision='2026.11.7', source=SOURCE, tree=TREE,
               run_id=45397189410, attempt=1)
COUNTS = {'linux-x86_64': 613, 'windows-x86_64': 255,
          'macos-arm64': 277, 'macos-x86_64': 277}
JOBS = {'linux-x86_64': 138455975798, 'windows-x86_64': 138455976256,
        'macos-arm64': 138455976227, 'macos-x86_64': 138455976307}
CHECKER = 'docs/preset-evidence/2026.11.7/verification/a40/source-ee0dabca/verify-native-contract.py'
CHECKER_SHA = 'cb9597061fe16b590f85cfc75ddbc98e5868c80d1a9d6763f77edfc71b27e32f'
SKIPPABLE = {'cs80_filter_batch_cap_tests', 'cs80_stereo_pra_exact_tests'}
NEW = ['cs80_piano_partial_decay_tests', 'cs80_body_noise_rate_tests',
       'cs80_string_body_formant_tests', 'cs80_low_choir_spectrum_tests']
SKIP_SOURCES = {
    'cs80_package_binary_identity_tests': 'scripts/test-package-binary-identity.py',
    'cs80_packaging_notice_tests': 'scripts/test-packaging-notices.py',
    'cs80_windows_installer_preflight_tests': 'scripts/test-windows-installer-preflight.py',
    'cs80_validator_diagnostic_selection_tests': 'scripts/test-pluginval-diagnostic-log.py',
    'cs80_preset_capture_tests': 'tests/test_preset_capture.py',
    'cs80_render_cli_tests': 'tests/test_render_cli.py',
    'cs80_truth_render_tests': 'tests/test_truth_render.py',
    'cs80_vca_reference_tests': 'tests/test_vca_reference.py',
    'cs80_float_wav_tests': 'tests/test_float_wav.py',
}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def record(path):
    raw = path.read_bytes()
    return dict(path=str(path), bytes=len(raw), sha256=digest(raw))


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('x') as f:
        json.dump(data, f, indent=2, allow_nan=False)
        f.write('\n')


def decode(raw):
    return raw.decode('utf-16' if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig').replace('\r\n', '\n')


def source(repo, path):
    return subprocess.check_output(['git', '-C', str(repo), 'show', SOURCE + ':' + path])


def method_review(repo, name, output, total, tests_run):
    path = SKIP_SOURCES[name]
    raw = source(repo, path)
    text = raw.decode()
    classes = [node for node in ast.walk(ast.parse(text)) if isinstance(node, ast.ClassDef)]
    candidates = [(cl.name, sorted([node for node in cl.body if isinstance(node, ast.FunctionDef)
                   and node.name.startswith('test_')], key=lambda node: node.name)) for cl in classes]
    candidates = sorted([(name, nodes) for name, nodes in candidates if nodes])
    require(candidates and 'def load_tests(' not in text, 'Custom/absent unittest loader: ' + path)
    methods = [node for _, nodes in candidates for node in nodes]
    require(len(methods) == tests_run, 'Unittest method count differs: ' + name)
    patterns = re.findall(r'^[.s]+$', output, re.M)
    explicit = re.findall(r'^(test_\w+)\s+\([^\n]+\) \.\.\. skipped ([^\n]+)$', output, re.M)
    if explicit:
        skipped = [method for method, _ in explicit]
        basis = 'Explicit original LastTest method and reason text.'
        pattern = None
    else:
        require(len(patterns) == 1 and len(patterns[0]) == len(methods), 'Ambiguous unittest pattern: ' + name)
        pattern = patterns[0]
        skipped = [node.name for node, char in zip(methods, pattern) if char == 's']
        basis = 'Inference from exact original nonverbose pattern and frozen-source default unittest class-then-method alphabetical order; terminal summary supplies the count.'
    require(len(skipped) == total, 'Method mapping does not reconcile skip count: ' + name)
    details = []
    for node in methods:
        if node.name not in skipped:
            continue
        inline = [dict(line=x.lineno, text=ast.get_source_segment(text, x)) for x in ast.walk(node)
                  if isinstance(x, ast.Call) and isinstance(x.func, ast.Attribute) and x.func.attr == 'skipTest']
        details.append(dict(method=node.name, line=node.lineno,
                            decorators=[ast.get_source_segment(text, x) for x in node.decorator_list],
                            inline_skip_branches=inline))
    result = dict(source=path, source_sha256=digest(raw), method_order=[n.name for n in methods],
                  qualified_method_order=[name+'.'+node.name for name,nodes in candidates for node in nodes],
                  actual_nonverbose_pattern=pattern, skipped_methods=details,
                  explicit_method_reasons=explicit, mapping_basis=basis)
    if name == 'cs80_render_cli_tests' and total == 1:
        result['runtime_reason_limit'] = ('Bounded-buffer method is identified, but nonverbose output does not distinguish '
            'missing/unavailable RLIMIT_AS from the specific incompatible ASan shadow-address-space cap branch. '
            'No exact dynamic reason or capped-memory execution is claimed. Separate uncapped long-write-failure test passes.')
    if name == 'cs80_float_wav_tests':
        result['partial_method_limit'] = ('The skipped method first checks bit patterns, bounds and no-overwrite behavior; '
            'its only frozen-source skip branch is ImportError while importing POSIX fault-injection modules. '
            'Do not claim that its final file-size fault-injection assertion executed on Windows.')
    return result


def lasttest_rows(text, expected):
    matches = list(re.finditer(r'^(\d+)/(\d+) Test: (.+)$', text, re.M))
    require(len(matches) == expected, 'Incomplete LastTest roster')
    require([int(m[1]) for m in matches] == list(range(1, expected + 1)), 'LastTest numbering differs')
    require({int(m[2]) for m in matches} == {expected}, 'LastTest denominators differ')
    require(len(re.findall(r'^Start testing:', text, re.M)) == 1 and
            len(re.findall(r'^End testing:', text, re.M)) == 1 and
            text.rfind('End testing:') > text.rfind('<end of output>'), 'Incomplete LastTest session')
    rows = {}
    for i, m in enumerate(matches):
        name = m[3]
        require(name not in rows, 'Duplicate LastTest name')
        chunk = text[m.end():matches[i+1].start() if i+1 < len(matches) else len(text)]
        out = re.search(r'^Output:\n-+\n(.*?)^<end of output>', chunk, re.S | re.M)
        command = re.search(r'^Command: (.+)$', chunk, re.M)
        require(out is not None and command is not None, 'Incomplete LastTest case: ' + name)
        tail = chunk[out.end():]
        status = re.findall(r'^Test (Passed|Skipped|Failed)\.', tail, re.M)
        reasons = re.findall(r'^Test (Pass|Fail) Reason:', tail, re.M)
        require(len(status) + len(reasons) == 1, 'Ambiguous LastTest result: ' + name)
        value = status[0] if status else ('Passed' if reasons[0] == 'Pass' else 'Failed')
        rows[name] = dict(number=int(m[1]), output=out[1], lasttest_status=value, command=command[1])
    return rows


def parse_target(repo, out_dir, item, contract, coverage):
    target = item['target']
    expected = contract['expected_names'][target]
    archive = Path(item['archive'])
    api_path, job_path, log_path = (Path(item[k]) for k in ('api', 'job_api', 'job_log'))
    raw = archive.read_bytes()
    api, job = (json.loads(p.read_text()) for p in (api_path, job_path))
    require(api['size_in_bytes'] == len(raw) and api['digest'] == 'sha256:' + digest(raw), 'ZIP/API byte binding differs')
    require(api['workflow_run']['id'] == CONTEXT['run_id'] and api['workflow_run']['head_sha'] == SOURCE, 'Artifact run/source differs')
    require(job['id'] == JOBS[target] and job['run_id'] == CONTEXT['run_id'] and job['run_attempt'] == 1
            and job['head_sha'] == SOURCE and job['status'] == 'completed' and job['conclusion'] == 'success', 'Original producer identity/result differs')
    tests = [x for x in job['steps'] if x['name'] == 'Test']
    require(len(tests) == 1 and tests[0]['status'] == 'completed' and tests[0]['conclusion'] == 'success', 'Original Test step is not successful')
    with zipfile.ZipFile(archive) as z:
        require(len(z.namelist()) == len(set(z.namelist())) == 3 and set(z.namelist()) ==
                {'ctest-results.xml', 'Testing/Temporary/LastTest.log', 'ci-juce-provenance.txt'}, 'Native ZIP member set differs')
        members = {name: z.read(name) for name in z.namelist()}
    root = ET.fromstring(members['ctest-results.xml'])
    cases = root.findall('testcase')
    require(root.tag == 'testsuite' and len(cases) == int(root.get('tests')) == len(expected), 'JUnit registration total differs')
    names = [c.get('name') for c in cases]
    require(len(names) == len(set(names)) and set(names) == set(expected), 'Exact registered native names differ')
    last = lasttest_rows(decode(members['Testing/Temporary/LastTest.log']), len(expected))
    require(list(last) == names, 'JUnit/LastTest ordered names differ')
    rows, formal, nested, mappings, warnings = [], [], [], {}, []
    for c in cases:
        name = c.get('name')
        require(c.find('failure') is None and c.find('error') is None, 'Failing JUnit case: ' + name)
        skip = c.find('skipped')
        if skip is not None:
            require(c.get('status') == 'notrun' and skip.get('message') == 'SKIP_RETURN_CODE=77'
                    and name in SKIPPABLE, 'Non-capability formal skip')
            result = 'formal_skipped'
            formal.append(dict(name=name, status=c.get('status'), message=skip.get('message')))
        else:
            require(c.get('status') == 'run', 'Unknown/disabled JUnit case: ' + name)
            result = 'passed'
        require(last[name]['lasttest_status'] in ('Passed', 'Skipped'), 'Nonpassing LastTest case: ' + name)
        if result == 'passed':
            require(last[name]['lasttest_status'] == 'Passed', 'JUnit/LastTest passing result differs')
        output = last[name]['output']
        rows.append(dict(name=name, number=last[name]['number'], result=result,
                         lasttest_status=last[name]['lasttest_status'], seconds=float(c.get('time')),
                         output_sha256=digest(output.encode()), junit_stdout_truncated='[This part of the test output was removed' in c.findtext('system-out', '')))
        summaries = []
        for m in re.finditer(r'^Ran (\d+) tests? in [^\n]+\n\s*\n?(OK|FAILED)(?: \(([^\n]*)\))?\s*$', output, re.M):
            count = re.search(r'(?:^|,\s*)skipped=(\d+)(?:,|$)', m[3] or '')
            summaries.append(dict(tests_run=int(m[1]), result=m[2], skipped=int(count[1]) if count else 0, summary=m[0].strip()))
        total = sum(x['skipped'] for x in summaries)
        explicit = [s for s in output.splitlines() if re.search(r'\.\.\. skipped [\"\']', s)]
        if total or explicit:
            require(result == 'passed' and len(explicit) <= total and len(summaries) == 1, 'Nested skip accounting differs')
            nested.append(dict(ctest_test=name, total=total, summaries=summaries, explicit_skip_lines=explicit,
                               scope='Unittest terminal summaries only; explicit lines are reasons, not additional counts.'))
            mappings[name] = method_review(repo, name, output, total, summaries[0]['tests_run'])
        warning_lines = [line for line in output.splitlines() if re.search(r'warning|report only|optional|SKIP:|unavailable', line, re.I)]
        if warning_lines:
            warnings.append(dict(ctest_test=name, lines=warning_lines))
    counts = dict(registered=len(rows), passed=sum(r['result'] == 'passed' for r in rows), failed=0,
                  formal_skipped=len(formal), disabled=0, unknown=0)
    for field, key in [('failures', 'failed'), ('disabled', 'disabled'), ('skipped', 'formal_skipped')]:
        require(int(root.get(field, 0)) == counts[key], 'JUnit aggregate disagrees with cases')
    producer_text = decode(log_path.read_bytes())
    clean = re.sub(r'\x1b\[[0-9;]*m', '', producer_text)
    clean = re.sub(r'^\d{4}-\d\d-\d\dT\S+Z ', '', clean, flags=re.M)
    console = re.findall(r'^\s*(\d+)/(\d+) Test\s+#(\d+):\s+(\S+)\s+\.+\s*(Passed|\*\*\*Skipped)\s+([\d.]+) sec$', clean, re.M)
    require(len(console) == len(rows), 'Original console does not contain the complete native roster')
    for c, r in zip(console, rows):
        require(int(c[0]) == int(c[2]) == r['number'] and int(c[1]) == len(rows) and c[3] == r['name']
                and c[4] == ('Passed' if r['result'] == 'passed' else '***Skipped'), 'Original console/JUnit/LastTest differs')
    commands = re.findall(r'^ctest --test-dir build[^\n]+$', clean, re.M)
    expected_prefix = 'ctest --test-dir build ' + ('-C Release ' if target == 'windows-x86_64' else '') + '--output-on-failure --no-tests=error --output-junit '
    require(len(commands) == 1 and commands[0].startswith(expected_prefix) and not any(x in commands[0] for x in ('||', ';', '&&')), 'Original CTest command differs or suppresses an exit')
    require(expected_prefix.rstrip() in source(repo, '.github/workflows/build.yml').decode().replace('\n          ', ' '), 'CTest command is absent from frozen workflow')
    totals = re.findall(r'^100% tests passed(?:, 0 tests failed)? out of (\d+)$', clean, re.M)
    durations = re.findall(r'^Total Test time \(real\) =\s*([\d.]+) sec$', clean, re.M)
    require(totals == [str(len(rows))] and len(durations) == 1, 'Original completion summary differs')
    provenance = dict(line.split('=', 1) for line in decode(members['ci-juce-provenance.txt']).splitlines() if '=' in line)
    for key, value in dict(workflow_commit=SOURCE, workflow_run=str(CONTEXT['run_id']), expected_sha='c352e2489000ffeb9e397051377d75eef2efe641', actual_sha='c352e2489000ffeb9e397051377d75eef2efe641', expected_version='8.0.11', actual_version='8.0.11', source_clean='true', verification='PASS').items():
        require(provenance.get(key) == value, 'Native JUCE/source provenance differs: ' + key)
    lookup = {r['name']: r for r in rows}
    for name in NEW:
        require(lookup[name]['result'] == 'passed' and re.search(r'\b0 failures\b', last[name]['output']), 'New .7 test did not pass: ' + name)
    ui = last['cs80_ui_smoke']['output']
    phases = ['instrument body host identity and recall', 'preset file host', 'session schema host', 'session undo host', 'engine I/O host']
    for phase in phases:
        begin = 'UI SMOKE PHASE: ' + phase
        timing = 'UI SMOKE TIMING: phase=' + phase + ' seconds='
        require(ui.count(begin + '\n') == 1 and ui.count(timing) == 1 and ui.index(begin) < ui.index(timing) < ui.index('UI SMOKE TEST PASSED'), 'Incomplete mapped UI phase: ' + phase)
    host = []
    for entry in coverage['entries']:
        joined = []
        for name in entry['ctest_names']:
            required = target in entry['required_on'][name]
            require(required == (name in expected), 'A42 platform registration map differs: ' + name)
            if required:
                require(lookup[name]['result'] == 'passed', 'Mapped host test did not pass: ' + name)
            joined.append(dict(name=name, registered=required, result=lookup[name]['result'] if required else None,
                               output_sha256=lookup[name]['output_sha256'] if required else None,
                               scope='Observed original CTest execution' if required else 'Not registered on this platform; no execution claim'))
        host.append(dict(id=entry['id'], behavior=entry['behavior'], ctests=joined, limit=entry['limit']))
    platform_dir = out_dir / target
    review = dict(provenance=CONTEXT, target=target, reviewed=True,
                  review_note='A53 independently parsed original JUnit, complete numbered LastTest and original producer console, reconciled frozen source predicates and default unittest method order, and preserved dynamic-reason uncertainty. Formal return-77 capability skips are separate from nested Python skips.',
                  formal_skips=formal, nested_python_skips=dict(total=sum(x['total'] for x in nested), suites=nested),
                  method_source_review=mappings,
                  limitations=['Formal capability skips are not passing SIMD/PRA executions.', 'Nonverbose dot patterns establish inferred method identity, not an emitted runtime reason.'])
    write_json(platform_dir / 'skip-review.json', review)
    manifest = dict(provenance=CONTEXT, native=[dict(target=target, archive=str(archive), api=str(api_path), skip_review=str((platform_dir/'skip-review.json').resolve()))])
    write_json(platform_dir / 'native-inputs.json', manifest)
    check = subprocess.run([sys.executable, '-B', str(repo/CHECKER), '--repo', str(repo), '--inputs', str(platform_dir/'native-inputs.json'), '--target', target, '--out', str(platform_dir/'a40-native-contract.json')], capture_output=True, text=True)
    (platform_dir/'a40-checker.log').write_text(check.stdout + check.stderr)
    require(check.returncode == 0, 'Frozen A40 checker rejected independent review: ' + check.stderr)
    frozen = json.loads((platform_dir/'a40-native-contract.json').read_text())['native'][0]
    require(frozen['counts'] == counts and frozen['formal_skips'] == formal and frozen['nested_python_skips'] == review['nested_python_skips'], 'Independent/A40 parse differs')
    write_json(platform_dir/'host-map.json', dict(provenance=CONTEXT, target=target, entries=host,
               scope='A42 source mapping joined only to native CTest cases actually registered/executed on this platform. Linux-only host-mirror and VST3 Program cases are not inferred.'))
    excerpts = {name: dict(command=last[name]['command'], output=last[name]['output'], lasttest_status=last[name]['lasttest_status'])
                for name in set(SKIPPABLE) | set(mappings) | set(NEW) | {'cs80_library_audit_tests'} |
                {n for e in coverage['entries'] for n in e['ctest_names'] if n in last}}
    write_json(platform_dir/'native-excerpts.json', excerpts)
    producer_warnings = [dict(line=i+1, text=line) for i, line in enumerate(clean.splitlines())
                         if re.search(r'warning|deprecated|optional|SKIP:', line, re.I)]
    write_json(platform_dir/'retained-warnings.json', dict(native=warnings, original_producer=producer_warnings,
               scope='All matched original native warning/report-only/optional/skip lines and producer warning/deprecation/optional lines are retained; no warning is upgraded to a pass or failure by this reconciliation.'))
    write_json(platform_dir/'native-roster.json', dict(provenance=CONTEXT, target=target, counts=counts, testcases=rows))
    input_files = [archive, api_path, job_path, log_path, Path(item['retrieval_receipt'])]
    result = dict(target=target, producer_job_id=job['id'], actual_native_evidence_inspected=True,
                  counts=counts, nested_python_skips=review['nested_python_skips']['total'],
                  formal_skip_names=[x['name'] for x in formal], exact_expected_name_set_verified=True,
                  complete_numbered_lasttest_verified=True, original_console_name_results_verified=True,
                  original_ctest_normal_exit=0,
                  original_exit_basis='Exact unsuppressed source-matched CTest command, successful original API Test step, complete original console matched to JUnit/LastTest and terminal completion; root retains authenticated acquisition.',
                  ctest_seconds=durations[0], artifact_id=api['id'], archive_sha256=digest(raw),
                  input_files=[record(p) for p in input_files],
                  members={name:dict(bytes=len(data), sha256=digest(data)) for name,data in members.items()},
                  mapped_registered_host_cases=sorted({n for e in coverage['entries'] for n in e['ctest_names'] if n in last}),
                  ui_body_phase_complete=True, all_four_new_tests_passed=True,
                  native_content_reconciliation_passed=True, platform_qualified=False, cross_platform_qualified=False, release_qualified=False)
    if target == 'macos-arm64':
        peer_path = repo/'docs/preset-evidence/2026.11.7/verification/a48/macos-arm64/skip-review.json'
        peer = json.loads(peer_path.read_text())
        require(peer['formal_skips'] == formal and peer['nested_python_skips'] == review['nested_python_skips'], 'A48 independent cross-check differs')
        result['a48_skip_review_agrees'] = record(peer_path)
    write_json(platform_dir/'reconciliation.json', result)
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo', type=Path, required=True)
    ap.add_argument('--inputs', type=Path, required=True)
    ap.add_argument('--out-dir', type=Path, required=True)
    args = ap.parse_args()
    repo, out = args.repo.resolve(), args.out_dir.resolve()
    require(not out.exists(), 'Use a fresh output directory')
    cfg = json.loads(args.inputs.read_text())
    require(cfg['provenance'] == CONTEXT, 'Manifest context differs')
    require(digest((repo/CHECKER).read_bytes()) == CHECKER_SHA, 'A40 checker changed')
    out.mkdir(parents=True)
    run = subprocess.run([sys.executable, '-B', str(repo/CHECKER), '--repo', str(repo), '--describe', '--out', str(out/'expected-native-contract.json')], capture_output=True, text=True)
    (out/'a40-describe.log').write_text(run.stdout+run.stderr)
    require(run.returncode == 0, 'A40 derivation failed')
    contract = json.loads((out/'expected-native-contract.json').read_text())
    require(contract['expected_registered'] == COUNTS and sum(COUNTS.values()) == 1422, 'Expected platform counts differ')
    coverage_path = repo/'docs/preset-evidence/2026.11.7/verification/a42/coverage-map.json'
    coverage = json.loads(coverage_path.read_text())
    require(coverage['source'] == SOURCE and coverage['tree'] == TREE and coverage['native_context'] == dict(run_id=45397189410, attempt=1), 'A42 map context differs')
    source_records = {}
    for entry in coverage['entries']:
        for src in entry['source']:
            raw = source(repo, src['path'])
            require(digest(raw) == src['sha256'] and src['anchor'] in raw.decode().splitlines()[src['line']-1], 'A42 frozen source anchor/hash differs: ' + src['path'])
            source_records[src['path']] = dict(bytes=len(raw), sha256=digest(raw))
    extra = ['CMakeLists.txt', '.github/workflows/build.yml', 'src/tools/ui_snapshot.cpp',
             'tests/FilterBatchCapTests.cpp', 'tests/StereoPraExactTests.cpp', 'src/core/Ig00156Batch.h', 'src/core/Ig00156Batch.cpp'] + list(SKIP_SOURCES.values())
    for path in extra:
        raw = source(repo,path)
        source_records[path] = dict(bytes=len(raw),sha256=digest(raw))
    write_json(out/'source-bindings.json', dict(provenance=CONTEXT, source_files=source_records,
               a40_checker=record(repo/CHECKER), a42_coverage_map=record(coverage_path), a42_source_anchors_match=True))
    actual = [parse_target(repo,out,item,contract,coverage) for item in cfg['native']]
    require(len(actual) == len({r['target'] for r in actual}), 'Duplicate supplied target')
    by_target = {r['target']:r for r in actual}
    matrix = []
    for target, expected in COUNTS.items():
        got = by_target.get(target)
        matrix.append(dict(target=target, producer_job_id=JOBS[target], expected_registered=expected,
                           exact_expected_names_sha256=digest(('\n'.join(contract['expected_names'][target])+'\n').encode()),
                           actual_native_evidence_inspected=got is not None, actual_counts=got['counts'] if got else None,
                           nested_python_skips=got['nested_python_skips'] if got else None,
                           status='original_native_reconciled' if got else 'pending_actual_artifact_review',
                           capability_scope='Only cs80_filter_batch_cap_tests and cs80_stereo_pra_exact_tests may formally skip; actual outcome is never inferred from this rule.'))
    summary = dict(schema=1,status='partial_original_native_reconciliation',provenance=CONTEXT,
                   platform_matrix=matrix, expected_global_registered=1422,
                   observed_native_totals={key:sum(r['counts'][key] for r in actual) for key in ('registered','passed','failed','formal_skipped','disabled','unknown')},
                   observed_nested_python_skips=sum(r['nested_python_skips'] for r in actual),
                   pending_expected_registered=sum(row['expected_registered'] for row in matrix if not row['actual_native_evidence_inspected']),
                   all_four_native_targets_checked=len(actual)==4, global_native_results_accepted=False,
                   cross_platform_qualified=False,release_qualified=False,
                   counting_scope='One CTest name execution per platform. Nested Python skips, internal assertions, duplicated JUnit/stdout text and unregistered platform cases are never added to CTest totals.',
                   limits=['This bounded reconciliation executes no product binary and cannot substitute for validators, packaging, auditions, public download or release gates.',
                           'A42 Linux-only real state-mirror host and VST3 Program-boundary results remain pending until original Linux evidence is reviewed.',
                           'Processing-queue-only wrapper Program parameter automation retains its documented controller dependency; native event-path tests cannot clear it.'])
    write_json(out/'reconciliation.json',summary)
    write_json(out/'input-manifest.json',cfg)
    files = [p for p in sorted(out.rglob('*')) if p.is_file()]
    write_json(out/'retained-files.json',dict(files=[record(p) for p in files],script=record(Path(__file__).resolve()),input_manifest=record(args.inputs.resolve())))
    print(json.dumps({k:summary[k] for k in ('status','observed_native_totals','observed_nested_python_skips','pending_expected_registered','all_four_native_targets_checked','release_qualified')}))


if __name__ == '__main__':
    try:
        main()
    except (ValueError,KeyError,OSError,ET.ParseError,zipfile.BadZipFile) as error:
        print('ERROR: '+str(error),file=sys.stderr)
        sys.exit(2)
