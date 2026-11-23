#!/usr/bin/env python3
"""Read-only evidence qualification; executes no product, test, or validator."""
from pathlib import Path, PurePosixPath
from datetime import datetime, timezone, timedelta
import hashlib
import io
import json
import re
import stat
import subprocess
import zipfile

ROOT = Path('/workspace/scratch/dde4ad85ed5b')
REPO = ROOT / 'yamaha-cs80'
OUT = ROOT / 'a56'
INPUT = ROOT / 'release-078c098/input'
SOURCE = '078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
RUN = 45246863788
ATTEMPT = 1
NOW = datetime.now(timezone.utc)


def require(ok, why):
    if not ok:
        raise ValueError(why)


def unique_json(pairs):
    data = {}
    for key, value in pairs:
        require(key not in data, 'Duplicate JSON key: ' + key)
        data[key] = value
    return data


def read_json(path):
    return json.loads(Path(path).read_text(), object_pairs_hook=unique_json)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def file_record(path):
    raw = Path(path).read_bytes()
    return {'file': str(Path(path).relative_to(ROOT)), 'bytes': len(raw), 'sha256': sha(raw)}


def stamp(text):
    return datetime.fromisoformat(text.replace('Z', '+00:00'))


def normalized(raw):
    return raw.decode('utf-8-sig').replace('\r\n', '\n')


def zip_inventory(raw, label):
    rows, seen, folded = [], {}, set()
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        members = archive.infolist()
        require(0 < len(members) <= 4096, label + ': invalid member count')
        require(sum(x.file_size for x in members) <= 1024**3, label + ': excessive expanded size')
        for item in members:
            name = item.filename[:-1] if item.is_dir() else item.filename
            require(name and not name.startswith('/') and '\\' not in name, 'Unsafe ZIP name')
            require(all(32 <= ord(c) != 127 for c in name), 'Control character in ZIP name')
            require(all(x not in ('', '.', '..') and ':' not in x for x in name.split('/')), 'Noncanonical ZIP name')
            require(str(PurePosixPath(name)) == name, 'Noncanonical ZIP path')
            require(name not in seen and name.casefold() not in folded, 'Duplicate/case-folding ZIP member')
            require(not item.flag_bits & 1 and item.compress_type in (0, 8), 'Encrypted/unsupported ZIP entry')
            require(0 <= item.file_size <= 256 * 1024**2, 'Excessive ZIP member size')
            mode = stat.S_IFMT(item.external_attr >> 16)
            kind = 'directory' if item.is_dir() else 'file'
            require(mode in (0, stat.S_IFDIR if item.is_dir() else stat.S_IFREG), 'Unsupported ZIP object type')
            data = archive.read(item)  # Reads all bytes and verifies each member CRC.
            require(len(data) == item.file_size, 'Incomplete ZIP member')
            require(not item.is_dir() or not data, 'Nonempty ZIP directory')
            seen[name] = kind
            folded.add(name.casefold())
            rows.append({'path': item.filename, 'canonical_path': name, 'type': kind,
                         'bytes': len(data), 'sha256': sha(data), 'compression': item.compress_type,
                         'external_mode': oct(item.external_attr >> 16), 'crc32': f'{item.CRC:08x}'})
        for name in seen:
            for parent in PurePosixPath(name).parents:
                if str(parent) != '.':
                    require(seen.get(str(parent), 'directory') == 'directory', 'File used as ZIP parent')
    return {'file': label, 'bytes': len(raw), 'sha256': sha(raw), 'members': rows,
            'canonical_unique_supported_entries': True, 'all_member_bytes_and_crc_read': True}


def clean_log(path):
    return re.sub(r'\x1b\[[0-9;]*m', '', normalized(path.read_bytes()))


def log_rows(text):
    rows = []
    for line in text.splitlines():
        match = re.match(r'^(\d{4}-\d\d-\d\dT[^ ]+Z) (.*)$', line)
        rows.append((match[1], match[2]) if match else (None, line))
    return rows


fresh = read_json(OUT / 'fresh-windows-api-bindings.json')
run = read_json(OUT / 'fresh-native-run-api.json')
require(run['id'] == RUN and run['run_attempt'] == ATTEMPT and run['head_sha'] == SOURCE,
        'Wrong original run/source/attempt')
