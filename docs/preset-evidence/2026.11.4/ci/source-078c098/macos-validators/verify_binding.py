#!/usr/bin/env python3
"""Read-only binding of eight original macOS validator lanes; no product execution."""
from pathlib import Path, PurePosixPath
from datetime import datetime, timedelta, timezone
import hashlib
import importlib.util
import io
import json
import re
import subprocess
import sys
import tarfile
import yaml

sys.dont_write_bytecode = True
BASE = Path(__file__).resolve().parent.parent
OUT = BASE / 'a55'
REPO = BASE / 'yamaha-cs80'
SOURCE = '078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
RUN = 45246863788
ATTEMPT = 1
REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
INPUT = BASE / 'release-078c098/input'
spec = importlib.util.spec_from_file_location('a45_parser', BASE / 'a45/parse_native_evidence.py')
a45 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(a45)

def require(ok, message):
    if not ok:
        raise ValueError(message)

def file_record(path):
    return a45.record(path.read_bytes(), path)

def dt(value):
    return datetime.fromisoformat(value.replace('Z', '+00:00'))

def within(timestamp, step):
    # API step times are whole seconds, logs preserve seven decimal places.
    return dt(step['started_at']) <= dt(timestamp) < dt(step['completed_at']) + timedelta(seconds=1)

def canonical(name):
    require(name and not name.startswith('/') and '\\' not in name and ':' not in name,
            'Noncanonical archive path')
    require(all(p not in ('', '.', '..') for p in name.split('/')) and PurePosixPath(name).as_posix() == name,
            'Noncanonical archive path')

def parse_log(path):
    records = []
    for index, line in enumerate(path.read_text().splitlines(), 1):
        match = re.match(r'^(\d{4}-\d\d-\d\dT\S+Z) (.*)$', line)
        text = match[2] if match else line
        records.append({'line': index, 'timestamp': match[1] if match else None,
                        'text': re.sub(r'\x1b\[[0-9;]*m', '', text)})
    starts = [i for i, row in enumerate(records) if row['text'].startswith('##[group]Run ')]
    blocks = []
    for index, start in enumerate(starts):
        end = starts[index + 1] if index + 1 < len(starts) else len(records)
        eg = next((i for i in range(start + 1, end) if records[i]['text'] == '##[endgroup]'), None)
        shell = next((i for i in range(start + 1, end) if records[i]['text'].startswith('shell: ')), None)
        commands = [x['text'].strip() for x in records[start + 1:shell]] if shell is not None and eg is not None and shell < eg else []
        blocks.append({'records': records[start:end], 'commands': commands,
                       'output': records[eg + 1:end] if eg is not None else [],
                       'start_line': start + 1, 'end_line': end,
                       'shell': records[shell]['text'] if commands else None})
    return records, blocks

