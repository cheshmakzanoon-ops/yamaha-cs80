#!/usr/bin/env python3
"""Bind the original failed Windows run; this can never qualify a release.

No product executable is loaded. The downloaded ZIP is an explicit input and
must match the original API and upload log before its XML is interpreted.
"""
import argparse
from collections import Counter
from datetime import datetime, timedelta
import hashlib
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET
import zipfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[5]
SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
PRIOR = 'c76ad288e02fbeca48f39652bf4fcdd99837f793'
RUN, ATTEMPT, PRODUCER, CONSUMER = 45366603800, 1, 138349817051, 138381888391
ARTIFACT = 14910589335
FAILED = 'cs80_preset_response_2026_11_6_comparison_tests'
LF_SHA = '0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611'
CRLF_SHA = '072f6692ec360248b8edf104452f2c59272be028495d3949b1e6523895311e59'
BASELINE = 'docs/preset-evidence/2026.11.5/baseline/inventory.json'


def need(ok, reason):
    if not ok:
        raise ValueError(reason)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def load(name):
    return json.loads((HERE / name).read_bytes())


def record(path):
    raw = path.read_bytes()
    return {'path': str(path.relative_to(REPO)), 'bytes': len(raw), 'sha256': sha(raw)}


def dt(value):
    return datetime.fromisoformat(value.replace('Z', '+00:00'))


def git_bytes(path, commit=SOURCE):
    return subprocess.check_output(['git', 'show', commit + ':' + path], cwd=REPO)


