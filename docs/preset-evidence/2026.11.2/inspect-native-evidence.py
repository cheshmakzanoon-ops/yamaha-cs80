#!/usr/bin/env python3
"""Review native CTest evidence without declaring later validators successful.

Supply the full raw GitHub jobs collection from actions/runs/RUN/jobs?per_page=100,
not the normalized connector job wrapper. A later failed validator remains a
failed job and is retained in the native-only proof with its error annotations.
"""
import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET
import zipfile

TARGETS = {
    'macos-arm64': ('macOS / macos-15 / VST3 + AU + Standalone', 'cs80-macos-15-test-evidence'),
    'macos-x86_64': ('macOS / macos-15-intel / VST3 + AU + Standalone', 'cs80-macos-15-intel-test-evidence'),
    'windows-x86_64': ('Windows / VST3 + Standalone', 'cs80-windows-test-evidence'),
    'linux-x86_64': ('Linux / VST3 + Standalone', 'cs80-linux-test-evidence'),
}
p = argparse.ArgumentParser()
for name in ['archive', 'artifact-json', 'jobs-json', 'job-log', 'out']:
    p.add_argument('--' + name, type=Path, required=True)
p.add_argument('--source', required=True)
p.add_argument('--run-id', type=int, required=True)
p.add_argument('--run-attempt', type=int, default=1)
p.add_argument('--job-id', type=int, required=True)
p.add_argument('--target', choices=TARGETS, required=True)
a = p.parse_args()
SOURCE, RUN = a.source, a.run_id
assert re.fullmatch(r'[0-9a-f]{40}', SOURCE)
assert RUN > 0 and a.run_attempt > 0 and a.job_id > 0
assert not a.out.exists()
meta = json.loads(a.artifact_json.read_text())
jobs = json.loads(a.jobs_json.read_text())
assert type(jobs.get('total_count')) is int and jobs['total_count'] == len(jobs['jobs'])
matches = [j for j in jobs['jobs'] if j['id'] == a.job_id]
assert len(matches) == 1
job = matches[0]
assert meta['name'] == TARGETS[a.target][1] and not meta['expired']
assert meta['workflow_run']['head_sha'] == SOURCE and meta['workflow_run']['id'] == RUN
assert meta['workflow_run']['head_branch'] == 'main'
assert job['name'] == TARGETS[a.target][0] and job['run_id'] == RUN and job['head_sha'] == SOURCE
assert job['run_attempt'] == a.run_attempt and job['head_branch'] == 'main'
assert job['status'] == 'completed' and job['conclusion'] in (
    'success', 'failure', 'cancelled', 'timed_out', 'neutral', 'skipped', 'action_required', 'stale')
steps = {s['name']: s for s in job['steps']}
assert len(steps) == len(job['steps'])
for name in ['Configure', 'Verify configured JUCE source', 'Build', 'Test']:
    assert steps[name]['status'] == 'completed' and steps[name]['conclusion'] == 'success', name
raw = a.archive.read_bytes()
assert len(raw) == meta['size_in_bytes'] and 'sha256:' + hashlib.sha256(raw).hexdigest() == meta['digest']
log_raw = a.job_log.read_text()
log = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', log_raw)
log = re.sub(r'(?m)^\d{4}-\d\d-\d\dT\S+Z\s+', '', log)
error_annotations = [{'line': log[:m.start()].count('\n') + 1, 'message': m.group(0)}
                     for m in re.finditer(r'(?m)^##\[error\][^\r\n]*', log)]
with zipfile.ZipFile(a.archive) as z:
    infos = z.infolist()
    assert 0 < len(infos) <= 12 and len({i.filename for i in infos}) == len(infos)
    assert sum(i.file_size for i in infos) <= 64 * 1024 * 1024
    assert all(not i.filename.startswith('/') and '..' not in i.filename.split('/') for i in infos)
    contents = {i.filename: z.read(i) for i in infos if not i.is_dir()}
def one(basename):
    names = [n for n in contents if Path(n).name == basename]
    assert len(names) == 1, (basename, names)
    return contents[names[0]]
xml = one('ctest-results.xml')
assert b'<!DOCTYPE' not in xml and b'<!ENTITY' not in xml
root = ET.fromstring(xml)
cases = list(root.iter('testcase'))
assert root.tag == 'testsuite' and int(root.attrib['tests']) == len(cases)
assert int(root.get('failures', '0')) == int(root.get('errors', '0')) == int(root.get('disabled', '0')) == 0
assert len({c.get('name') for c in cases}) == len(cases)
rows, skips = [], []
for c in cases:
    assert c.find('failure') is None and c.find('error') is None
    skipped = c.find('skipped')
    if skipped is not None:
        assert c.get('status') in ['notrun', 'skipped']
        skips.append({'name': c.get('name'), 'status': c.get('status'), 'message': skipped.get('message', '')})
        result = 'formal_skip'
    else:
        assert c.get('status') == 'run', (c.get('name'), c.get('status'))
        result = 'passed'
    rows.append({'name': c.get('name'), 'result': result, 'seconds': float(c.get('time', '0'))})
assert len(skips) == int(root.get('skipped', '0'))
summary_matches = list(re.finditer(
    r'(?m)^100% tests passed(?:, 0 tests failed)? out of\s*(\d+)[ \t]*$', log))
