from pathlib import Path
import json, re, hashlib

HERE = Path(__file__).resolve().parent
SOURCE = 'bcc86aa39a33a0162254614e5502705f0113d701'
RELEASE = '078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
EXPECTED = {'cs80_ig00156_batch_tests', 'cs80_filter_batch_engine_tests',
            'cs80_filter_batch_scheduling_tests', 'cs80_filter_batch_admission_tests'}
NAMES = {f'{compiler} / batch {mode}' for compiler in ('g++', 'clang++')
         for mode in ('DEFAULT', 'ON', 'OFF')}
api = json.loads((HERE / 'final-api.json').read_text())
run = api['run']
jobs = api['jobs']['jobs']
assert run['id'] == 45244531626 and run['head_sha'] == SOURCE
assert run['run_attempt'] == 1 and run['status'] == 'completed' and run['conclusion'] == 'success'
assert len(jobs) == 6 and {j['name'] for j in jobs} == NAMES
binding = json.loads((HERE / 'source-equivalence.json').read_text())
assert binding['run_source'] == SOURCE and binding['release_source'] == RELEASE
assert binding['all_identical'] and binding['input_count'] == 91
rows = []
for job in jobs:
    assert job['run_id'] == run['id'] and job['run_attempt'] == 1
    assert job['status'] == 'completed' and job['conclusion'] == 'success'
    gate_steps = [next(s for s in job['steps'] if s['name'] == n) for n in
                  ('Configure core only', 'Build focused qualification tests', 'Exactness and scheduling gates')]
    assert all(s['status'] == 'completed' and s['conclusion'] == 'success' for s in gate_steps)
    f = HERE / f"job-{job['id']}.log"
    raw = f.read_bytes()
    text = raw.decode()
    assert re.search(r'^.*Z ' + SOURCE + r'$', text, re.M)
    matches = re.findall(r'\d+/4 Test\s+#\d+: (cs80_\w+)\s+\.+\s+Passed\s+([0-9.]+) sec', text)
    assert len(matches) == 4 and {n for n, _ in matches} == EXPECTED
    assert text.count('100% tests passed, 0 tests failed out of 4') == 1
    configured = re.findall(r'^.*Z CS80_ENABLE_AVX2_FILTER_BATCH:BOOL=(ON|OFF)$', text, re.M)
    mode = job['name'].split()[-1]
    assert configured == ['OFF' if mode == 'OFF' else 'ON']
    rows.append({'job_id': job['id'], 'name': job['name'], 'attempt': job['run_attempt'],
                 'conclusion': job['conclusion'], 'html_url': job['html_url'],
                 'started_at': job['started_at'], 'completed_at': job['completed_at'],
                 'configured_mode': configured[0],
                 'compiler': re.findall(r'The CXX compiler identification is (.+)', text),
                 'gate_steps': gate_steps,
                 'observed_ctest_results': [{'name': n, 'status': 'passed', 'seconds': float(t)} for n, t in matches],
                 'ctest_summary': {'passed': 4, 'failed': 0, 'skipped': 0},
                 'log': f.name, 'log_bytes': len(raw), 'log_sha256': hashlib.sha256(raw).hexdigest()})
summary = {'reviewer': 'a46_filter_ci_binding', 'run_id': run['id'], 'run_attempt': 1,
           'run_url': run['html_url'], 'run_status': run['status'], 'run_conclusion': run['conclusion'],
           'run_source': SOURCE, 'release_source': RELEASE, 'event': run['event'],
           'workflow': run['path'], 'source_equivalence_file': 'source-equivalence.json',
           'source_equivalence_inputs': 91, 'all_relevant_inputs_byte_identical': True,
           'observed_executions': {'jobs': 6, 'passed_ctests': 24, 'failed_ctests': 0,
                                   'skipped_ctests': 0, 'unique_ctest_definitions': 4},
           'jobs': rows,
           'scope': ['Supporting filter evidence remains an execution of bcc86aa, not of release commit078c0981.',
                     'Clang ON and DEFAULT explicitly remain scalar; matrix is not six AVX2 executions.',
                     'No earlier failure or cancellation is reclassified; this record covers original attempt1 only.',
                     'No native plugin, artifact byte identity or acoustic realism conclusion follows from these four core gates.']}
(HERE / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps({'run': run['id'], 'attempt': 1, 'jobs': 6, 'passed': 24, 'failed': 0, 'skipped': 0}))