def main(archive):
    run, jobs, artifacts = load('run-api.json'), load('jobs-api.json'), load('artifacts-api.json')
    producer, consumer, api = load('job-api.json'), load('validator-job-api.json'), load('native-api.json')
    need(run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == ATTEMPT,
         'Original run identity')
    need(run['path'] == '.github/workflows/build.yml' and run['event'] == 'push', 'Workflow/event')
    need(run['repository']['full_name'] == run['head_repository']['full_name']
         == 'cheshmakzanoon-ops/yamaha-cs80', 'Repository identity')
    need(producer == next(j for j in jobs['jobs'] if j['id'] == PRODUCER), 'Producer selection')
    need(consumer == next(j for j in jobs['jobs'] if j['id'] == CONSUMER), 'Consumer selection')
    need(api == next(a for a in artifacts['artifacts'] if a['id'] == ARTIFACT), 'Artifact selection')
    for job in [producer, consumer]:
        need(job['run_id'] == RUN and job['head_sha'] == SOURCE and job['run_attempt'] == ATTEMPT
             and job['status'] == 'completed' and job['labels'] == ['windows-2022'], 'Original job context')
    need(producer['conclusion'] == 'failure' and consumer['conclusion'] == 'skipped'
         and not consumer.get('steps'), 'Failed producer/skipped consumer')
    steps = {s['name']: s for s in producer['steps']}
    for name in ['Run actions/checkout@v5', 'Configure', 'Verify configured JUCE source', 'Build',
                 'Upload test and JUCE source evidence']:
        need(steps[name]['conclusion'] == 'success', name)
    need(steps['Test']['conclusion'] == 'failure', 'Actual failed Test step')
    for name in ['Upload Windows VST3', 'Package Windows VST3 and Standalone', 'Upload Windows install bundle']:
        need(steps[name]['conclusion'] == 'skipped', 'Post-failure packaging gate: ' + name)
    raw = archive.read_bytes()
    need(len(raw) == 82356 and api['size_in_bytes'] == len(raw)
         and api['digest'] == 'sha256:' + sha(raw)
         and sha(raw) == '8a74d70710f3c4fbb27fa6ae9478225b155be1a97cabfc90d2837f1ba217ea00', 'Actual ZIP bytes')
    need(api['name'] == 'cs80-windows-test-evidence' and not api['expired']
         and api['workflow_run']['id'] == RUN and api['workflow_run']['head_sha'] == SOURCE,
         'Original artifact identity')
    original = (HERE / 'original-producer-job.log').read_bytes()
    timestamped = original.decode('utf-8-sig').splitlines()
    clean = [re.sub(r'\x1b\[[0-9;]*m', '', re.sub(r'^\d{4}-\d\d-\d\dT\S+Z ?', '', s)) for s in timestamped]
    text = '\n'.join(clean) + '\n'
    need('\n' + SOURCE + '\n' in text, 'Actual checkout SHA')
    need('ctest --test-dir build -C Release --output-on-failure --no-tests=error --output-junit ' in text,
         'Original unfiltered CTest command')
    need('99% tests passed, 1 tests failed out of 251' in text
         and text.count('##[error]Process completed with exit code 1.') == 1,
         'Original CTest summary and failed step exit')
    upload = f'Artifact {api["name"]} has been successfully uploaded! Final size is {len(raw)} bytes. Artifact ID is {ARTIFACT}'
    need(text.count(upload) == 1
         and text.count('SHA256 digest of uploaded artifact zip is ' + sha(raw)) == 1, 'Original ZIP upload identity')
    upload_line = next(s for s in timestamped if upload in s)
    upload_step = steps['Upload test and JUCE source evidence']
    need(dt(upload_step['started_at']) <= dt(upload_line.split(' ', 1)[0])
         < dt(upload_step['completed_at']) + timedelta(seconds=1), 'Upload step/log timestamp binding')
    need(dt(upload_step['started_at']) <= dt(api['created_at'])
         <= dt(upload_step['completed_at']) + timedelta(seconds=1), 'Artifact creation timestamp')
    names = ['ctest-results.xml', 'Testing/Temporary/LastTest.log', 'ci-juce-provenance.txt']
    members = []
    with zipfile.ZipFile(archive) as z:
        need(sorted(z.namelist()) == sorted(names), 'Exact native ZIP members')
        for name in names:
            data = z.read(name)
            path = HERE / 'native-members' / name
            need(path.read_bytes() == data, 'Extracted member bytes: ' + name)
            members.append(record(path))
    root = ET.fromstring((HERE / 'native-members/ctest-results.xml').read_bytes())
    cases = list(root.iter('testcase'))
    mapping = {c.attrib['name']: c for c in cases}
    need(len(cases) == len(mapping) == 251, 'Unique registered test names')
    counts = Counter('failed' if c.find('failure') is not None or c.find('error') is not None
                     else 'formal_skipped' if c.find('skipped') is not None
                     else 'passed' if c.attrib.get('status') == 'run' else 'unknown' for c in cases)
    need(counts == Counter({'passed':248, 'failed':1, 'formal_skipped':2}), 'Actual outcomes')
    need(root.attrib['tests'] == '251' and root.attrib['failures'] == '1'
         and root.attrib['disabled'] == '0' and root.attrib['skipped'] == '2', 'JUnit summary')
    failures = [c.attrib['name'] for c in cases if c.find('failure') is not None or c.find('error') is not None]
    need(failures == [FAILED], 'Sole actual failing test')
    skips = [{'name': c.attrib['name'], 'message': c.find('skipped').get('message'),
              'output': c.findtext('system-out', '')} for c in cases if c.find('skipped') is not None]
    need({s['name'] for s in skips} == {'cs80_filter_batch_cap_tests','cs80_stereo_pra_exact_tests'}
         and all(s['message'] == 'SKIP_RETURN_CODE=77' for s in skips), 'Formal skip identity')
    nested = []
    for c in cases:
        output = c.findtext('system-out', '')
        totals = [int(x) for x in re.findall(r'OK \(skipped=(\d+)\)', output)]
        verbose = len(re.findall(r'^test_.*\.\.\. skipped ', output, re.M))
        count = sum(totals) if totals else verbose
        if count:
            nested.append({'test': c.attrib['name'], 'count': count,
                           'count_basis': 'unittest summary' if totals else 'verbose individual skipped record'})
    need(sum(s['count'] for s in nested) == 11, 'Separate nested skip total')
    last = (HERE / 'native-members/Testing/Temporary/LastTest.log').read_text()
    last_names = re.findall(r'^\d+/251 Test: (\S+)$', last, re.M)
    need(len(last_names) == len(set(last_names)) == 251 and set(last_names) == set(mapping), 'LastTest complete matching roster')
    need(last.count('Test Failed.') == 1 and FAILED in last
         and LF_SHA in last and CRLF_SHA in last, 'LastTest actual failure')
    prior_path = REPO / 'docs/preset-evidence/2026.11.5/ci/source-c76ad28/windows/native-original/ctest-results.xml'
    prior_names = {c.attrib['name'] for c in ET.fromstring(prior_path.read_bytes()).iter('testcase')}
    delta = subprocess.check_output(['git','diff','--unified=0',PRIOR,SOURCE,'--','CMakeLists.txt'], cwd=REPO, text=True)
    need(not any(s.startswith('-') and not s.startswith('---') for s in delta.splitlines()), 'No registration removal')
    added = re.findall(r'^\+\s*add_test\(NAME\s+(\w+)', delta, re.M)
    need(len(prior_names) == 244 and len(added) == len(set(added)) == 7
         and set(mapping) == prior_names | set(added) and prior_names.isdisjoint(added), 'Exact 244+7 native roster')
    contract = load('source-contract.json')
    expected = contract['required_family_tests']
    need(FAILED in expected and all(mapping[n].find('failure') is None and mapping[n].find('skipped') is None
                                  and mapping[n].attrib['status'] == 'run' for n in expected if n != FAILED),
         'Other required family/material tests passed')
    for name in ['cs80_ui_smoke','cs80_parameter_state_index_tests','cs80_typed_value_unit_tests']:
        need(mapping[name].attrib['status'] == 'run' and mapping[name].find('failure') is None, 'Native host gate: ' + name)
    need('UI SMOKE PHASE: instrument body host identity and recall' in last
         and 'UI SMOKE TIMING: phase=instrument body host identity and recall' in last, 'Actual body host phase')
    fields = dict(line.split('=',1) for line in (HERE/'native-members/ci-juce-provenance.txt').read_text().splitlines() if '=' in line)
    for key, value in {'actual_sha':'c352e2489000ffeb9e397051377d75eef2efe641', 'actual_version':'8.0.11',
                       'source_clean':'true','verification':'PASS','workflow_commit':SOURCE,'workflow_run':str(RUN),'source_override':''}.items():
        need(fields[key] == value, 'JUCE field: ' + key)
    baseline = git_bytes(BASELINE)
    need(baseline.count(b'\n') == 1 and b'\r' not in baseline and sha(baseline) == LF_SHA, 'Pinned LF Git blob')
    changed = baseline.replace(b'\n',b'\r\n')
    need(sha(changed) == CRLF_SHA, 'Exact CRLF transform reproduces Windows digest')
    failed_output = mapping[FAILED].findtext('system-out','')
    need(LF_SHA in failed_output and CRLF_SHA in failed_output
         and 'test_cli_valid_report_and_failed_comparison_are_atomic' in failed_output, 'Failure location')
    outcome = {
        'schema':1,'status':'original_windows_native_failure_bound', 'platform_qualified':False,'release_qualified':False,
        'source_commit':SOURCE,'factory_revision':'2026.11.6','run_id':RUN,'run_attempt':ATTEMPT,
        'producer_job_id':PRODUCER,'validator_job_id':CONSUMER,'producer_conclusion':'failure','validator_conclusion':'skipped',
        'native_counts':{'registered':251,'passed':248,'failed':1,'formal_skipped':2,'disabled':0,'unknown':0},
        'failed_test':FAILED,'formal_skips':skips,'nested_python_skips':{'total':11,'suites':nested},
        'roster':{'prior_registered':244,'added':sorted(added),'removed':[],'prior_roster':record(prior_path)},
        'other_required_family_tests_passed':[n for n in expected if n != FAILED], 'expanded_body_host_phase_executed':True,
        'ctest_step_exit':1,'ctest_exit_scope':'GitHub step exit; PowerShell step reports 1. No exact direct ctest process status is inferred.',
        'failed_assertion':{'expected_lf_sha256':LF_SHA,'observed_windows_sha256':CRLF_SHA,
                            'git_blob_bytes':len(baseline),'crlf_variant_bytes':len(changed),
                            'cause':'Single LF-to-CRLF checkout translation exactly reproduces the failed byte authentication digest.'},
        'installation_artifact_available':False,'validator_invocations':0,'validation_retries':0,
        'artifact':{'id':ARTIFACT,'name':api['name'],'bytes':len(raw),'sha256':sha(raw),
                    'original_upload_timestamp':upload_line.split(' ',1)[0],'members':members},
        'juce':fields,'inputs':{n:record(HERE/n) for n in ['run-api.json','jobs-api.json','artifacts-api.json','job-api.json',
            'validator-job-api.json','native-api.json','source-contract.json','original-producer-job.log']},
        'verifier':record(Path(__file__)),
        'limits':['This source failed qualification; the remaining passing tests do not override the failed gate.',
                  'No Windows installation archive or validator result exists for this producer; none is accepted.',
                  'The failure is a fixture-byte contract error. No plugin runtime crash was observed in this run.',
                  'The run API snapshot was taken while other platforms remained in progress; no later all-run result is inferred.',
                  'Correction requires a new source commit and fresh native jobs; this record remains historical.']}
    (HERE/'failed-native-qualification.json').write_text(json.dumps(outcome,indent=2)+'\n')
    print(json.dumps({k:outcome[k] for k in ['status','native_counts','validator_invocations','platform_qualified','release_qualified']}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--archive', type=Path, required=True)
    args = parser.parse_args()
    main(args.archive)