assert [m[1] for m in summary_matches] == [str(len(cases))], [m[0] for m in summary_matches]
ctest_summary_line = log[:summary_matches[0].start()].count('\n') + 1
assert all(item['line'] > ctest_summary_line for item in error_annotations), 'Error annotation during native build/CTest'
numbered = re.findall(r'(?m)^\s*\d+/(\d+)\s+Test\s+#\d+:\s+(\S+)\s+\.+', log)
assert len(numbered) == len(cases) and {int(n) for n, _ in numbered} == {len(cases)}
assert sorted(n for _, n in numbered) == sorted(c.get('name') for c in cases)
last = one('LastTest.log').decode()
blocks = list(re.finditer(r'(?m)^(\d+)/(\d+) Testing: ([^\r\n]+)', last))
assert len(blocks) == len(cases) and sorted(b[3] for b in blocks) == sorted(c.get('name') for c in cases)
nested, focused = [], []
focus_names = {
    'cs80_library_audit_tests', 'cs80_preset_voicing_tests', 'cs80_preset_character_tests',
    'cs80_preset_precision_tests', 'cs80_circuit_policy_host_tests', 'cs80_circuit_source_host_tests',
    'cs80_preset_capture_tests', 'cs80_ui_smoke', 'cs80_panel_golden_tests',
}
for index, match in enumerate(blocks):
    block = last[match.end():blocks[index + 1].start() if index + 1 < len(blocks) else len(last)]
    name = match[3]
    tests = re.findall(r'(?m)^Ran (\d+) tests? in ', block)
    skip_counts = re.findall(r'(?m)^OK \(skipped=(\d+)\)', block)
    if skip_counts:
        assert len(tests) == len(skip_counts) == 1
        nested.append({'ctest_test': name, 'python_tests_run': int(tests[0]),
                       'python_skipped': int(skip_counts[0]),
                       'details': [line for line in block.splitlines() if '... skipped ' in line]})
    if name in focus_names:
        lines = [line for line in block.splitlines() if re.search(r'checks|regressions:|audit passed|UI SMOKE TEST|golden main-|^OK|^Ran ', line)]
        focused.append({'name': name, 'summary_lines': lines[-12:]})
provenance = {}
for line in one('ci-juce-provenance.txt').decode().splitlines():
    if '=' in line:
        k, v = line.split('=', 1)
        assert k not in provenance
        provenance[k] = v
assert provenance['workflow_commit'] == SOURCE and provenance['verification'] == 'PASS'
assert provenance['actual_sha'] == 'c352e2489000ffeb9e397051377d75eef2efe641'
assert provenance['actual_version'] == '8.0.11' and provenance['source_clean'] == 'true'
elapsed = re.findall(r'Total Test time \(real\) =\s*([0-9.]+) sec', log)
assert len(elapsed) == 1
report = {
    'schema': 2, 'source': SOURCE, 'run_id': RUN, 'run_attempt': a.run_attempt, 'target': a.target,
    'status': 'native_ctest_verified', 'reviewed_at_utc': datetime.now(timezone.utc).isoformat(),
    'job_api': job, 'artifact_api': meta,
    'job_conclusion': job['conclusion'], 'native_only': True,
    'platform_acceptance': 'not_assessed; requires separate validator and install-archive acceptance',
    'jobs_api': {'endpoint': f'https://api.github.com/repos/cheshmakzanoon-ops/yamaha-cs80/actions/runs/{RUN}/jobs?per_page=100',
                 'total_count': jobs['total_count'], 'sha256': hashlib.sha256(a.jobs_json.read_bytes()).hexdigest(),
                 'policy': 'Full raw jobs collection, selected by explicit job ID; normalized wrapper is not accepted.'},
    'job_error_annotations': error_annotations,
    'later_failed_steps': [{'name': s['name'], 'number': s['number'], 'status': s['status'], 'conclusion': s['conclusion']}
                           for s in job['steps'] if s['number'] > steps['Test']['number']
                           and s['conclusion'] not in ('success', 'skipped')],
    'counts': {'registered': len(cases), 'passed': len(cases) - len(skips), 'failed': 0,
               'formal_skipped': len(skips), 'disabled': 0},
    'ctest_exit': {'value': 0, 'basis': 'Successful unmasked Test step, matching complete console and JUnit results'},
    'ctest_console_summary': summary_matches[0][0],
    'zero_failures_basis': 'Successful completed Test step and complete JUnit with explicit zero failures/errors/disabled; console and LastTest names match.',
    'ctest_seconds': float(elapsed[0]), 'formal_skips': skips,
    'python_subtest_skips': {'total': sum(x['python_skipped'] for x in nested), 'suites': nested,
                             'scope': 'Actual LastTest unittest summaries; distinct from formal CTest skips. Nonverbose suites may omit individual skip reasons.'},
    'console_junit_lasttest_names_match': True, 'focused_tests': focused, 'cases': rows,
    'juce_provenance': provenance,
    'archive': {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()},
    'artifact_members': [{'name': n, 'bytes': len(b), 'sha256': hashlib.sha256(b).hexdigest()} for n, b in contents.items()],
    'job_log': {'bytes': a.job_log.stat().st_size, 'sha256': hashlib.sha256(a.job_log.read_bytes()).hexdigest()},
    'scope': 'Native build/CTest and exact configured JUCE provenance only. A failed overall job remains failed in job_conclusion and job_api; downstream error annotations and failed steps are retained. This proof never constitutes full platform acceptance or replaces separate validator-lane/install-archive checks.'
}
with a.out.open('x') as stream:
    json.dump(report, stream, indent=2)
    stream.write('\n')
print(json.dumps({k: report[k] for k in ['status', 'target', 'job_conclusion', 'native_only', 'counts', 'ctest_seconds', 'python_subtest_skips', 'later_failed_steps']}))
