#!/usr/bin/env python3
"""A48: join one retained ee0dabca macOS execution with its exact artifacts.

This engine accepts only a fixed reviewed Apple Silicon or Intel wrapper for
source ee0dabca and its original native run. Its authenticated inputs were
acquired by the repository owner in this session. Offline hashes preserve that
acquisition; they do not provide
independent authentication. No downloaded product or validator is executed.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import subprocess
import struct
import tarfile
import sys
import zipfile

sys.dont_write_bytecode = True
SOURCE = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
TREE = '17115913cbb29157bd00539485bcc5f38b57219c'
RUN, ATTEMPT, REPOSITORY_ID = 45397189410, 1, 1436742091
REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
REVISION = '2026.11.7'
JUCE = 'c352e2489000ffeb9e397051377d75eef2efe641'
WORKSPACE = '/Users/runner/work/yamaha-cs80/yamaha-cs80'
BASE = Path('docs/preset-evidence/2026.11.7')
COUNTS = dict(registered=277, passed=275, failed=0, formal_skipped=2, disabled=0, unknown=0)
STATIC_PINS = {
    'docs/preset-evidence/2026.11.5/verification/a41-verify-install-artifact.py': '803ff4378746937039400e777ff61bfce8ebaeb2ab1bba6699fe76076f520305',
    'docs/preset-evidence/2026.11.6/verification/a51/verify-macos-binding.py': 'f7050e482d8e3700d0053d459bddb21463a1d475ff720c42b8f348dd39ca66c9',
    'docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py': 'f192bf2ff70d4501f73875eb5bb6416b9adc3b77912e165da4305302724cd846',
    'docs/preset-evidence/2026.11.6/verification/a52/bind-parser-archives.py': '343cbc293d388fcf72484e05eaf52933e1bdbed3e13f194e59799af2cdf58b11',
    str(BASE / 'verification/a40/source-ee0dabca/verify-native-contract.py'): 'cb9597061fe16b590f85cfc75ddbc98e5868c80d1a9d6763f77edfc71b27e32f',
    str(BASE / 'verification/a48/inspect-packaged-docs.py'): '8ac1fb82206e92131eed0523dd569ed7b97460b8158d9951e056c105e2f7723d',
}
TARGET_CONTEXT = {
    'macos-arm64': ('arm64', 'macos-15', 138455976227, 0x0100000c),
    'macos-x86_64': ('x86_64', 'macos-15-intel', 138455976307, 0x01000007),
}


def configure(fixed):
    global FIXED, TARGET, ARCH, LABEL, JOB, CPU, CI, EVIDENCE, PINS, ARTIFACTS
    require(fixed.get('reviewed_actual_inputs') is True, 'Prepared only; actual inputs have not been reviewed')
    TARGET = fixed['target']
    require(TARGET in TARGET_CONTEXT, 'Unrecognized macOS target')
    ARCH, LABEL, JOB, CPU = TARGET_CONTEXT[TARGET]
    require((fixed['source'], fixed['tree'], fixed['run'], fixed['attempt'], fixed['job'])
            == (SOURCE, TREE, RUN, ATTEMPT, JOB), 'Fixed reviewed context differs')
    CI = BASE / 'ci/source-ee0dabca' / TARGET
    EVIDENCE = BASE / 'verification/a48' / TARGET
    required = {str(CI / name) for name in ('retrieval-receipt.json', 'install-acceptance.json', 'archive-payload-binding.json')}
    required |= {str(EVIDENCE / name) for name in ('native-contract.json', 'observations.json', 'parser-archive-binding.json', 'skip-review.json')}
    require(set(fixed['pins']) == required, 'Fixed input pin roster differs')
    require(all(re.fullmatch(r'[0-9a-f]{64}', digest) for digest in fixed['pins'].values()), 'Invalid fixed input digest')
    require(re.fullmatch(r'[0-9a-f]{64}', fixed['documents_sha256']), 'Invalid reviewed document digest')
    PINS = {**STATIC_PINS, **fixed['pins']}
    ARTIFACTS = fixed['artifacts']
    names = {'install': 'cs80-' + LABEL + '-install-bundle', 'native': 'cs80-' + LABEL + '-test-evidence', 'validator': 'cs80-pluginval-' + LABEL}
    steps = {'install': 'Upload native macOS install bundle', 'native': 'Upload test and JUCE source evidence', 'validator': 'Upload pluginval reports'}
    require(set(ARTIFACTS) == set(names), 'Fixed artifact roster differs')
    for kind, record in ARTIFACTS.items():
        require(len(record) == 5 and record[1] == names[kind] and record[4] == steps[kind]
                and type(record[0]) is int and type(record[2]) is int and record[2] > 0
                and re.fullmatch(r'[0-9a-f]{64}', record[3]), 'Invalid reviewed artifact identity')
    require(fixed['nested_python_skips'] == 5, 'Unexpected Mac nested skip count; explicit review required')
    require(re.fullmatch(r'[0-9]+[.][0-9]{2}', fixed['ctest_seconds']), 'Invalid reviewed CTest elapsed time')
    FIXED = fixed


REQUIRED = [
    'Run actions/checkout@v5', 'Record native platform', 'Configure',
    'Verify configured JUCE source', 'Build', 'Test', 'Fetch pluginval 1.0.4',
    'Register built AU', 'Validate VST3 with pluginval', 'Validate AU with pluginval',
    'Validate AU with pluginval GUI', 'Validate AU GUI fixed-seed regression',
    'Validate AU GUI regression seed 0x3c99748',
    'Package native macOS VST3, AU and Standalone', 'Upload native macOS install bundle',
    'Upload pluginval reports', 'Upload test and JUCE source evidence',
]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def identity(path, raw):
    return dict(path=str(path), bytes=len(raw), sha256=sha(raw))


def dt(value):
    return datetime.fromisoformat(value.replace('Z', '+00:00'))


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate JSON key: ' + key)
        result[key] = value
    return result


def parsed(raw):
    return json.loads(raw, object_pairs_hook=unique_object)


def source_commands(workflow):
    """Read this fixed source's uncomplicated YAML run blocks without executing them."""
    block = workflow.split('\n  macos:\n', 1)[1].split('\n  windows:\n', 1)[0]
    require('continue-on-error:' not in block, 'macOS permits suppressed step failure')
    result = []
    for chunk in re.split(r'^      - ', block, flags=re.M)[1:]:
        name = re.match(r'name: (.+)', chunk)
        match = re.search(r'^        run: ([^\n]*)(?:\n|$)', chunk, re.M)
        if match is None:
            continue
        require(name is not None, 'Unnamed source run step')
        body = match[1]
        if body in ('|', '>'):
            lines = []
            for line in chunk[match.end():].splitlines():
                if line.startswith('          '):
                    lines.append(line[10:])
                elif not line.strip():
                    lines.append('')
                else:
                    break
            body = (' '.join(x.strip() for x in lines).strip() if body == '>'
                    else '\n'.join(lines).rstrip())
        result.append((name[1], body.replace('${{ github.workspace }}', WORKSPACE)))
    return result


