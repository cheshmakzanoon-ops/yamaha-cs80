#!/usr/bin/env python3
"""Bind the downloaded Intel evidence to its original successful producer."""
from datetime import datetime, timedelta, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess

BASE = Path('/workspace/scratch/dde4ad85ed5b')
ROOT = BASE / 'release-078c098'
INPUT = ROOT / 'input'
OUT = ROOT / 'verified'
REPO = BASE / 'yamaha-cs80'
PREFIX = 'macos-x86_64'
SOURCE = '078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
RUN = 45246863788
JOB = 137945806008

def read(name):
    return json.loads((INPUT / name).read_text())

def record(path):
    raw = path.read_bytes()
    return {'path': str(path), 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}

def require(condition, message):
    if not condition:
        raise ValueError(message)

def utc(value):
    return datetime.fromisoformat(value.replace('Z', '+00:00'))

def fresh_json(path, value):
    with path.open('x') as stream:
        stream.write(json.dumps(value, indent=2) + '\n')

job = read(PREFIX + '-native-job-api.json')
run = read(PREFIX + '-native-run-api.json')
require(job['id'] == JOB and job['run_id'] == RUN and job['run_attempt'] == 1
        and job['head_sha'] == SOURCE, 'Producer job identity mismatch')
require(job['status'] == 'completed' and job['conclusion'] == 'success', 'Original producer did not succeed')
require(run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == 1, 'Run identity mismatch')
steps = {s['name']: s for s in job['steps']}
log_path = INPUT / (PREFIX + '-native-job.log')
log = log_path.read_text()
lines = log.splitlines()
require(SOURCE in log, 'Original producer log lacks checkout source')
checkout_rows = [(i, lines[i + 1]) for i, line in enumerate(lines[:-1]) if 'git log -1 --format=%H' in line]
require(len(checkout_rows) == 1 and checkout_rows[0][1].split(' ', 1)[1] == SOURCE,
        'Original checkout command did not report the expected source')
workflow_path = INPUT / (PREFIX + '-native-workflow.yml')
workflow = workflow_path.read_bytes()
require(workflow == subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':.github/workflows/build.yml']),
        'Saved workflow differs from exact source')
macos_block = workflow.decode().split('  macos:\n', 1)[1].split('\n  windows:', 1)[0]
test_block = macos_block.split('      - name: Test\n', 1)[1].split('      - name:', 1)[0]
require('ctest --test-dir build --output-on-failure --no-tests=error' in test_block
        and 'continue-on-error' not in test_block, 'Original CTest command propagation mismatch')
required = ['Configure', 'Verify configured JUCE source', 'Build', 'Test',
            'Package native macOS VST3, AU and Standalone', 'Upload native macOS install bundle',
            'Upload pluginval reports', 'Upload test and JUCE source evidence']
for name in required:
    require(steps[name]['status'] == 'completed' and steps[name]['conclusion'] == 'success', 'Required step failed: ' + name)

