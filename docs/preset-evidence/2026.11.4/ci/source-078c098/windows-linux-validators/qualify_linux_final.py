#!/usr/bin/env python3
"""Qualify original Linux evidence and merge the accepted Windows checkpoint.

No product, validator, diagnostic execution, external write, or CI rerun occurs.
The Windows qualification is retained byte-for-byte in its checkpoint snapshot.
"""
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
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate JSON key: ' + key)
        result[key] = value
    return result


def read_json(path):
    return json.loads(Path(path).read_text(), object_pairs_hook=unique_json)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def record(path):
    path = Path(path)
    raw = path.read_bytes()
    return {'file': str(path.relative_to(ROOT)), 'bytes': len(raw), 'sha256': sha(raw)}


def stamp(text):
    return datetime.fromisoformat(text.replace('Z', '+00:00'))


def decode(raw):
    return raw.decode('utf-8-sig').replace('\r\n', '\n')


def safe_path(name):
    require(name and not name.startswith('/') and '\\' not in name, 'Unsafe archive path')
    require(all(32 <= ord(c) != 127 for c in name), 'Control character in archive path')
    require(all(x not in ('', '.', '..') and ':' not in x for x in name.split('/')), 'Noncanonical archive path')
    require(str(PurePosixPath(name)) == name, 'Noncanonical archive path spelling')
    return name


def zip_inventory(path, allow_symlinks=False):
    raw = path.read_bytes()
    seen, rows = {}, []
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        members = archive.infolist()
        require(0 < len(members) <= 4096 and sum(x.file_size for x in members) <= 1024**3,
                'Archive count/expanded-size limit')
        for item in members:
            name = safe_path(item.filename[:-1] if item.is_dir() else item.filename)
            require(name not in seen, 'Duplicate canonical archive member')
            require(not item.flag_bits & 1 and item.compress_type in (0, 8), 'Encrypted/unsupported ZIP entry')
            require(0 <= item.file_size <= 256 * 1024**2, 'Oversized ZIP entry')
            mode = stat.S_IFMT(item.external_attr >> 16)
            kind = 'directory' if item.is_dir() else 'symlink' if mode == stat.S_IFLNK else 'file'
            require(mode in ((0, stat.S_IFDIR) if kind == 'directory' else
                             (stat.S_IFLNK,) if kind == 'symlink' and allow_symlinks else (0, stat.S_IFREG)),
                    'Unsupported ZIP object')
            data = archive.read(item)  # Read full member and verify its ZIP CRC.
            require(len(data) == item.file_size and (kind != 'directory' or not data), 'Invalid member byte count')
            row = {'path': item.filename, 'canonical_path': name, 'type': kind, 'bytes': len(data),
                   'sha256': sha(data), 'compression': item.compress_type,
                   'external_mode': oct(item.external_attr >> 16), 'crc32': f'{item.CRC:08x}'}
            if kind == 'symlink':
                require(len(data) <= 4096, 'Oversized symlink target')
                target = data.decode('utf-8')
                require(target and not target.startswith('/') and '\\' not in target
                        and all(32 <= ord(c) != 127 for c in target), 'Unsafe symlink target')
                row['symlink_target'] = target
            rows.append(row)
            seen[name] = kind
        for name in seen:
            for parent in PurePosixPath(name).parents:
                if str(parent) != '.':
                    require(seen.get(str(parent), 'directory') == 'directory', 'Non-directory ZIP parent')
    return {**record(path), 'members': rows, 'canonical_unique_supported_entries': True,
            'all_member_bytes_and_crc_read': True}


def clean_log(path):
    return re.sub(r'\x1b\[[0-9;]*m', '', decode(path.read_bytes()))


def log_rows(text):
    rows = []
    for line in text.splitlines():
        match = re.match(r'^(\d{4}-\d\d-\d\dT[^ ]+Z) (.*)$', line)
        rows.append((match[1], match[2]) if match else (None, line))
    return rows


