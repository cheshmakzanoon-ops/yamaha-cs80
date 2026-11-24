#!/usr/bin/env python3
"""Bind this one actual ef1c472 Intel execution; no product execution or publication.

Recovered source: final commit plus original Actions artifacts and authenticated
API/log inputs acquired after the transient workspace reset. Historical helper
bytes are reused unchanged only for parsing and inspection. No previous native
result is reused. Source/run/job/ZIP and reviewed receipt pins are fixed below.
"""
from __future__ import annotations

import argparse
import ast
from datetime import datetime, timezone
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import struct
import subprocess
import tarfile
import zipfile

SOURCE = 'ef1c472b161c37c490945f7c469086508aae6aef'
TREE = 'f4e00753cde135a36172d9ea549223dfdce5cc82'
RUN, ATTEMPT, JOB, REPO_ID = 45439302351, 1, 138605635495, 1436742091
REPOSITORY, REVISION, TARGET = 'cheshmakzanoon-ops/yamaha-cs80', '2026.11.8', 'macos-x86_64'
JUCE = 'c352e2489000ffeb9e397051377d75eef2efe641'
COUNTS = dict(registered=283, passed=281, failed=0, formal_skipped=2, disabled=0, unknown=0)
ARTIFACTS = {
    'native': (14947404390, 'cs80-macos-15-intel-test-evidence', 89932, '5d4dc86a704d09d4921e84061ed5e6ae90315700565c1a19b1c35fcceffffbe8', 'Upload test and JUCE source evidence'),
    'validator': (14946709999, 'cs80-pluginval-macos-15-intel', 12719, '41b9577b65b5c093361fdf190732d224fc579c1467df8213f6a00852cb30562c', 'Upload pluginval reports'),
    'install': (14946874694, 'cs80-macos-15-intel-install-bundle', 16286380, 'a8784577d2c04ea41df7dc2dd4f819338566953cb22469315e865ec446888015', 'Upload native macOS install bundle'),
}
HELPERS = {
    'parser': ('parse-native-evidence.py', 'f192bf2ff70d4501f73875eb5bb6416b9adc3b77912e165da4305302724cd846'),
    'binder': ('bind-parser-archives.py', '343cbc293d388fcf72484e05eaf52933e1bdbed3e13f194e59799af2cdf58b11'),
    'log_parser': ('historical-macos-qualification.py', '0b881f00d480ef32d31f6866eb03a3aa31a5ae809de05bafa9ad0d816f3b4fb1'),
}