require(run['path'] == '.github/workflows/build.yml' and run['event'] == 'push', 'Unexpected original workflow')
jobs = {x['id']: x for x in fresh['jobs']}
consumer = read_json(INPUT / 'windows-x86_64-pluginval-job-api.json')
producer = read_json(INPUT / 'windows-x86_64-native-job-api.json')
for job in (producer, consumer):
    require(job == jobs[job['id']], 'Saved job differs from freshly fetched job collection')
    require(job['run_id'] == RUN and job['run_attempt'] == ATTEMPT and job['head_sha'] == SOURCE, 'Wrong job provenance')
    require(job['status'] == 'completed' and job['conclusion'] == 'success', 'Original job not successful')

source_records = []
for relative in ('.github/workflows/build.yml', 'scripts/run-pluginval-linux.sh',
                 'scripts/run-pluginval-with-core.py', 'scripts/download-pluginval-windows.py',
                 'scripts/verify-ci-bundle.py', 'scripts/package-ci-artifacts.py',
                 'docs/preset-evidence/2026.11.4/ci/verification-tools/verify-validator-payload.py'):
    raw = subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':' + relative])
    require(raw == (REPO / relative).read_bytes(), 'Working source differs from pinned commit: ' + relative)
    source_records.append({'repository_path': relative, 'commit': SOURCE, 'bytes': len(raw),
                           'sha256': sha(raw), 'current_file_exactly_matches_source': True})
workflow = (OUT / 'build.yml').read_text()
windows_workflow = workflow.split('\n  pluginval-windows:\n', 1)[1]
linux_workflow = workflow.split('\n  pluginval:\n', 1)[1].split('\n  macos:\n', 1)[0]
require('continue-on-error' not in windows_workflow, 'Windows exit suppression in workflow')
require(windows_workflow.count('if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }') == 4,
        'Unexpected Windows fetch/validator exit propagation')
require(windows_workflow.count('--strictness-level 10') == 2 and windows_workflow.count('--skip-gui-tests') == 2,
        'Wrong Windows lane contract')
require(windows_workflow.count('--validate-in-process') == 2 and '--random-seed 0x5037a0c' in windows_workflow,
        'Wrong Windows process/seed contract')
require(windows_workflow.count("-Pattern '^SUCCESS$'") == 2, 'Missing explicit success gates')
native_log = clean_log(INPUT / 'windows-x86_64-native-job.log')
consumer_log = clean_log(INPUT / 'windows-x86_64-pluginval-job.log')
for label, text in [('producer', native_log), ('consumer', consumer_log)]:
    require(any(message == SOURCE for _, message in log_rows(text)), label + ': missing checked-out source SHA')
    require(not re.search(r'https?://\S+\?(?:\S*[&?])?(?:sig|token|X-Amz-Signature|se)=', text, re.I),
            label + ': signed URL must not be retained in new output')


def step(job, name):
    rows = [x for x in job['steps'] if x['name'] == name]
    require(len(rows) == 1 and rows[0]['status'] == 'completed' and rows[0]['conclusion'] == 'success',
            'Missing successful original step: ' + name)
    return rows[0]


def upload_binding(stem, artifact_id, name, job, joblog, upload_step):
    api = read_json(INPUT / (stem + '-api.json'))
    fresh_api = [x for x in fresh['artifacts'] if x['id'] == artifact_id]
    require(len(fresh_api) == 1 and api == fresh_api[0], 'Saved artifact API differs from current collection')
    raw = (INPUT / (stem + '.zip')).read_bytes()
    require(api['id'] == artifact_id and api['name'] == name, 'Wrong artifact ID/name')
    require(api['size_in_bytes'] == len(raw) and api['digest'] == 'sha256:' + sha(raw), 'Artifact byte identity mismatch')
    require(api['expired'] is False and stamp(api['expires_at']) > NOW, 'Expired artifact')
    require(api['workflow_run']['id'] == RUN and api['workflow_run']['head_sha'] == SOURCE, 'Artifact source/run mismatch')
    require(api['workflow_run']['repository_id'] == 1436742091 and api['workflow_run']['head_repository_id'] == 1436742091,
            'Wrong artifact repository')
    bound_step = step(job, upload_step)
    expected = f'Artifact {name} has been successfully uploaded! Final size is {len(raw)} bytes. Artifact ID is {artifact_id}'
    matches = [(t, m) for t, m in log_rows(joblog) if m == expected]
    require(len(matches) == 1, 'Missing unique authentic upload identity')
    upload_at = matches[0][0]
    offset = joblog.index(expected)
    section_start = joblog.rfind('##[group]Run actions/upload-artifact@v5', 0, offset)
    require(section_start >= 0, 'Upload action boundary absent')
    section = joblog[section_start:offset + len(expected)]
    require('  name: ' + name in section and 'SHA256 digest of uploaded artifact zip is ' + sha(raw) in section,
            'Upload name/digest not in original upload action')
    lower, upper = stamp(bound_step['started_at']), stamp(bound_step['completed_at']) + timedelta(seconds=1)
    require(lower <= stamp(upload_at) < upper, 'Upload log timestamp outside successful step')
    require(lower <= stamp(api['created_at']) < upper and lower <= stamp(api['updated_at']) < upper,
            'Artifact creation/update outside original upload step')
    return {'api': api, 'archive': file_record(INPUT / (stem + '.zip')), 'producer_job_id': job['id'],
            'run_attempt': job['run_attempt'], 'upload_step': bound_step,
            'logged_upload_completion_utc': upload_at, 'archive_matches_api': True,
            'fresh_api_matches_saved_api': True, 'original_upload_name_id_size_digest_timestamps_bound': True}