def step(job, name, conclusion='success'):
    found = [x for x in job['steps'] if x['name'] == name]
    require(len(found) == 1 and found[0]['status'] == 'completed' and found[0]['conclusion'] == conclusion,
            'Original step outcome mismatch: ' + name)
    return found[0]


def contiguous_occurrences(haystack, needle):
    require(bool(needle), 'Empty log segment')
    return [i for i, line in enumerate(haystack) if line == needle[0] and haystack[i:i+len(needle)] == needle]


fresh = read_json(OUT / 'linux-final-api-bindings.json')
run = fresh['run']
require(run['id'] == RUN and run['run_attempt'] == ATTEMPT and run['head_sha'] == SOURCE
        and run['path'] == '.github/workflows/build.yml', 'Wrong run/source/attempt/workflow')
producer = read_json(INPUT / 'linux-x86_64-native-job-api.json')
consumer = read_json(INPUT / 'linux-x86_64-pluginval-job-api.json')
require(producer['id'] == 137945804596, 'Wrong original Linux producer')
require(consumer['name'] == 'pluginval / Linux VST3 (strictness 10)', 'Wrong original Linux validator job')
jobs = {x['id']: x for x in fresh['jobs']}
for job in (producer, consumer):
    require(job == jobs[job['id']], 'Saved job API differs from fresh collection')
    require(job['run_id'] == RUN and job['run_attempt'] == ATTEMPT and job['head_sha'] == SOURCE,
            'Job source/run/attempt differs')
    require(job['status'] == 'completed' and job['conclusion'] == 'success', 'Original job did not succeed')
    require(job['labels'] == ['ubuntu-24.04'], 'Unexpected original Linux runner label')
producer_log = clean_log(INPUT / 'linux-x86_64-native-job.log')
consumer_log = clean_log(INPUT / 'linux-x86_64-pluginval-job.log')
runtime = log_rows(consumer_log)
messages = [m for _, m in runtime]
for label, log in [('producer', producer_log), ('consumer', consumer_log)]:
    require(any(m == SOURCE for _, m in log_rows(log)), label + ': exact checkout SHA absent')
    require(not re.search(r'https?://\S+\?(?:\S*[&?])?(?:sig|token|X-Amz-Signature|se)=', log, re.I),
            label + ': signed URL must not be retained in new output')

source_records = []
source_paths = ['.github/workflows/build.yml', 'scripts/run-pluginval-linux.sh',
                'scripts/run-pluginval-with-core.py', 'scripts/verify-ci-bundle.py',
                'scripts/package-ci-artifacts.py',
                'docs/preset-evidence/2026.11.4/ci/verification-tools/verify-validator-payload.py']
for relative in source_paths:
    raw = subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':' + relative])
    require(raw == (REPO / relative).read_bytes(), 'Runtime/checker differs from exact source: ' + relative)
    source_records.append({'repository_path': relative, 'source': SOURCE, 'bytes': len(raw),
                           'sha256': sha(raw), 'current_file_exactly_matches_source': True})
workflow = (OUT / 'build.yml').read_text()
require(sha(workflow.encode()) == source_records[0]['sha256'], 'Saved workflow differs from exact source')
linux_workflow = workflow.split('\n  pluginval:\n', 1)[1].split('\n  macos:\n', 1)[0]
require('continue-on-error' not in linux_workflow and 'needs: linux' in linux_workflow,
        'Original validator gate has altered suppression/dependency')
gate_source = linux_workflow.split('      - name: Validate VST3 headless', 1)[1].split('      - name: Show first-attempt core diagnostics', 1)[0]
require(gate_source.count('--strictness-level 10') == 3 and gate_source.count('--validate-in-process') == 3
        and gate_source.count('--skip-gui-tests') == 1 and '--random-seed 0x2da72db' in gate_source,
        'Original Linux lane options differ')