def need(ok, message):
    if not ok:
        raise ValueError(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def record(raw, name):
    return dict(path=str(name), bytes=len(raw), sha256=sha(raw))


def raw_file(path, limit=32 * 1024**2):
    need(path.is_file() and not path.is_symlink(), 'Nonregular input: ' + str(path))
    with path.open('rb') as stream:
        raw = stream.read(limit + 1)
    need(0 < len(raw) <= limit, 'Input size bound: ' + str(path))
    return raw


def parse(raw):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            need(key not in result, 'Duplicate JSON key: ' + key)
            result[key] = value
        return result
    return json.loads(raw, object_pairs_hook=unique)


def module(path, expected, name):
    need(sha(raw_file(path)) == expected, 'Helper bytes changed: ' + str(path))
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo', type=Path, required=True)
    ap.add_argument('--a46', type=Path, required=True)
    ap.add_argument('--inputs-dir', type=Path, default=Path(__file__).resolve().parent)
    ap.add_argument('--artifacts', type=Path, required=True)
    ap.add_argument('--out-dir', type=Path, required=True)
    args = ap.parse_args()
    need(not args.out_dir.exists() and not args.out_dir.is_symlink(), 'Use a new output directory')
    base, original = args.inputs_dir, args.artifacts
    input_records, source_records, raw_sources = {}, {}, {}

    def load(name, parent=base):
        raw = raw_file(parent / name)
        input_records[str(parent / name)] = record(raw, parent / name)
        return parse(raw)

    def source(path):
        if path not in raw_sources:
            raw_sources[path] = subprocess.check_output(['git', '-C', str(args.repo), 'show', SOURCE + ':' + path])
            source_records[path] = record(raw_sources[path], path)
        return raw_sources[path]

    pins = load('reviewed-input-pins.json')
    need(pins['source'] == SOURCE and pins['job_id'] == JOB, 'Reviewed pin context')
    for relative, expected in pins['inputs'].items():
        need(sha(raw_file(base / relative)) == expected, 'Reviewed receipt changed: ' + relative)
    need(sha(raw_file(args.a46 / 'source-contract.json')) == pins['a46_source_contract_sha256'], 'Source roster contract changed')
    parser = module(args.a46 / 'helpers' / HELPERS['parser'][0], HELPERS['parser'][1], 'a49_parser')
    binder = module(args.a46 / 'helpers' / HELPERS['binder'][0], HELPERS['binder'][1], 'a49_binder')
    old = module(base / 'helpers' / HELPERS['log_parser'][0], HELPERS['log_parser'][1], 'a49_old_log_utilities')
    need(subprocess.check_output(['git', '-C', str(args.repo), 'rev-parse', SOURCE + '^{tree}'], text=True).strip() == TREE, 'Frozen tree differs')
    workflow = source('.github/workflows/build.yml').decode()
    need(b'curatedLibraryRevision = "2026.11.8"' in source('src/core/FactoryPresets.h'), 'Source revision differs')
    runs = [load('run-api.json'), load('attempt-api.json')]
    for run in runs:
        need((run['id'], run['head_sha'], run['run_attempt']) == (RUN, SOURCE, ATTEMPT), 'Original run/attempt differs')
        need(run['event'] == 'push' and run['path'] == '.github/workflows/build.yml', 'Workflow/event differs')
        need(all(run[k]['id'] == REPO_ID and run[k]['full_name'] == REPOSITORY for k in ('repository', 'head_repository')), 'Run repository differs')
    jobs = load('jobs-api.json')['jobs']
    matches = [j for j in jobs if j['id'] == JOB]
    need(len(matches) == 1, 'Original Intel job absent/duplicate')
    job = matches[0]
    need((job['run_id'], job['run_attempt'], job['head_sha']) == (RUN, ATTEMPT, SOURCE), 'Job binding differs')
    need(job['status'] == 'completed' and job['conclusion'] == 'success' and job['runner_id'] > 0 and job['runner_name'], 'Original job did not succeed on a runner')
    need(job['labels'] == ['macos-15-intel'] and job['name'] == 'macOS / macos-15-intel / VST3 + AU + Standalone', 'Job target differs')
    steps = {s['name']: s for s in job['steps']}
    need(len(steps) == len(job['steps']), 'Duplicate step')
    for name in old.REQUIRED:
        step = steps[name]
        need(step['status'] == 'completed' and step['conclusion'] == 'success', 'Original step failed: ' + name)
        need(old.dt(job['started_at']) <= old.dt(step['started_at']) <= old.dt(step['completed_at']) <= old.dt(job['completed_at']), 'Step outside job')
    for previous, following in zip(old.REQUIRED, old.REQUIRED[1:]):
        need(steps[previous]['number'] < steps[following]['number'] and old.dt(steps[previous]['completed_at']) <= old.dt(steps[following]['started_at']), 'Step order differs')
    need(all(steps[n]['conclusion'] == 'skipped' for n in ('AU registration diagnostics', 'Report pluginval failure')), 'Unexpected failure diagnostic')

    log_raw = raw_file(original / 'macos-x86_64-job.log')
    need(len(log_raw) == 263197 and sha(log_raw) == '89fc20bca65f21c543c240fde390e65a6d25ff6b9354b8ffbe01a5f4fe24ec46', 'Original job log changed')
    need(hashlib.sha1(b'blob ' + str(len(log_raw)).encode() + b'\0' + log_raw).hexdigest() == 'e0400e4f53657fe61efa90cc6fde29703388167b', 'Original recovered log blob differs')
    lines, groups = old.parse_log(log_raw)
    text = '\n'.join(r['text'] for r in lines) + '\n'
    need('\n' + SOURCE + '\n' in text and '\nx86_64\n' in text, 'Original checkout/architecture absent')
    need('##[error]' not in text and 'Process completed with exit code' not in text, 'Original process failure')
    need('-- The CXX compiler identification is AppleClang 17.0.0.17000013' in text, 'Original native compiler differs')
    expected = [(n, b) for n, b in old.source_commands(workflow) if n in old.REQUIRED]
    command_groups = [g for g in groups if g['command']]
    need(len(expected) == len(command_groups) == 13 and len(groups) == 17, 'Unexpected/missing command or action group')
    need([n for n, b in old.source_commands(workflow) if n not in old.REQUIRED] == ['AU registration diagnostics', 'Report pluginval failure'], 'Unexpected source run step')
    named, command_proof = {}, []
    for (name, script), group in zip(expected, command_groups):
        need(script == group['command'], 'Original command differs from final source: ' + name)
        need(any(r['text'] == 'shell: /bin/bash -e {0}' for r in group['header']), 'Unexpected shell')
        need(old.within(group['header'][0]['timestamp'], steps[name]), 'Command time differs')
        need(all(old.within(r['timestamp'], steps[name]) for r in group['runtime'] if r['timestamp']), 'Runtime time differs')
        named[name] = group
        command_proof.append(dict(step=name, number=steps[name]['number'], command=script, command_sha256=sha(script.encode()), exact_source_match=True, log_first_line=group['records'][0]['number'], log_last_line=group['records'][-1]['number']))

    apis = load('artifacts-api.json')['artifacts']
    archives, artifact_apis, uploads = {}, {}, []
    for kind, (artifact_id, name, size, digest, step_name) in ARTIFACTS.items():
        raw = raw_file(original / f'artifact-{artifact_id}.zip')
        need((len(raw), sha(raw)) == (size, digest), 'Actual ZIP bytes differ: ' + kind)
        api = load(f'artifact-{artifact_id}.json', original)
        fresh = [a for a in apis if a['id'] == artifact_id]
        need(len(fresh) == 1 and fresh[0] == api, 'Fresh authenticated artifact API differs')
        need((api['name'], api['size_in_bytes'], api['digest'], api['expired']) == (name, size, 'sha256:' + digest, False), 'Artifact identity differs')
        wr = api['workflow_run']
        need((wr['id'], wr['head_sha'], wr['repository_id'], wr['head_repository_id']) == (RUN, SOURCE, REPO_ID, REPO_ID), 'Artifact source/run differs')
        route = f'https://api.github.com/repos/{REPOSITORY}/actions/artifacts/{artifact_id}'
        need(api['url'] == route and api['archive_download_url'] == route + '/zip', 'Artifact route differs')
        need(old.dt(api['expires_at']) > datetime.now(timezone.utc), 'Artifact expired')
        matches = [g for g in groups if any(r['text'] == '  name: ' + name for r in g['header'])]
        need(len(matches) == 1, 'Missing/duplicate upload group')
        group, step = matches[0], steps[step_name]
        success = [r for r in group['runtime'] if r['text'] == f'Artifact {name} has been successfully uploaded! Final size is {size} bytes. Artifact ID is {artifact_id}']
        hashes = [r for r in group['runtime'] if r['text'] == 'SHA256 digest of uploaded artifact zip is ' + digest]
        need(len(success) == len(hashes) == 1, 'Original uploaded ID/size/digest absent')
        need(all(old.within(t, step) for t in (group['header'][0]['timestamp'], success[0]['timestamp'], hashes[0]['timestamp'], api['created_at'])), 'Upload timestamp differs')
        archives[kind], artifact_apis[kind] = raw, api
        uploads.append(dict(kind=kind, id=artifact_id, name=name, bytes=size, sha256=digest, original_step=step, upload_timestamp=success[0]['timestamp'], created_at=api['created_at'], expires_at=api['expires_at'], producing_attempt_verified=True))

    observations, content = load('observations.json'), load('native-contract.json')
    archive_binding = load('parser-archive-binding.json')
    rebound = binder.bind(observations, TARGET, archives['native'], artifact_apis['native'], archives['validator'], artifact_apis['validator'], SOURCE, RUN)
    need(archive_binding['bindings'] == rebound, 'Saved parser archive/member binding differs')
    need(observations['all_requested_observations_green'] is True, 'Native/validator observation failed')
    need(len(observations['native']) == len(observations['validators']) == 1, 'Unexpected observation targets')
    native = observations['native'][0]
    need(native['target'] == TARGET and native['counts'] == COUNTS, 'Actual native accounting differs')
    need(content['content_checks_conform'] and content['lasttest_session_complete'] and content['six_new_tests_passed'], 'Native contract incomplete')
    need((content['source'], content['source_tree'], content['run_id'], content['run_attempt']) == (SOURCE, TREE, RUN, ATTEMPT), 'Native contract source differs')
    need(content['native']['testcases'] == native['testcases'], 'Native parser receipts disagree')
    contract = load('source-contract.json', args.a46)
    expected_names = contract['expected_native_names'][TARGET]
    need(len(expected_names) == len(set(expected_names)) == 283 and set(expected_names) == {r['name'] for r in native['testcases']}, 'Exact native roster differs')
    runtime = '\n'.join(r['text'] for r in named['Test']['runtime'])
    rows = re.findall(r'^\s*(\d+)/283 Test\s+#(\d+): (\w+) \.+\s*(Passed|\*\*\*Skipped)\s+[0-9.]+ sec$', runtime, re.M)
    need(len(rows) == 283 and [int(x[0]) for x in rows] == list(range(1, 284)) and [int(x[1]) for x in rows] == list(range(1, 284)), 'Original complete serial roster absent')
    need([(r[2], 'passed' if r[3] == 'Passed' else 'formal_skipped') for r in rows] == [(c['name'], c['result']) for c in native['testcases']], 'Original results differ from JUnit/LastTest')
    need('100% tests passed out of 283' in runtime and 'Total Test time (real) = 2584.11 sec' in runtime, 'Native terminal incomplete')
    with zipfile.ZipFile(io.BytesIO(archives['native'])) as z:
        last_raw, juce_raw = z.read('Testing/Temporary/LastTest.log'), z.read('ci-juce-provenance.txt')
    cases = parser.lasttest_cases(parser.decode(last_raw))
    results = {r['name']: r for r in native['testcases']}
    juce = dict(line.split('=', 1) for line in juce_raw.decode().splitlines() if '=' in line)
    need(all(juce.get(k) == v for k, v in dict(actual_sha=JUCE, actual_version='8.0.11', source_clean='true', verification='PASS', workflow_commit=SOURCE, workflow_run=str(RUN), source_override='').items()), 'JUCE provenance differs')
    need(f'Verified clean JUCE 8.0.11 at {JUCE};' in text, 'Original JUCE verification absent')
    terminals = {
        'cs80_instrument_damper_tests': 'Instrument damper: 231 checks, 0 failures',
        'cs80_body_articulation_engine_tests': 'Body articulation integration: 284 checks, 0 failures',
        'cs80_preset_voicing_tests': 'Preset voicing regressions: 284/284 checks passed; 81.3 seconds generated.',
        'cs80_library_audit_tests': 'library audit passed: 301 curated + 222 original circuit programs',
    }
    excerpts = {}
    for name, terminal in terminals.items():
        need(results[name]['result'] == 'passed' and cases[name]['lasttest_status'] == 'Passed' and cases[name]['output'].count(terminal) == 1, 'Required native terminal absent: ' + name)
        excerpts[name] = dict(seconds=results[name]['seconds'], terminal=terminal, output_sha256=sha(cases[name]['output'].encode()), selected_lines=[s for s in cases[name]['output'].splitlines() if re.search(r'^(?:TINE|KEYS|phrase peak|library audit|Instrument damper|Body articulation|Preset voicing)', s)])
    ui = cases['cs80_ui_smoke']['output']
    need(results['cs80_ui_smoke']['result'] == 'passed' and cases['cs80_ui_smoke']['lasttest_status'] == 'Passed' and 'FAIL:' not in ui, 'UI smoke failed')
    need('UI SMOKE PHASE: instrument body host identity and recall' in ui and 'UI SMOKE TIMING: phase=instrument body host identity and recall' in ui, 'UI body phase incomplete')
    ui_fixtures = re.findall(r'^source screenshot fixture: (.+), finite=(\d), actual host sample peak=([\d.]+), meter=(.+)$', ui, re.M)
    need(ui_fixtures == [('STRING 3 complete / STRING 2 inferred', '1', '0.0834435672', 'SAMPLE PEAK -21.6 dBFS READ-ONLY'), ('PANEL headroom', '1', '0.980465114', 'SAMPLE PEAK -2.5 dBFS READ-ONLY')], 'Actual source screenshot meter witnesses differ')
    for name in ['cs80_output_meter_tests', 'cs80_output_meter_display_tests', 'cs80_output_meter_audit_tests']:
        need(results[name]['result'] == 'passed', 'Meter test failed')
    excerpts['cs80_ui_smoke'] = dict(seconds=results['cs80_ui_smoke']['seconds'], output_sha256=sha(ui.encode()), fixtures=ui_fixtures, selected_lines=[s for s in ui.splitlines() if 'source screenshot fixture:' in s or 'phase=source performance captures ' in s or 'phase=instrument body host identity and recall ' in s], limitation='This original native normal-timer pass does not explain earlier Intel failure or establish broad DAW compatibility.')
    need(b'damper' in source('tests/InstrumentDamperTests.cpp').lower(), 'Final damper source absent')
    ui_source = source('src/tools/ui_snapshot.cpp')
    need(ui.count('UI SMOKE TEST PASSED') == 1 and 'UI SMOKE TEST:' not in ui, 'Original UI success terminal absent or failure terminal present')
    need(b'source screenshot meter reflects real rendered audio through its normal GUI timer' in ui_source, 'Original meter assertion changed')
    damper_source = source('tests/InstrumentDamperTests.cpp')
    need(b'void damperBoundaryRetimingAndReuse()' in damper_source and b'    damperBoundaryRetimingAndReuse();' in damper_source, 'Final portable damper regression missing or uninvoked')
    skip_review = review_skips(native, cases, source, last_raw)

    validator = observations['validators'][0]
    need(validator['target'] == TARGET and validator['required_lanes'] == validator['validation_invocations'] == 5 and validator['validation_retries'] == 0, 'Five original validator invocations required')
    need([r['lane'] for r in validator['lanes']] == ['vst3', 'au', 'au-gui', 'au-gui-seed', 'au-gui-seed-c76'], 'Validator lane roster differs')
    outcomes = []
    with zipfile.ZipFile(io.BytesIO(archives['validator'])) as z:
        download = parse(z.read('pluginval-download.json'))
        official = dict(repository='Tracktion/pluginval', version='1.0.4', release_id=188928656, asset_id=210999197, asset_name='pluginval_macOS.zip', expected_size=9202795, expected_sha256='3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f', verified=True, actual_size=9202795, actual_sha256='3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f')
        need(all(download.get(k) == v for k, v in official.items()), 'Official validator pin differs')
        fetch = '\n'.join(r['text'] for r in named['Fetch pluginval 1.0.4']['runtime'])
        need('pluginval - 1.0.4' in fetch and f"Verified official pluginval 1.0.4 macOS asset 210999197: 9202795 bytes, SHA256 {official['actual_sha256']}" in fetch, 'Original validator acquisition/version absent')
        for lane in validator['lanes']:
            group = named[lane['required_workflow_step']]
            console, report = z.read(lane['console']['path']), z.read(lane['report']['path'])
            need(console == ('\n'.join(r['text'] for r in group['runtime']) + '\n').encode(), 'Full original console differs: ' + lane['lane'])
            need(report == b'\n'.join(console.split(b'\n')[1:]), 'Report differs from original console')
            command = group['command']
            need(command.startswith('set -euo pipefail\n') and '--validate-in-process \\' in command and '||' not in command and 'set +e' not in command and 'grep -qx' in command, 'Original validator exit suppressed')
            need(('--skip-gui-tests' not in command) == lane['gui_required'], 'GUI command mismatch')
            for role, raw in [('console', console), ('report', report)]:
                s = parser.console_summary(raw)
                need(s == lane[role]['summary'] and s['strictness'] == [10] and s['program_counts'] == [523] and s['success_lines'] == s['validation_start_count'] == 1 and s['final_nonempty_line'] == 'SUCCESS' and not s['failure_markers'] and s['clean_text_success'] and s['editor_test_started'] == lane['gui_required'], 'Incomplete validator output')
                need(len(s['seeds']) == 1 and (not lane['fixed_seed'] or s['seeds'] == [lane['fixed_seed']]), 'Seed mismatch')
            need(lane['validation_invocations'] == 1 and lane['validation_retries'] == 0 and not lane['preserved_failed_attempts'] and not lane['text_findings'], 'Unexpected retry/substitution')
            need(lane['lane'] == 'vst3' or console.count(b'auval exited with code: 0') == 1, 'AU child did not exit normally')
            outcomes.append(dict(lane=lane['lane'], step=lane['required_workflow_step'], strictness=10, programs=523, seed=lane['console']['summary']['seeds'][0], gui=lane['gui_required'], original_normal_exit=0, full_original_console_equals_archive=True, report_equals_console_body=True, console=record(console, lane['console']['path']), report=record(report, lane['report']['path']), retained_warnings=lane['console']['summary']['warnings'], retained_optional_skips=lane['console']['summary']['explicit_skips']))
        au_files = {phase: z.read('au-payload-' + phase + '-validation.sha256') for phase in ('before', 'after')}
    need(au_files['before'] == au_files['after'], 'AU before/after differ')
    register, package = named['Register built AU'], named['Package native macOS VST3, AU and Standalone']
    command = register['command']
    need(command.index('codesign --force --sign - "$AU_SOURCE"') < command.index('cp -R "$AU_SOURCE" "$AU_DEST"') < command.index('diff -qr "$AU_SOURCE" "$AU_DEST"') < command.index('killall -9 AudioComponentRegistrar'), 'AU signing/copy/registration order differs')
    for phase, group in [('before', register), ('after', package)]:
        need(group['command'].count('diff -qr "$AU_SOURCE" "$AU_DEST"') == 1 and group['command'].startswith('set -euo pipefail\n'), 'Whole-AU comparison absent/suppressed')
        hashes = [r['text'] for r in group['runtime'] if re.match(r'^[0-9a-f]{64}  /', r['text'])]
        need(('\n'.join(hashes) + '\n').encode() == au_files[phase], 'Original AU hashes differ from archive')
        need(not any(re.search(r'^(?:Files .+ differ|Only in |diff:)', r['text']) for r in group['runtime']), 'AU comparison failure')
    need(text.count('diff -qr "$AU_SOURCE" "$AU_DEST"') == 2, 'Unexpected comparison count')
    install, payload, docs = load('install-acceptance.json'), load('archive-payload-binding.json'), load('docs-and-notices.json')
    need(install['status'] == 'artifact_bytes_verified' and (install['source'], install['source_tree'], install['preset_library_revision'], install['run_id'], install['run_attempt'], install['target']) == (SOURCE, TREE, REVISION, RUN, ATTEMPT, TARGET), 'Install context differs')
    need(install['inventory_entries'] == 39 and set(install['formats']) == {'VST3', 'AU', 'Standalone'}, 'Install products/inventory differ')
    need(payload['status'] == 'archive_consistency_and_au_executable_binding_verified' and payload['expected'] == dict(architecture='x86_64', attempt=ATTEMPT, repository=REPOSITORY, revision=REVISION, run=RUN, source=SOURCE), 'Payload context differs')
    need(docs['status'] == 'source_specific_docs_and_notices_verified' and (docs['source'], docs['revision'], docs['target'], docs['artifact_id']) == (SOURCE, REVISION, TARGET, ARTIFACTS['install'][0]), 'Document context differs')
    need(docs['outer_bytes'] == len(archives['install']) and docs['outer_sha256'] == sha(archives['install']) and docs['inventory_counts'] == {'directory': 13, 'file': 26} and docs['symlink_count'] == 0, 'Document archive binding differs')
    need(len(docs['required_metadata']) == 13 and len(docs['source_comparisons']) == 10 and all(r['result'] == 'PASS' for r in docs['source_comparisons']), 'Document/notice inspection incomplete')
    need(next(r for r in docs['required_metadata'] if r['path'] == 'evidence/juce-provenance.txt')['sha256'] == sha(juce_raw), 'Packaged/native JUCE evidence differs')
    with zipfile.ZipFile(io.BytesIO(archives['install'])) as outer:
        inner_raw = outer.read(install['inner_archive']['file'])
    need((len(inner_raw), sha(inner_raw)) == (install['inner_archive']['bytes'], install['inner_archive']['sha256']) and sha(inner_raw) == payload['inner_archive']['sha256'], 'Actual inner archive differs')
    headers = []
    with tarfile.open(fileobj=io.BytesIO(inner_raw), mode='r:gz') as inner:
        root = install['inner_archive']['file'].removesuffix('.tar.gz')
        for format_name, bundle in payload['packaged_bundles'].items():
            need(not any(r['type'] == 'symlink' for r in bundle['inventory']), 'Bundle symlinks need separate comparison review')
            member = inner.getmember(root + '/' + bundle['executable'])
            need(member.isfile() and member.mode == 0o755, 'Actual executable role/mode differs')
            with inner.extractfile(member) as stream:
                raw = stream.read()
            need(sha(raw) == bundle['executable_sha256'], 'Executable identity differs')
            old_header = next(h for h in install['binary_headers'] if h['path'] == bundle['executable'])
            need(old_header['sha256'] == sha(raw) and old_header['header_check'] == 'PASS' and old_header['architecture'] == 'x86_64' and old_header['mode'] == '0o755', 'Binary inspections disagree')
            need(raw[:4] == b'\xcf\xfa\xed\xfe' and len(raw) >= 32, 'Not thin 64-bit Mach-O')
            cpu, subtype, role = struct.unpack_from('<III', raw, 4)
            need(cpu == 0x01000007 and role == (2 if format_name == 'Standalone' else 8), 'Actual native CPU/role differs')
            headers.append(dict(format=format_name, path=bundle['executable'], bytes=len(raw), sha256=sha(raw), cpu_type=hex(cpu), cpu_subtype=hex(subtype), mach_o_filetype=role, mode='0o755', thin_native_x86_64=True))
    need(len(headers) == len(install['binary_headers']) == 3, 'Missing product binary')
    au_digest = payload['packaged_bundles']['AU']['executable_sha256']
    need(len(payload['au_before']) == len(payload['au_after']) == 2 and all(r['sha256'] == au_digest for r in payload['au_before'] + payload['au_after']), 'Packaged AU differs from four original hashes')
    need(au_files['before'].decode().split()[0] == au_digest, 'Original AU/package binding differs')
    package_output = '\n'.join(r['text'] for r in package['runtime'])
    need('Packaging self-test passed: 46 fixture/rejection checks; synthetic artifacts only.' in package_output and '"sha256": "' + sha(inner_raw) + '"' in package_output and '"archive": "packages/' + install['inner_archive']['file'] + '"' in package_output and '"architecture": "x86_64"' in package_output, 'Original package identity/self-test differs')
    for path, expected in payload['source_contract_files'].items():
        raw = source(path)
        need((len(raw), sha(raw)) == (expected['bytes'], expected['sha256']), 'Payload source contract drift')
    vst3_path = old.WORKSPACE + '/build/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3'
    need(payload['validator_input_paths']['vst3']['path'] == vst3_path, 'VST3 original path differs')
    packager = source('scripts/package-ci-artifacts.py').decode()
    need(all(s in packager for s in ['with tempfile.TemporaryDirectory(prefix="cs80-package-fixtures-") as temp:', 'args.build_dir / "CS80Reference_artefacts/Release"', 'Package output must be outside build products', '"licenses/JUCE-LICENSE.md": license_file.read_bytes()']), 'Read-only packager invariants changed')
    for path in ['CMakeLists.txt', 'tests/LibraryAuditTests.cpp', 'tests/PresetVoicingTests.cpp', 'tests/BodyArticulationEngineTests.cpp']:
        source(path)

    verified_at = datetime.now(timezone.utc).isoformat()
    proof = dict(schema=1, status='original_platform_execution_bound', recovery_source='Fresh actual final-source artifacts and authenticated original Actions records; no old local result reused.', source_commit=SOURCE, source_tree=TREE, factory_revision=REVISION, run_id=RUN, run_attempt=ATTEMPT, job_id=JOB, target=TARGET, verified_at_utc=verified_at, original_job=job, run_snapshot_status=runs[0]['status'], run_snapshot_conclusion=runs[0]['conclusion'], original_job_log=record(log_raw, original / 'macos-x86_64-job.log'), original_job_log_git_blob='e0400e4f53657fe61efa90cc6fde29703388167b', original_run_attempt_bound=True, artifact_producing_attempts_bound=True, uploads=uploads, original_commands=command_proof, original_ctest_normal_exit=0, ctest_exit_basis='Original successful unsuppressed serial Test; all 283 ordered console results match bound JUnit/LastTest; normal terminal completion.', native_counts=COUNTS, ctest_seconds=2584.11, native_excerpts=excerpts, native_capability_skip_review=skip_review, juce=juce, validator_invocations=5, validator_retries=0, original_validator_exit_basis='Each successful original API step executes exact source set -euo pipefail/in-process/tee/terminal-SUCCESS-grep command; the complete original output equals the archived console and report body.', validator_outcomes=outcomes, pluginval_download=download, pluginval_transport_attempts=len(download['attempts']), pluginval_transport_scope='One downloader transport attempt is distinct from five validator invocations and zero validation retries.', whole_au_comparisons_passed=True, au_signing_before_copy_and_registration_verified=True, all_four_au_hashes_match_packaged_executable=True, au_packaged_executable_sha256=au_digest, au_hash_member=record(au_files['before'], 'au-payload-before/after-validation.sha256'), au_binding_limit='Complete original diff -qr checks ordinary-file content and membership. It does not prove ACL/xattr/ownership/signature validity; packaged modes are checked separately.', vst3_validated_bundle_identity_established=True, vst3_binding_basis='Exact original validated build-tree VST3 path is the source-pinned packaging input. All intervening original commands are accounted for; no VST3 rebuild/mutation/retry. No independent VST3 before/after tree hash is claimed.', actual_native_binary_headers=headers, docs_notice_metadata_count=13, docs_exact_source_comparisons=10, source_contract_files=source_records, input_files=input_records, platform_qualified=False, cross_platform_qualified=False, release_qualified=False)
    proof_raw = (json.dumps(proof, indent=2) + '\n').encode()
    receipt = dict(schema=1, status='platform_qualified', platform_qualified=True, source_commit=SOURCE, source_tree=TREE, factory_revision=REVISION, run_id=RUN, run_attempt=ATTEMPT, job_id=JOB, target=TARGET, verified_at_utc=verified_at, recovery_source=proof['recovery_source'], original_run_attempt_bound=True, artifact_producing_attempts_bound=True, native_results_accepted=True, native_counts=COUNTS, exact_283_native_names_verified=True, all_six_new_tests_passed=True, damper_checks=231, damper_failures=0, nested_python_skips=5, formal_skips=skip_review['formal_skips'], skip_review_completed=True, ctest_seconds=2584.11, validator_lanes_accepted=5, original_validator_normal_zero_exits=5, validator_retries=0, whole_au_bundle_binding_established=True, vst3_validated_bundle_identity_established=True, packaged_au_sha256=au_digest, install_artifact_id=ARTIFACTS['install'][0], artifacts=uploads, original_execution_binding=record(proof_raw, 'original-execution-binding.json'), actual_native_binary_headers=headers, docs_and_notices=record(raw_file(base / 'docs-and-notices.json'), 'docs-and-notices.json'), verifier=record(raw_file(Path(__file__)), Path(__file__).name), cross_platform_qualified=False, release_qualified=False, public_delivery_verified=False, limitations=['Other platforms, audition acceptance and all-five-route public GET verification are separate root gates.', 'Four AU current-program -1 warnings remain retained; separate optional Steinberg validator is unconfigured.', 'The render bounded-buffer skip has no exact runtime reason in nonverbose original output; source/method mapping and that absence are retained.', 'No product was executed here. Native header checks do not establish a separate Standalone launch, publisher signing/notarization, listening quality, hardware equivalence or a broad DAW matrix.', 'Previous failed/superseded source histories remain unchanged.'])
    args.out_dir.mkdir(parents=True)
    for name, value in [('original-execution-binding.json', proof), ('platform-qualification.json', receipt), ('skip-review.json', skip_review), ('native-reviewed-excerpts.json', excerpts)]:
        with (args.out_dir / name).open('x') as stream:
            json.dump(value, stream, indent=2, allow_nan=False)
            stream.write('\n')
    print(json.dumps({'status': receipt['status'], 'target': TARGET, 'native_counts': COUNTS, 'damper_checks': 231, 'validator_lanes': 5, 'cross_platform_qualified': False, 'release_qualified': False, 'public_delivery_verified': False}))


def review_skips(native, cases, source, last_raw):
    expected = [dict(name='cs80_filter_batch_cap_tests', status='notrun', message='SKIP_RETURN_CODE=77'), dict(name='cs80_stereo_pra_exact_tests', status='notrun', message='SKIP_RETURN_CODE=77')]
    need(native['formal_skips'] == expected, 'Formal skip roster/message differs')
    need(cases[expected[0]['name']]['output'].strip() == 'SKIP: AVX2 unavailable or disabled; no SIMD qualification', 'AVX2 actual skip reason differs')
    need(cases[expected[1]['name']]['output'].strip() == '', 'PRA actual skip output differs; review required')
    cap = source('tests/FilterBatchCapTests.cpp').decode()
    batch = source('src/core/Ig00156Batch.h').decode()
    pra = source('tests/StereoPraExactTests.cpp').decode()
    need('return 77;' in cap and 'defined(__x86_64__) && defined(__GNUC__) && !defined(__clang__)' in batch, 'AVX2 capability guard changed')
    need('defined(__linux__) && defined(__x86_64__)' in pra and '__GNUC__ == 14 && __GNUC_MINOR__ == 2' in pra and 'int main(){ return 77; }' in pra, 'PRA capability guard changed')
    suites = native['nested_python_skips']['suites']
    need(native['nested_python_skips']['total'] == 5 and [(s['ctest_test'], s['total']) for s in suites] == [('cs80_package_binary_identity_tests', 1), ('cs80_packaging_notice_tests', 1), ('cs80_windows_installer_preflight_tests', 2), ('cs80_render_cli_tests', 1)], 'Actual nested skip roster differs')
    mapping = {}
    definitions = [('cs80_package_binary_identity_tests', 'scripts/test-package-binary-identity.py', '...s.........', ['test_native_windows_junction_is_rejected']), ('cs80_windows_installer_preflight_tests', 'scripts/test-windows-installer-preflight.py', '...ss.......', ['test_native_directory_junction_rejected', 'test_native_inno_accepts_valid_and_rejects_bad_payloads']), ('cs80_render_cli_tests', 'tests/test_render_cli.py', '..s....', ['test_long_render_has_bounded_buffers'])]
    for test, path, pattern, expected_methods in definitions:
        raw = source(path)
        methods = sorted(n.name for n in ast.walk(ast.parse(raw)) if isinstance(n, ast.FunctionDef) and n.name.startswith('test_'))
        need(len(methods) == len(pattern) and cases[test]['output'].splitlines()[0] == pattern, 'Nonverbose method map differs')
        selected = [name for name, mark in zip(methods, pattern) if mark == 's']
        need(selected == expected_methods, 'Skipped method inference differs')
        mapping[test] = dict(source=path, source_record=record(raw, path), method_order=methods, actual_pattern=pattern, inferred_skipped_methods=selected, inference='Default unittest alphabetical method order plus exact original dot/skip pattern. Terminal summaries determine counts.')
    notice = source('scripts/test-packaging-notices.py')
    need('test_synthetic_deb_notice_bytes (__main__.NoticeTests.test_synthetic_deb_notice_bytes) ... skipped \'Native Linux amd64, Debian packaging tools and cc required\'' in cases['cs80_packaging_notice_tests']['output'], 'Explicit DEB skip differs')
    need(b'Native Linux amd64, Debian packaging tools and cc required' in notice, 'DEB source guard differs')
    return dict(status='actual_skips_reviewed', reviewed=True, source=SOURCE, run_id=RUN, attempt=ATTEMPT, job_id=JOB, target=TARGET, formal_skips=expected, nested_python_skips=native['nested_python_skips'], individual_review=[dict(test='cs80_filter_batch_cap_tests', kind='formal', count=1, basis='Actual AVX2 unavailable/disabled message; final source excludes native x86_64 AppleClang from the GNU-only backend; the original configure log identifies AppleClang. No CPU AVX2 availability claim is inferred. No SIMD qualification.'), dict(test='cs80_stereo_pra_exact_tests', kind='formal', count=1, basis='Actual JUnit return-77 skip with empty stdout. Final source only runs the Linux x86_64 GNU14.2 ISA/FP specialization. No specialization qualification.'), dict(test='cs80_package_binary_identity_tests', kind='nested', count=1, basis='Actual ...s......... maps to the native Windows junction fixture, guarded by os.name == nt.'), dict(test='cs80_packaging_notice_tests', kind='nested', count=1, basis='Actual explicit skip for the native Linux amd64 Debian tools fixture.'), dict(test='cs80_windows_installer_preflight_tests', kind='nested', count=2, basis='Actual ...ss....... maps to the native Windows directory-junction and ISCC fixtures; both excluded on macOS.'), dict(test='cs80_render_cli_tests', kind='nested', count=1, basis='Actual ..s.... maps to bounded-buffer resource-limit coverage. The frozen source admits unavailable RLIMIT_AS or an incompatible ASan cap; exact dynamic runtime reason is absent. The uncapped long-render write-failure case passed.', exact_runtime_skip_reason_retained=False)], nonverbose_method_mapping=mapping, actual_lasttest=record(last_raw, 'Testing/Temporary/LastTest.log'), counting='Five nested Python skips remain separate from two formal CTest skips and 283 registered CTest executions.', platform_qualified=False, release_qualified=False)


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        print(type(exc).__name__ + ': ' + str(exc))
        raise SystemExit(2)
