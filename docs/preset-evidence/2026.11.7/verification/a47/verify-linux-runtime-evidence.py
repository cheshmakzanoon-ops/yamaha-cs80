#!/usr/bin/env python3
"""Bind frozen-source original Linux jobs, uploads, consumption and lane history.

Offline text/archive inspection only. This supplement does not authenticate its
inputs, accept the full native roster/skips or installation, or qualify a release.
All original files are read-only; outputs must be new. No product binary runs.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import signal
import stat
import subprocess
import sys
import zipfile

sys.dont_write_bytecode = True
SOURCE = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
TREE = '17115913cbb29157bd00539485bcc5f38b57219c'
RUN = 45397189410
ATTEMPT = 1
PRODUCER = 138455975798
REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
CONTEXT = dict(factory_revision='2026.11.7', source=SOURCE, tree=TREE,
               run_id=RUN, attempt=ATTEMPT)
HERE = Path(__file__).resolve().parent
ANSI = re.compile(r'\x1b\[[0-9;]*m')
TIME = re.compile(r'^(\d{4}-\d\d-\d\dT[^ ]+Z) (.*)$')
SKIPPED = {'Diagnose validator failure with gdb',
           'Diagnose validator with fault-thread trace', 'Report pluginval failure'}
UPLOADS = {'native': ('cs80-linux-test-evidence', 'Upload test and JUCE source evidence'),
           'vst3': ('cs80-linux-vst3', 'Upload Linux VST3'),
           'install': ('cs80-linux-install-bundle', 'Upload Linux install bundle'),
           'validator': ('cs80-pluginval-reports', 'Upload pluginval reports')}


def need(ok, reason):
    if not ok:
        raise ValueError(reason)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def unique(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, 'Duplicate JSON key: ' + key)
        result[key] = value
    return result


def snapshot(path, limit=128 * 1024**2):
    with os.fdopen(os.open(path, os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0)), 'rb') as stream:
        before = os.fstat(stream.fileno())
        need(stat.S_ISREG(before.st_mode) and 0 < before.st_size <= limit, 'Invalid input size/type: ' + str(path))
        raw = stream.read(limit + 1)
        after = os.fstat(stream.fileno())
        need((before.st_ino, before.st_size, before.st_mtime_ns) ==
             (after.st_ino, after.st_size, after.st_mtime_ns) and len(raw) == before.st_size,
             'Input changed while reading: ' + str(path))
    return raw


def json_input(path):
    raw = snapshot(path, 32 * 1024**2)
    value = json.loads(raw, object_pairs_hook=unique)
    need(isinstance(value, dict), 'Expected an object: ' + str(path))
    return value, record(raw, str(path))


def record(raw, path):
    return dict(path=path, bytes=len(raw), sha256=sha(raw))


def text(raw):
    encoding = 'utf-16' if raw[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig'
    return raw.decode(encoding).replace('\r\n', '\n')


def lines(raw):
    value = text(raw)
    # Only text encoding, CRLF representation and the final line terminator are
    # normalized. Do not trim content or strip output escape sequences.
    return value.removesuffix('\n').split('\n')


def stamp(value):
    result = datetime.fromisoformat(value.replace('Z', '+00:00'))
    need(result.tzinfo is not None, 'Missing timestamp timezone')
    return result


def within(value, step):
    return (value is not None and stamp(step['started_at']) <= stamp(value)
            < stamp(step['completed_at']) + timedelta(seconds=1))


def log_groups(raw):
    records, last_time = [], None
    for line in lines(raw):
        match = TIME.match(line)
        if match:
            last_time, line = match.groups()
        records.append(dict(timestamp=last_time, message=line))
    starts = [i for i, row in enumerate(records) if row['message'].startswith('##[group]Run ')]
    groups = []
    for n, start in enumerate(starts):
        end = starts[n + 1] if n + 1 < len(starts) else len(records)
        header_end = next((i for i in range(start + 1, end)
                           if records[i]['message'] == '##[endgroup]'), None)
        need(header_end is not None, 'Unclosed original command group')
        header = records[start + 1:header_end]
        runtime = records[header_end + 1:end]
        # Cleanup is outside the final source-defined action, not its output.
        cleanup = next((i for i, row in enumerate(runtime)
                        if row['message'] == 'Post job cleanup.'), len(runtime))
        command = [ANSI.sub('', row['message']) for row in header
                   if row['message'].startswith('\x1b[36;1m')]
        groups.append(dict(timestamp=records[start]['timestamp'],
                           heading=records[start]['message'], header=header,
                           command=command, runtime=runtime[:cleanup],
                           original_line_start=start + 1,
                           original_line_end=header_end + 1 + cleanup))
    return groups


def messages(group):
    return [row['message'] for row in group['runtime']]


def job_check(job, expected_id, expected_name):
    need(type(expected_id) is int and expected_id > 0, 'Actual consumer job ID is required')
    need(job['id'] == expected_id and job['name'] == expected_name, 'Original job identity differs')
    need(job['run_id'] == RUN and job['run_attempt'] == ATTEMPT and job['head_sha'] == SOURCE,
         'Original job source/run/attempt differs')
    need(job['status'] == 'completed' and job['conclusion'] == 'success', 'Original job is not successful')
    need(job['url'] == f'https://api.github.com/repos/{REPOSITORY}/actions/jobs/{expected_id}',
         'Original job API route differs')


def bind_steps(job, groups, source_steps):
    bound = {}
    for index, source_step in enumerate(source_steps):
        name = source_step.get('name', 'Run ' + source_step.get('uses', ''))
        name = name.replace('${{ env.PLUGINVAL_VERSION }}', '1.0.4')
        found = [step for step in job['steps'] if step['name'] == name]
        need(len(found) == 1, 'Missing/nonunique original step: ' + name)
        step = found[0]
        need(step['number'] == index + 2 and step['status'] == 'completed', 'Source step order/status differs: ' + name)
        if name in SKIPPED:
            need(step['conclusion'] == 'skipped', 'Failure diagnostic/report step ran: ' + name)
            continue
        need(step['conclusion'] == 'success', 'Original step did not succeed: ' + name)
        candidates = [g for g in groups if within(g['timestamp'], step)]
        if 'uses' in source_step:
            candidates = [g for g in candidates if g['heading'] == '##[group]Run ' + source_step['uses']]
            if 'name' in source_step.get('with', {}):
                artifact_name = source_step['with']['name'].replace('${{ github.sha }}', SOURCE)
                candidates = [g for g in candidates if '  name: ' + artifact_name
                              in [r['message'] for r in g['header']]]
        else:
            expected = source_step['run'].replace('${{ github.workspace }}', bound['workspace']).splitlines()
            candidates = [g for g in candidates if g['command'] == expected]
        need(len(candidates) == 1, 'Exact source command/action is absent or nonunique: ' + name)
        group = candidates[0]
        need(not any(line.startswith(('##[error]', '::error::')) or
                     re.search(r'^Process completed with exit code [1-9]', line)
                     for line in messages(group)), 'Original runtime contains an error: ' + name)
        if name == 'Run actions/checkout@v5':
            need(messages(group).count(SOURCE) == 1, 'Original checkout SHA is absent/nonunique')
            workspaces = [m[1] for line in messages(group)
                          if (m := re.fullmatch(r"Working directory is '(/[^']+)'", line))]
            need(len(workspaces) == 1, 'Original workspace is absent/nonunique')
            bound['workspace'] = workspaces[0]
        bound[name] = dict(api=step, group=group)
    ordered = [v['api'] for k, v in bound.items() if k != 'workspace']
    need(all(stamp(a['completed_at']) <= stamp(b['started_at']) + timedelta(seconds=1)
             for a, b in zip(ordered, ordered[1:])), 'Original source steps overlap/out of order')
    return bound


def archive_record(path, retain_text=False):
    members = {}
    with os.fdopen(os.open(path, os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0)), 'rb') as stream:
        before = os.fstat(stream.fileno())
        need(stat.S_ISREG(before.st_mode) and 0 < before.st_size <= 512 * 1024**2, 'Archive size/type bound')
        digest = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
        if retain_text:
            stream.seek(0)
            with zipfile.ZipFile(stream) as archive:
                infos = archive.infolist()
                need(len(infos) <= 4096 and len({i.filename for i in infos}) == len(infos), 'Duplicate/excess archive members')
                need(sum(i.file_size for i in infos) <= 128 * 1024**2, 'Evidence archive expanded bound')
                for info in infos:
                    need(not info.flag_bits & 1, 'Encrypted evidence archive')
                    if not info.is_dir():
                        need(info.file_size <= 32 * 1024**2, 'Evidence member size bound')
                        members[info.filename] = archive.read(info)
        after = os.fstat(stream.fileno())
        need((before.st_ino, before.st_size, before.st_mtime_ns) ==
             (after.st_ino, after.st_size, after.st_mtime_ns), 'Archive changed while reading')
    return dict(path=str(path), bytes=before.st_size, sha256=digest.hexdigest()), members


def artifact_binding(key, item, collection, run, step):
    api, api_record = json_input(item['api'])
    name, _ = UPLOADS[key]
    need([a for a in collection['artifacts'] if a['name'] == name] == [api], 'Artifact collection/object differs: ' + key)
    need(api['name'] == name and type(api['id']) is int and api['id'] > 0, 'Artifact identity differs')
    route = f'https://api.github.com/repos/{REPOSITORY}/actions/artifacts/{api["id"]}'
    need(api['url'] == route and api['archive_download_url'] == route + '/zip', 'Artifact API route differs')
    need(api['expired'] is False and stamp(api['expires_at']) > datetime.now(timezone.utc), 'Artifact retention expired/unknown')
    workflow = api['workflow_run']
    need(workflow['id'] == RUN and workflow['head_sha'] == SOURCE and
         workflow['repository_id'] == workflow['head_repository_id'] == run['repository']['id'], 'Artifact run/source/repository differs')
    archive, members = archive_record(item['archive'], key in ('native', 'validator'))
    need(api['size_in_bytes'] == archive['bytes'] and api['digest'] == 'sha256:' + archive['sha256'], 'Artifact bytes/digest differs')
    output = messages(step['group'])
    size, digest, identity = archive['bytes'], archive['sha256'], api['id']
    terminal = f'Artifact {name} has been successfully uploaded! Final size is {size} bytes. Artifact ID is {identity}'
    need(output.count(terminal) == 1 and output.count('SHA256 digest of uploaded artifact zip is ' + digest) == 1,
         'Original upload ID/size/digest differs: ' + key)
    need(output.count(f'Artifact {name}.zip successfully finalized. Artifact ID {identity}') == 1,
         'Original upload finalization differs: ' + key)
    uploaded = [int(m[1]) for line in output if (m := re.fullmatch(r'Uploaded bytes (\d+)', line))]
    need(uploaded and uploaded[-1] == size and uploaded == sorted(set(uploaded)), 'Original cumulative uploaded bytes differ')
    need(output.count(f'Artifact download URL: https://github.com/{REPOSITORY}/actions/runs/{RUN}/artifacts/{identity}') == 1,
         'Original upload download URL differs')
    final = [r for r in step['group']['runtime'] if r['message'] == terminal][0]
    need(all(within(t, step['api']) for t in [final['timestamp'], api['created_at'], api['updated_at']]),
         'Artifact timestamps fall outside original upload step')
    return dict(artifact=api, metadata=api_record, archive=archive,
                original_upload_step=step['api'], original_upload_utc=final['timestamp']), members


def terminations(raw):
    value = text(raw)
    blocks = value.split('=== validator attempt: local core diagnostics ===\n')
    need(blocks[0] == '' and len(blocks) > 1, 'Missing/extra core-log attempt framing')
    result = []
    for block in blocks[1:]:
        norms = re.findall(r'^Original validator PID: (\d+); exit: (\d+)$', block, re.M)
        terms = re.findall(r'^Original validator termination: PID=(\d+); raw returncode=(-?\d+); kind=(exit|signal); ([^\n]+)$', block, re.M)
        need(len(norms) == len(terms) == 1, 'Core attempt lacks exactly one normalized/raw termination')
        pid, status = map(int, norms[0])
        term_pid, code, kind, details = terms[0]
        code = int(code)
        need(pid > 0 and pid == int(term_pid) and status == (code if code >= 0 else 128 - code), 'PID or normalized/raw exit mismatch')
        if code >= 0:
            need(kind == 'exit' and details == f'exit_code={code}', 'Raw exit mislabeled as a signal')
        else:
            expected_name = signal.Signals(-code).name
            need(kind == 'signal' and details == f'signal_number={-code}; signal_name={expected_name}', 'Raw signal details differ')
        result.append(dict(pid=pid, raw_returncode=code, normalized_exit=status, kind=kind))
    need(len({r['pid'] for r in result}) == len(result), 'Duplicate original invocation PID')
    return result


def retry_eligible(raw, termination):
    value = text(raw)
    started, phase = False, None
    for line in value.splitlines():
        # EXACT frozen AWK regexes: do not make the colon optional.
        if 'Starting tests in:' in line:
            started, phase = True, 'started'
        if 'Completed tests in:' in line:
            phase = 'completed'
    return (termination['normalized_exit'] == 139 and 'SUCCESS' not in value
            and 'FAILED' not in value and started and phase == 'completed')


def retry_warning(next_attempt):
    return ('Exit 139 after a completed test phase, without SUCCESS or FAILED. '
            'Crash ownership is unproven; retrying the full command '
            f'(next attempt {next_attempt} of 3).')


def lane_binding(lane, members, step, workspace):
    console, report, core = (members[lane[k]] for k in ('console', 'report', 'core'))
    cl = lines(console)
    selected = workspace + '/vst3/CS-80 Reference.vst3'
    need(cl[0] == 'Started validating: ' + selected and
         sum(line.startswith('Started validating: ') for line in cl) == 1, 'Wrong/nonunique selected VST3 path')
    need(cl[1:] == lines(report), 'Full report differs from final console beyond selected-path header')
    need(cl.count('Validation started') == cl.count('Strictness level: 10') == cl.count('Num programs: 523') == 1,
         'Final lane start/strictness/program contract differs')
    need(len([line for line in cl if line.startswith('Strictness level:')]) == 1 and
         len([line for line in cl if line.startswith('Num programs:')]) == 1, 'Extra strictness/program records')
    seeds = [m[1] for line in cl if (m := re.fullmatch(r'Random seed: (0x[0-9a-fA-F]+)', line))]
    need(len(seeds) == 1 and (lane['fixed_seed'] is None or seeds[0].lower() == lane['fixed_seed']), 'Seed contract differs')
    need(cl.count('SUCCESS') == 1 and cl[-1] == 'SUCCESS' and not any('FAILED' in line or
         (line.startswith('!!!') and not line.startswith('!!! WARNING:')) for line in cl), 'Missing clean terminal success')
    gui_phases = ['Editor', 'Open editor whilst processing', 'Editor Automation']
    for phase in gui_phases:
        start, done = 'Starting tests in: pluginval / ' + phase + '...', 'Completed tests in pluginval / ' + phase
        if lane['gui']:
            need(cl.count(start) == cl.count(done) == 1 and cl.index(start) < cl.index(done), 'GUI phase missing/incomplete: ' + phase)
        else:
            need(start not in cl and done not in cl, 'Headless lane contains GUI phase')
    stem = lane['console'][:-4]
    prior = sorted(name for name in members if re.fullmatch(re.escape(stem) + r'-attempt\d+\.log', name))
    need(prior == [f'{stem}-attempt{i}.log' for i in range(1, len(prior) + 1)] and len(prior) <= 2, 'Noncontiguous/excess failed attempts')
    terms = terminations(core)
    need(len(terms) == len(prior) + 1 and terms[-1]['raw_returncode'] == 0 and
         terms[-1]['kind'] == 'exit', 'Missing actual final normal-zero exit or history count mismatch')
    runtime = messages(step['group'])
    cursor, attempts = 0, []
    for number, name in enumerate(prior + [lane['console']], 1):
        content = members[name]
        invocation = lines(content)
        need(invocation and invocation[0] == 'Started validating: ' + selected and
             sum(line.startswith('Started validating: ') for line in invocation) == 1,
             'Prior/final invocation selected another product')
        need(runtime[cursor:cursor + len(invocation)] == invocation, 'Archived invocation does not exactly match original step segment: ' + name)
        cursor += len(invocation)
        eligible = None
        if number <= len(prior):
            eligible = retry_eligible(content, terms[number - 1])
            need(eligible, 'Preserved attempt is ineligible under exact source wrapper: ' + name)
            warning = retry_warning(number + 1)
            allowed = ('##[warning]' + warning, '::warning title=pluginval post-test SIGSEGV::' + warning)
            need(cursor < len(runtime) and runtime[cursor] in allowed, 'Missing/changed original retry warning')
            cursor += 1
        attempts.append(dict(number=number, console=record(content, name),
                             termination=terms[number - 1], retry_eligible=eligible))
    need(cursor == len(runtime), 'Unmatched original invocation, warning or trailing runtime output')
    return dict(lane=lane['lane'], workflow_step=step['api'], seed=seeds[0],
                report=record(report, lane['report']), core=record(core, lane['core']),
                attempts=attempts, invocations=len(attempts), retries=len(prior),
                complete_console_report_and_original_segments_match=True,
                original_normal_exit_zero=True,
                warnings=[line for line in cl if line.startswith('!!! WARNING:')],
                explicit_skips=[line for line in cl if re.search(r'\b[Ss]kipping\b', line)])


def main(args):
    manifest, manifest_record = json_input(args.inputs)
    need(manifest['provenance'] == CONTEXT, 'Input context differs')
    contract, contract_record = json_input(HERE / 'acceptance-contract.json')
    need(contract['provenance'] == CONTEXT, 'Preparation context differs')
    repo = args.repo.resolve()
    need(subprocess.check_output(['git', '-C', str(repo), 'rev-parse', SOURCE + '^{tree}']).decode().strip() == TREE, 'Frozen source tree differs')
    for path, expected in contract['source_files'].items():
        raw = subprocess.check_output(['git', '-C', str(repo), 'show', SOURCE + ':' + path])
        need(dict(bytes=len(raw), sha256=sha(raw)) == expected, 'Frozen source input differs: ' + path)
    for path, expected in contract['helpers'].items():
        raw = snapshot(repo / path)
        need(dict(bytes=len(raw), sha256=sha(raw)) == expected, 'Prepared helper changed: ' + path)
    import yaml
    workflow = yaml.safe_load(subprocess.check_output(['git', '-C', str(repo), 'show', SOURCE + ':.github/workflows/build.yml']))
    need({k: workflow['jobs'][k]['steps'] for k in ('linux', 'pluginval')} == contract['source_steps'], 'Prepared command roster differs from source')
    inputs, records = {}, [manifest_record, contract_record]
    for key in ('run_api', 'producer_job_api', 'validator_job_api', 'artifacts_api'):
        inputs[key], rec = json_input(manifest[key]); records.append(rec)
    run = inputs['run_api']
    need(run['id'] == RUN and run['run_attempt'] == ATTEMPT and run['head_sha'] == SOURCE
         and run['path'] == '.github/workflows/build.yml', 'Run identity differs')
    need(run['repository']['full_name'] == run['head_repository']['full_name'] == REPOSITORY, 'Run repository differs')
    producer, consumer = inputs['producer_job_api'], inputs['validator_job_api']
    job_check(producer, PRODUCER, 'Linux / VST3 + Standalone')
    job_check(consumer, manifest['consumer_job_id'], 'pluginval / Linux VST3 (strictness 10)')
    bound = {}
    for role, job, source_key in [('producer', producer, 'linux'), ('validator', consumer, 'pluginval')]:
        raw = snapshot(manifest[role + '_job_log'])
        records.append(record(raw, manifest[role + '_job_log']))
        bound[role] = bind_steps(job, log_groups(raw), workflow['jobs'][source_key]['steps'])
    producer_steps, consumer_steps = bound['producer'], bound['validator']
    artifacts, archives = {}, {}
    for key, (name, upload_name) in UPLOADS.items():
        steps = consumer_steps if key == 'validator' else producer_steps
        artifacts[key], archives[key] = artifact_binding(key, manifest['artifacts'][key], inputs['artifacts_api'], run, steps[upload_name])
    product = artifacts['vst3']['artifact']
    download = messages(consumer_steps['Download Linux VST3']['group'])
    expected = f'- cs80-linux-vst3 (ID: {product["id"]}, Size: {product["size_in_bytes"]}, Expected Digest: {product["digest"]})'
    need(download.count(expected) == 1 and download.count('SHA256 digest of downloaded artifact is ' + product['digest'][7:]) == 1
         and download.count('Artifact download completed successfully.') == 1, 'Original consumer product download differs')
    download_header = [r['message'] for r in consumer_steps['Download Linux VST3']['group']['header']]
    need('  path: vst3' in download_header, 'Original consumer download destination differs')
    members = archives['validator']
    expected_members = {'pluginval-version.txt', 'pluginval-core-fixture.log', 'pluginval-gdb-normal-exit.log'}
    for lane in contract['lanes']:
        expected_members.update(lane[key] for key in ('console', 'report', 'core'))
        stem = lane['console'][:-4]
        expected_members.update(name for name in members
                                if re.fullmatch(re.escape(stem) + r'-attempt\d+\.log', name))
    need(set(members) == expected_members, 'Unexpected/missing validator member; inspect all unrecognized retry or diagnostic evidence')
    need(set(archives['native']) == {'ctest-results.xml', 'Testing/Temporary/LastTest.log', 'ci-juce-provenance.txt'},
         'Unexpected/missing original native evidence members')
    need(lines(members['pluginval-version.txt']) == ['pluginval - 1.0.4'], 'Archived version differs')
    need(messages(consumer_steps['Fetch pluginval 1.0.4']['group']).count('pluginval - 1.0.4') == 1, 'Original version output differs')
    lanes = [lane_binding(lane, members, consumer_steps[lane['step']], consumer_steps['workspace']) for lane in contract['lanes']]
    core_lines = sum((lines(members[lane['core']]) for lane in contract['lanes']), [])
    need(messages(consumer_steps['Show first-attempt core diagnostics']['group']) == core_lines, 'Complete archived core logs differ from original ordered job output')
    fixture_output = messages(consumer_steps['Verify diagnostic exit propagation']['group'])
    need(fixture_output.count('Diagnostic exit propagation and normal-inferior guard checked.') == 1, 'Diagnostic propagation completion missing')
    fixtures = terminations(members['pluginval-core-fixture.log'])
    need([(r['raw_returncode'], r['kind']) for r in fixtures[:3]] == [(0, 'exit'), (7, 'exit'), (139, 'exit')], 'Diagnostic fixture exit propagation differs')
    pipe_skip = 'Synthetic signal fixture unavailable with kernel pipe collector; exit139 fixture remains required.'
    if pipe_skip in fixture_output:
        need(fixture_output.count(pipe_skip) == 1 and len(fixtures) == 3, 'Pipe-collector fixture history differs')
    else:
        need(len(fixtures) == 4 and fixtures[3]['raw_returncode'] == -11 and fixtures[3]['kind'] == 'signal', 'Original synthetic SIGSEGV fixture missing')
    need(re.search(r'^\[Inferior \d+ \(process \d+\) exited normally\]$', text(members['pluginval-gdb-normal-exit.log']), re.M), 'GDB normal-exit fixture incomplete')
    diagnostics = [name for name in members if name.startswith(('pluginval-diagnostic-reports/', 'pluginval-trace-reports/'))
                   or name in ('pluginval-gui-gdb.log', 'pluginval-gui-trace.log', 'pluginval-fault-trace.log')]
    need(not diagnostics, 'Diagnostic invocations coexist with skipped source diagnostic steps')
    parser_path = repo / 'docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py'
    spec = importlib.util.spec_from_file_location('a47_unchanged_a52', parser_path)
    parser = importlib.util.module_from_spec(spec); spec.loader.exec_module(parser)
    native_text = text(archives['native']['Testing/Temporary/LastTest.log'])
    audit = parser.lasttest_cases(native_text)['cs80_library_audit_tests']
    need(audit['lasttest_status'] == 'Passed' and audit['output'].splitlines().count(contract['library_audit']['final_marker']) == 1,
         'Full 523-program LibraryAudit completion missing')
    need(len(re.findall(r'^phrase peak max .+ dBFS, loudest-window RMS .+ dBFS, stress peak max .+ dBFS$', audit['output'], re.M)) == 1,
         'LibraryAudit render phase missing (data-only execution is not accepted)')
    test_group = producer_steps['Test']['group']
    need(messages(test_group).count('100% tests passed, 0 tests failed out of 613') == 1, 'Original serial CTest terminal summary differs')
    need(not any('CS80_LIBRARY_AUDIT_DATA_ONLY' in r['message'] for r in test_group['header']), 'LibraryAudit data-only environment present')
    juce = messages(producer_steps['Verify configured JUCE source']['group'])
    need(juce.count('-- Verified clean JUCE 8.0.11 at c352e2489000ffeb9e397051377d75eef2efe641; evidence: '
                    + producer_steps['workspace'] + '/build/ci-juce-provenance.txt') == 1, 'Original clean JUCE confirmation differs')
    result = dict(schema=1, status='original_linux_runtime_binding_verified_external_gates_pending', provenance=CONTEXT,
                  target='linux-x86_64', producer_job_id=PRODUCER, consumer_job_id=consumer['id'],
                  run_status=run['status'], run_conclusion=run['conclusion'], artifact_bindings=artifacts,
                  original_source_commands_and_steps_verified=True, original_consumer_product_bound=True,
                  original_ctest_normal_zero_exit_bound_by_unsuppressed_successful_step=True,
                  library_audit_full_render_and_523_program_completion=True, validator_lanes=lanes,
                  validation_invocations=sum(r['invocations'] for r in lanes), validation_retries=sum(r['retries'] for r in lanes),
                  diagnostic_invocations=0, diagnostic_fixture_terminations=fixtures,
                  runtime_warnings={role: [dict(step=k, line=line) for k, item in steps.items() if k != 'workspace'
                                    for line in messages(item['group']) if 'warning' in line.lower()]
                                    for role, steps in bound.items()},
                  source_step_records={role: {k: dict(api=v['api'], command=v['group']['command'],
                      original_line_start=v['group']['original_line_start'], original_line_end=v['group']['original_line_end'])
                      for k, v in steps.items() if k != 'workspace'} for role, steps in bound.items()},
                  input_records=records, checker=record(Path(__file__).read_bytes(), str(Path(__file__).resolve())),
                  limitations=['Input authentication/acquisition receipts remain external.',
                    'A40 must independently accept the exact 613-name native roster, JUnit/LastTest, formal/nested skip review and UI/body phase.',
                    'A52 parser/archive binder, A41 installation, A30 guides/notices and A36 complete product/package equality remain required.',
                    'Linux pluginval has source-pinned version/URL but no independently pinned asset-digest receipt.',
                    'Audition content/CI acceptance belongs to A50; other platforms and global release require root aggregation.'],
                  actual_artifacts_checked=True, native_results_accepted=False, platform_qualified=False, release_qualified=False)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open('x') as stream:
        json.dump(result, stream, indent=2, allow_nan=False); stream.write('\n')
    print(json.dumps(dict(status=result['status'], lanes=len(lanes), invocations=result['validation_invocations'],
                          platform_qualified=False, release_qualified=False)))


if __name__ == '__main__':
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--repo', type=Path, required=True)
    cli.add_argument('--inputs', type=Path, required=True)
    cli.add_argument('--out', type=Path, required=True)
    try:
        main(cli.parse_args())
    except (ValueError, KeyError, OSError, zipfile.BadZipFile) as error:
        print('ERROR: ' + str(error), file=sys.stderr)
        sys.exit(2)