def upload_binding(stem, name, job, log, upload_step):
    api = read_json(INPUT / (stem + '-api.json'))
    candidates = [x for x in fresh['artifacts'] if x['id'] == api['id']]
    require(len(candidates) == 1 and candidates[0] == api, 'Artifact API differs from fresh collection')
    require(type(api['id']) is int and api['id'] > 0 and api['name'] == name, 'Invalid artifact ID/name')
    archive = record(INPUT / (stem + '.zip'))
    require(api['size_in_bytes'] == archive['bytes'] and api['digest'] == 'sha256:' + archive['sha256'],
            'Artifact byte count/digest differs from API')
    require(api['expired'] is False and stamp(api['expires_at']) > NOW, 'Expired artifact')
    require(api['workflow_run']['id'] == RUN and api['workflow_run']['head_sha'] == SOURCE
            and api['workflow_run']['repository_id'] == 1436742091
            and api['workflow_run']['head_repository_id'] == 1436742091, 'Wrong artifact source/run/repository')
    bound_step = step(job, upload_step)
    expected = f"Artifact {name} has been successfully uploaded! Final size is {api['size_in_bytes']} bytes. Artifact ID is {api['id']}"
    matched = [(t, m) for t, m in log_rows(log) if m == expected]
    require(len(matched) == 1, 'Missing/ambiguous original upload identity')
    at = log.index(expected)
    start = log.rfind('##[group]Run actions/upload-artifact@v5', 0, at)
    require(start >= 0, 'Original upload action boundary missing')
    section = log[start:at+len(expected)]
    require('  name: ' + name in section and 'SHA256 digest of uploaded artifact zip is ' + archive['sha256'] in section,
            'Original upload digest/name mismatch')
    progress = [int(x) for x in re.findall(r'Uploaded bytes (\d+)', section)]
    require(progress and progress == sorted(set(progress)) and progress[-1] == archive['bytes'],
            'Original upload cumulative byte count mismatch')
    lower = stamp(bound_step['started_at']) - timedelta(seconds=1)
    upper = stamp(bound_step['completed_at']) + timedelta(seconds=1)
    for value in (matched[0][0], api['created_at'], api['updated_at']):
        require(lower <= stamp(value) < upper, 'Artifact timestamp outside original successful upload')
    return {'api': api, 'archive': archive, 'original_producer_job': job['id'], 'run_attempt': ATTEMPT,
            'original_upload_step': bound_step, 'logged_upload_completion_utc': matched[0][0],
            'cumulative_uploaded_bytes': progress, 'all_api_and_upload_bindings_passed': True}


bindings = [
    upload_binding('linux-x86_64-install', 'cs80-linux-install-bundle', producer, producer_log, 'Upload Linux install bundle'),
    upload_binding('linux-x86_64-vst3-validator-input', 'cs80-linux-vst3', producer, producer_log, 'Upload Linux VST3'),
    upload_binding('linux-x86_64-pluginval', 'cs80-pluginval-reports', consumer, consumer_log, 'Upload pluginval reports')]
raw_api = bindings[1]['api']
require(raw_api['id'] == 14869179841, 'Original Linux raw VST3 ID differs from producer handoff')
expected_download = f"- cs80-linux-vst3 (ID: {raw_api['id']}, Size: {raw_api['size_in_bytes']}, Expected Digest: {raw_api['digest']})"
require(sum(m == expected_download for m in messages) == 1
        and sum(m == 'SHA256 digest of downloaded artifact is ' + raw_api['digest'].split(':')[1] for m in messages) == 1,
        'Original consumer does not bind the exact raw VST3 artifact')