def parse_log(raw):
    lines = []
    for number, original in enumerate(raw.decode('utf-8-sig').splitlines(), 1):
        stamp = re.match(r'^(\d{4}-\d\d-\d\dT\S+Z) ?', original)
        text = original[stamp.end():] if stamp else original
        lines.append(dict(number=number, timestamp=stamp[1] if stamp else None,
                          text=re.sub(r'\x1b\[[0-9;]*m', '', text),
                          command='\x1b[36;1m' in text))
    starts = [i for i, line in enumerate(lines) if line['text'].startswith('##[group]Run ')]
    groups = []
    for index, start in enumerate(starts):
        end = starts[index + 1] if index + 1 < len(starts) else len(lines)
        header_end = next(i for i in range(start, end) if lines[i]['text'] == '##[endgroup]')
        groups.append(dict(
            title=lines[start]['text'], records=lines[start:end],
            header=lines[start:header_end + 1], runtime=lines[header_end + 1:end],
            command='\n'.join(line['text'] for line in lines[start:header_end] if line['command']),
        ))
    return lines, groups


def within(timestamp, step):
    # GitHub API boundaries truncate fractional seconds; keep this allowance explicit.
    return dt(step['started_at']) <= dt(timestamp) < dt(step['completed_at']) + timedelta(seconds=1)


