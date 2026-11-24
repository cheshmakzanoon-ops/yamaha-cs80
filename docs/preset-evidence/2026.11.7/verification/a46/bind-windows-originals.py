#!/usr/bin/env python3
"""Bind ee0 Windows original commands, jobs, uploads, downloads and validator logs.

This is a bounded CI provenance supplement, not a platform qualifier. Its API
inputs still require root's authenticated acquisition review. Native content,
skip review, package inspection, complete VST3 equality and documentation are
separate mandatory checks. No downloaded native executable is run.
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
import sys
import zipfile

sys.dont_write_bytecode = True
SOURCE = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
TREE = '17115913cbb29157bd00539485bcc5f38b57219c'
RUN = 45397189410
PRODUCER = 138455976256
REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
CONTEXT = dict(factory_revision='2026.11.7', source=SOURCE, tree=TREE, run_id=RUN, attempt=1)
JUCE = 'c352e2489000ffeb9e397051377d75eef2efe641'
WORKFLOW_SHA = '9cb87b71aefd18f0e753f203d8f2fa2b537935fedde33fdc87f49eb64b04fccb'
A41 = 'docs/preset-evidence/2026.11.5/verification/a41-verify-install-artifact.py'
A52 = 'docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py'
PINS = {A41: '803ff4378746937039400e777ff61bfce8ebaeb2ab1bba6699fe76076f520305',
        A52: 'f192bf2ff70d4501f73875eb5bb6416b9adc3b77912e165da4305302724cd846'}
PRODUCER_STEPS = ['Run actions/checkout@v5', 'Configure', 'Verify configured JUCE source', 'Build', 'Test',
                  'Upload Windows VST3', 'Package Windows VST3 and Standalone',
                  'Upload Windows install bundle', 'Upload test and JUCE source evidence']
CONSUMER_STEPS = ['Run actions/checkout@v5', 'Download Windows VST3', 'Fetch pluginval 1.0.4',
                  'Validate VST3 headless', 'Validate state regression seed 0x5037a0c', 'Upload pluginval reports']
ARTIFACTS = {
    'native': ('cs80-windows-test-evidence', 'Upload test and JUCE source evidence'),
    'vst3': ('cs80-windows-vst3', 'Upload Windows VST3'),
    'install': ('cs80-windows-install-bundle', 'Upload Windows install bundle'),
    'validator': ('cs80-pluginval-windows', 'Upload pluginval reports'),
}
UPLOAD_PATHS = {
    'native': ['  path: build/Testing/Temporary/LastTest.log', 'build/ctest-results.xml', 'build/ci-juce-provenance.txt'],
    'vst3': ['  path: build/CS80Reference_artefacts/Release/VST3'],
    'install': ['  path: packages/'],
    'validator': ['  path: pluginval-download.json', 'pluginval-reports', 'pluginval-windows.log',
                  'pluginval-windows-seed-regression.log', 'pluginval-version.txt'],
}
LANES = [('headless', 'Validate VST3 headless', 'pluginval-windows.log', 'pluginval-reports/cs80-windows', None),
         ('state-seed-regression', 'Validate state regression seed 0x5037a0c',
          'pluginval-windows-seed-regression.log', 'pluginval-reports/cs80-windows-seed-regression', '0x5037a0c')]


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def identity(path, raw):
    return dict(path=str(path), bytes=len(raw), sha256=sha(raw))


def stamp(text):
    result = datetime.fromisoformat(text.replace('Z', '+00:00'))
    require(result.tzinfo is not None, 'Timestamp lacks time zone')
    return result


def decode(raw):
    return raw.decode('utf-16' if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig').replace('\r\n', '\n')


def log_rows(raw):
    rows = []
    current_action = None
    path_continuation = False
    for line in decode(raw).splitlines():
        match = re.fullmatch(r'(\d{4}-\d\d-\d\dT[^ ]+Z) (.*)', line)
        if match is not None:
            timestamp, message = match[1], match[2]
            if message.startswith('##[group]Run '):
                current_action = message.removeprefix('##[group]Run ')
            path_continuation = (current_action == 'actions/upload-artifact@v5'
                                 and message.startswith('  path: '))
            rows.append((timestamp, message))
        else:
            # Actions emits multiline upload path input with one leading time.
            # Only those action-input continuations inherit that time; native
            # and validator runtime lines must retain their own timestamp.
            require(rows and path_continuation, 'Unframed original runtime/job-log line')
            rows.append((rows[-1][0], line))
    require(rows and all(stamp(a[0]) <= stamp(b[0]) for a, b in zip(rows, rows[1:])), 'Original timestamps are unordered')
    return rows


def plain_command(text):
    # Strip runner syntax-highlighting only when comparing command prologues.
    return re.sub(r'\x1b\[[0-9;]*m', '', text)


def step(job, name):
    items = [item for item in job['steps'] if item['name'] == name]
    require(len(items) == 1 and items[0]['status'] == 'completed'
            and items[0]['conclusion'] == 'success', 'Original step did not succeed: ' + name)
    return items[0]


def within(timestamp, entry):
    return stamp(entry['started_at']) <= stamp(timestamp) < stamp(entry['completed_at']) + timedelta(seconds=1)


def selected_rows(rows, entry):
    return [(time, message) for time, message in rows if within(time, entry)]


def run_commands(workflow, name, workspace):
    # Frozen source workflow uses only literal/folded scalar run commands here.
    literal_name = 'Fetch pluginval ${{ env.PLUGINVAL_VERSION }}' if name == 'Fetch pluginval 1.0.4' else name
    marker = '      - name: ' + literal_name + '\n'
    require(workflow.count(marker) == 1, 'Missing/ambiguous source step: ' + name)
    body = workflow.split(marker, 1)[1].split('\n      - ', 1)[0]
    match = re.search(r'^        run: ([|>])\n((?:          [^\n]*(?:\n|$)|\n)*)', body, re.M)
    if match:
        lines = [line[10:] for line in match[2].rstrip('\n').splitlines()]
        commands = [' '.join(line.strip() for line in lines)] if match[1] == '>' else lines
    else:
        match = re.search(r'^        run: ([^\n]+)$', body, re.M)
        require(match is not None, 'Missing source run command: ' + name)
        commands = [match[1]]
    return [line.replace('${{ github.workspace }}', workspace) for line in commands]


def bind_command(rows, entry, expected):
    candidates = []
    for index, (timestamp, message) in enumerate(rows):
        if not within(timestamp, entry) or message != '##[group]Run ' + expected[0]:
            continue
        end = index + 1
        while end < len(rows) and not rows[end][1].startswith('shell: '):
            end += 1
        if end < len(rows) and [plain_command(message) for _, message in rows[index + 1:end]] == expected:
            candidates.append((index, end))
    require(len(candidates) == 1, 'Exact original command prologue differs: ' + entry['name'])
    start, end = candidates[0]
    require('pwsh.EXE -command' in rows[end][1], 'Unexpected original command shell')
    require(within(rows[end][0], entry), 'Command extends outside its original step')
    prologue_end = next(index for index in range(end, len(rows)) if rows[index][1] == '##[endgroup]')
    runtime_end = next((index for index in range(prologue_end + 1, len(rows))
                        if rows[index][1].startswith('##[group]Run ')), len(rows))
    return dict(step=entry, source_commands=expected, command_sha256=sha(('\n'.join(expected) + '\n').encode()),
                first_command_utc=rows[start][0], last_command_utc=rows[end][0],
                exact_source_commands_verified=True), rows[prologue_end + 1:runtime_end]


def check_job(job, expected_id, expected_name, names):
    require(type(job['id']) is int and job['id'] > 0 and (expected_id is None or job['id'] == expected_id), 'Wrong original job ID')
    require(job['name'] == expected_name and job['head_sha'] == SOURCE and job['run_id'] == RUN
            and type(job['run_attempt']) is int and job['run_attempt'] == 1,
            'Original job source/run/attempt/name mismatch')
    base = f'https://api.github.com/repos/{REPOSITORY}/actions/'
    require(job['url'] == base + 'jobs/' + str(job['id']) and job['run_url'] == base + 'runs/' + str(RUN),
            'Original job API route differs')
    require(job['status'] == 'completed' and job['conclusion'] == 'success', 'Original job did not succeed')
    required = [step(job, name) for name in names]
    require([item['number'] for item in required] == sorted({item['number'] for item in required}), 'Original step order differs')
    require(all(stamp(a['completed_at']) <= stamp(b['started_at']) for a, b in zip(required, required[1:])),
            'Original required step time order differs')
    allowed = set(names) | {'Set up job', 'Complete job', 'Post Run actions/checkout@v5',
                            'Report CMake failure' if expected_id == PRODUCER else 'Report pluginval failure'}
    require(all(item['name'] in allowed for item in job['steps']), 'Unreviewed additional original job step')
    return required


def checkout_proof(job, rows):
    entry = step(job, 'Run actions/checkout@v5')
    part = selected_rows(rows, entry)
    messages = [message for _, message in part]
    workspaces = [match[1] for message in messages if (match := re.fullmatch(r"Working directory is '([^']+)'", message))]
    require(len(workspaces) == 1 and re.fullmatch(r'[A-Za-z]:\\[^\r\n]+', workspaces[0]), 'Original checkout workspace missing')
    workspace = workspaces[0]
    require(messages.count('  clean: true') == 1 and messages.count(SOURCE) == 1,
            'Original clean checkout/source record missing')
    require("Deleting the contents of '" + workspace + "'" in messages
            and any(message.startswith('Initialized empty Git repository in ') for message in messages),
            'Original fresh clean checkout sequence missing')
    require(any(' checkout --progress --force -B main refs/remotes/origin/main' in message for message in messages),
            'Original force checkout missing')
    return dict(step=entry, workspace=workspace, source=SOURCE, action_clean_true=True,
                original_fresh_repository_initialization=True, observed_ci_git_eol_configuration=None)


def check_download_receipt(receipt):
    expected = dict(repository='Tracktion/pluginval', version='1.0.4', release_id=188928656,
                    asset_id=210999198, asset_name='pluginval_Windows.zip', expected_size=2408590,
                    actual_size=2408590, expected_sha256='c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab',
                    actual_sha256='c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab', verified=True)
    require(all(receipt.get(key) == value and type(receipt.get(key)) is type(value) for key, value in expected.items()),
            'Pinned official pluginval asset receipt differs')
    attempts = receipt['attempts']
    require(1 <= len(attempts) <= 3 and [a['number'] for a in attempts] == list(range(1, len(attempts) + 1)),
            'Validator transfer attempt count/order differs')
    routes = ['https://api.github.com/repos/Tracktion/pluginval/releases/assets/210999198',
              'https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip']
    require([a['source_url'] for a in attempts] == [routes[i] for i in (0, 1, 0)[:len(attempts)]], 'Unexpected transfer routes')
    require(attempts[-1]['transfer_exit'] == 0 and attempts[-1]['reason'] == 'downloaded'
            and attempts[-1]['bytes'] == 2408590, 'Original pinned transfer did not complete')
    for index, attempt in enumerate(attempts[:-1]):
        if attempt['transfer_exit'] == 12:
            require(index == 0 and len(attempts) == 2 and attempt['reason'] == 'http' and attempt['http_status'] == 403,
                    'Invalid alternate-public-route condition')
        else:
            require(attempt['transfer_exit'] == 10 and attempt['reason'] in ('http', 'transport', 'truncated', 'wall_timeout'),
                    'Invalid transfer retry')
            if attempt['reason'] == 'http':
                require(attempt['http_status'] in (408, 429) or 500 <= attempt['http_status'] <= 599,
                        'Ineligible HTTP transfer retry')
    return receipt


def verify(repo, inputs):
    modules = []
    for relative, expected in PINS.items():
        path = repo / relative
        require(sha(path.read_bytes()) == expected, 'Frozen helper changed: ' + relative)
        spec = importlib.util.spec_from_file_location('a46_original_' + path.stem.replace('-', '_'), path)
        loaded = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(loaded)
        modules.append(loaded)
    a41, parser = modules
    raw_inputs = a41.snapshot(inputs, 32 * 1024**2)
    manifest = a41.parse_json(raw_inputs)
    require(manifest['provenance'] == CONTEXT, 'Input manifest is not exact frozen ee0 context')
    base = inputs.resolve().parent
    input_records = {'manifest': identity(inputs, raw_inputs)}

    def read_path(label, path, limit=32 * 1024**2):
        resolved = (base / path).resolve()
        raw = a41.snapshot(resolved, limit)
        input_records[label] = identity(resolved, raw)
        return raw

    def read_json(label):
        return a41.parse_json(read_path(label, manifest[label]))

    run, jobs, artifacts = [read_json(key) for key in ('run_api', 'jobs_api', 'artifacts_api')]
    producer, consumer = [read_json(key) for key in ('producer_api', 'consumer_api')]
    require(run['id'] == RUN and run['head_sha'] == SOURCE and type(run['run_attempt']) is int and run['run_attempt'] == 1,
            'Run source/attempt differs')
    require(run['repository']['full_name'] == run['head_repository']['full_name'] == REPOSITORY
            and run['repository']['id'] == run['head_repository']['id'], 'Run repository differs')
    require(run['path'] == '.github/workflows/build.yml' and run['event'] == 'push', 'Run workflow/event differs')
    for job in (producer, consumer):
        require([item for item in jobs['jobs'] if item['id'] == job['id']] == [job], 'Job differs from retained exact-attempt collection')
    producer_steps = check_job(producer, PRODUCER, 'Windows / VST3 + Standalone', PRODUCER_STEPS)
    consumer_steps = check_job(consumer, None, 'pluginval / Windows VST3 (strictness 10)', CONSUMER_STEPS)
    require(len([job for job in jobs['jobs'] if job['name'] == consumer['name']]) == 1, 'Ambiguous validator consumer job')
    raw_logs = {key: read_path(key, manifest[key], 64 * 1024**2) for key in ('producer_log', 'consumer_log')}
    producer_rows, consumer_rows = [log_rows(raw_logs[key]) for key in ('producer_log', 'consumer_log')]
    for rows in (producer_rows, consumer_rows):
        require(not any(re.search(r'^(?:##\[error\]|Process completed with exit code [1-9])', message)
                        for _, message in rows), 'Original job contains an error/failed process record')
    producer_checkout, consumer_checkout = checkout_proof(producer, producer_rows), checkout_proof(consumer, consumer_rows)
    workflow_raw = subprocess.check_output(['git', '-C', str(repo), 'show', SOURCE + ':.github/workflows/build.yml'])
    require(sha(workflow_raw) == WORKFLOW_SHA, 'Frozen source workflow differs')
    require(subprocess.check_output(['git', '-C', str(repo), 'rev-parse', SOURCE + '^{tree}']).decode().strip() == TREE,
            'Frozen source tree differs')
    workflow = workflow_raw.decode()
    native_workflow = workflow.split('\n  windows:\n', 1)[1].split('\n  pluginval-windows:\n', 1)[0]
    consumer_workflow = workflow.split('\n  pluginval-windows:\n', 1)[1]
    require('continue-on-error' not in consumer_workflow and consumer_workflow.count('if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }') == 4,
            'Validator exit propagation differs')
    command_bindings, runtimes = {}, {}
    for job, rows, source_part, workspace, names in (
            (producer, producer_rows, native_workflow, producer_checkout['workspace'],
             ['Configure', 'Verify configured JUCE source', 'Build', 'Test', 'Package Windows VST3 and Standalone']),
            (consumer, consumer_rows, consumer_workflow, consumer_checkout['workspace'],
             ['Fetch pluginval 1.0.4', *[row[1] for row in LANES]])):
        for name in names:
            command_bindings[name], runtimes[name] = bind_command(rows, step(job, name), run_commands(source_part, name, workspace))
    require([message for _, message in runtimes['Test']].count('100% tests passed, 0 tests failed out of 255') == 1,
            'Original CTest final result differs')
    juce_prefix = '-- Verified clean JUCE 8.0.11 at ' + JUCE + '; evidence: '
    require(sum(message.startswith(juce_prefix) for _, message in runtimes['Verify configured JUCE source']) == 1,
            'Original configured-clean-JUCE verification missing')

    require(set(manifest['artifacts']) == set(ARTIFACTS), 'Expected all four distinct Windows artifact roles')
    artifact_bindings, artifact_raws, apis = {}, {}, {}
    for role, (name, upload_name) in ARTIFACTS.items():
        item = manifest['artifacts'][role]
        api = a41.parse_json(read_path(role + '_api', item['api']))
        raw = read_path(role + '_archive', item['archive'], 512 * 1024**2)
        apis[role], artifact_raws[role] = api, raw
        require([item for item in artifacts['artifacts'] if item['name'] == name] == [api], 'Artifact differs from retained collection: ' + name)
        require(type(api['id']) is int and api['id'] > 0 and api['name'] == name
                and type(api['size_in_bytes']) is int and api['size_in_bytes'] == len(raw)
                and api['digest'] == 'sha256:' + sha(raw), 'Artifact ID/size/digest mismatch: ' + name)
        url = f'https://api.github.com/repos/{REPOSITORY}/actions/artifacts/{api["id"]}'
        require(api['url'] == url and api['archive_download_url'] == url + '/zip', 'Artifact API route differs')
        require(api['expired'] is False and stamp(api['expires_at']) > datetime.now(timezone.utc), 'Artifact retention elapsed')
        context = api['workflow_run']
        require(context['id'] == RUN and context['head_sha'] == SOURCE
                and context['repository_id'] == context['head_repository_id'] == run['repository']['id'], 'Artifact source/run/repository differs')
        if 'run_attempt' in context:
            require(type(context['run_attempt']) is int and context['run_attempt'] == 1, 'Artifact declared attempt differs')
        job, rows = (consumer, consumer_rows) if role == 'validator' else (producer, producer_rows)
        upload_step = step(job, upload_name)
        finish = f'Artifact {name} has been successfully uploaded! Final size is {len(raw)} bytes. Artifact ID is {api["id"]}'
        positions = [index for index, (_, message) in enumerate(rows) if message == finish]
        require(len(positions) == 1, 'Original upload ID/size/name record missing: ' + name)
        last = positions[0]
        first = max(index for index in range(last) if rows[index][1] == '##[group]Run actions/upload-artifact@v5')
        part = rows[first:last + 1]
        messages = [message for _, message in part]
        require(messages.count('  name: ' + name) == 1 and '  overwrite: false' in messages
                and messages.count('SHA256 digest of uploaded artifact zip is ' + sha(raw)) == 1,
                'Original upload action/digest differs')
        require(all(messages.count(path) == 1 for path in UPLOAD_PATHS[role]), 'Original source-defined upload paths differ')
        uploaded = [int(match[1]) for message in messages if (match := re.fullmatch(r'Uploaded bytes (\d+)', message))]
        require(uploaded and uploaded[-1] == len(raw) and uploaded == sorted(set(uploaded)), 'Original cumulative upload bytes differ')
        require(all(within(timestamp, upload_step) for timestamp in (rows[first][0], rows[last][0], api['created_at'], api['updated_at'])),
                'Original upload/API timestamp outside successful step')
        require(stamp(api['created_at']) <= stamp(api['updated_at'])
                and stamp(api['created_at']) <= stamp(rows[last][0]) + timedelta(seconds=1), 'Artifact API upload chronology differs')
        artifact_bindings[role] = dict(name=name, artifact_id=api['id'], bytes=len(raw), sha256=sha(raw),
                                      producing_job_id=job['id'], producing_attempt=job['run_attempt'],
                                      upload_step=upload_step, upload_start_utc=rows[first][0], upload_finish_utc=rows[last][0],
                                      api_created_at=api['created_at'], api_updated_at=api['updated_at'], expires_at=api['expires_at'],
                                      original_upload_id_size_digest_and_time_verified=True)
    require(len({api['id'] for api in apis.values()}) == 4, 'Artifact roles share an ID')

    download_step = step(consumer, 'Download Windows VST3')
    downloaded = selected_rows(consumer_rows, download_step)
    vst = apis['vst3']
    expected_receipt = f'- cs80-windows-vst3 (ID: {vst["id"]}, Size: {vst["size_in_bytes"]}, Expected Digest: {vst["digest"]})'
    expected_download_messages = [expected_receipt, 'SHA256 digest of downloaded artifact is ' + sha(artifact_raws['vst3']),
                                  'Artifact download completed successfully.',
                                  'Starting download of artifact to: ' + consumer_checkout['workspace'] + '\\vst3',
                                  '##[group]Run actions/download-artifact@v5', '  name: cs80-windows-vst3',
                                  '  path: vst3', '  repository: ' + REPOSITORY, '  run-id: ' + str(RUN)]
    require(all(sum(message == expected for _, message in downloaded) == 1 for expected in expected_download_messages),
            'Original consumer product ID/size/digest/completion/destination binding differs')
    selected = consumer_checkout['workspace'] + '\\vst3\\CS-80 Reference.vst3'
    require([message for _, message in consumer_rows if message.startswith('Started validating: ')]
            == ['Started validating: ' + selected] * 2, 'Original selected bundle paths/count differ')
    consumer_binding = dict(consumer_job_id=consumer['id'], attempt=1, download_step=download_step,
                            artifact_id=vst['id'], bytes=len(artifact_raws['vst3']), sha256=sha(artifact_raws['vst3']),
                            matched_original_rows=[dict(timestamp=t, message=m) for t, m in downloaded if m in expected_download_messages],
                            selected_bundle_path=selected, original_invocations=2,
                            independent_of_compiler_sidecar_presence=True)

    with zipfile.ZipFile(io.BytesIO(artifact_raws['native'])) as archive:
        require(len(archive.infolist()) == 3 and set(archive.namelist()) ==
                {'Testing/Temporary/LastTest.log', 'ctest-results.xml', 'ci-juce-provenance.txt'}, 'Native member roster differs')
        provenance_raw = archive.read('ci-juce-provenance.txt')
    field_pairs = [line.split('=', 1) for line in decode(provenance_raw).splitlines() if '=' in line]
    fields = dict(field_pairs)
    require(len(fields) == len(field_pairs), 'Duplicate native JUCE provenance field')
    require(all(fields.get(key) == value for key, value in dict(expected_sha=JUCE, actual_sha=JUCE,
                expected_version='8.0.11', actual_version='8.0.11', source_clean='true', verification='PASS',
                workflow_commit=SOURCE, workflow_run=str(RUN), source_override='').items())
            and fields.get('configured_source') == fields.get('expected_source') and fields.get('configured_source'),
            'Native clean JUCE/source provenance differs')
    lanes = []
    with zipfile.ZipFile(io.BytesIO(artifact_raws['validator'])) as archive:
        expected_members = {'pluginval-download.json', 'pluginval-version.txt', *[row[2] for row in LANES], *[row[3] for row in LANES]}
        require(len(archive.infolist()) == 6 and set(archive.namelist()) == expected_members, 'Validator text member roster differs')
        fetch_raw = archive.read('pluginval-download.json')
        fetch = check_download_receipt(a41.parse_json(fetch_raw))
        version_raw = archive.read('pluginval-version.txt')
        require(decode(version_raw).strip() == 'pluginval - 1.0.4'
                and sum(message == 'pluginval - 1.0.4' for _, message in runtimes['Fetch pluginval 1.0.4']) == 1,
                'Actual executed pluginval version differs')
        verified_line = 'Verified official pluginval 1.0.4 Windows asset 210999198: 2408590 bytes, SHA256 ' + fetch['actual_sha256']
        require(sum(message == verified_line for _, message in runtimes['Fetch pluginval 1.0.4']) == 1, 'Original verified-fetch output missing')
        for lane, name, console_name, report_name, fixed_seed in LANES:
            console_raw, report_raw = archive.read(console_name), archive.read(report_name)
            console, report = decode(console_raw).splitlines(), decode(report_raw).splitlines()
            require(console and console[0] == 'Started validating: ' + selected and console[1:] == report,
                    'Console/report content or selected path differs')
            runtime = runtimes[name]
            require([message for _, message in runtime] == console, 'Archived console differs from complete original runtime: ' + lane)
            entry = step(consumer, name)
            require(all(within(timestamp, entry) for timestamp, _ in runtime), 'Original validator output outside successful step')
            summary = parser.console_summary(console_raw)
            require(summary['strictness'] == [10] and summary['program_counts'] == [523]
                    and summary['validation_start_count'] == 1 and len(summary['seeds']) == 1
                    and summary['clean_text_success'] and not summary['editor_test_started'], 'Incomplete/failed headless validator lane')
            require(fixed_seed is None or summary['seeds'][0].lower() == fixed_seed, 'Fixed seed differs')
            lanes.append(dict(lane=lane, original_step=entry, console=identity(console_name, console_raw),
                              report=identity(report_name, report_raw), summary=summary,
                              first_output_utc=runtime[0][0], terminal_output_utc=runtime[-1][0],
                              original_normal_exit=0, numeric_child_exit_directly_logged=False,
                              exit_basis='Successful original PowerShell step and exact unsuppressed source command with immediate LASTEXITCODE check; complete runtime equals archived console.',
                              console_matches_original_runtime=True, console_report_equivalent=True,
                              invocations=1, validation_retries=0))
    acquisition = manifest.get('acquisition_receipts', [])
    require(isinstance(acquisition, list) and acquisition, 'Retained authenticated acquisition receipt paths required')
    for index, path in enumerate(acquisition):
        read_path('acquisition_receipt_' + str(index), path)
    return dict(schema=1, status='windows_original_job_upload_and_consumer_binding_verified', **CONTEXT,
                target='windows-x86_64', verified_at_utc=datetime.now(timezone.utc).isoformat(),
                producer_job_id=producer['id'], consumer_job_id=consumer['id'],
                original_required_producer_steps=producer_steps, original_required_consumer_steps=consumer_steps,
                producer_checkout=producer_checkout, consumer_checkout=consumer_checkout,
                source_workflow=dict(path='.github/workflows/build.yml', bytes=len(workflow_raw), sha256=sha(workflow_raw)),
                command_bindings=command_bindings, artifact_bindings=artifact_bindings,
                original_ctest_exit_zero_verified=True, numeric_ctest_exit_directly_logged=False,
                ctest_exit_basis='Successful original Test step, exact direct source CTest command, complete original terminal summary, and no error suppression.',
                original_clean_checkout_juce_build_test_package_sequence_verified=True,
                juce_provenance=dict(fields=fields, **identity('ci-juce-provenance.txt', provenance_raw)),
                consumer_download_binding=consumer_binding, validators=dict(required_lanes=2, actual_invocations=2,
                    validation_retries=0, lanes=lanes, pinned_download=fetch, pinned_download_member=identity('pluginval-download.json', fetch_raw),
                    version_member=identity('pluginval-version.txt', version_raw)),
                timestamp_tolerance='API whole-second completed_at is inclusive through the following second; starts have no negative tolerance.',
                log_representation='Raw logs retained by hash. Decode BOM/UTF-16/UTF-8 and CRLF; timestamp-free multiline upload path metadata inherits its immediately preceding framed path-input time. Runtime messages retain exact content; runner ANSI highlighting is removed only for source-command comparison.',
                input_records=input_records, checker_sha256=sha(Path(__file__).read_bytes()), helper_sha256=PINS,
                api_authenticity_independently_established_by_this_offline_helper=False,
                authenticity_boundary='Root must review the retained authenticated acquisition receipts. This helper checks raw input consistency and original binding only.',
                native_results_accepted=False, platform_qualified=False, release_qualified=False,
                remaining_gates=['A40 exact 255-name native content and separate formal/nested skip review.',
                                 'A52 parsed-archive/member binding to these exact native/report inputs.',
                                 'A41 complete package inventory and headers; A46 exact guides/notices.',
                                 'A36 complete distributed-to-consumed VST3 equality and exact optional compiler-sidecar pair.',
                                 'Final platform receipt composing all checks; all other platforms and release gates remain separate.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--inputs', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    require(not args.out.exists() and not args.out.is_symlink(), 'Refusing to replace original-binding evidence')
    result = verify(args.repo, args.inputs)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open('x') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print(json.dumps(dict(status=result['status'], producer_job_id=result['producer_job_id'],
                          consumer_job_id=result['consumer_job_id'], platform_qualified=False, release_qualified=False)))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, TypeError, OSError, UnicodeError, StopIteration, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        print('Windows original binding failed: ' + str(error), file=sys.stderr)
        raise SystemExit(2)