step(consumer, 'Download Linux VST3')
step(consumer, 'Fetch pluginval 1.0.4')
step(consumer, 'Verify diagnostic exit propagation')
step(consumer, 'Show first-attempt core diagnostics')
step(consumer, 'Select failed validator log for diagnostics')
step(consumer, 'Build and test fault-thread diagnostic shim')
diagnostic_steps = [step(consumer, name, 'skipped') for name in
                    ('Diagnose validator failure with gdb', 'Diagnose validator with fault-thread trace', 'Report pluginval failure')]

inventories = [zip_inventory(INPUT / (stem + '.zip'), allow_symlinks=stem.endswith('validator-input'))
               for stem in ('linux-x86_64-install', 'linux-x86_64-vst3-validator-input', 'linux-x86_64-pluginval')]
(OUT / 'linux-zip-inventories.json').write_text(json.dumps(inventories, indent=2) + '\n')
observations = read_json(OUT / 'linux-validator-observations.json')
require(observations['all_requested_observations_green'] and observations['release_qualified'] is False,
        'Linux parser observations fail or exceed scope')
observed = observations['validators'][0]
require(observed['target'] == 'linux-x86_64' and observed['required_lanes'] == 3, 'Wrong parsed Linux target/lanes')
lanes = []
with zipfile.ZipFile(INPUT / 'linux-x86_64-pluginval.zip') as archive:
    names = set(archive.namelist())
    allowed = {'pluginval-version.txt', 'pluginval-core-fixture.log', 'pluginval-gdb-normal-exit.log'}
    for lane in observed['lanes']:
        allowed.update((lane['console']['path'], lane['report']['path'], lane['core_log']['path']))
        allowed.update(x['member'] for x in lane['preserved_failed_attempts'])
    require({n.rstrip('/') for n in names if not n.endswith('/')} == allowed,
            'Unexpected or missing Linux validator report archive files')
    version_text = decode(archive.read('pluginval-version.txt')).strip()
    require(version_text == 'pluginval - 1.0.4' and sum(m == version_text for m in messages) == 1,
            'Original Linux version evidence differs')
    for lane in observed['lanes']:
        console_name = lane['console']['path']
        command_match = [i for i, m in enumerate(messages) if './scripts/run-pluginval-linux.sh ' + console_name + ' ' in m]
        require(len(command_match) == 1, 'Original lane wrapper command missing or ambiguous')
        begin = max(i for i in range(command_match[0]+1) if messages[i].startswith('##[group]Run '))
        end = next((i for i in range(command_match[0]+1, len(messages)) if messages[i].startswith('##[group]Run ')), len(messages))
        section = messages[begin:end]
        bound_step = step(consumer, lane['required_workflow_step'])
        console = decode(archive.read(console_name)).splitlines()
        report = decode(archive.read(lane['report']['path'])).splitlines()
        require(console[1:] == report, 'Final console/report disagree beyond selected-path header')
        require(console[0] == 'Started validating: /home/runner/work/yamaha-cs80/yamaha-cs80/vst3/CS-80 Reference.vst3',
                'Original lane selected an unexpected plugin path')
        attempts = list(lane['preserved_failed_attempts'])
        terms = lane['core_log']['original_terminations']
        require(len(terms) == len(attempts)+1 and len(terms) <= 3, 'Original termination/attempt count mismatch')
        require(terms[-1]['raw_returncode'] == 0 and terms[-1]['kind'] == 'exit', 'Final original child did not exit normally with zero')
        failed = []
        console_order = []
        for index, previous in enumerate(attempts):
            text = decode(archive.read(previous['member']))
            lines = text.splitlines()
            positions = contiguous_occurrences(section, lines)
            require(len(positions) == 1, 'Failed original attempt not preserved exactly in job output')
            console_order.append(positions[0])
            code = terms[index]['raw_returncode']
            normalized_exit = code if code >= 0 else 128-code
            require(normalized_exit == 139, 'Preserved retry was not original status 139')
            require('SUCCESS' not in text and 'FAILED' not in text, 'Retry included SUCCESS or FAILED')
            phase, started = None, False
            for line in lines:
                if 'Starting tests in:' in line:
                    started, phase = True, 'started'
                if 'Completed tests in:' in line:
                    phase = 'completed'
            require(started and phase == 'completed', 'Preserved original failure is not source retry eligible')
            failed.append({**previous, 'original_termination': terms[index], 'normalized_original_exit': normalized_exit,
                           'retry_eligible_under_exact_source': True, 'console_exactly_matches_original_runtime': True,
                           'does_not_establish_crash_ownership_or_full_suite_completion': True})
        positions = contiguous_occurrences(section, console)
        require(len(positions) == 1, 'Final original console does not exactly match original runtime')
        console_order.append(positions[0])
        require(console_order == sorted(set(console_order)), 'Original attempt ordering differs')
        runtime_starts = [m for m in section if m.startswith('Started validating: ')]
        require(len(runtime_starts) == len(terms), 'Original lane invocation count differs from preserved evidence')
        retry_warnings = [m for m in section if '::warning title=pluginval post-test SIGSEGV::Exit 139' in m]
        require(len(retry_warnings) == len(attempts), 'Retry warning count differs from preserved failures')
        core_text = decode(archive.read(lane['core_log']['path']))
        core_lines = core_text.splitlines()
        normalized_terms = [{'pid': int(pid), 'exit': int(code)} for pid, code in
                            re.findall(r'Original validator PID: (\d+); exit: (\d+)', core_text)]
        require(len(normalized_terms) == len(terms), 'Normalized/raw child termination counts differ')
        for normal, original in zip(normalized_terms, terms):
            code = original['raw_returncode']
            require(normal['pid'] == original['pid'] and normal['exit'] == (code if code >= 0 else 128-code),
                    'Normalized/raw original child termination differs')
            require(original['kind'] == ('exit' if code >= 0 else 'signal'), 'Original termination kind differs from raw status')
        require(len(contiguous_occurrences(messages, core_lines)) == 1, 'Core log not exactly present in original consumer output')
        final_start = begin + positions[0]
        final_end = final_start + len(console)-1
        require(stamp(bound_step['started_at']) <= stamp(runtime[final_start][0])
                and stamp(runtime[final_end][0]) < stamp(bound_step['completed_at']) + timedelta(seconds=1),
                'Final original runtime outside successful step')
        summary = lane['console']['summary']
        require(summary['strictness'] == [10] and summary['program_counts'] == [523]
                and summary['success_lines'] == 1 and summary['clean_text_success']
                and summary['editor_test_started'] == lane['gui_required'], 'Required final lane observations differ')
        editor_completed = 'Completed tests in pluginval / Editor' in console
        editor_automation_started = 'Starting tests in: pluginval / Editor Automation...' in console
        editor_automation_completed = 'Completed tests in pluginval / Editor Automation' in console
        require(editor_completed == lane['gui_required'] and editor_automation_started == lane['gui_required']
                and editor_automation_completed == lane['gui_required'], 'Original editor coverage differs from lane contract')
        all_warning_lines = [line for line in console if re.search('warning', line, re.I)]
        lanes.append({'lane': lane['lane'], 'seed': summary['seeds'][0], 'strictness': 10, 'programs': 523,
                      'original_workflow_step': bound_step, 'original_process_exit': 0,
                      'original_process_normal_exit': True, 'numeric_exit_printed_directly': True,
                      'original_terminations': terms, 'normalized_original_terminations': normalized_terms,
                      'final_original_termination': terms[-1],
                      'first_final_attempt_output_utc': runtime[final_start][0],
                      'terminal_success_utc': runtime[final_end][0],
                      'console_exactly_matches_original_runtime': True,
                      'report_equals_console_after_selected_path_header': True,
                      'core_log_exactly_matches_original_job_output': True,
                      'invocations': len(terms), 'validation_retries': len(attempts),
                      'preserved_failed_attempts': failed, 'retry_warnings': retry_warnings,
                      'editor_tests_started': summary['editor_test_started'],
                      'editor_tests_completed': editor_completed,
                      'editor_automation_tests_started': editor_automation_started,
                      'editor_automation_tests_completed': editor_automation_completed,
                      'warnings': summary['warnings'], 'all_runtime_warning_lines': all_warning_lines,
                      'explicit_skips': summary['explicit_skips'],
                      'console': lane['console'], 'report': lane['report'], 'core_log': lane['core_log']})
    # Preserve every actual report member after the entire ZIP has passed canonical/type checks.
    report_dir = OUT / 'linux-original-validator-files'
    for member in archive.infolist():
        target = report_dir / safe_path(member.filename.rstrip('/'))
        if member.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            data = archive.read(member)
            if target.exists():
                require(target.read_bytes() == data, 'Refusing changed original extracted evidence')
            else:
                target.write_bytes(data)