def check(repo, archives):
    cache = {}

    def read(relative):
        key = str(relative)
        if key not in cache:
            path = repo / key
            require(path.is_file() and not path.is_symlink(), 'Not a regular retained file: ' + key)
            cache[key] = path.read_bytes()
        return cache[key]

    def load(relative):
        return parsed(read(relative))

    for path, expected in PINS.items():
        require(sha(read(path)) == expected, 'Reviewed input changed: ' + path)
    retrieval = load(CI / 'retrieval-receipt.json')
    require((retrieval['source_commit'], retrieval['run_id'], retrieval['attempt'], retrieval['job_id'])
            == (SOURCE, RUN, ATTEMPT, JOB), 'Acquisition context')
    for name, claim in retrieval['files'].items():
        raw = read(CI / name)
        require((len(raw), sha(raw)) == (claim['bytes'], claim['sha256']), 'Acquired file changed: ' + name)
    source = {}
    for path in ['.github/workflows/build.yml', 'CMakeLists.txt', 'scripts/package-ci-artifacts.py',
                 'scripts/verify-ci-bundle.py', 'scripts/verify-ci-juce-source.cmake',
                 'scripts/download-pluginval-macos.py', 'src/core/FactoryPresets.h',
                 'tests/LibraryAuditTests.cpp']:
        source[path] = subprocess.check_output(['git', '-C', str(repo), 'show', SOURCE + ':' + path])
    actual_tree = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', SOURCE + '^{tree}'], text=True).strip()
    require(actual_tree == TREE, 'Frozen source tree')
    require(sha(source['.github/workflows/build.yml']) == '9cb87b71aefd18f0e753f203d8f2fa2b537935fedde33fdc87f49eb64b04fccb', 'Reviewed workflow')
    require(sha(source['scripts/package-ci-artifacts.py']) == 'f2df2edfd4a55a759ad5c0bad72fa05a5f3880d323412e1211ede1d15bdaa8cf', 'Reviewed read-only packager')
    require(sha(source['tests/LibraryAuditTests.cpp']) == '958e67fc42fa3438c5dd47425f9f4dc8012650e44d814283aa0e3d2bbc4f69cd', 'Reviewed heap-owned audit source')

    runs = [load(CI / name) for name in ('run-api-at-platform-completion.json', 'attempt1-api-at-platform-completion.json')]
    for run in runs:
        require((run['id'], run['head_sha'], run['run_attempt']) == (RUN, SOURCE, ATTEMPT), 'Original run/attempt context')
        require(run['event'] == 'push' and run['path'] == '.github/workflows/build.yml', 'Original workflow/event')
        for field in ('repository', 'head_repository'):
            require(run[field]['full_name'] == REPOSITORY and run[field]['id'] == REPOSITORY_ID, 'Run repository identity')
    job = load(CI / 'producer-job-api.json')
    require((job['id'], job['run_id'], job['head_sha'], job['run_attempt']) == (JOB, RUN, SOURCE, ATTEMPT), 'Original producer identity')
    require(job['status'] == 'completed' and job['conclusion'] == 'success'
            and job['labels'] == [LABEL] and job['name'] == 'macOS / ' + LABEL + ' / VST3 + AU + Standalone', 'Original Mac result')
    steps = {step['name']: step for step in job['steps']}
    require(len(steps) == len(job['steps']), 'Duplicate API step name')
    for name in REQUIRED:
        step = steps[name]
        require(step['status'] == 'completed' and step['conclusion'] == 'success', 'Required original step: ' + name)
        require(dt(job['started_at']) <= dt(step['started_at']) <= dt(step['completed_at']) <= dt(job['completed_at']), 'Step outside job: ' + name)
    for previous, following in zip(REQUIRED, REQUIRED[1:]):
        require(steps[previous]['number'] < steps[following]['number']
                and dt(steps[previous]['completed_at']) <= dt(steps[following]['started_at']), 'Original step order')
    require(all(steps[name]['conclusion'] == 'skipped' for name in ('AU registration diagnostics', 'Report pluginval failure')), 'Failure diagnostics unexpectedly ran')

    original = read(CI / 'producer-job.log')
    lines, groups = parse_log(original)
    text = '\n'.join(line['text'] for line in lines) + '\n'
    require('\n' + SOURCE + '\n' in text and '\n' + ARCH + '\n' in text, 'Original checkout/platform')
    require('##[error]' not in text and 'Process completed with exit code' not in text, 'Original error/nonzero exit')
    logged_scripts = [group for group in groups if group['command']]
    all_source_scripts = source_commands(source['.github/workflows/build.yml'].decode())
    require([name for name, body in all_source_scripts if name not in REQUIRED]
            == ['AU registration diagnostics', 'Report pluginval failure'], 'Unexpected unexecuted source run step')
    expected_scripts = [(name, body) for name, body in all_source_scripts if name in REQUIRED]
    require(len(logged_scripts) == len(expected_scripts) == 13, 'Unexpected/missing original command group')
    command_proof, named = [], {}
    for (name, expected), group in zip(expected_scripts, logged_scripts):
        require(group['command'] == expected, 'Original command differs from frozen workflow: ' + name)
        require(any(line['text'] == 'shell: /bin/bash -e {0}' for line in group['header']), 'Unexpected original shell')
        require(within(group['header'][0]['timestamp'], steps[name]), 'Command outside original API step: ' + name)
        require(all(within(line['timestamp'], steps[name]) for line in group['runtime'] if line['timestamp']), 'Output outside original API step: ' + name)
        named[name] = group
        command_proof.append(dict(name=name, original_api_step=steps[name],
                                  log_first_line=group['records'][0]['number'], log_last_line=group['records'][-1]['number'],
                                  command=expected, command_sha256=sha(expected.encode()),
                                  exact_source_command_match=True))
    require(len(groups) == 17, 'Extra or missing original action execution')

    archive_bytes, uploads = {}, []
    for kind, (artifact_id, name, size, digest, step_name) in ARTIFACTS.items():
        api = load(CI / (kind + '-api.json'))
        path = archives / (TARGET + '-' + kind + '.zip')
        require(path.is_file() and not path.is_symlink(), 'Invalid actual archive: ' + kind)
        raw = path.read_bytes()
        archive_bytes[kind] = raw
        require((len(raw), sha(raw)) == (size, digest), 'Actual ZIP bytes: ' + kind)
        require((api['id'], api['name'], api['size_in_bytes'], api['digest'], api['expired'])
                == (artifact_id, name, size, 'sha256:' + digest, False), 'Artifact API bytes/name: ' + kind)
        wr = api['workflow_run']
        require((wr['id'], wr['head_sha'], wr['repository_id'], wr['head_repository_id'])
                == (RUN, SOURCE, REPOSITORY_ID, REPOSITORY_ID), 'Artifact original source/run: ' + kind)
        route = f'https://api.github.com/repos/{REPOSITORY}/actions/artifacts/{artifact_id}'
        require(api['url'] == route and api['archive_download_url'] == route + '/zip', 'Artifact route')
        require(dt(api['expires_at']) > datetime.now(timezone.utc), 'Artifact expired now')
        matched = [g for g in groups if any(line['text'] == '  name: ' + name for line in g['header'])]
        require(len(matched) == 1, 'Unique original upload group: ' + kind)
        group, step = matched[0], steps[step_name]
        successes = [line for line in group['runtime'] if line['text'] == f'Artifact {name} has been successfully uploaded! Final size is {size} bytes. Artifact ID is {artifact_id}']
        hashes = [line for line in group['runtime'] if line['text'] == 'SHA256 digest of uploaded artifact zip is ' + digest]
        require(len(successes) == len(hashes) == 1 and within(successes[0]['timestamp'], step)
                and within(hashes[0]['timestamp'], step) and within(api['created_at'], step)
                and within(group['header'][0]['timestamp'], step), 'Original upload ID/hash/time binding: ' + kind)
        retained = next(item for item in retrieval['archives'] if item['kind'] == kind)
        require((retained['artifact_id'], retained['bytes'], retained['sha256'], retained['api_identity_matched'])
                == (artifact_id, size, digest, True), 'Acquisition archive binding')
        uploads.append(dict(kind=kind, artifact_id=artifact_id, name=name, bytes=size, sha256=digest,
                            original_api_step=step, original_upload_timestamp=successes[0]['timestamp'],
                            original_digest_timestamp=hashes[0]['timestamp'], created_at=api['created_at'],
                            expires_at=api['expires_at'], producing_attempt_bound=True))

    observation = load(EVIDENCE / 'observations.json')
    contract = load(EVIDENCE / 'native-contract.json')
    binding = load(EVIDENCE / 'parser-archive-binding.json')
    require(observation['all_requested_observations_green'] is True
            and contract['source'] == SOURCE and contract['tree'] == TREE
            and contract['run_id'] == RUN and contract['attempt'] == ATTEMPT
            and binding['status'] == 'parser_archive_and_member_input_binding_verified', 'Actual native content/binding status')
    require(len(contract['native']) == len(observation['native']) == 1, 'Unexpected native target')
    native, native_contract = observation['native'][0], contract['native'][0]
    require(native['target'] == native_contract['target'] == TARGET
            and native['counts'] == native_contract['counts'] == COUNTS
            and native_contract['native_content_contract_verified'] is True
            and native_contract['ui_body_identity_phase_complete'] is True
            and native['nested_python_skips']['total'] == 5, 'Native exact contract')
    for item in binding['bindings']:
        raw = archive_bytes[item['kind']]
        claim = item['actual_zip_api_binding']
        require((claim['bytes'], claim['sha256']) == (len(raw), sha(raw)), 'Actual archive identity')
        with zipfile.ZipFile(io.BytesIO(raw)) as archive:
            for member in item['parsed_member_records_bound']:
                member_raw = archive.read(member['path'])
                require((len(member_raw), sha(member_raw)) == (member['bytes'], member['sha256']), 'Actual member binding')
    ctest = named['Test']
    runtime = '\n'.join(line['text'] for line in ctest['runtime'])
    rows = re.findall(r'^\s*(\d+)/277 Test\s+#(\d+): (\w+) \.+\s*(Passed|\*\*\*Skipped)\s+[0-9.]+ sec$', runtime, re.M)
    require(len(rows) == 277 and [int(row[0]) for row in rows] == list(range(1, 278))
            and [int(row[1]) for row in rows] == list(range(1, 278)), 'Original complete serial CTest roster')
    require([(row[2], 'passed' if row[3] == 'Passed' else 'formal_skipped') for row in rows]
            == [(case['name'], case['result']) for case in native['testcases']], 'Original CTest names/results differ from JUnit/LastTest')
    require('100% tests passed out of 277' in runtime and 'Total Test time (real) = ' + FIXED['ctest_seconds'] + ' sec' in runtime, 'Original CTest terminal result')
    with zipfile.ZipFile(io.BytesIO(archive_bytes['native'])) as archive:
        juce_raw = archive.read('ci-juce-provenance.txt')
        lasttest_raw = archive.read('Testing/Temporary/LastTest.log')
    juce = dict(line.split('=', 1) for line in juce_raw.decode().splitlines() if '=' in line)
    for key, value in dict(actual_sha=JUCE, actual_version='8.0.11', source_clean='true', verification='PASS',
                           workflow_commit=SOURCE, workflow_run=str(RUN), source_override='').items():
        require(juce[key] == value, 'Native JUCE provenance: ' + key)
    require(f'Verified clean JUCE 8.0.11 at {JUCE};' in text, 'Original clean JUCE verification')

    validators = observation['validators']
    require(len(validators) == 1 and validators[0]['target'] == TARGET
            and validators[0]['validation_invocations'] == validators[0]['required_lanes'] == 5
            and validators[0]['validation_retries'] == 0, 'Five original lanes only')
    require([lane['lane'] for lane in validators[0]['lanes']]
            == ['vst3', 'au', 'au-gui', 'au-gui-seed', 'au-gui-seed-c76'], 'Exact five-lane roster')
    parser_path = repo / 'docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py'
    parser_spec = importlib.util.spec_from_file_location('a48_pinned_a52_parser', parser_path)
    parser_module = importlib.util.module_from_spec(parser_spec)
    parser_spec.loader.exec_module(parser_module)
    lasttest_cases = parser_module.lasttest_cases(parser_module.decode(lasttest_raw))
    audit = lasttest_cases['cs80_library_audit_tests']
    audit_case = next(case for case in native['testcases'] if case['name'] == 'cs80_library_audit_tests')
    require(audit['lasttest_status'] == 'Passed' and audit_case['result'] == 'passed'
            and audit['output'].count('library audit passed: 301 curated + 222 original circuit programs') == 1,
            'Actual fresh complete LibraryAudit did not pass')
    audit_proof = dict(name='cs80_library_audit_tests', original_ctest_result='Passed',
                      junit_seconds=audit_case['seconds'], lasttest_output=audit['output'],
                      output_sha256=sha(audit['output'].encode()), all_523_programs_completed=True,
                      source_sha256=sha(source['tests/LibraryAuditTests.cpp']),
                      predecessor_failure_preserved='c5549f7d Intel run 45385716988 job 138415960996 remains failed; this original ee0dabca execution is a separate source/run result.')
    new_tests = [case for case in native['testcases'] if case['name'] in {
        'cs80_piano_partial_decay_tests', 'cs80_body_noise_rate_tests',
        'cs80_string_body_formant_tests', 'cs80_low_choir_spectrum_tests'}]
    require(len(new_tests) == 4 and all(case['result'] == 'passed' for case in new_tests), 'Actual four new tests')
    outcomes = []
    with zipfile.ZipFile(io.BytesIO(archive_bytes['validator'])) as archive:
        download = parsed(archive.read('pluginval-download.json'))
        official = dict(repository='Tracktion/pluginval', version='1.0.4', release_id=188928656,
                        asset_id=210999197, asset_name='pluginval_macOS.zip', expected_size=9202795,
                        expected_sha256='3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f',
                        verified=True, actual_size=9202795,
                        actual_sha256='3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f')
        require(all(download[k] == v for k, v in official.items()), 'Pinned official pluginval receipt')
        fetch = '\n'.join(line['text'] for line in named['Fetch pluginval 1.0.4']['runtime'])
        require('pluginval - 1.0.4' in fetch and f"Verified official pluginval 1.0.4 macOS asset 210999197: 9202795 bytes, SHA256 {official['actual_sha256']}" in fetch, 'Original validator download/version')
        for lane in validators[0]['lanes']:
            name = lane['required_workflow_step']
            group = named[name]
            console = archive.read(lane['console']['path'])
            original_console = ('\n'.join(line['text'] for line in group['runtime']) + '\n').encode()
            require(console == original_console, 'Original full console/archive identity: ' + name)
            require(archive.read(lane['report']['path']) == b'\n'.join(console.split(b'\n')[1:]),
                    'Original console/report content identity: ' + name)
            require(group['command'].startswith('set -euo pipefail\n')
                    and '--validate-in-process \\' in group['command']
                    and '||' not in group['command'] and 'set +e' not in group['command'], 'Unsuppressed original validator exit')
            require(('--skip-gui-tests' not in group['command']) == lane['gui_required'], 'GUI/headless source command')
            for role in ('console', 'report'):
                summary = lane[role]['summary']
                require(summary == parser_module.console_summary(archive.read(lane[role]['path'])),
                        'Pinned parser recheck differs from retained summary: ' + name + '/' + role)
                require(summary['strictness'] == [10] and summary['program_counts'] == [523]
                        and summary['success_lines'] == 1 and summary['final_nonempty_line'] == 'SUCCESS'
                        and summary['validation_start_count'] == 1 and not summary['failure_markers']
                        and summary['clean_text_success'] is True
                        and summary['editor_test_started'] == lane['gui_required'], 'Complete validator result: ' + name)
                if lane['fixed_seed']:
                    require(summary['seeds'] == [lane['fixed_seed']], 'Fixed-seed result')
            require(lane['validation_invocations'] == 1 and lane['validation_retries'] == 0
                    and not lane['preserved_failed_attempts'] and not lane['text_findings'], 'No substituted/retried validator lane')
            if lane['lane'] != 'vst3':
                require(console.count(b'auval exited with code: 0') == 1, 'Original AU validator child success')
            outcomes.append(dict(lane=lane['lane'], step=name, original_normal_exit=0,
                                 exit_basis='Completed/success original API step; exact source command with set -euo pipefail, --validate-in-process, tee, and unsuppressed terminal SUCCESS grep; full original console equals archived console.',
                                 seed=lane['console']['summary']['seeds'][0], strictness=10, programs=523,
                                 gui=lane['gui_required'], console=lane['console'], report=lane['report'],
                                 retained_warnings=[line for line in console.decode().splitlines() if 'WARNING:' in line],
                                 optional_steinberg_validator_skipped="INFO: Skipping vst3 validator as validator path hasn't been set" in console.decode(),
                                 log_first_line=group['records'][0]['number'], log_last_line=group['records'][-1]['number']))
        au_files = {phase: archive.read('au-payload-' + phase + '-validation.sha256') for phase in ('before', 'after')}
    require(au_files['before'] == au_files['after'], 'AU before/after executable bytes')
    register = named['Register built AU']
    package = named['Package native macOS VST3, AU and Standalone']
    command = register['command']
    require(command.index('codesign --force --sign - "$AU_SOURCE"') < command.index('cp -R "$AU_SOURCE" "$AU_DEST"')
            < command.index('diff -qr "$AU_SOURCE" "$AU_DEST"'), 'Sign before copy before whole-directory comparison')
    for phase, group in [('before', register), ('after', package)]:
        require(group['command'].count('diff -qr "$AU_SOURCE" "$AU_DEST"') == 1, 'Required complete-directory diff')
        hashes = [line['text'] for line in group['runtime'] if re.match(r'^[0-9a-f]{64}  /', line['text'])]
        require(('\n'.join(hashes) + '\n').encode() == au_files[phase], 'Original AU hash output/member identity')
        require(not any(re.search(r'^(?:Files .+ differ|Only in |diff:)', line['text']) for line in group['runtime']), 'AU directory difference')
    require(text.count('diff -qr "$AU_SOURCE" "$AU_DEST"') == 2, 'Unexpected AU comparison count')
    install, payload = load(CI / 'install-acceptance.json'), load(CI / 'archive-payload-binding.json')
    require(install['status'] == 'artifact_bytes_verified' and install['source'] == SOURCE
            and install['source_tree'] == TREE and install['target'] == TARGET
            and install['preset_library_revision'] == REVISION and install['run_id'] == RUN
            and install['run_attempt'] == ATTEMPT and install['inventory_entries'] == 39
            and set(install['formats']) == {'AU', 'VST3', 'Standalone'}, 'Installation acceptance context')
    require(len(install['binary_headers']) == 3 and all(b['architecture'] == ARCH
            and b['header_check'] == 'PASS' and b['mode'] == '0o755' for b in install['binary_headers']), 'All native executable architectures/roles')
    require(payload['status'] == 'archive_consistency_and_au_executable_binding_verified'
            and payload['expected'] == dict(architecture=ARCH, attempt=ATTEMPT, repository=REPOSITORY,
                                            revision=REVISION, run=RUN, source=SOURCE), 'Mac payload binding context')
    for format_name, bundle in payload['packaged_bundles'].items():
        require(not any(entry['type'] == 'symlink' for entry in bundle['inventory']), 'Actual bundle has symlinks; diff scope requires separate review')
        header = next(item for item in install['binary_headers'] if item['path'] == bundle['executable'])
        require(header['sha256'] == bundle['executable_sha256'], 'Independent package binary hashes disagree')
    actual_headers = []
    with zipfile.ZipFile(io.BytesIO(archive_bytes['install'])) as outer:
        inner_raw = outer.read(install['inner_archive']['file'])
    require((len(inner_raw), sha(inner_raw)) == (install['inner_archive']['bytes'], install['inner_archive']['sha256']), 'Actual inner archive bytes')
    root = install['inner_archive']['file'].removesuffix('.tar.gz')
    with tarfile.open(fileobj=io.BytesIO(inner_raw), mode='r:gz') as inner:
        for format_name, bundle in payload['packaged_bundles'].items():
            member = inner.getmember(root + '/' + bundle['executable'])
            require(member.isfile() and member.mode == 0o755, 'Actual executable member role/mode')
            with inner.extractfile(member) as stream:
                raw = stream.read()
            require(sha(raw) == bundle['executable_sha256'], 'Actual executable byte identity')
            require(raw[:4] == b'\xcf\xfa\xed\xfe' and len(raw) >= 32, 'Actual payload is not thin little-endian Mach-O 64')
            cpu, subtype, role = struct.unpack_from('<III', raw, 4)
            require(cpu == CPU and role == (2 if format_name == 'Standalone' else 8), 'Actual Mach-O native CPU/role')
            actual_headers.append(dict(format=format_name, path=bundle['executable'], bytes=len(raw),
                                       sha256=sha(raw), mode='0o755', magic=raw[:4].hex(),
                                       cpu_type=hex(cpu), cpu_subtype=hex(subtype), mach_o_filetype=role,
                                       thin_native_64_bit=True, header_role='executable' if role == 2 else 'bundle'))
    au_digest = payload['packaged_bundles']['AU']['executable_sha256']
    require(all(row['sha256'] == au_digest for row in payload['au_before'] + payload['au_after']), 'Packaged AU differs from original checked executable')
    require(au_files['before'].decode().split()[0] == au_digest, 'Original hash/member/package binding')
    require(install['inner_archive']['sha256'] == payload['inner_archive']['sha256'], 'Independent inner archive identities disagree')
    package_runtime = '\n'.join(line['text'] for line in package['runtime'])
    require('"sha256": "' + install['inner_archive']['sha256'] + '"' in package_runtime
            and '"archive": "packages/' + install['inner_archive']['file'] + '"' in package_runtime
            and '"architecture": "' + ARCH + '"' in package_runtime, 'Original packaged inner archive identity')
    for path, record in payload['source_contract_files'].items():
        require((len(source[path]), sha(source[path])) == (record['bytes'], record['sha256']), 'Payload source contract drift')
    require(payload['validator_input_paths']['vst3']['path'] == WORKSPACE + '/build/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3', 'VST3 validated source path')
    packager = source['scripts/package-ci-artifacts.py'].decode()
    require('with tempfile.TemporaryDirectory(prefix="cs80-package-fixtures-") as temp:' in packager
            and 'args.build_dir / "CS80Reference_artefacts/Release"' in packager
            and 'Package output must be outside build products' in packager
            and '"licenses/JUCE-LICENSE.md": license_file.read_bytes()' in packager, 'Reviewed packaging source invariants')

    proof = dict(
        schema=1, status='original_platform_execution_bound', source_commit=SOURCE, source_tree=TREE,
        factory_revision=REVISION, run_id=RUN, run_attempt=ATTEMPT, job_id=JOB, target=TARGET,
        acquisition='Repository-owner authenticated connector inputs retained by root; this offline checker verifies those exact retained bytes and their internal original-execution chain.',
        run_snapshot_status=runs[0]['status'], run_snapshot_conclusion=runs[0]['conclusion'],
        original_job=job, original_run_attempt_bound=True, artifact_producing_attempts_bound=True,
        original_commands=command_proof, uploads=uploads, original_ctest_normal_exit=0,
        ctest_exit_basis='Exact unsuppressed original serial ctest command, successful original API Test step, 277 ordered console names/results matching bound JUnit/LastTest, and original terminal completion.',
        original_ctest_console_names_match_bound_junit_lasttest=True, native_counts=COUNTS,
        ctest_seconds=FIXED['ctest_seconds'], library_audit=audit_proof, new_tests=new_tests,
        native_content_contract=identity(EVIDENCE / 'native-contract.json', read(EVIDENCE / 'native-contract.json')),
        juce=juce, juce_native_member=identity('ci-juce-provenance.txt', juce_raw),
        pluginval_download=download, validator_invocations=5, validation_retries=0, validator_outcomes=outcomes,
        pluginval_transport_attempts=len(download['attempts']),
        pluginval_transport_scope='Downloader attempts are retained in full; they are acquisition transports, not validation invocations or validation retries.',
        actual_native_binary_headers=actual_headers,
        whole_au_directory_comparisons_passed=True, au_signing_before_copy_verified=True,
        au_before_after_member=identity('au-payload-before/after-validation.sha256', au_files['before']),
        au_packaged_executable_sha256=au_digest,
        whole_au_bundle_binding_basis='Original successful unsuppressed diff -qr of source and installed AU before and after all validation; signing precedes copy; exact timestamped commands match frozen workflow; no intervening rebuild or bundle mutation; source-pinned packager reads that source AU. Complete packaged AU inventory includes resources/signature records and contains no symlinks.',
        whole_au_directory_comparison_limit='diff -qr establishes ordinary-file content equality. It does not establish permissions, owner, ACL, xattr equivalence, or signature validity; packaged inventory modes are checked separately.',
        vst3_validated_bundle_identity_established=True,
        vst3_binding_basis='Original in-process VST3 validation input is the same build/CS80Reference_artefacts/Release/VST3 path subsequently read by the source-pinned packager. Exact intervening commands only validate AU and compare AU; no rebuild, signing, replacement, mutation, or retry of VST3. This is ordered source-path binding, not an independently recorded VST3 before/after tree hash.',
        packaging_self_test_scope='The 46 original packaging checks use inert products under TemporaryDirectory; they do not rebuild or mutate actual build products.',
        juce_license_scope='Packager reads LICENSE.md directly from the clean pinned configured JUCE source, after original successful verification and with no intervening dependency-source mutation; no separate local JUCE checkout is substituted.',
        source_contract_files={path:identity(path, raw) for path, raw in source.items()},
        input_files={path:identity(path, raw) for path, raw in cache.items()},
        platform_qualified=False, cross_platform_qualified=False, release_qualified=False,
        scope='Original target-specific Mac execution and payload bindings only; final platform receipt additionally joins document/notice acceptance. No other native target, audition or public download is accepted here.',
    )
    return proof, install, payload, native_contract, cache


