#!/usr/bin/env python3
"""Offline evidence acceptance for one exact Linux job; never execute payloads."""
from __future__ import annotations
import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import io
import json
from pathlib import Path
import re
import stat
import subprocess
import zipfile

SOURCE = '9c0d2baef5643a983a0e7a378b6be033371248f4'
RUN = 45112404257
ATTEMPT = 1
REPO = 'cheshmakzanoon-ops/yamaha-cs80'
REPOSITORY_ID = 1436742091
NATIVE_JOB = 137487485114
STEP_CLOCK_TOLERANCE_SECONDS = 2
SOURCE_HASHES = {
    '.github/workflows/build.yml': '8b851dfadbec0127da6e618971c72b55967282ddf229806419d2ffeb77c4e220',
    'scripts/run-pluginval-linux.sh': '41aa495e5fe84cddfe37c2f415e65cff0db71e72c8d549b67c1bbf5e4c5af105',
    'scripts/run-pluginval-with-core.py': '929292993b4a9cc1241185676bd0e6977e5e608cf894691b390178ff820a7eea',
}
INSTALL_CHECKER_SHA = '4cced6aa065a41c2df45e4f9b5090814ac58dc959281ce6844de5c1bac2aca4f'
LANES = {
    'headless': ('pluginval-headless', 'cs80-headless', 'Validate VST3 headless', False, None),
    'gui': ('pluginval-gui', 'cs80-gui', 'Validate VST3 GUI under Xvfb', True, None),
    'state-regression': ('pluginval-gui-seed-regression', 'cs80-gui-seed-regression',
                         'Validate GUI state regression seed 0x2da72db', True, '0x2da72db'),
}
TIMESTAMP = r'(\d{4}-\d\d-\d\dT\S+Z)'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text())


def identity(path):
    data = Path(path).read_bytes()
    return {'path': str(path), 'bytes': len(data), 'sha256': sha(data)}


def decode(data):
    require(b'\0' not in data, 'NUL in expected UTF-8 Linux text')
    return data.decode('utf-8-sig').replace('\r\n', '\n')


def plain(text):
    text = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', text)
    return re.sub(r'(?m)^\d{4}-\d\d-\d\dT\S+Z[ \t]+', '', text)


def instant(value):
    parsed = datetime.fromisoformat(value.replace('Z', '+00:00'))
    require(parsed.tzinfo is not None, 'Timestamp has no timezone')
    return parsed


def safe_name(name):
    require(name and not name.startswith('/') and '\\' not in name and ':' not in name
            and all(p not in ('', '.', '..') for p in name.split('/'))
            and all(32 <= ord(c) != 127 for c in name), 'Unsafe artifact path')


def artifact_api(api, name):
    require(api['name'] == name and api['expired'] is False, 'Wrong or expired artifact: ' + name)
    origin = api['workflow_run']
    require(origin['id'] == RUN and origin['head_sha'] == SOURCE and origin['head_branch'] == 'main'
            and origin['repository_id'] == REPOSITORY_ID
            and origin['head_repository_id'] == REPOSITORY_ID, 'Wrong artifact source/run/repository')
    require(type(api['id']) is int and api['id'] > 0
            and type(api['size_in_bytes']) is int and 0 < api['size_in_bytes'] <= 512*1024*1024
            and re.fullmatch(r'sha256:[0-9a-f]{64}', api['digest']), 'Invalid artifact identity')


def archive_files(path, api, name):
    artifact_api(api, name)
    require(Path(path).stat().st_size == api['size_in_bytes'], 'Wrong archive byte count')
    raw = Path(path).read_bytes()
    require('sha256:' + sha(raw) == api['digest'], 'Archive differs from API SHA256')
    files, directories, seen = {}, set(), set()
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        infos = z.infolist()
        require(len(infos) <= 4096 and sum(i.file_size for i in infos) <= 512*1024*1024,
                'Archive exceeds reviewed bounds')
        for item in infos:
            member = item.filename[:-1] if item.is_dir() else item.filename
            safe_name(member)
            require(item.orig_filename == item.filename and member.casefold() not in seen,
                    'Duplicate, NUL or case-colliding archive member')
            seen.add(member.casefold())
            require(not item.flag_bits & 1
                    and stat.S_IFMT(item.external_attr >> 16) in (0, stat.S_IFREG, stat.S_IFDIR),
                    'Encrypted or unsupported ZIP member')
            if item.is_dir():
                require(item.file_size == 0, 'Directory has payload')
                directories.add(member)
            else:
                files[member] = z.read(item)  # Also verifies ZIP CRC.
    return files, directories, {'artifact_id': api['id'], 'bytes': len(raw), 'sha256': sha(raw)}