require(lanes[2]['seed'] == '0x2da72db', 'Linux fixed seed differs')
require(sum(m.startswith('Started validating: ') for m in messages) == sum(x['invocations'] for x in lanes),
        'Consumer contains an additional/unaccounted validator invocation')
require(not any(m.startswith('##[error]') or re.search(r'Process completed with exit code [1-9]', m) for m in messages),
        'Original consumer error marker present')

fetch_command = [i for i, m in enumerate(messages) if m.startswith('curl -L --fail --retry 3 --retry-delay 2')]
require(len(fetch_command) == 1, 'Original Linux download command differs')
fetch_begin = max(i for i in range(fetch_command[0]+1) if messages[i].startswith('##[group]Run '))
fetch_end = next(i for i in range(fetch_command[0]+1, len(messages)) if messages[i].startswith('##[group]Run '))
fetch_messages = messages[fetch_begin:fetch_end]
require(any('https://github.com/Tracktion/pluginval/releases/download/v${PLUGINVAL_VERSION}/pluginval_Linux.zip' in m
            for m in fetch_messages) and any(m.strip() == 'PLUGINVAL_VERSION: 1.0.4' for m in fetch_messages),
        'Original fetch URL/environment do not bind pluginval 1.0.4')
download_retries = [m for m in fetch_messages if re.search(r'\bWill retry in\b', m, re.I)]
require(len(download_retries) <= 3, 'Download retries exceed source policy')
download_evidence = {'version': '1.0.4', 'version_file_text': version_text,
                     'public_release_url_template': 'https://github.com/Tracktion/pluginval/releases/download/v${PLUGINVAL_VERSION}/pluginval_Linux.zip',
                     'source_environment_version': '1.0.4', 'original_fetch_step': step(consumer, 'Fetch pluginval 1.0.4'),
                     'curl_command_count': 1, 'observed_transfer_attempts': 1+len(download_retries),
                     'observed_transfer_retries': len(download_retries), 'retry_messages': download_retries,
                     'maximum_configured_transfer_retries': 3,
                     'asset_zip_sha256': None,
                     'asset_byte_identity_limit': 'The original Linux fetch does not upload or hash pluginval.zip. Version is established by the exact versioned source fetch and original executable version output, not a claim about a retained release-asset digest.',
                     'scope': 'Curl transfer retries, excluding HTTP redirects; separate from original validator retries.'}