artifacts = []
for suffix, expected_name, step_name in [
    ('install', 'cs80-macos-15-intel-install-bundle', 'Upload native macOS install bundle'),
    ('test-evidence', 'cs80-macos-15-intel-test-evidence', 'Upload test and JUCE source evidence'),
    ('pluginval', 'cs80-pluginval-macos-15-intel', 'Upload pluginval reports'),
]:
    api_path = INPUT / (PREFIX + '-' + suffix + '-api.json')
    api = json.loads(api_path.read_text())
    archive = record(INPUT / (PREFIX + '-' + suffix + '.zip'))
    require(api['name'] == expected_name and api['expired'] is False, 'Wrong or expired artifact')
    require(utc(api['expires_at']) > datetime.now(timezone.utc), 'Artifact expiry elapsed')
    require(api['workflow_run']['id'] == RUN and api['workflow_run']['head_sha'] == SOURCE, 'Artifact source/run mismatch')
    require(api['size_in_bytes'] == archive['bytes'] and api['digest'] == 'sha256:' + archive['sha256'], 'API/archive byte mismatch')
    successes = [(i, re.search(r'Artifact (.+) has been successfully uploaded! Final size is (\d+) bytes\. Artifact ID is (\d+)', line))
                 for i, line in enumerate(lines)]
    successes = [(i, m) for i, m in successes if m and m[1] == expected_name]
    require(len(successes) == 1, 'Expected exactly one original upload: ' + expected_name)
    index, match = successes[0]
    begin = max(i for i in range(index) if '##[group]Run actions/upload-artifact@' in lines[i])
    region = list(enumerate(lines[begin:index + 1], begin))
    digests = [(i, re.search(r'SHA256 digest of uploaded artifact zip is ([0-9a-f]{64})', line)) for i, line in region]
    digests = [(i, m) for i, m in digests if m]
    byte_lines = [(i, re.search(r'Uploaded bytes (\d+)', line)) for i, line in region]
    byte_lines = [(i, m) for i, m in byte_lines if m]
    require(len(digests) == 1 and len(byte_lines) >= 1, 'Missing or ambiguous logged upload digest/bytes')
    digest_index, digest_match = digests[0]
    byte_index, byte_match = byte_lines[-1]
    progress_bytes = [int(m[1]) for _, m in byte_lines]
    require(progress_bytes == sorted(set(progress_bytes)) and all(0 < b <= archive['bytes'] for b in progress_bytes)
            and byte_index < digest_index < index, 'Invalid or out-of-order upload progress')
    require(int(match[2]) == archive['bytes'] and int(match[3]) == api['id']
            and digest_match[1] == archive['sha256'] and int(byte_match[1]) == archive['bytes'], 'Upload log/API/archive mismatch')
    step = steps[step_name]
    start, end = utc(step['started_at']), utc(step['completed_at']) + timedelta(seconds=1)
    timestamps = {'digest': lines[digest_index].split(' ', 1)[0], 'uploaded_bytes': lines[byte_index].split(' ', 1)[0],
                  'finalized': lines[index].split(' ', 1)[0], 'api_created_at': api['created_at']}
    progress = [{'bytes': int(m[1]), 'timestamp': lines[i].split(' ', 1)[0], 'line': i + 1} for i, m in byte_lines]
    require(all(start <= utc(t) < end for t in timestamps.values()), 'Upload timestamp outside original step')
    require(all(start <= utc(row['timestamp']) < end for row in progress), 'Upload progress outside original step')
    artifacts.append({'artifact_api': api, 'api_file': record(api_path), 'archive': archive,
        'api_size_digest_match': True, 'upload_step': step,
        'upload_log': {'name': expected_name, 'id': int(match[3]), 'bytes': int(match[2]), 'sha256': digest_match[1],
                       'timestamps': timestamps, 'progress': progress, 'digest_line': digest_index + 1, 'finalized_line': index + 1},
        'producer_attempt_and_upload_bound': True})

install_path = OUT / (PREFIX + '-install.json')
install = json.loads(install_path.read_text())
require(install['status'] == 'artifact_bytes_verified' and install['target'] == PREFIX
        and install['source'] == SOURCE and install['run_id'] == RUN
        and install['source_scripts_compared_to_commit'] is True, 'Installation verification identity mismatch')
native_path = OUT / (PREFIX + '-native-observations.json')
parsed = json.loads(native_path.read_text())
require(parsed['all_requested_observations_green'] is True and len(parsed['native']) == 1
        and len(parsed['validators']) == 0, 'Native observation mismatch')
native = parsed['native'][0]
require(native['target'] == PREFIX and native['status'] == 'observed_tests_green', 'Wrong native target/status')
provenance = dict(line.split('=', 1) for line in native['juce_provenance']['text'].splitlines() if '=' in line)
require(provenance['workflow_commit'] == SOURCE and provenance['workflow_run'] == str(RUN)
        and provenance['verification'] == 'PASS' and provenance['source_clean'] == 'true'
        and provenance['actual_sha'] == 'c352e2489000ffeb9e397051377d75eef2efe641'
        and provenance['actual_version'] == '8.0.11', 'Native JUCE source/run mismatch')
ctest_summary = re.findall(r'100% tests passed(?:, 0 tests failed)? out of (\d+)', log)
ctest_seconds = re.findall(r'Total Test time \(real\) =\s*([0-9.]+) sec', log)
require(ctest_summary == [str(native['counts']['registered'])] and len(ctest_seconds) == 1, 'Original CTest console/JUnit mismatch')
au = [b for b in install['binary_headers'] if b['path'].startswith('AU/')]
require(len(au) == 1, 'Expected one distributed AU executable')
au_record = {'schema': 1, 'source': SOURCE, 'run_id': RUN, 'run_attempt': 1, 'target': PREFIX,
    'producer_job_id': JOB, 'artifact': artifacts[0]['artifact_api'], 'executable': au[0],
    'installation_report': record(install_path),
    'scope': 'SHA256 of actual AU executable bytes read from original distributed installation archive; validator-input identity remains A56 work.'}