workflow_path = OUT / 'build-source-078c098.yml'
workflow_bytes = subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':.github/workflows/build.yml'])
require(workflow_path.read_bytes() == workflow_bytes, 'Saved workflow mismatch')
workflow = yaml.safe_load(workflow_bytes)
macos = workflow['jobs']['macos']
require(not macos.get('continue-on-error', False), 'Job masks failure')
source_steps = {s['name']: s for s in macos['steps'] if 'name' in s}
observations = json.loads((OUT / 'validator-observations.json').read_text())
require(observations['all_requested_observations_green'], 'Parser found a lane issue')
require(observations['validator_totals'] == {'required_lanes': 8, 'validation_invocations': 8, 'validation_retries': 0}, 'Unexpected lane count')
source_records = {}
for src, saved in [('.github/workflows/build.yml', 'build-source-078c098.yml'),
                   ('scripts/package-ci-artifacts.py', 'package-ci-artifacts-source-078c098.py'),
                   ('scripts/download-pluginval-macos.py', 'download-pluginval-macos-source-078c098.py')]:
    data = subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':' + src])
    require((OUT / saved).read_bytes() == data, 'Saved source mismatch')
    source_records[src] = {**file_record(OUT / saved), 'git_blob': subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', SOURCE + ':' + src], text=True).strip()}

packager = (OUT / 'package-ci-artifacts-source-078c098.py').read_text()
for literal in ['vst = base / "VST3" / f"{PRODUCT}.vst3"',
                'args.build_dir / "CS80Reference_artefacts/Release"',
                'tar.add(path, arcname=name + "/" + relative, recursive=False)',
                'with tempfile.TemporaryDirectory(prefix="cs80-package-fixtures-") as temp:']:
    require(literal in packager, 'Packaging path/copy/self-test assertion changed: ' + literal)

configs = [
    {'target': 'macos-arm64', 'runner': 'macos-15', 'job': BASE / 'a51/job.json', 'run': BASE / 'a51/run.json', 'log': BASE / 'a51/job.log', 'job_id': 137945805542,
     'au_sha': '25de9ef4917a9a2716277b0f95440897a6bb37667fe7052baf06ac959282358b'},
    {'target': 'macos-x86_64', 'runner': 'macos-15-intel', 'job': INPUT / 'macos-x86_64-native-job-api.json', 'run': INPUT / 'macos-x86_64-native-run-api.json', 'log': INPUT / 'macos-x86_64-native-job.log', 'job_id': 137945806008,
     'au_sha': '2d820e1147b5d6c0ec4372b1e9f107c7d8c19ab0935c850acf62937c0db61876'}]
results = []
for cfg in configs:
    target = cfg['target']
    job = json.loads(cfg['job'].read_text())
    run = json.loads(cfg['run'].read_text())
    require((job['id'], job['run_id'], job['run_attempt'], job['head_sha'], job['status'], job['conclusion']) == (cfg['job_id'], RUN, ATTEMPT, SOURCE, 'completed', 'success'), 'Job identity/outcome mismatch')
    require((run['id'], run['run_attempt'], run['head_sha'], run['path']) == (RUN, ATTEMPT, SOURCE, '.github/workflows/build.yml'), 'Run identity mismatch')
    require(run['repository']['full_name'] == REPOSITORY, 'Wrong repository')
    steps = {s['name']: s for s in job['steps']}
    records, blocks = parse_log(cfg['log'])
    require(any(r['text'] == SOURCE and i and 'git log -1 --format=%H' in records[i-1]['text'] for i, r in enumerate(records)), 'Original checkout SHA not observed')

    def bind_step(name):
        step = steps[name]
        require(step['status'] == 'completed' and step['conclusion'] == 'success', 'Original step not successful: ' + name)
        source = source_steps[name]
        require(not source.get('continue-on-error', False), 'Step masks errors: ' + name)
        expected = [x.strip() for x in source['run'].splitlines()]
        found = [b for b in blocks if b['commands'] == expected]
        require(len(found) == 1, 'Original command block not unique: ' + name)
        block = found[0]
        require(block['shell'] == 'shell: /bin/bash -e {0}', 'Unexpected shell')
        require(within(block['records'][0]['timestamp'], step), 'Step command outside API time')
        return step, source, block

    artifacts = []
    for kind, upload_name in [('pluginval', 'Upload pluginval reports'), ('install', 'Upload native macOS install bundle'), ('test-evidence', 'Upload test and JUCE source evidence')]:
        archive = INPUT / (target + '-' + kind + '.zip')
        api_path = INPUT / (target + '-' + kind + '-api.json')
        api = json.loads(api_path.read_text())
        expected_name = ('cs80-pluginval-' + cfg['runner']) if kind == 'pluginval' else ('cs80-' + cfg['runner'] + ('-install-bundle' if kind == 'install' else '-test-evidence'))
        receipt = file_record(archive)
        require(api['name'] == expected_name and api['size_in_bytes'] == receipt['bytes'] and api['digest'] == 'sha256:' + receipt['sha256'], 'Artifact API byte identity mismatch')
        require(api['workflow_run']['id'] == RUN and api['workflow_run']['head_sha'] == SOURCE and api['workflow_run']['repository_id'] == run['repository']['id'], 'Artifact source/run/repository mismatch')
        require(api['expired'] is False and dt(api['expires_at']) > datetime.now(timezone.utc), 'Artifact expired')
        step = steps[upload_name]
        require(step['status'] == 'completed' and step['conclusion'] == 'success', 'Upload not successful')
        require(within(api['created_at'], step), 'Artifact creation outside upload')
        candidates = [b for b in blocks if any(r['text'] == '  name: ' + expected_name for r in b['records'])]
        require(len(candidates) == 1, 'Upload block not unique')
        block = candidates[0]
        progress = [r for r in block['records'] if re.fullmatch(r'Uploaded bytes \d+', r['text'])]
        values = [int(r['text'].rsplit(' ', 1)[1]) for r in progress]
        require(values and values[-1] == receipt['bytes'] and all(a < b for a, b in zip([0] + values, values)), 'Cumulative upload progress mismatch')
        require(all(within(r['timestamp'], step) for r in progress), 'Upload progress outside step')
        expected = [f'SHA256 digest of uploaded artifact zip is {receipt["sha256"]}',
                    f'Artifact {expected_name}.zip successfully finalized. Artifact ID {api["id"]}',
                    f'Artifact {expected_name} has been successfully uploaded! Final size is {receipt["bytes"]} bytes. Artifact ID is {api["id"]}',
                    f'Artifact download URL: https://github.com/{REPOSITORY}/actions/runs/{RUN}/artifacts/{api["id"]}']
        witnesses = []
        for text in expected:
            found = [r for r in block['records'] if r['text'] == text]
            require(len(found) == 1 and within(found[0]['timestamp'], step), 'Upload witness mismatch')
            witnesses += found
        e = a45.Evidence({'archive': str(archive)}, BASE)
        e.close()
        artifacts.append({'kind': kind, 'artifact_api': api, 'api_file': file_record(api_path), 'archive': receipt, 'upload_step': step,
                          'cumulative_uploaded_bytes': values, 'upload_witnesses': witnesses, 'canonical_unique_supported_zip_entries': True,
                          'source_run_attempt_upload_bound': True})

    fetch_step, fetch_source, fetch_block = bind_step('Fetch pluginval 1.0.4')
    reg_step, reg_source, reg_block = bind_step('Register built AU')
    package_step, package_source, package_block = bind_step('Package native macOS VST3, AU and Standalone')
    require('set -euo pipefail' in reg_source['run'] and 'diff -qr "$AU_SOURCE" "$AU_DEST"' in reg_source['run'], 'Pre-validation full bundle diff missing')
    require('set -euo pipefail' in package_source['run'] and 'diff -qr "$AU_SOURCE" "$AU_DEST"' in package_source['run'], 'Post-validation full bundle diff missing')
    pv = a45.Evidence({'archive': str(INPUT / (target + '-pluginval.zip'))}, BASE)
    download = json.loads(pv.read('pluginval-download.json'))
    validator_sha = '3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f'
    require(download['version'] == '1.0.4' and download['verified'] is True and download['asset_id'] == 210999197 and download['actual_size'] == download['expected_size'] == 9202795 and download['actual_sha256'] == download['expected_sha256'] == validator_sha, 'Validator version/digest mismatch')
    require(any(r['text'] == f'Verified official pluginval 1.0.4 macOS asset 210999197: 9202795 bytes, SHA256 {validator_sha}' for r in fetch_block['output']), 'Original fetch confirmation missing')

    install = a45.Evidence({'archive': str(INPUT / (target + '-install.zip'))}, BASE)
    roots = [n for n in install.names if n.endswith('.tar.gz')]
    require(len(roots) == 1, 'Inner archive not unique')
    inner_name = roots[0]
    inner = install.read(inner_name)
    inner_record = a45.record(inner, inner_name)
    package_output = '\n'.join(r['text'] for r in package_block['output'])
    require('"sha256": "' + inner_record['sha256'] + '"' in package_output and 'packages/' + inner_name in package_output, 'Original package digest/path mismatch')
    tar = tarfile.open(fileobj=io.BytesIO(inner), mode='r:gz')
    members = tar.getmembers()
    seen = set()
    for member in members:
        canonical(member.name)
        require(member.name not in seen, 'Duplicate inner TAR member')
        seen.add(member.name)
        require(member.isfile() or member.isdir(), 'Unsupported actual inner TAR member type')
        require(member.size <= 64 * 1024 * 1024, 'Oversize inner TAR member')
    require(sum(m.size for m in members) <= 256 * 1024 * 1024, 'Oversize inner TAR')
    root = inner_name.removesuffix('.tar.gz')
    binary_records = {}
    existing_install = json.loads((BASE / 'release-078c098/verified' / (target + '-install.json')).read_text())
    require(existing_install['source'] == SOURCE and existing_install['run_id'] == RUN and existing_install['status'] == 'artifact_bytes_verified', 'Existing install acceptance mismatch')
    for fmt, suffix in [('AU', '.component'), ('VST3', '.vst3')]:
        path = f'{fmt}/CS-80 Reference{suffix}/Contents/MacOS/CS-80 Reference'
        member = tar.getmember(root + '/' + path)
        require(member.isfile(), 'Executable not a regular file')
        data = tar.extractfile(member).read()
        binary_records[fmt] = a45.record(data, path)
        previous = [x for x in existing_install['binary_headers'] if x['path'] == path]
        require(len(previous) == 1 and previous[0]['sha256'] == binary_records[fmt]['sha256'] and previous[0]['bytes'] == len(data), 'Fresh executable digest disagrees with installer check')
    require(binary_records['AU']['sha256'] == cfg['au_sha'], 'Unexpected distributed AU digest')
    tar.close()
    install.close()
    paths = ['/Users/runner/work/yamaha-cs80/yamaha-cs80/build/CS80Reference_artefacts/Release/AU/CS-80 Reference.component/Contents/MacOS/CS-80 Reference',
             '/Users/runner/Library/Audio/Plug-Ins/Components/CS-80 Reference.component/Contents/MacOS/CS-80 Reference']
    digest_records = []
    for phase, block in [('before', reg_block), ('after', package_block)]:
        member = 'au-payload-' + phase + '-validation.sha256'
        data = pv.read(member)
        lines = data.decode().strip().splitlines()
        require(lines == [cfg['au_sha'] + '  ' + p for p in paths], 'AU digest file does not match both exact paths')
        witness = [r for r in block['output'] if r['text'] in lines]
        require([r['text'] for r in witness] == lines, 'Original AU hash output differs from artifact')
        digest_records.append({'phase': phase, 'member': a45.record(data, member), 'hash_lines': lines, 'original_log_witnesses': witness})

    target_obs = next(t for t in observations['validators'] if t['target'] == target)
    lanes = []
    for lane in target_obs['lanes']:
        name = lane['required_workflow_step']
        step, source, block = bind_step(name)
        command = source['run']
        require(command.startswith('set -euo pipefail\n') and command.count('./pluginval/pluginval.app/Contents/MacOS/pluginval') == 1 and '||' not in command,
                'Original validator exit can be masked')
        require('--strictness-level 10' in command and '--validate-in-process' in command and '--timeout-ms 120000' in command, 'Required validator invocation changed')
        require((' --skip-gui-tests' in command) == (not lane['gui_required']), 'GUI command mismatch')
        require('grep -qx \'SUCCESS\' ' + lane['console']['path'] in command, 'SUCCESS gate missing')
        console = pv.read(lane['console']['path']).decode().strip().splitlines()
        output = '\n'.join(r['text'] for r in block['output']).strip().splitlines()
        require(console == output, 'Original console differs from uploaded lane')
        require(lane['console']['summary']['validation_start_count'] == 1 and lane['validation_invocations'] == 1 and lane['validation_retries'] == 0, 'Unexpected repeated validator invocation')
        success = [r for r in block['output'] if r['text'] == 'SUCCESS']
        require(len(success) == 1 and within(success[0]['timestamp'], step), 'Original success missing/outside step')
        fmt = 'VST3' if lane['lane'] == 'vst3' else 'AU'
        expected_input = '/Users/runner/work/yamaha-cs80/yamaha-cs80/build/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3' if fmt == 'VST3' else '/Users/runner/Library/Audio/Plug-Ins/Components/CS-80 Reference.component'
        require(console[0] == 'Started validating: ' + expected_input, 'Validated input path mismatch')
        require(reg_step['number'] < step['number'] < package_step['number'], 'Validation not between identity checks and packaging')
        lanes.append({'lane': lane['lane'], 'qualified_within_scope': True, 'seed': lane['console']['summary']['seeds'][0],
                      'strictness': 10, 'programs': 523, 'gui_tests': lane['gui_required'], 'original_process_exit': 0,
                      'exit_evidence': 'Inferred from original successful step and exact source/log-matched Bash set -euo pipefail pipeline; direct validator, no retry or error suppression; grep also requires SUCCESS.',
                      'step': step, 'console_matches_original_output': True, 'original_log_start_line': block['start_line'],
                      'original_log_end_line': block['end_line'], 'original_success': success[0],
                      'input_path': expected_input, 'distributed_executable': binary_records[fmt],
                      'validation_invocations': 1, 'validation_retries': 0, 'warnings': lane['console']['summary']['warnings'],
                      'explicit_skips': lane['console']['summary']['explicit_skips'], 'console': lane['console'], 'report': lane['report']})
    pv.close()
    result = {'target': target, 'source': SOURCE, 'run_id': RUN, 'run_attempt': ATTEMPT,
              'producer_job_id': job['id'], 'producer_job_url': job['html_url'], 'producer_job_conclusion': job['conclusion'],
              'run_snapshot_conclusion': run['conclusion'], 'job_api': file_record(cfg['job']), 'run_api': file_record(cfg['run']),
              'original_job_log': file_record(cfg['log']), 'artifacts': artifacts, 'validator_download': download,
              'fetch_step': fetch_step, 'register_step': reg_step, 'package_step': package_step,
              'au_identity': {'full_recursive_bundle_diff_before_and_after_passed': True, 'distributed_executable': binary_records['AU'], 'hash_evidence': digest_records,
                              'meaning': 'Successful unsuppressed diff -qr gates establish registered copy equals packaging source before/after validation; both executable hashes at both phases match freshly hashed distributed bytes.'},
              'vst3_identity': {'distributed_executable': binary_records['VST3'], 'source_packager': source_records['scripts/package-ci-artifacts.py'],
                                'original_package_inner_archive': inner_record, 'package_log_inner_digest_matches': True,
                                'meaning': 'Validated exact build/CS80Reference_artefacts/Release/VST3 bundle is read from that same base by the source-pinned packager; ordered workflow has no intervening rebuild or VST3 mutation. Package self-test only uses a separate TemporaryDirectory.'},
              'inner_tar_preflight': {'entries': len(members), 'canonical_unique_regular_files_or_directories': True, 'extracted': False},
              'lanes': lanes, 'validation_invocations': 4, 'validation_retries': 0,
              'diagnostic_steps': [steps['AU registration diagnostics'], steps['Report pluginval failure']],
              'auval_scope': 'auval -a is only a failure-only registration listing; it was skipped and is not independent AU validation.'}
    results.append(result)

report = {'schema': 1, 'worker': 'a55', 'verified_at_utc': datetime.now(timezone.utc).isoformat(),
          'status': 'eight_original_macos_validator_lanes_and_distributed_payloads_bound',
          'repository': REPOSITORY, 'source': SOURCE, 'run_id': RUN, 'run_attempt': ATTEMPT,
          'release_qualified': False, 'macos_validator_scope_qualified': True,
          'validator_totals': {'required_lanes': 8, 'successful_lanes': 8, 'validation_invocations': 8, 'validation_retries': 0},
          'warning_lane_count': 6, 'external_vst3_validator_skip_lane_count': 2,
          'parser_observations': file_record(OUT / 'validator-observations.json'),
          'source_files': source_records, 'targets': results,
          'limits': ['Overall release qualification remains the parent responsibility; this scope is eight macOS pluginval lanes.',
                     'No new native execution, validator execution, diagnostic rerun, CI rerun, repository edit, or external mutation.',
                     'No acoustic-realism, release-signing/notarization, separate Steinberg validator, standalone auval validation, or DAW-coverage claim.',
                     'The independent A45 parser deliberately leaves its own exit fields unknown and release_qualified false; this separate report binds original step success, exact commands, provenance and payloads.',
                     'ARM pluginval acquisition had two transfer attempts (API 403 then release-URL success); Intel had one transfer. These are download attempts, not validator retries.']}
output = OUT / 'macos-validator-binding.json'
require(not output.exists(), 'Refusing an existing binding report')
output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'status': report['status'], 'validator_totals': report['validator_totals'], 'targets': [x['target'] for x in results], 'output': str(output)}))