payload = read_json(OUT / 'linux-vst3-payload.json')
require(payload['status'] == 'distributed_vst3_matches_validator_input' and payload['source'] == SOURCE
        and payload['run_id'] == RUN and payload['platform'] == 'linux', 'Linux full VST3 identity failed')
require(payload['installation_artifact']['artifact_id'] == bindings[0]['api']['id']
        and payload['validator_input_artifact']['artifact_id'] == raw_api['id'], 'Payload comparison used different artifacts')
require(not payload['validator_container_sidecars'], 'Unexpected Linux container exclusion')

linux_review = {'status': 'original_three_lanes_and_distributed_vst3_input_bound',
                'source': SOURCE, 'run_id': RUN, 'run_attempt': ATTEMPT, 'reviewed_at_utc': NOW.isoformat(),
                'producer_job': producer, 'consumer_job': consumer, 'artifact_bindings': bindings,
                'lanes': lanes, 'invocations': sum(x['invocations'] for x in lanes),
                'validation_retries': sum(x['validation_retries'] for x in lanes),
                'download_evidence': download_evidence, 'full_payload_identity': payload,
                'validator_warnings': [warning for lane in lanes for warning in lane['all_runtime_warning_lines']],
                'runner_action_warnings': [m for m in messages if 'DeprecationWarning:' in m or m.startswith('##[warning]')],
                'diagnostic_rerun_steps_skipped': diagnostic_steps,
                'diagnostic_fixture_logs_not_counted_as_validator_invocations': ['pluginval-core-fixture.log', 'pluginval-gdb-normal-exit.log'],
                'source_records': source_records, 'all_zip_entries_verified': record(OUT / 'linux-zip-inventories.json'),
                'original_consumer_log': record(INPUT / 'linux-x86_64-pluginval-job.log'),
                'original_producer_log': record(INPUT / 'linux-x86_64-native-job.log'),
                'fresh_api_bindings': record(OUT / 'linux-final-api-bindings.json'),
                'parser_observations': record(OUT / 'linux-validator-observations.json'),
                'parser_exit_code': 0, 'payload_helper_exit_code': 0,
                'qualification_script': record(OUT / 'qualify_linux_final.py'),
                'native_and_install_scope_owner': 'A53; original CTest case counts were not recomputed here',
                'release_qualified': False}