bindings = [
    upload_binding('windows-x86_64-install', 14856804857, 'cs80-windows-install-bundle', producer, native_log,
                   'Upload Windows install bundle'),
    upload_binding('windows-x86_64-vst3-validator-input', 14857530802, 'cs80-windows-vst3', producer, native_log,
                   'Upload Windows VST3'),
    upload_binding('windows-x86_64-pluginval', 14857949768, 'cs80-pluginval-windows', consumer, consumer_log,
                   'Upload pluginval reports')]
inventories = [zip_inventory((INPUT / (stem + '.zip')).read_bytes(), stem + '.zip') for stem in
               ('windows-x86_64-install', 'windows-x86_64-vst3-validator-input', 'windows-x86_64-pluginval')]
inner_name = 'cs80-reference-windows-x86_64-078c0981ffa1.zip'
with zipfile.ZipFile(INPUT / 'windows-x86_64-install.zip') as outer:
    inventories.append(zip_inventory(outer.read(inner_name), inner_name))
(OUT / 'windows-zip-inventories.json').write_text(json.dumps(inventories, indent=2) + '\n')

observations = read_json(OUT / 'windows-validator-observations.json')
require(observations['all_requested_observations_green'] and observations['release_qualified'] is False,
        'Parser observations failed or scope was broadened')
observed = observations['validators'][0]
require(observed['required_lanes'] == 2 and observed['validation_invocations'] == 2 and observed['validation_retries'] == 0,
        'Unexpected Windows invocation count')
runtime_rows = log_rows(consumer_log)
starts = [i for i, (_, m) in enumerate(runtime_rows) if m.startswith('Started validating: ')]
require(len(starts) == 2, 'Original Windows log does not contain exactly two validation starts')
lanes = []
with zipfile.ZipFile(INPUT / 'windows-x86_64-pluginval.zip') as archive:
    require(set(archive.namelist()) == {'pluginval-download.json', 'pluginval-version.txt',
            'pluginval-windows.log', 'pluginval-windows-seed-regression.log',
            'pluginval-reports/cs80-windows', 'pluginval-reports/cs80-windows-seed-regression'},
            'Unexpected Windows report archive members')
    download = json.loads(normalized(archive.read('pluginval-download.json')), object_pairs_hook=unique_json)
    require(download['verified'] is True and download['version'] == '1.0.4' and download['release_id'] == 188928656
            and download['asset_id'] == 210999198 and download['expected_size'] == download['actual_size'] == 2408590
            and download['expected_sha256'] == download['actual_sha256'] == 'c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab',
            'Validator download identity mismatch')
    require(len(download['attempts']) == 1 and download['attempts'][0]['number'] == 1
            and download['attempts'][0]['transfer_exit'] == 0 and download['attempts'][0]['reason'] == 'downloaded',
            'Unexpected download attempts')
    version_text = normalized(archive.read('pluginval-version.txt')).strip()
    require(version_text == 'pluginval - 1.0.4', 'Unexpected version text')
    require(any(m == version_text for _, m in runtime_rows), 'Version text absent from original consumer log')
    for index, lane in enumerate(observed['lanes']):
        console = normalized(archive.read(lane['console']['path'])).splitlines()
        report = normalized(archive.read(lane['report']['path'])).splitlines()
        require(console[1:] == report, 'Console/report differ beyond the single selected-path header')
        start = starts[index]
        end = start + len(console)
        require([m for _, m in runtime_rows[start:end]] == console, 'Artifact console differs from original job output')
        require(console[-1] == 'SUCCESS', 'Console lacks terminal success')
        following = runtime_rows[end][1]
        require(following.startswith('##[group]Run '), 'Unexpected post-SUCCESS runtime output')
        bound_step = step(consumer, lane['required_workflow_step'])
        require(stamp(bound_step['started_at']) <= stamp(runtime_rows[start][0])
                and stamp(runtime_rows[end-1][0]) < stamp(bound_step['completed_at']) + timedelta(seconds=1),
                'Validator runtime outside original workflow step')
        summary = lane['console']['summary']
        require(summary['strictness'] == [10] and summary['program_counts'] == [523]
                and summary['success_lines'] == 1 and summary['clean_text_success']
                and not summary['editor_test_started'] and not summary['warnings'] and not summary['failure_markers'],
                'Unexpected validation observation')
        lanes.append({'lane': lane['lane'], 'seed': summary['seeds'][0], 'strictness': 10,
                      'programs': 523, 'original_workflow_step': bound_step,
                      'first_output_utc': runtime_rows[start][0], 'terminal_success_utc': runtime_rows[end-1][0],
                      'original_process_exit': 0, 'original_process_normal_exit': True,
                      'numeric_exit_printed_directly': False,
                      'exit_evidence_basis': 'Original successful PowerShell step plus source-pinned immediate if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }. A nonzero exit after SUCCESS would fail this step; no continue-on-error is present.',
                      'console_exactly_matches_original_job_output': True,
                      'report_equals_console_after_selected_path_header': True,
                      'invocations': 1, 'validation_retries': 0, 'editor_tests_started': False,
                      'warnings': summary['warnings'], 'explicit_skips': summary['explicit_skips'],
                      'console': lane['console'], 'report': lane['report']})