def terminations(text):
    prefix = 'Original validator termination: '
    pattern = (r'Original validator termination: PID=(\d+); raw returncode=(-?\d+); '
               r'kind=(exit|signal); (.+)')
    records = []
    for line in text.splitlines():
        if not line.startswith(prefix):
            continue
        match = re.fullmatch(pattern, line)
        require(match is not None, 'Malformed raw child termination record')
        pid, code, kind, details = match.groups()
        pid, code = int(pid), int(code)
        require(pid > 0, 'Invalid child PID')
        if code >= 0:
            require(kind == 'exit' and details == f'exit_code={code}', 'Inconsistent raw exit record')
        else:
            require(code == -11 and kind == 'signal'
                    and details == 'signal_number=11; signal_name=SIGSEGV',
                    'Unexpected or inconsistent raw signal record')
        records.append({'pid': pid, 'raw_returncode': code, 'kind': kind, 'details': details,
                        'normalized_exit': code if code >= 0 else 128-code})
    return records


def lane_seed(text, fixed):
    seeds = re.findall(r'^Random seed: (0x[0-9a-fA-F]+)$', text, re.M)
    require(len(seeds) == 1 and (fixed is None or seeds[0].lower() == fixed),
            'Missing, repeated or wrong validator seed')
    return seeds[0]


def check_lane(lane, files):
    """Parse a lane only. This is NOT a source/job/artifact acceptance entry point."""
    prefix, report, step, gui, fixed = LANES[lane]
    final_name, core_name, report_name = prefix+'.log', prefix+'-core.log', 'pluginval-reports/'+report
    require({final_name, core_name, report_name} <= files.keys(), 'Missing lane/report/core files: ' + lane)
    final, core = decode(files[final_name]), decode(files[core_name])
    records = terminations(core)
    require(1 <= len(records) <= 3, 'Missing or excessive internal attempts: ' + lane)
    require(core.splitlines().count('=== validator attempt: local core diagnostics ===') == len(records),
            'Core attempt headers and termination count differ')
    pid_exits = re.findall(r'^Original validator PID: (\d+); exit: (\d+)$', core, re.M)
    require(pid_exits == [(str(r['pid']), str(r['normalized_exit'])) for r in records],
            'Normalized and raw child histories differ')
    # Check actual child exit before the report: the historical failed GUI report
    # is empty, but its decisive failure is raw -11 AFTER console SUCCESS.
    require(records[-1]['raw_returncode'] == 0 and records[-1]['kind'] == 'exit',
            f'{lane}: final raw child exit {records[-1]["raw_returncode"]}; SUCCESS cannot override a crash')
    earlier = []
    for index, record in enumerate(records[:-1], 1):
        name = f'{prefix}-attempt{index}.log'
        require(name in files, 'Unpreserved earlier attempt: ' + name)
        text = decode(files[name])
        require(record['normalized_exit'] == 139 and record['raw_returncode'] in (-11, 139),
                'Ineligible retry status')
        require('SUCCESS' not in text and 'FAILED' not in text, 'Retry after a terminal marker')
        # Exact production awk spelling, including the colon on BOTH patterns.
        phases = re.findall(r'(Starting|Completed) tests in:', text)
        require('Starting' in phases and phases[-1] == 'Completed', 'Ineligible retry phase history')
        earlier.append({'path': name, 'bytes': len(files[name]), 'sha256': sha(files[name]),
                        'seed': lane_seed(text, fixed), 'original_termination': record,
                        'status': 'failed_prior_internal_attempt', 'retry_eligibility_only': True})
    names = {n for n in files if re.fullmatch(re.escape(prefix)+r'-attempt\d+\.log', n)}
    require(names == {x['path'] for x in earlier}, 'Unaccounted internal attempt logs')
    seed = lane_seed(final, fixed)
    require(final.splitlines().count('SUCCESS') == 1 and final.rstrip().endswith('\nSUCCESS')
            and 'FAILED' not in final
            and not re.search(r'Assertion failed:|SIGSEGV|received Abort|^FAILURE', final, re.M),
            'Final console is not a clean complete SUCCESS')
    require(re.findall(r'^Strictness level: (\d+)$', final, re.M) == ['10']
            and re.findall(r'^Num programs: (\d+)$', final, re.M) == ['523']
            and re.findall(r'^Num plugins found: (\d+)$', final, re.M) == ['1'],
            'Wrong strictness/program/plugin scope')
    first, separator, body = final.partition('\n')
    require(separator and first.startswith('Started validating: ')
            and first.endswith('/vst3/CS-80 Reference.vst3'), 'Wrong validation input path')
    require(decode(files[report_name]) == body, 'Final report differs from complete console body')
    if gui:
        for phase in ('Editor', 'Open editor whilst processing', 'Editor Automation'):
            require(final.splitlines().count('Completed tests in pluginval / '+phase) == 1,
                    'Missing or repeated GUI completion: ' + phase)
    else:
        require('Starting tests in: pluginval / Editor' not in final, 'Headless lane unexpectedly contains GUI phase')
    members = [{'path': n, 'bytes': len(files[n]), 'sha256': sha(files[n])}
               for n in (final_name, core_name, report_name)]
    return {'lane': lane, 'step': step, 'gui': gui, 'seed': seed, 'fixed_seed': fixed,
            'strictness': 10, 'program_count': 523, 'final_raw_exit': 0,
            'internal_attempt_count': len(records), 'internal_retry_count': len(records)-1,
            'original_terminations': records, 'preserved_failed_attempts': earlier, 'members': members}


