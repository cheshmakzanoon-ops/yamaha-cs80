#!/usr/bin/env python3
"""Bind this Apple Silicon GUI observation to its original job and exact ZIP.

Run from the repository root. This qualifies the five original validator
process outcomes only; it does not qualify installation bytes or a release.
"""
from pathlib import Path
import hashlib
import importlib.util
import io
import json
import re
import subprocess
import zipfile

ROOT = Path.cwd()
HERE = Path(__file__).resolve().parent
SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
RUN = 45366603800
JOB = 138349816955
ARTIFACT = 14910405103
ARCHIVE = Path('/workspace/scratch/b9c11dd92abc/native-artifacts/14910405103.zip')
PARSER = ROOT / 'docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py'
PARSER_SHA = 'f192bf2ff70d4501f73875eb5bb6416b9adc3b77912e165da4305302724cd846'


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def record(data, name):
    return {'path': name, 'bytes': len(data), 'sha256': sha(data)}


def git_bytes(name, commit=SOURCE):
    return subprocess.check_output(['git', 'show', commit + ':' + name])


def source_step(workflow, name):
    marker = '      - name: ' + name + '\n'
    require(workflow.count(marker) == 1, 'Missing/duplicate source step: ' + name)
    section = workflow.split(marker, 1)[1].split('\n      - ', 1)[0]
    require('continue-on-error' not in section, 'Suppressed step failure')
    run = section.split('        run: |\n', 1)[1]
    return '\n'.join(line[10:] for line in run.splitlines() if line.startswith('          ')) + '\n'