require([x['seed'] for x in lanes] == ['0xdcb650', '0x5037a0c'], 'Unexpected Windows seeds')
require(not any(re.search(r'(^##\[error\]|Segmentation fault|0xC0000005|Process completed with exit code [1-9])', m)
                for _, m in runtime_rows), 'Original consumer runtime contains a failure marker')
step(consumer, 'Download Windows VST3')
step(consumer, 'Fetch pluginval 1.0.4')
payload = read_json(OUT / 'windows-vst3-payload.json')
require(payload['status'] == 'distributed_vst3_matches_validator_input'
        and payload['source'] == SOURCE and payload['run_id'] == RUN
        and payload['validator_container_contract']['consumer_log']['artifact_download_identity_matched'],
        'Full payload identity not established')
require(payload['payload_entries'] == 6 and payload['files'] == 2 and payload['symlinks'] == 0, 'Unexpected Windows VST3 tree')

linux_job = jobs[137945804596]
linux_contract = {
    'status': 'source_contract_prepared_original_linux_evidence_pending',
    'source': SOURCE, 'run_id': RUN, 'run_attempt': ATTEMPT,
    'latest_observation_utc': fresh['observed_at'], 'native_producer_job': linux_job,
    'required_original_lanes': [
        {'lane': 'headless', 'step': 'Validate VST3 headless', 'console': 'pluginval-headless.log',
         'report': 'pluginval-reports/cs80-headless', 'core_log': 'pluginval-headless-core.log',
         'gui_required': False, 'seed': 'randomized'},
        {'lane': 'gui', 'step': 'Validate VST3 GUI under Xvfb', 'console': 'pluginval-gui.log',
         'report': 'pluginval-reports/cs80-gui', 'core_log': 'pluginval-gui-core.log',
         'gui_required': True, 'seed': 'randomized'},
        {'lane': 'gui-seed', 'step': 'Validate GUI state regression seed 0x2da72db',
         'console': 'pluginval-gui-seed-regression.log', 'report': 'pluginval-reports/cs80-gui-seed-regression',
         'core_log': 'pluginval-gui-seed-regression-core.log', 'gui_required': True, 'seed': '0x2da72db'}],
    'all_required_strictness': 10, 'all_required_program_count': 523, 'required_pluginval_version': '1.0.4',
    'retry_policy': {'max_attempts_per_lane': 3, 'eligible_original_status': 139,
                     'must_have_started_test_phase_and_last_phase_completed': True,
                     'must_have_neither_success_nor_failed': True,
                     'preserve_each_failed_console_as_attemptN': True,
                     'final_original_raw_returncode': 0, 'final_original_termination_kind': 'exit',
                     'success_followed_by_crash_is_failure_and_not_retry_eligible': True,
                     'diagnostic_reruns_cannot_substitute': True,
                     'status_139_does_not_alone_prove_SIGSEGV_or_crash_ownership': True},
    'download_retries_separate': 'curl --retry 3 is a transfer policy; actual transfers must be read from original fetch log and kept separate from validator retries.',
    'pending': ['Original Linux producer artifacts and upload bindings', 'Original Linux validator consumer job and all three step outcomes',
                'Authentic validator report archive API/bytes/upload binding', 'All three original core termination records and any failed-attempt logs',
                'Full distributed VST3 tree equality against exact downloaded Linux validator input'],
    'source_records': source_records
}
(OUT / 'linux-source-contract.json').write_text(json.dumps(linux_contract, indent=2) + '\n')