def job_steps(job, required):
    steps = {s['name']: s for s in job['steps']}
    require(len(steps) == len(job['steps']), 'Duplicate job step names')
    for name in required:
        require(name in steps and steps[name]['status'] == 'completed'
                and steps[name]['conclusion'] == 'success', 'Required step did not pass: ' + name)
    return steps


def select_job(jobs, job_id, name):
    selected = [j for j in jobs if j['id'] == job_id]
    require(len(selected) == 1, 'Job selection is not unique')
    job = selected[0]
    require(job['name'] == name and job['run_id'] == RUN and job['head_sha'] == SOURCE
            and job['run_attempt'] == ATTEMPT and job['head_branch'] == 'main', 'Wrong job identity/attempt')
    require(job['status'] == 'completed' and job['conclusion'] == 'success', 'Required job is not successful')
    return job


def upload_binding(api, job, step, raw_log):
    pattern = (r'(?m)^'+TIMESTAMP+r'[ \t]+Artifact '+re.escape(api['name'])
               +r' has been successfully uploaded! Final size is (\d+) bytes\. Artifact ID is (\d+)$')
    hits = re.findall(pattern, raw_log)
    require(len(hits) == 1 and hits[0][1:] == (str(api['size_in_bytes']), str(api['id'])),
            'Producer log does not uniquely bind the exact artifact ID and size')
    logged, created = instant(hits[0][0]), instant(api['created_at'])
    begin, end = instant(step['started_at']), instant(step['completed_at'])
    tolerance = timedelta(seconds=STEP_CLOCK_TOLERANCE_SECONDS)
    require(instant(job['started_at']) <= created <= instant(job['completed_at']),
            'Artifact creation is outside selected producer job')
    require(begin-tolerance <= created <= end+tolerance
            and begin-tolerance <= logged <= end+tolerance,
            'Upload timestamp is outside reviewed step clock tolerance')
    return {'job_id': job['id'], 'run_attempt': job['run_attempt'], 'artifact_id': api['id'],
            'upload_step_number': step['number'], 'artifact_created_at': api['created_at'],
            'upload_step_started_at': step['started_at'], 'upload_step_completed_at': step['completed_at'],
            'upload_success_log_at': hits[0][0], 'artifact_minus_step_end_seconds': (created-end).total_seconds(),
            'artifact_minus_success_log_seconds': (created-logged).total_seconds(),
            'step_clock_tolerance_seconds': STEP_CLOCK_TOLERANCE_SECONDS,
            'basis': 'Exact artifact ID/name/size in this job log is decisive; times are supporting bounds. Attempt comes from raw job API, never a package manifest.'}