def main():
    run = json.loads((HERE / 'run-api.json').read_bytes())
    job = json.loads((HERE / 'job-api.json').read_bytes())
    artifacts = json.loads((HERE / 'artifacts-final-api.json').read_bytes())['artifacts']
    require(run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == 1, 'Run context mismatch')
    require(job['id'] == JOB and job['run_id'] == RUN and job['head_sha'] == SOURCE
            and job['run_attempt'] == 1 and job['status'] == 'completed'
            and job['conclusion'] == 'success' and job['labels'] == ['macos-15'], 'Original producer mismatch/failure')
    steps = {s['name']: s for s in job['steps']}
    required = ['Test', 'Fetch pluginval 1.0.4', 'Register built AU',
                'Package native macOS VST3, AU and Standalone',
                'Upload native macOS install bundle', 'Upload pluginval reports']
    for name in required:
        require(steps[name]['status'] == 'completed' and steps[name]['conclusion'] == 'success', 'Unsuccessful prerequisite: ' + name)

    artifact = next(a for a in artifacts if a['id'] == ARTIFACT)
    raw = ARCHIVE.read_bytes()
    require(artifact['name'] == 'cs80-pluginval-macos-15' and artifact['expired'] is False
            and artifact['size_in_bytes'] == len(raw) and artifact['digest'] == 'sha256:' + sha(raw), 'Artifact byte identity mismatch')
    require(artifact['workflow_run']['id'] == RUN and artifact['workflow_run']['head_sha'] == SOURCE, 'Artifact source mismatch')
    upload = steps['Upload pluginval reports']
    require(upload['started_at'] <= artifact['created_at'] <= upload['completed_at'], 'Artifact outside original upload window')
    original = (HERE / 'original-producer-job.log').read_bytes()
    lines = [re.sub(r'\x1b\[[0-9;]*m', '', re.sub(r'^\d{4}-\d\d-\d\dT\S+Z ?', '', line))
             for line in original.decode('utf-8-sig').splitlines()]
    transcript = '\n'.join(lines) + '\n'
    require('\n' + SOURCE + '\n' in transcript and '\narm64\n' in transcript, 'Checkout/architecture not in original log')
    require('Artifact cs80-pluginval-macos-15.zip successfully finalized. Artifact ID ' + str(ARTIFACT) in transcript
            and 'SHA256 digest of uploaded artifact zip is ' + sha(raw) in transcript, 'Original upload does not bind actual ZIP')
    require('\npluginval - 1.0.4\n' in transcript, 'Executed validator version missing')
    require(sha(PARSER.read_bytes()) == PARSER_SHA, 'Reviewed parser changed')
    spec = importlib.util.spec_from_file_location('a45_parser', PARSER)
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    workflow = git_bytes('.github/workflows/build.yml').decode()
    downloader = git_bytes('scripts/download-pluginval-macos.py').decode()
    source_editor = git_bytes('src/plugin/PluginEditor.cpp')
    require(source_editor == git_bytes('src/plugin/PluginEditor.cpp', 'c76ad288e02fbeca48f39652bf4fcdd99837f793'), 'Renderer source changed; update scope')
    groups = re.split(r'(?=^##\[group\]Run )', transcript, flags=re.M)
    outcomes = []
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        receipt = json.loads(archive.read('pluginval-download.json'))
        expected_sha = re.search(r'^EXPECTED_SHA256 = "([0-9a-f]{64})"$', downloader, re.M)[1]
        require(receipt['repository'] == 'Tracktion/pluginval' and receipt['version'] == '1.0.4'
                and receipt['asset_id'] == 210999197 and receipt['verified'] is True
                and receipt['actual_size'] == receipt['expected_size'] == 9202795
                and receipt['actual_sha256'] == receipt['expected_sha256'] == expected_sha,
                'Official pinned validator receipt mismatch')
        for lane, name, console, report, gui, fixed_seed in parser.LANES['macos-arm64']:
            step = steps[name]
            require(step['status'] == 'completed' and step['conclusion'] == 'success', 'Failed original validator step: ' + name)
            matches = [g for g in groups if '2>&1 | tee ' + console + '\n' in g]
            require(len(matches) == 1, 'Missing/duplicate validator execution group')
            group = matches[0]
            command = source_step(workflow, name)
            require(command.rstrip() in group, 'Executed command differs from exact source')
            require(command.startswith('set -euo pipefail\n') and '--strictness-level 10' in command
                    and '--validate-in-process' in command and '--timeout-ms 120000' in command
                    and "grep -qx 'SUCCESS' " + console in command and '||' not in command,
                    'Exit/settings contract differs')
            require(('--skip-gui-tests' not in command) == gui, 'GUI command contract differs')
            cb = archive.read(console)
            rb = archive.read('pluginval-reports/' + report)
            ct = cb.decode('utf-8-sig')
            require(ct.rstrip('\n') in group, 'Console not bound to original timestamped process output')
            require(cb.split(b'\n', 1)[1] == rb, 'Report is not exact console body')
            summary = parser.console_summary(cb)
            require(summary['clean_text_success'] and summary['validation_start_count'] == 1
                    and summary['strictness'] == [10] and summary['program_counts'] == [523]
                    and summary['editor_test_started'] == gui and len(summary['seeds']) == 1,
                    'Incomplete validator result')
            require(not fixed_seed or summary['seeds'] == [fixed_seed], 'Wrong fixed seed')
            if gui:
                for test in ('Editor', 'Open editor whilst processing', 'Editor Automation'):
                    require('Completed tests in pluginval / ' + test + '\n' in ct, 'Incomplete GUI case: ' + test)
                require(not summary['explicit_skips'], 'Skipped GUI tests')
            require('Abort trap' not in group and 'Process completed with exit code' not in group, 'Crash/nonzero after result')
            outcomes.append({'lane': lane, 'step': name, 'seed': summary['seeds'][0], 'gui': gui,
                             'strictness': 10, 'programs': 523, 'original_normal_exit': 0,
                             'exit_basis': 'Original successful API step plus exact set -euo pipefail / in-process validator / tee / terminal SUCCESS command; no suppression or retry.',
                             'started_at': step['started_at'], 'completed_at': step['completed_at'],
                             'console': record(cb, console), 'report': record(rb, 'pluginval-reports/' + report)})
        before = archive.read('au-payload-before-validation.sha256')
        after = archive.read('au-payload-after-validation.sha256')
        require(before == after, 'AU hashes changed during validation')
        digests = [line.split()[0] for line in before.decode().splitlines()]
        require(len(digests) == 2 and len(set(digests)) == 1, 'Built/registered AU executable differed')
        require(before.decode().rstrip() in transcript and after.decode().rstrip() in transcript, 'AU records not in original log')
        require('diff -qr "$AU_SOURCE" "$AU_DEST"' in source_step(workflow, 'Register built AU')
                and 'diff -qr "$AU_SOURCE" "$AU_DEST"' in source_step(workflow, 'Package native macOS VST3, AU and Standalone'),
                'Whole-directory comparison missing')
        require(not any(re.search(r'-attempt\d+\.log$', n) for n in archive.namelist()), 'Unexpected validator retry evidence')
    require(transcript.count('\nValidation started\n') == 5, 'Original job validation count differs')
    require(len(outcomes) == 5 and sum(x['gui'] for x in outcomes) == 3, 'Lane roster differs')
    require(max(x['completed_at'] for x in outcomes) <= steps['Package native macOS VST3, AU and Standalone']['started_at'], 'Packaging preceded validation')
    result = {'schema': 1, 'source_commit': SOURCE, 'run_id': RUN, 'run_attempt': 1, 'job_id': JOB,
              'scope': 'Apple Silicon original GUI/validator process qualification; installation bytes and full release remain separate.',
              'gui_validation_qualified': True, 'platform_release_qualified': False, 'release_qualified': False,
              'validator_invocations': 5, 'gui_invocations': 3, 'validation_retries': 0,
              'official_validator_receipt': receipt, 'artifact': {'id': ARTIFACT, 'name': artifact['name'], **record(raw, ARCHIVE.name)},
              'original_job_log': record(original, 'original-producer-job.log'), 'outcomes': outcomes,
              'au_before_after_executable_sha256': digests[0], 'whole_au_directory_comparisons_passed': True,
              'renderer_changed_since_failed_c76': False, 'past_crash_root_cause_established': False,
              'interpretation': 'The new source passes both recorded seeds and the randomized GUI lane. This is current-source qualification and non-reproduction, not proof of a renderer fix or of identical old automation actions.',
              'remaining_gates': ['Actual install ZIP source, architecture, inner checksums and validated AU/VST3 payload binding.',
                                  'Native CTest/JUCE archive acceptance, all other platforms, audition and public delivery.'],
              'source_files': {n: record(git_bytes(n), n) for n in ('.github/workflows/build.yml', 'scripts/download-pluginval-macos.py', 'src/plugin/PluginEditor.cpp')},
              'parser': record(PARSER.read_bytes(), str(PARSER.relative_to(ROOT))),
              'verifier': record(Path(__file__).read_bytes(), 'verify_gui.py'),
              'api_inputs': {n: record((HERE / n).read_bytes(), n)
                             for n in ('run-api.json', 'job-api.json', 'artifacts-final-api.json')}}
    (HERE / 'gui-qualification.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: result[k] for k in ('gui_validation_qualified', 'validator_invocations', 'gui_invocations', 'validation_retries', 'release_qualified')}))


if __name__ == '__main__':
    main()