def main(fixed, verifier_path):
    configure(fixed)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[5])
    parser.add_argument('--archives', type=Path, default=Path('/tmp/cs80-ci-1cee5a/ee0dabca'))
    parser.add_argument('--documents-receipt', type=Path, required=True)
    parser.add_argument('--out-dir', type=Path, required=True)
    args = parser.parse_args()
    outputs = [args.out_dir / 'original-execution-binding.json', args.out_dir / 'platform-qualification.json']
    for output in outputs:
        require(not output.exists() and not output.is_symlink(), 'Refusing to overwrite evidence: ' + str(output))
    proof, install, payload, native, cache = check(args.repo.resolve(), args.archives)
    # The actual A30-derived source-specific document/notice supplement is a required final gate.
    document_raw = args.documents_receipt.read_bytes()
    require(sha(document_raw) == FIXED['documents_sha256'],
            'Reviewed actual A48 document/notice supplement changed')
    documents = parsed(document_raw)
    require(documents.get('source') == SOURCE and documents.get('revision') == REVISION
            and documents.get('target') == TARGET, 'Document supplement source/revision/target')
    require(documents.get('status') == 'source_specific_docs_and_notices_verified'
            and documents.get('artifact_id') == ARTIFACTS['install'][0]
            and documents.get('outer_bytes') == ARTIFACTS['install'][2]
            and documents.get('outer_sha256') == ARTIFACTS['install'][3]
            and documents.get('inventory_counts') == {'directory': 13, 'file': 26}
            and documents.get('symlink_count') == 0
            and len(documents['required_metadata']) == 13
            and len(documents['source_comparisons']) == 10
            and all(item['result'] == 'PASS' for item in documents['source_comparisons']),
            'Document/notice supplement not accepted')
    require(next(item for item in documents['required_metadata'] if item['path'] == 'evidence/juce-provenance.txt')['sha256']
            == proof['juce_native_member']['sha256'], 'Packaged/native JUCE evidence differs')
    proof['verified_at_utc'] = datetime.now(timezone.utc).isoformat()
    proof_raw = (json.dumps(proof, indent=2) + '\n').encode()
    receipt = dict(
        schema=1, status='platform_qualified', platform_qualified=True,
        cross_platform_qualified=False, release_qualified=False, source_commit=SOURCE, source_tree=TREE,
        factory_revision=REVISION, run_id=RUN, run_attempt=ATTEMPT, job_id=JOB, target=TARGET,
        verified_at_utc=proof['verified_at_utc'], original_run_attempt_bound=True,
        artifact_producing_attempts_bound=True, native_results_accepted=True,
        native_counts=COUNTS, nested_python_skips=5, formal_skips=native['formal_skips'],
        skip_review=identity(EVIDENCE / 'skip-review.json', cache[str(EVIDENCE / 'skip-review.json')]),
        exact_source_derived_native_names_verified=True, all_four_new_tests_passed=True,
        ui_body_identity_phase_complete=True, original_ctest_normal_exit=0,
        ctest_seconds=proof['ctest_seconds'], library_audit=proof['library_audit'], new_tests=proof['new_tests'],
        juce=proof['juce'], pluginval_version='1.0.4', validator_lanes=5, validator_invocations=5,
        validation_retries=0, validator_original_normal_exits=[0, 0, 0, 0, 0],
        validator_outcomes=proof['validator_outcomes'], total_required_global_validator_lanes=15,
        whole_au_directory_comparisons_passed=True, au_signing_before_copy_verified=True,
        validated_bundle_identity_established=True, vst3_binding_basis=proof['vst3_binding_basis'],
        installation_inventory_entries=39, binary_headers=install['binary_headers'],
        actual_native_binary_headers=proof['actual_native_binary_headers'],
        pluginval_transport_attempts=proof['pluginval_transport_attempts'],
        pluginval_transport_scope=proof['pluginval_transport_scope'],
        packaged_bundle_inventories={name:{key:bundle[key] for key in ('root', 'inventory_entries', 'inventory_sha256', 'executable_sha256')}
                                     for name, bundle in payload['packaged_bundles'].items()},
        uploads=proof['uploads'], inner_archive=install['inner_archive'],
        document_notice_acceptance=identity(args.documents_receipt, document_raw),
        original_execution_binding=identity(outputs[0], proof_raw),
        verifier=identity(verifier_path, verifier_path.read_bytes()),
        qualification_engine=identity(Path(__file__), Path(__file__).read_bytes()),
        remaining_release_gates=['Other three native platforms and all 15 required global validator lanes.',
                                 'Actual .7 audition archive acceptance and user listening acceptance.',
                                 'Final fresh public GET/size/hash/retention checks for all five advertised downloads.',
                                 'Final cross-platform aggregate and publication.'],
        limits=['Only this actual Mac target/source/run/job and these three artifacts are platform-qualified.',
                'The original run status and conclusion are preserved from the retained platform-completion snapshots; no aggregate outcome is inferred.',
                'No Mac product executed on this Linux inspection host; original native CI execution supplies runtime evidence.',
                'Standalone is compiled, inventory/header checked, and covered by native processor/UI tests; no separate end-user launch check is claimed.',
                'The complete AU diff -qr comparisons establish ordinary-file content equality, not permissions/owner/ACL/xattr equivalence or signature validity. Packaged modes are checked independently.',
                'Original AU current-program -1 warning is retained; all four AU lanes expose 523 programs and pass. The optional external Steinberg VST3 validator is not configured; the pluginval VST3 lane itself passed.',
                'No publisher signing/notarization, DAW-wide compatibility, acoustic equivalence, or listening approval is inferred.',
                'Unchanged generic helper receipts retain their own limited false qualification fields; their inherited 13-lane prose does not replace the explicit .7 requirement of 15.'],
    )
    args.out_dir.mkdir(parents=True, exist_ok=True)
    for output, raw in zip(outputs, [proof_raw, (json.dumps(receipt, indent=2) + '\n').encode()]):
        with output.open('xb') as stream:
            stream.write(raw)
    print(json.dumps({key:receipt[key] for key in ('status', 'target', 'native_counts', 'validator_lanes', 'validation_retries', 'cross_platform_qualified', 'release_qualified')}))


if __name__ == '__main__':
    print('Use a fixed reviewed A48 macOS qualification wrapper; direct generic acceptance is unavailable.', file=sys.stderr)
    sys.exit(2)