def download_binding(api, step, raw_log):
    pattern = (r'(?m)^'+TIMESTAMP+r'[ \t]+- '+re.escape(api['name'])
               +r' \(ID: (\d+), Size: (\d+), Expected Digest: (sha256:[0-9a-f]{64})\)$')
    hits = re.findall(pattern, raw_log)
    require(len(hits) == 1 and hits[0][1:] == (str(api['id']), str(api['size_in_bytes']), api['digest']),
            'Validator download does not bind the exact VST3 ID/size/digest')
    actual = re.findall(r'(?m)^'+TIMESTAMP+r'[ \t]+SHA256 digest of downloaded artifact is ([0-9a-f]{64})$', raw_log)
    require(len(actual) == 1 and actual[0][1] == api['digest'][7:], 'Downloaded VST3 digest differs')
    tolerance = timedelta(seconds=STEP_CLOCK_TOLERANCE_SECONDS)
    for when in (hits[0][0], actual[0][0]):
        require(instant(step['started_at'])-tolerance <= instant(when) <= instant(step['completed_at'])+tolerance,
                'Download identity is outside selected download step')
    require(plain(raw_log).splitlines().count('Artifact download completed successfully.') == 1,
            'Missing or ambiguous artifact download completion')
    return {'artifact_id': api['id'], 'listed_at': hits[0][0], 'digest_verified_at': actual[0][0],
            'sha256': api['digest'][7:], 'step_number': step['number']}


