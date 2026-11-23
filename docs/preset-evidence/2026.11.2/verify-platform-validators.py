#!/usr/bin/env python3
"""Read exact-source Windows/Intel Mac validator evidence; never run payloads."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone, timedelta
import hashlib
import json
from pathlib import Path
import re
import stat
import subprocess
import zipfile

SOURCE = '366c1454fbf3fa6898b9b79d78bde4322a6e3f87'
RUN = 45096447502
WORKFLOW_SHA = '8b851dfadbec0127da6e618971c72b55967282ddf229806419d2ffeb77c4e220'
MAC = [
 ('Validate VST3 with pluginval', 'pluginval-vst3.log', 'cs80-vst3', False, None),
 ('Validate AU with pluginval', 'pluginval-au.log', 'cs80-au', False, None),
 ('Validate AU with pluginval GUI', 'pluginval-au-gui.log', 'cs80-au-gui', True, None),
 ('Validate AU GUI fixed-seed regression', 'pluginval-au-seed-regression.log', 'cs80-au-seed-regression', True, '0x2b4eef5'),
]
WINDOWS = [
 ('Validate VST3 headless', 'pluginval-windows.log', 'cs80-windows', False, None),
 ('Validate state regression seed 0x5037a0c', 'pluginval-windows-seed-regression.log', 'cs80-windows-seed-regression', False, '0x5037a0c'),
]

def require(ok, message):
    if not ok: raise ValueError(message)

def digest(data): return hashlib.sha256(data).hexdigest()

def read_json(path): return json.loads(Path(path).read_text())

def safe_name(name):
    require(name and not name.startswith('/') and '\\' not in name and ':' not in name
            and all(p not in ('', '.', '..') for p in name.split('/'))
            and all(ord(c) >= 32 and ord(c) != 127 for c in name), 'Unsafe artifact path')

def checked_archive(path, api, expected_name):
    require(api['name'] == expected_name and api['expired'] is False, 'Wrong/expired artifact')
    origin = api['workflow_run']
    require(origin['id'] == RUN and origin['head_sha'] == SOURCE and origin['head_branch'] == 'main', 'Wrong artifact source/run')
    raw = Path(path).read_bytes()
    require(len(raw) == api['size_in_bytes'] and 'sha256:' + digest(raw) == api['digest'], 'Artifact size/hash mismatch')
    z = zipfile.ZipFile(path)
    infos = z.infolist(); seen = set()
    require(len(infos) <= 4096 and sum(i.file_size for i in infos) <= 512*1024*1024, 'Evidence archive too large')
    for i in infos:
        name = i.filename[:-1] if i.is_dir() else i.filename
        safe_name(name)
        require(i.orig_filename == i.filename and name.casefold() not in seen, 'Duplicate/NUL/case-colliding member')
        seen.add(name.casefold())
        require(stat.S_IFMT(i.external_attr >> 16) in (0, stat.S_IFREG, stat.S_IFDIR)
                and not i.flag_bits & 1, 'Unsupported artifact member')
        require(not i.is_dir() or i.file_size == 0, 'Directory has payload')
    return z, {'id': api['id'], 'name': api['name'], 'bytes': len(raw), 'sha256': digest(raw)}

def decode(data):
    encoding = 'utf-16' if data.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig'
    return data.decode(encoding).replace('\r\n', '\n'), encoding

def compare_vst3(path, api, proof_path, run_attempt):
    install = read_json(proof_path)
    require(install['status'] == 'artifact_bytes_verified' and install['source'] == SOURCE
            and install['run_id'] == RUN and install['run_attempt'] == run_attempt
            and install['target'] == 'windows-x86_64', 'Wrong installation proof')
    prefix = 'VST3/CS-80 Reference.vst3'
    inventory = {i['path']: i for i in install['additional_metadata']['inventory']
                 if i['path'] == prefix or i['path'].startswith(prefix + '/')}
    expected_files = {p: i for p, i in inventory.items() if i['type'] == 'file'}
    expected_dirs = {p for p, i in inventory.items() if i['type'] == 'directory'}
    require(expected_files and len(expected_files) + len(expected_dirs) == len(inventory), 'Unexpected VST3 inventory type')
    z, identity = checked_archive(path, api, 'cs80-windows-vst3')
    files, dirs, explicit_dirs, linker_sidecars = {}, set(), set(), []
    with z:
        for i in z.infolist():
            name = i.filename[:-1] if i.is_dir() else i.filename
            # Visual Studio also writes these link-time products to the
            # uploaded VST3 parent folder. They are outside the loadable bundle
            # and intentionally absent from the installation packager's bundle
            # inventory. Account for both explicitly; never skip bundle files.
            if name in ('CS-80 Reference.exp', 'CS-80 Reference.lib'):
                require(not i.is_dir(), 'Linker sidecar is not a regular file')
                data = z.read(i)
                if name.endswith('.lib'):
                    require(data.startswith(b'!<arch>\n'), 'Unexpected import-library format')
                    kind = 'COFF import library archive'
                else:
                    require(len(data) >= 20 and data[:2] == b'\x64\x86'
                            and data[16:18] == b'\0\0', 'Unexpected x64 COFF exports object')
                    kind = 'x64 COFF exports object'
                linker_sidecars.append({'path': name, 'bytes': len(data), 'sha256': digest(data), 'format': kind,
                                         'scope': 'Link-time sidecar outside the VST3 bundle; not installed or loaded by validator.'})
                continue
            require(name == 'CS-80 Reference.vst3' or name.startswith('CS-80 Reference.vst3/'), 'Wrong VST3 artifact root')
            canonical = 'VST3/' + name
            if i.is_dir(): explicit_dirs.add(canonical); dirs.add(canonical)
            else:
                data = z.read(i)
                files[canonical] = {'bytes': len(data), 'sha256': digest(data)}
            parent = canonical.rpartition('/')[0]
            while parent and parent != 'VST3':
                dirs.add(parent); parent = parent.rpartition('/')[0]
    require({x['path'] for x in linker_sidecars} == {'CS-80 Reference.exp', 'CS-80 Reference.lib'}, 'Missing expected Visual Studio linker sidecars')
    require(set(files) == set(expected_files), 'VST3 file membership differs from installation')
    require(dirs == expected_dirs, 'VST3 directory membership differs after implicit-parent normalization')
    matches = []
    for name, actual in sorted(files.items()):
        expected = expected_files[name]
        require(actual['bytes'] == expected['size'] and actual['sha256'] == expected['sha256'], 'VST3 payload differs: ' + name)
        matches.append({'path': name, **actual, 'matches_install_inventory': True})
    return {'status': 'byte_identical_to_install_vst3', 'artifact': identity,
            'install_artifact_id': install['artifact_api']['id'], 'file_count': len(files), 'directory_count': len(dirs),
            'explicit_input_directories': len(explicit_dirs), 'normalized_directories': sorted(dirs),
            'normalization': 'Add the VST3/ package prefix; derive implicit parent directories from every VST3 bundle file and include explicit directory entries. Every bundle file is compared; the two top-level Visual Studio linker sidecars are separately inventoried.',
            'mismatch_count': 0, 'files': matches, 'linker_sidecars': linker_sidecars,
            'all_artifact_files_accounted_for': len(files) + len(linker_sidecars)}

def preserved_prior_attempt(path, attempt, target):
    if attempt == 1:
        require(path is None, 'Initial-attempt proof cannot claim a prior-attempt record')
        return None
    require(path is not None, 'A retry requires the preserved original failure record')
    raw = path.read_bytes(); prior = json.loads(raw)
    require(prior['source'] == SOURCE and prior['run_id'] == RUN
            and prior['run_attempt'] == attempt - 1 and prior['target'] == target,
            'Previous-attempt record has wrong source/run/attempt/target')
    require(prior['status'] == 'platform_blocked' and prior['job_conclusion'] == 'failure'
            and prior['ctest_exit'] != 0 and prior['install_bundle_available'] is False,
            'Original native failure is not preserved')
    return {'record_path': str(path), 'record_bytes': len(raw), 'record_sha256': digest(raw),
            'source': prior['source'], 'run_id': prior['run_id'], 'run_attempt': prior['run_attempt'],
            'job_id': prior['job_id'], 'job_conclusion': prior['job_conclusion'],
            'ctest_exit': prior['ctest_exit'], 'ctest_seconds': prior['ctest_seconds'],
            'counts': prior['counts'],
            'failed_tests': [{k: test[k] for k in ('name', 'status', 'seconds', 'failure')}
                             for test in prior['failed_tests']],
            'validator_and_package_steps': prior['validator_and_package_steps'],
            'install_bundle_available': prior['install_bundle_available'],
            'native_artifact_id': prior['artifact_api']['id'],
            'scope': 'Preserved failed earlier native job. A later successful attempt cannot alter this result.'}


def bind_validator_upload(api, job, steps, log, upload_step_name='Upload pluginval reports'):
    upload = steps[upload_step_name]
    require(upload['status'] == 'completed' and upload['conclusion'] == 'success',
            'Artifact was not successfully uploaded by the selected job')
    pattern = (r'(?m)^(\d{4}-\d\d-\d\dT\S+Z)\s+Artifact '
               + re.escape(api['name'])
               + r' has been successfully uploaded! Final size is (\d+) bytes\. Artifact ID is (\d+)$')
    matches = re.findall(pattern, log)
    require(len(matches) == 1 and matches[0][1:] == (str(api['size_in_bytes']), str(api['id'])),
            'Selected job log does not uniquely identify this artifact ID and byte count')
    def instant(value): return datetime.fromisoformat(value.replace('Z', '+00:00'))
    created = instant(api['created_at']); success = instant(matches[0][0])
    job_start, job_end = instant(job['started_at']), instant(job['completed_at'])
    step_start, step_end = instant(upload['started_at']), instant(upload['completed_at'])
    # Actual accepted ARM uploads have second-resolution API creation times
    # one second after the reported step end. Keep exact upload identity and
    # the strict producer-job interval; allow only this bounded clock margin.
    tolerance_seconds = 2
    margin = timedelta(seconds=tolerance_seconds)
    require(job_start <= created <= job_end and job_start <= success <= job_end,
            'Artifact creation or upload-success log timestamp is outside the selected job')
    require(step_start - margin <= created <= step_end + margin,
            'Artifact API creation time exceeds the upload-step clock tolerance')
    require(step_start - margin <= success <= step_end + margin,
            'Upload-success log timestamp exceeds the upload-step clock tolerance')
    def outside_interval(value):
        if value < step_start: return round((value - step_start).total_seconds(), 6)
        if value > step_end: return round((value - step_end).total_seconds(), 6)
        return 0.0
    return {'job_id': job['id'], 'run_attempt': job['run_attempt'], 'artifact_id': api['id'],
            'artifact_created_at': api['created_at'], 'upload_success_log_timestamp': matches[0][0],
            'job_started_at': job['started_at'], 'job_completed_at': job['completed_at'],
            'upload_step_name': upload_step_name, 'upload_step_number': upload['number'],
            'upload_step_started_at': upload['started_at'], 'upload_step_completed_at': upload['completed_at'],
            'log_matches_artifact_id_and_bytes': True, 'timestamps_within_selected_job': True,
            'upload_step_timestamp_tolerance_seconds': tolerance_seconds,
            'api_creation_outside_upload_step_seconds': outside_interval(created),
            'success_log_outside_upload_step_seconds': outside_interval(success),
            'api_creation_minus_success_log_seconds': round((created - success).total_seconds(), 6),
            'timestamps_within_upload_step_tolerance': True,
            'basis': 'Artifact API has no run-attempt field. Bind the exact uploaded ID and size from this selected job log to the raw producer job API run_attempt. Both timestamps must be inside the job interval and within two seconds of the upload-step interval; original timestamps and measured offsets remain recorded.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for arg in ['source-root', 'jobs-json', 'run-json', 'job-log', 'archive', 'artifact-json', 'out']:
        p.add_argument('--'+arg, type=Path, required=True)
    p.add_argument('--target', choices=['windows-x86_64', 'macos-x86_64'], required=True)
    p.add_argument('--job-id', type=int, required=True)
    p.add_argument('--run-attempt', type=int, required=True)
    p.add_argument('--previous-attempt-record', type=Path)
    p.add_argument('--vst3-archive', type=Path)
    p.add_argument('--vst3-api', type=Path)
    p.add_argument('--install-proof', type=Path)
    a = p.parse_args()
    require(not a.out.exists(), 'Proof exists')
    require(a.run_attempt in (1, 2), 'Only reviewed attempt 1 or targeted retry attempt 2 is supported')
    prior = preserved_prior_attempt(a.previous_attempt_record, a.run_attempt, a.target)
    run, jobs, api = read_json(a.run_json), read_json(a.jobs_json), read_json(a.artifact_json)
    require(run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == a.run_attempt
            and run['head_branch'] == 'main' and run['event'] == 'push'
            and run['path'] == '.github/workflows/build.yml', 'Wrong source run')
    require(jobs['total_count'] == len(jobs['jobs']), 'Incomplete jobs collection')
    selected = [j for j in jobs['jobs'] if j['id'] == a.job_id]
    require(len(selected) == 1, 'Job not uniquely selected')
    job = selected[0]
    windows = a.target == 'windows-x86_64'
    expected_job = 'pluginval / Windows VST3 (strictness 10)' if windows else 'macOS / macos-15-intel / VST3 + AU + Standalone'
    require(job['name'] == expected_job and job['run_id'] == RUN and job['head_sha'] == SOURCE
            and job['run_attempt'] == a.run_attempt and job['head_branch'] == 'main', 'Wrong validator job identity')
    require(job['status'] == 'completed' and job['conclusion'] == 'success', 'Required validator job did not succeed')
    workflow = subprocess.check_output(['git', '-C', str(a.source_root), 'show', SOURCE + ':.github/workflows/build.yml'])
    require(digest(workflow) == WORKFLOW_SHA, 'Unreviewed workflow exit-enforcement contract')
    lanes = WINDOWS if windows else MAC
    steps = {s['name']: s for s in job['steps']}
    prerequisites = ['Download Windows VST3', 'Fetch pluginval 1.0.4'] if windows else ['Configure', 'Verify configured JUCE source', 'Build', 'Test', 'Fetch pluginval 1.0.4', 'Register built AU', 'Package native macOS VST3, AU and Standalone', 'Upload native macOS install bundle']
    for name in prerequisites + [v[0] for v in lanes]:
        require(steps[name]['status'] == 'completed' and steps[name]['conclusion'] == 'success', 'Required step failed: '+name)
    z, artifact = checked_archive(a.archive, api, 'cs80-pluginval-windows' if windows else 'cs80-pluginval-macos-15-intel')
    expected = {'pluginval-download.json'} | ({'pluginval-version.txt'} if windows else set())
    for _, log, report, _, _ in lanes: expected.update({log, 'pluginval-reports/'+report})
    outcomes = []
    with z:
        require({i.filename for i in z.infolist() if not i.is_dir()} == expected, 'Unexpected validator file inventory')
        download = json.loads(z.read('pluginval-download.json'))
        size, sha, asset = ((2408590, 'c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab', 210999198) if windows
                            else (9202795, '3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f', 210999197))
        require(download['verified'] is True and download['version'] == '1.0.4'
                and download['release_id'] == 188928656 and download['asset_id'] == asset
                and download['actual_size'] == download['expected_size'] == size
                and download['actual_sha256'] == download['expected_sha256'] == sha, 'Wrong validator archive pin')
        attempts = download['attempts']
        require(1 <= len(attempts) <= 3 and [x['number'] for x in attempts] == list(range(1, len(attempts)+1))
                and attempts[-1]['transfer_exit'] == 0 and all(x['transfer_exit'] != 0 for x in attempts[:-1]), 'Invalid transfer attempt record')
        for step, logfile, reportfile, gui, fixed in lanes:
            raw = z.read(logfile); text, encoding = decode(raw)
            seeds = re.findall(r'^Random seed: (0x[0-9a-fA-F]+)$', text, re.M)
            require(len(seeds) == 1 and (fixed is None or seeds[0].lower() == fixed), 'Missing/duplicate/wrong validator seed')
            require(re.findall(r'^SUCCESS$', text, re.M) == ['SUCCESS'], 'Validator console lacks exact sole SUCCESS')
            require('Strictness level: 10' in text and 'Num programs: 523' in text, 'Wrong validation scope')
            require(not re.search(r'Assertion failed:|received Abort|SIGSEGV|^FAILURE|^FAILED', text, re.M), 'Validator failure diagnostic')
            report_raw = z.read('pluginval-reports/'+reportfile); report_text, report_encoding = decode(report_raw)
            require(re.findall(r'^SUCCESS$', report_text, re.M) == ['SUCCESS'], 'Incomplete validator report')
            if gui:
                for phase in ['Editor', 'Open editor whilst processing', 'Editor Automation']:
                    require('Completed tests in pluginval / '+phase+'\n' in text, 'Incomplete GUI phase '+phase)
            outcomes.append({'step': step, 'seed': seeds[0], 'fixed_regression': fixed is not None,
                             'strictness': 10, 'gui': gui, 'program_count': 523, 'exit': 0,
                             'exit_basis': 'Required completed successful step and exact source workflow enforcing child exit, plus SUCCESS in both console and report.',
                             'validation_invocations': 1, 'validation_retries': 0,
                             'log': logfile, 'log_bytes': len(raw), 'log_sha256': digest(raw), 'log_encoding': encoding,
                             'report': 'pluginval-reports/'+reportfile, 'report_bytes': len(report_raw), 'report_sha256': digest(report_raw),
                             'explicit_skips': [line for line in text.splitlines() if re.search(r'skipp|validator.*not set', line, re.I)]})
        members = [{'path': i.filename, 'bytes': i.file_size, 'sha256': digest(z.read(i))} for i in z.infolist() if not i.is_dir()]
    log_raw = a.job_log.read_bytes(); log, _ = decode(log_raw)
    require(not re.search(r'(?m)^(?:\d{4}-\d\d-\d\dT\S+Z\s+)?##\[error\]', log), 'Error annotation in successful validator job log')
    upload_binding = bind_validator_upload(api, job, steps, log)
    identity = None
    if windows:
        require(a.vst3_archive and a.vst3_api and a.install_proof, 'Windows requires actual validator-input/package identity evidence')
        vst_api = read_json(a.vst3_api)
        identity = compare_vst3(a.vst3_archive, vst_api, a.install_proof, a.run_attempt)
        require(str(vst_api['id']) in log and 'cs80-windows-vst3' in log, 'Validator download log does not identify expected input artifact')
    proof = {'schema': 1, 'status': 'required_validators_verified', 'source': SOURCE, 'run_id': RUN,
             'run_attempt': a.run_attempt, 'job_id': job['id'], 'target': a.target, 'job_conclusion': job['conclusion'],
             'verified_at_utc': datetime.now(timezone.utc).isoformat(), 'job_api': job, 'artifact_api': api,
             'run_status_at_check': run['status'], 'run_snapshot_attempt': run['run_attempt'],
             'overall_run_acceptance': 'not assessed', 'preserved_previous_attempt': prior,
             'validator_artifact_attempt_binding': upload_binding,
             'workflow_sha256': WORKFLOW_SHA, 'validator_download': download,
             'validator_download_attempts': len(attempts), 'validator_download_retries_or_alternate_routes': len(attempts)-1,
             'validation_invocations': len(outcomes), 'validation_retries': 0,
             'invocation_scope': 'Selected job attempt only. Prior native-test failures remain recorded separately; a whole-job rerun is not a validator invocation retry.',
             'validators': outcomes,
             'validator_input_identity': identity, 'artifact': artifact, 'members': members,
             'job_log': {'bytes': len(log_raw), 'sha256': digest(log_raw)},
             'scope': 'Exact-source required validator steps and their original results only; transfer attempts are distinct from validator invocations. No failed invocation is replaced by a retry. Native CTest and install archive checks are separate. No full workflow, signing, all-DAW or acoustic/hardware-equivalence claim.'}
    with a.out.open('x') as f: json.dump(proof, f, indent=2); f.write('\n')
    print(json.dumps({k:proof[k] for k in ['status','target','job_id','validation_invocations','validation_retries','validator_download_attempts']}))

if __name__ == '__main__': main()