(OUT / 'linux-validator-binding.json').write_text(json.dumps(linux_review, indent=2) + '\n')
accepted = read_json(OUT / 'windows-only-a56-review.json')
require(accepted['status'] == 'partial_windows_verified_linux_pending'
        and accepted['verified_original_validator_lanes'] == 2 and accepted['verified_payload_bindings'] == 1,
        'Accepted Windows checkpoint differs from assigned handoff')
review = {**accepted, 'status': 'assigned_windows_linux_scope_verified',
          'complete_assigned_windows_linux_scope': True, 'release_qualified': False,
          'reviewed_at_utc': NOW.isoformat(), 'required_original_validator_lanes': 5,
          'verified_original_validator_lanes': 5, 'required_payload_bindings': 2, 'verified_payload_bindings': 2,
          'total_original_validator_invocations': accepted['windows']['invocations'] + linux_review['invocations'],
          'total_validation_retries': accepted['windows']['validation_retries'] + linux_review['validation_retries'],
          'linux': linux_review,
          'accepted_windows_checkpoint': {'json': record(OUT / 'windows-only-a56-review.json'),
                                          'markdown': record(OUT / 'windows-only-a56-review.md'),
                                          'windows_findings_retained_unchanged': True,
                                          'windows_qualification_not_rerun_on_resumption': True},
          'source_records': source_records,
          'completion_scope': 'Five original Windows/Linux pluginval lanes and both complete distributed-to-validator-input VST3 bindings only.',
          'continuation': 'Assigned worker56 scope is complete. Overall release, acoustic, signing, DAW, and other platform qualification are outside this review.'}
require(review['windows'] == accepted['windows'], 'Accepted Windows findings changed during merge')
(ROOT / 'a56-review.json').write_text(json.dumps(review, indent=2) + '\n')
print(json.dumps({'status': review['status'], 'release_qualified': False,
                  'verified_original_validator_lanes': 5, 'verified_payload_bindings': 2,
                  'total_original_validator_invocations': review['total_original_validator_invocations'],
                  'total_validation_retries': review['total_validation_retries'],
                  'linux_tree_sha256': payload['payload_tree_sha256'],
                  'original_linux_consumer_job': consumer['id']}))