def compare_vst3(files, explicit_dirs, install):
    prefix = 'VST3/CS-80 Reference.vst3'
    entries = [i for i in install['additional_metadata']['inventory']
               if i['path'] == prefix or i['path'].startswith(prefix+'/')]
    inventory = {i['path']: i for i in entries}
    require(len(inventory) == len(entries) and inventory, 'Duplicate or absent install VST3 inventory')
    expected_files = {p: i for p, i in inventory.items() if i['type'] == 'file'}
    expected_dirs = {p for p, i in inventory.items() if i['type'] == 'directory'}
    require(expected_files and len(expected_files)+len(expected_dirs) == len(inventory), 'Unsupported install VST3 member type')
    for name in set(files) | explicit_dirs:
        require(name == 'CS-80 Reference.vst3' or name.startswith('CS-80 Reference.vst3/'),
                'Unknown extra outside VST3 bundle')
    actual_files = {'VST3/'+name: data for name, data in files.items()}
    actual_dirs = {'VST3/'+name for name in explicit_dirs}
    for name in set(actual_files) | set(actual_dirs):
        parent = name.rpartition('/')[0]
        while parent and parent != 'VST3':
            actual_dirs.add(parent)
            parent = parent.rpartition('/')[0]
    require(set(actual_files) == set(expected_files), 'VST3 file inventory differs')
    require(actual_dirs == expected_dirs, 'VST3 normalized directory inventory differs')
    matches = []
    for name, data in sorted(actual_files.items()):
        expected = expected_files[name]
        require(len(data) == expected['size'] and sha(data) == expected['sha256'], 'VST3 bytes differ: '+name)
        matches.append({'path': name, 'bytes': len(data), 'sha256': sha(data)})
    return {'status': 'complete_vst3_payload_matches_install', 'file_count': len(matches),
            'directory_count': len(actual_dirs), 'explicit_directory_count': len(explicit_dirs),
            'normalized_directories': sorted(actual_dirs), 'files': matches, 'mismatch_count': 0,
            'normalization': 'Add VST3/ prefix; derive every implicit parent directory. No sidecars or unknown extras accepted.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'run-json', 'jobs-json', 'validator-job-log', 'native-job-log',
                 'validator-archive', 'validator-api', 'vst3-archive', 'vst3-api', 'install-proof', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--validator-job-id', type=int, required=True)
    args = parser.parse_args()
    require(not args.out.exists(), 'Output already exists; never overwrite evidence')
    for path, expected in SOURCE_HASHES.items():
        data = subprocess.check_output(['git', '-C', str(args.source_root), 'show', SOURCE+':'+path])
        require(sha(data) == expected, 'Unreviewed workflow/wrapper/helper: '+path)
    run, jobs = read_json(args.run_json), read_json(args.jobs_json)
    require(run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == ATTEMPT
            and run['head_branch'] == 'main' and run['event'] == 'push' and run['name'] == 'build'
            and run['path'] == '.github/workflows/build.yml'
            and run['head_repository']['full_name'] == REPO, 'Wrong run/source/attempt/workflow')
    require(jobs['total_count'] == len(jobs['jobs'])
            and len({j['id'] for j in jobs['jobs']}) == len(jobs['jobs']), 'Incomplete or duplicate raw jobs collection')
    native = select_job(jobs['jobs'], NATIVE_JOB, 'Linux / VST3 + Standalone')
    validator = select_job(jobs['jobs'], args.validator_job_id, 'pluginval / Linux VST3 (strictness 10)')
    native_steps = job_steps(native, ['Configure', 'Verify configured JUCE source', 'Build', 'Test',
                                     'Upload Linux VST3', 'Package Linux VST3 and Standalone', 'Upload Linux install bundle'])
    steps = job_steps(validator, [lane[2] for lane in LANES.values()] + [
        'Download Linux VST3', 'Install pluginval runtime', 'Fetch pluginval 1.0.4',
        'Verify diagnostic exit propagation', 'Show first-attempt core diagnostics',
        'Select failed validator log for diagnostics', 'Build and test fault-thread diagnostic shim',
        'Upload pluginval reports'])
    for name in ('Diagnose validator failure with gdb', 'Diagnose validator with fault-thread trace', 'Report pluginval failure'):
        require(steps[name]['conclusion'] == 'skipped', 'Diagnostic execution cannot replace a gate: '+name)
    native_log, validator_log = decode(args.native_job_log.read_bytes()), decode(args.validator_job_log.read_bytes())
    for label, log in [('native', native_log), ('validator', validator_log)]:
        require(not re.search(r'^##\[error(?:\s|\])', plain(log), re.M), 'Error annotation in '+label+' job')
    api, vst_api, install = read_json(args.validator_api), read_json(args.vst3_api), read_json(args.install_proof)
    files, dirs, archive = archive_files(args.validator_archive, api, 'cs80-pluginval-reports')
    require(decode(files['pluginval-version.txt']) == 'pluginval - 1.0.4\n', 'Not stable pluginval 1.0.4')
    lanes = [check_lane(name, files) for name in LANES]
    expected = {'pluginval-version.txt', 'pluginval-core-fixture.log', 'pluginval-gdb-normal-exit.log'}
    for lane in lanes:
        expected.update(x['path'] for x in lane['members'])
        expected.update(x['path'] for x in lane['preserved_failed_attempts'])
    require(set(files) == expected and dirs <= {'pluginval-reports'}, 'Unknown/missing report members or diagnostic extras')
    raw_records = terminations(plain(validator_log))
    require(raw_records == [r for lane in lanes for r in lane['original_terminations']],
            'Complete raw job termination history differs from lane core logs')
    warnings = re.findall(r'retrying the full command \(next attempt ([23]) of 3\)\.', plain(validator_log))
    require(warnings == [str(n) for lane in lanes for n in range(2, lane['internal_attempt_count']+1)],
            'Wrapper retry messages and retained attempt history differ')
    require(install['status'] == 'artifact_bytes_verified' and install['source'] == SOURCE
            and install['run_id'] == RUN and install['run_attempt'] == ATTEMPT
            and install['target'] == 'linux-x86_64' and install['checker_sha256'] == INSTALL_CHECKER_SHA,
            'Wrong or unreviewed Linux installation proof')
    manifest = install['manifest_identity']
    require(manifest['commit'] == SOURCE and manifest['workflow_run_id'] == str(RUN)
            and manifest['platform'] == 'linux' and manifest['architecture'] == 'x86_64'
            and manifest['preset_library_revision'] == '2026.11.2', 'Wrong install manifest identity')
    install_api = install['artifact_api']
    artifact_api(install_api, 'cs80-linux-install-bundle')
    require(install['archive']['bytes'] == install_api['size_in_bytes']
            and 'sha256:'+install['archive']['sha256'] == install_api['digest']
            and install['archive']['api_digest_matches'] is True, 'Install proof/API archive mismatch')
    vst_files, vst_dirs, vst_archive = archive_files(args.vst3_archive, vst_api, 'cs80-linux-vst3')
    payload = compare_vst3(vst_files, vst_dirs, install)
    bindings = {
        'validator_reports_upload': upload_binding(api, validator, steps['Upload pluginval reports'], validator_log),
        'vst3_producer_upload': upload_binding(vst_api, native, native_steps['Upload Linux VST3'], native_log),
        'install_producer_upload': upload_binding(install_api, native, native_steps['Upload Linux install bundle'], native_log),
        'validator_vst3_download': download_binding(vst_api, steps['Download Linux VST3'], validator_log),
    }
    inputs = {name: identity(getattr(args, name)) for name in (
        'run_json', 'jobs_json', 'validator_job_log', 'native_job_log', 'validator_archive',
        'validator_api', 'vst3_archive', 'vst3_api', 'install_proof')}
    proof = {'schema': 1, 'status': 'linux_required_validators_and_payload_verified',
             'source': SOURCE, 'run_id': RUN, 'run_attempt': ATTEMPT, 'target': 'linux-x86_64',
             'verified_at_utc': datetime.now(timezone.utc).isoformat(),
             'checker_sha256': sha(Path(__file__).read_bytes()), 'frozen_source_hashes': SOURCE_HASHES,
             'native_job_id': native['id'], 'validator_job_id': validator['id'],
             'native_job_api': native, 'validator_job_api': validator, 'run_status_at_check': run['status'],
             'overall_run_acceptance': 'not assessed', 'validator_version': '1.0.4', 'lanes': lanes,
             'validation_invocations': sum(x['internal_attempt_count'] for x in lanes),
             'internal_wrapper_retries': sum(x['internal_retry_count'] for x in lanes),
             'diagnostic_validation_invocations': 0, 'artifact_bindings': bindings,
             'validator_artifact': archive, 'validator_artifact_api': api,
             'vst3_artifact': vst_archive, 'vst3_artifact_api': vst_api,
             'install_artifact_id': install_api['id'], 'complete_vst3_payload_identity': payload,
             'validator_members': [{'path': n, 'bytes': len(d), 'sha256': sha(d)} for n, d in sorted(files.items())],
             'inputs': inputs, 'limitations': [
                 'Offline supplied evidence only. No download, installation, payload execution, rerun or network operation.',
                 'Native CTest counts and auditions are accepted separately; completed native steps are prerequisites here.',
                 'GitHub attempt 1 does not imply zero internal wrapper retries; every original failure is retained above.',
                 'Stable Linux validator version is checked from the frozen workflow and emitted version file; this workflow does not retain a validator-download digest record.',
                 'Exact ID/name/size binds upload identity; the explicit two-second step clock tolerance handles API/log precision only.',
                 'No complete workflow, signing, DAW installation, acoustic equivalence or earlier-crash ownership claim.']}
    with args.out.open('x') as out:
        json.dump(proof, out, indent=2)
        out.write('\n')
    print(json.dumps({k: proof[k] for k in ('status', 'source', 'validator_job_id', 'validation_invocations', 'internal_wrapper_retries')}))


if __name__ == '__main__':
    main()
