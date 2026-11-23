#!/usr/bin/env python3
"""Bind downloaded Linux evidence to one original successful producer job."""
from pathlib import Path
from datetime import datetime, timezone, timedelta
import hashlib
import json
import re

ROOT = Path('/workspace/scratch/dde4ad85ed5b')
HERE = ROOT / 'a53'
INPUT = ROOT / 'release-078c098/input'
SOURCE = '078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
RUN = 45246863788
JOB = 137945804596

def read(path):
    return json.loads(path.read_text())

def dt(value):
    return datetime.fromisoformat(value.replace('Z', '+00:00'))

def record(path):
    data = path.read_bytes()
    return {'path': str(path), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}

run, job = read(HERE / 'run.json'), read(HERE / 'job.json')
assert run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == 1
assert job['id'] == JOB and job['run_id'] == RUN and job['head_sha'] == SOURCE and job['run_attempt'] == 1
assert job['name'] == 'Linux / VST3 + Standalone'
assert job['labels'] == ['ubuntu-24.04']
assert job['status'] == 'completed' and job['conclusion'] == 'success'
steps = {item['name']: item for item in job['steps']}
required = ['Run actions/checkout@v5', 'Install build dependencies', 'Configure',
            'Verify configured JUCE source', 'Build', 'Test',
            'Capture curated preset auditions', 'Upload curated preset auditions',
            'Upload Linux VST3', 'Render demo and screenshots', 'Upload render artifacts',
            'Package Linux VST3 and Standalone', 'Upload Linux install bundle',
            'Upload test and JUCE source evidence']
for name in required:
    assert steps[name]['conclusion'] == 'success', name
log = (HERE / 'job.log').read_text()
lines = log.splitlines()
assert SOURCE in log, 'Exact source missing from original job log'
assert '100% tests passed, 0 tests failed out of' in log, 'Missing original CTest summary'
assert 'ctest --test-dir build --output-on-failure --no-tests=error' in log

def line_records(pattern):
    return [{'line': i + 1, 'text': line} for i, line in enumerate(lines) if re.search(pattern, line)]

artifacts = []
definitions = [
    ('linux-x86_64-install', 'cs80-linux-install-bundle', 'Upload Linux install bundle', True),
    ('linux-x86_64-test-evidence', 'cs80-linux-test-evidence', 'Upload test and JUCE source evidence', True),
    ('linux-x86_64-vst3-validator-input', 'cs80-linux-vst3', 'Upload Linux VST3', True),
    ('linux-x86_64-audition', 'cs80-curated-preset-audition-' + SOURCE, 'Upload curated preset auditions', False),
]
for stem, name, step, need_archive in definitions:
    api_path = INPUT / (stem + '-api.json')
    api = read(api_path)
    assert api['name'] == name and api['expired'] is False
    assert api['workflow_run']['id'] == RUN and api['workflow_run']['head_sha'] == SOURCE
    assert re.fullmatch(r'sha256:[0-9a-f]{64}', api['digest'])
    assert dt(api['expires_at']) > datetime.now(timezone.utc)
    start, end = dt(steps[step]['started_at']), dt(steps[step]['completed_at'])
    created = dt(api['created_at'])
    assert start - timedelta(seconds=1) <= created <= end + timedelta(seconds=1)
    finals = line_records(r'Artifact ' + re.escape(name) + r'\.zip successfully finalized\. Artifact ID ' + str(api['id']) + r'\b')
    summaries = line_records(r'Artifact ' + re.escape(name) + r' has been successfully uploaded! Final size is ' + str(api['size_in_bytes']) + r' bytes\. Artifact ID is ' + str(api['id']) + r'\b')
    assert len(finals) == 1 and len(summaries) == 1, 'Original upload identity missing or ambiguous'
    summary_index = summaries[0]['line'] - 1
    begin = max(i for i in range(summary_index) if '##[group]Run actions/upload-artifact@' in lines[i])
    region = list(enumerate(lines[begin:summary_index + 1], begin))
    digest_line = [{'line': i + 1, 'text': line, 'sha256': match[1]}
                   for i, line in region
                   if (match := re.search(r'SHA256 digest of uploaded artifact zip is ([0-9a-f]{64})', line))]
    assert len(digest_line) == 1 and 'sha256:' + digest_line[0]['sha256'] == api['digest'], 'Original upload digest mismatch or ambiguity'
    progress = [{'line': i + 1, 'text': line, 'bytes': int(match[1])}
                for i, line in region
                if (match := re.search(r'Uploaded bytes (\d+)', line))]
    values = [item['bytes'] for item in progress]
    assert values and values == sorted(set(values)), 'Missing or non-monotonic cumulative upload progress'
    assert all(0 < value <= api['size_in_bytes'] for value in values)
    assert values[-1] == api['size_in_bytes'], 'Final uploaded byte count differs from artifact API'
    assert progress[-1]['line'] < digest_line[0]['line'] < finals[0]['line'] < summaries[0]['line'], 'Upload evidence order mismatch'
    for evidence in finals + summaries + digest_line + progress:
        timestamp = evidence['text'].split(' ', 1)[0]
        assert start - timedelta(seconds=1) <= dt(timestamp) <= end + timedelta(seconds=1)
    identity = record(INPUT / (stem + '.zip')) if need_archive else None
    if identity:
        assert identity['bytes'] == api['size_in_bytes']
        assert api['digest'] == 'sha256:' + identity['sha256']
    artifacts.append({'api': api, 'download': identity, 'api_file': record(api_path),
                      'upload_step': steps[step], 'upload_finalization_lines': finals,
                      'upload_summary_lines': summaries, 'upload_digest_lines': digest_line,
                      'cumulative_upload_progress': progress, 'final_uploaded_byte_count_matches_api': True,
                      'producer_created_at_within_upload_step': True,
                      'download_qualification_scope': 'Actual archive bytes' if need_archive else 'API and original upload only; audition ZIP qualification belongs to A58'})

out = {'schema': 1, 'status': 'original_linux_producer_and_download_bytes_bound',
       'source': SOURCE, 'run_id': RUN, 'run_attempt': 1, 'native_job_id': JOB,
       'native_job_conclusion': job['conclusion'], 'run_conclusion_at_snapshot': run['conclusion'],
       'required_steps': [steps[name] for name in required], 'artifacts': artifacts,
       'ctest_summary_lines': line_records(r'100% tests passed, 0 tests failed out of|Total Test time \(real\)'),
       'source_checkout_lines': line_records(re.escape(SOURCE)),
       'original_job_log': record(HERE / 'job.log'),
       'run_api': record(HERE / 'run.json'), 'job_api': record(HERE / 'job.json'),
       'binding_script': record(Path(__file__)),
       'authentic_artifact_catalog': record(HERE / 'artifacts.json'),
       'artifact_api_record_origin': 'Unmodified single artifact objects from the authentic run artifact collection.',
       'scope': 'Linux installation and original CTest producer only. Validator lanes/payload identity and audition audio are reviewed separately.',
       'release_qualified': False}
output = HERE / 'producer-binding.json'
assert not output.exists(), 'Refusing stale output'
output.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps({'status': out['status'], 'artifact_ids': [item['api']['id'] for item in artifacts]}))