review = {
    'schema': 1, 'worker': 56, 'requested_worker_count': 60,
    'status': 'partial_windows_verified_linux_pending', 'release_qualified': False,
    'complete_assigned_windows_linux_scope': False, 'source': SOURCE, 'run_id': RUN, 'run_attempt': ATTEMPT,
    'reviewed_at_utc': NOW.isoformat(), 'required_original_validator_lanes': 5,
    'verified_original_validator_lanes': 2, 'verified_payload_bindings': 1,
    'mutations_or_reexecutions': {'repository_source_changes': False, 'workflow_changes': False,
                                'CI_reruns': False, 'product_or_validator_execution': False,
                                'external_mutations': False, 'synthetic_product_evidence': False},
    'windows': {
        'status': 'original_two_lanes_and_distributed_vst3_input_bound',
        'producer_job': producer, 'consumer_job': consumer, 'artifact_bindings': bindings,
        'lanes': lanes, 'invocations': 2, 'validation_retries': 0,
        'pluginval_version': version_text, 'pluginval_download': download,
        'pluginval_download_attempts': 1, 'pluginval_download_retries': 0,
        'validator_warnings': [],
        'runner_action_warnings': [m for _, m in runtime_rows if 'DeprecationWarning:' in m],
        'failure_report_step_skipped': next(x for x in consumer['steps'] if x['name'] == 'Report pluginval failure'),
        'coverage_limits': ['Both Windows lanes are headless by source contract and contain no started Editor test.',
                            'Each lane explicitly skips the external VST3 validator because no validator path is configured.'],
        'full_payload_identity': payload,
        'console_report_counted_as_one_invocation_per_lane': True,
        'fresh_original_consumer_log_matches_supplied_bytes': True,
        'fresh_original_consumer_log_method': 'github_fetch_workflow_job_logs for job 137969986229; exact UTF-8 byte equality confirmed in tool transcript',
        'qualification_commands': [
            {'purpose': 'A45 observations only', 'exit_code': 0, 'report': file_record(OUT / 'windows-validator-observations.json')},
            {'purpose': 'Source-pinned full selected VST3 equality', 'exit_code': 0, 'report': file_record(OUT / 'windows-vst3-payload.json')}],
        'zip_inventories': file_record(OUT / 'windows-zip-inventories.json'),
        'installation_and_native_scope_owner': 'a52-review.json / a52-review.md; native CTest counts were not recomputed here'
    },
    'linux': {'status': linux_contract['status'], 'source_contract': file_record(OUT / 'linux-source-contract.json'),
              'latest_native_producer_state': {'id': linux_job['id'], 'status': linux_job['status'], 'conclusion': linux_job['conclusion']},
              'original_validator_job_available': False, 'verified_original_lanes': 0, 'verified_payload_bindings': 0,
              'pending': linux_contract['pending']},
    'source_records': source_records,
    'evidence_records': [file_record(INPUT / f) for f in (
        'windows-x86_64-pluginval-job.log', 'windows-x86_64-pluginval-job-api.json',
        'windows-x86_64-native-job.log', 'windows-x86_64-native-job-api.json')]
        + [file_record(OUT / f) for f in ('fresh-windows-api-bindings.json', 'fresh-native-run-api.json',
                                         'windows-validator-inputs.json', 'qualify_windows_checkpoint.py')],
    'scope_exclusions': ['Overall release qualification', 'macOS validator lanes', 'Acoustic equivalence', 'Signing/notarization',
                         'DAW hosting qualification', 'Independent external VST3 SDK validator coverage'],
    'continuation': 'Worker56 must be resumed after original Linux artifacts and validator job finish; this checkpoint does not satisfy the complete five-lane/two-payload assignment.'
}
(ROOT / 'a56-review.json').write_text(json.dumps(review, indent=2) + '\n')
print(json.dumps({'status': review['status'], 'release_qualified': False,
                  'verified_original_validator_lanes': 2, 'verified_payload_bindings': 1,
                  'windows_tree_sha256': payload['payload_tree_sha256'],
                  'original_consumer_job': consumer['id'], 'linux_pending': True}))