au_path = OUT / (PREFIX + '-distributed-au.json')
fresh_json(au_path, au_record)
binding = {'schema': 1, 'worker': 'a54', 'status': 'intel_macos_install_and_native_evidence_bound',
    'verified_at_utc': datetime.now(timezone.utc).isoformat(), 'release_qualified': False,
    'repository': 'cheshmakzanoon-ops/yamaha-cs80', 'source': SOURCE, 'run_id': RUN, 'run_attempt': 1,
    'target': PREFIX, 'producer_job_id': JOB, 'producer_job_url': job['html_url'], 'producer_job_conclusion': job['conclusion'],
    'source_binding': {'run_api_identity_matches': True, 'job_api_identity_matches': True, 'job_log_contains_checkout_source': True,
        'checkout_source_log_line': checkout_rows[0][0] + 2,
        'workflow_at_source': record(workflow_path),
        'workflow_git_blob': subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', SOURCE + ':.github/workflows/build.yml'], text=True).strip()},
    'original_successful_steps': [steps[n] for n in required], 'artifacts': artifacts,
    'native': {'status': 'original_test_step_success_and_artifact_observations_green', 'test_step': steps['Test'],
        'counts': native['counts'], 'formal_skips': native['formal_skips'], 'nested_python_skips': native['nested_python_skips'],
        'ctest_seconds_junit': native['ctest_seconds_junit'], 'ctest_console_total_seconds': float(ctest_seconds[0]),
        'original_ctest_numeric_exit_not_separately_printed': True,
        'exit_evidence': 'Original Test step completed/success; exact-source workflow invokes CTest directly without continue-on-error. No native test rerun.',
        'test_workflow_block': test_block, 'parser_report': record(native_path),
        'juce_provenance': native['juce_provenance'], 'juce_source_run_and_clean_verification_match': True},
    'installation': {'report': record(install_path), 'status': install['status'], 'inner_archive': install['inner_archive'],
        'inventory_entries': install['inventory_entries'], 'formats': install['formats'], 'binary_headers': install['binary_headers'],
        'source_scripts_compared_to_commit': True, 'signing_status': install['signing_status']},
    'distributed_au': record(au_path),
    'input_records': {key: record(INPUT / (PREFIX + suffix)) for key, suffix in [
        ('run_api', '-native-run-api.json'), ('job_api', '-native-job-api.json'), ('producer_job_log', '-native-job.log'),
        ('native_parser_inputs', '-native-inputs.json')]},
    'remaining_qualification': ['A56 must qualify original Mac validator lanes and installation payload identity.',
        'All native platform gates, all 13 validator lanes, and supporting evidence remain required for overall release qualification.'],
    'no_source_or_workflow_changes': True, 'no_external_mutations': True, 'no_ci_reruns': True, 'no_product_executed': True}
binding_path = OUT / (PREFIX + '-producer-binding.json')
fresh_json(binding_path, binding)
summary = {key: binding[key] for key in ['schema', 'worker', 'status', 'verified_at_utc', 'release_qualified', 'repository',
    'source', 'run_id', 'run_attempt', 'target', 'producer_job_id', 'producer_job_url', 'producer_job_conclusion']}
summary.update({'binding_report': record(binding_path), 'installation_report': record(install_path),
    'native_report': record(native_path), 'native_counts': native['counts'], 'formal_skips': native['formal_skips'],
    'nested_python_skips_total': native['nested_python_skips']['total'], 'distributed_au': au_record,
    'artifacts': [{'id': a['artifact_api']['id'], 'name': a['artifact_api']['name'], 'api_path': a['api_file']['path'],
                   'archive': a['archive'], 'producer_attempt_and_upload_bound': True} for a in artifacts],
    'remaining_qualification': binding['remaining_qualification'], 'review_path': str(BASE / 'a54-review.md')})
(BASE / 'a54-review.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps({'status': summary['status'], 'native_counts': native['counts'],
    'nested_python_skips_total': native['nested_python_skips']['total'], 'au_sha256': au[0]['sha256'],
    'binding_report': str(binding_path), 'release_qualified': False}))
