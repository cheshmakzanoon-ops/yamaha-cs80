#!/usr/bin/env python3
"""Reconcile the explicit partial handoff without granting release acceptance.

Reads frozen helpers and original archives; never executes a product, changes a
gate, fetches a network resource, or emits release-qualification.json. A fresh
output directory is mandatory. Later attempts require their own actual handoff.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
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
BASE = Path('docs/preset-evidence/2026.11.7')
CI = BASE / 'ci/source-ee0dabca'
A40 = BASE / 'verification/a40/source-ee0dabca'
EXPECTED_COUNTS = {'linux-x86_64': 613, 'windows-x86_64': 255,
                   'macos-arm64': 277, 'macos-x86_64': 277}
PRODUCERS = {'linux-x86_64': 138455975798, 'windows-x86_64': 138455976256,
             'macos-arm64': 138455976227, 'macos-x86_64': 138455976307}
HELPERS = {
    A40 / 'verify-native-contract.py':
        'cb9597061fe16b590f85cfc75ddbc98e5868c80d1a9d6763f77edfc71b27e32f',
    Path('docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py'):
        'f192bf2ff70d4501f73875eb5bb6416b9adc3b77912e165da4305302724cd846',
    Path('docs/preset-evidence/2026.11.6/verification/a52/bind-parser-archives.py'):
        '343cbc293d388fcf72484e05eaf52933e1bdbed3e13f194e59799af2cdf58b11',
}


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--repo', type=Path, required=True)
    ap.add_argument('--out-dir', type=Path, required=True)
    args = ap.parse_args()
    repo = args.repo.resolve()
    out = args.out_dir.resolve()
    require(not out.exists(), 'Refusing an existing output directory')
    inputs = {}

    def record(path):
        path = path if path.is_absolute() else repo / path
        raw = path.read_bytes()
        try:
            name = str(path.relative_to(repo))
        except ValueError:
            name = str(path)
        row = {'path': name, 'bytes': len(raw), 'sha256': digest(raw)}
        if name in inputs:
            require(inputs[name] == row, 'Input changed during inspection: ' + name)
        inputs[name] = row
        return raw, row

    def read(path):
        return json.loads(record(path)[0])

    def reference(path):
        return record(path)[1]

    def git(*args):
        return subprocess.check_output(['git', '-C', str(repo), *args]).decode().strip()

    def dump(name, obj):
        (out / name).write_text(json.dumps(obj, indent=2) + '\n')

    for path, pin in HELPERS.items():
        require(record(path)[1]['sha256'] == pin, 'Frozen helper changed: ' + str(path))
    spec = importlib.util.spec_from_file_location('a58_frozen_native_contract', repo / A40 / 'verify-native-contract.py')
    native_contract = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native_contract)
    parser, binder = native_contract.load_helpers(repo)
    contract = native_contract.derive_contract(repo)
    require(contract == read(A40 / 'contract.json'), 'Prepared native contract differs from fresh derivation')
    require(contract['expected_registered'] == EXPECTED_COUNTS, 'Expected native roster changed')
    require(sum(EXPECTED_COUNTS.values()) == 1422, 'Expected aggregate roster differs')
    require(git('rev-parse', SOURCE + '^{tree}') == TREE, 'Frozen source tree differs')
    protected = ['.github', 'CMakeLists.txt', 'cmake', 'scripts', 'src', 'tests', 'assets', 'installers']
    changed = git('diff', '--name-only', SOURCE, '--', *protected).splitlines()
    untracked = git('ls-files', '--others', '--exclude-standard', '--', *protected).splitlines()
    require(not changed and not untracked, 'Protected working source differs from binary source')

    inventory_path = BASE / 'verification/a12/inventory.json'
    require(record(inventory_path)[1]['sha256'] == '55c08568074a6560ef10c8c525733a40aec42d9b8c6d20644bf4984cdd0d3e0d',
            'Independent inventory changed')
    audition_contract = read(A40 / 'prepared-contract.json')
    require(audition_contract['source_commit'] == SOURCE and audition_contract['source_tree'] == TREE,
            'Audition contract source differs')
    require(audition_contract['accepted_archive_content'] is False, 'Preparation is not actual audition acceptance')
    for name in ('a03', 'a40', 'a42', 'a43', 'a53', 'a54', 'a55', 'a56', 'a57'):
        reference(BASE / 'reviews' / (name + '.md'))

    folders = {
        'macos-arm64': CI / 'macos-arm64',
        'windows-x86_64': CI / 'windows-x86_64',
        'macos-x86_64': CI / 'macos-x86_64/attempt-1-failed',
    }
    artifacts, jobs, archives, logs = [], {}, {}, {}
    for target, folder in folders.items():
        receipt = read(folder / 'retrieval-receipt.json')
        require(receipt['source_commit'] == SOURCE and receipt['run_id'] == RUN and receipt['attempt'] == 1,
                'Retrieval context differs: ' + target)
        for name, expected in receipt['files'].items():
            actual = reference(folder / name)
            require(all(actual[k] == expected[k] for k in ('bytes', 'sha256')), 'Acquired input changed: ' + name)
        job = read(folder / 'producer-job-api.json')
        require((job['head_sha'], job['run_id'], job['run_attempt'], job['id']) ==
                (SOURCE, RUN, 1, PRODUCERS[target]), 'Original producer context differs')
        require(job['status'] == 'completed', 'Partial snapshot needs a completed producer')
        jobs[target] = job
        logs[target] = parser.decode(record(folder / 'producer-job.log')[0])
        if target == 'windows-x86_64':
            consumer = read(folder / 'validator-job-api.json')
            require((consumer['head_sha'], consumer['run_id'], consumer['run_attempt'], consumer['id']) ==
                    (SOURCE, RUN, 1, 138483772246), 'Original Windows consumer differs')
            require(consumer['status'] == 'completed' and consumer['conclusion'] == 'success', 'Windows consumer failed')
            logs['windows-consumer'] = parser.decode(record(folder / 'validator-job.log')[0])
        for entry in receipt['archives']:
            path = Path(entry['local_path'])
            raw, local_record = record(path)
            require(all(local_record[k] == entry[k] for k in ('bytes', 'sha256')), 'Acquired archive changed')
            api = read(folder / (entry['kind'] + '-api.json'))
            require(api['id'] == entry['artifact_id'] and api['size_in_bytes'] == len(raw)
                    and api['digest'] == 'sha256:' + digest(raw), 'Archive/API identity differs')
            require(api['workflow_run']['head_sha'] == SOURCE and api['workflow_run']['id'] == RUN,
                    'Archive/API source/run differs')
            require(api['expired'] is False, 'Artifact metadata reports expired')
            owner = consumer if target == 'windows-x86_64' and entry['kind'] == 'validator' else job
            log = logs['windows-consumer'] if owner['id'] == 138483772246 else logs[target]
            success = f"Artifact {api['name']} has been successfully uploaded! Final size is {len(raw)} bytes. Artifact ID is {api['id']}"
            final = f"Artifact {api['name']}.zip successfully finalized. Artifact ID {api['id']}"
            require(log.count(success) == 1 and log.count(final) == 1,
                    'Unique original upload identity missing: ' + api['name'])
            require(log.count('SHA256 digest of uploaded artifact zip is ' + digest(raw)) == 1,
                    'Unique original upload digest missing')
            row = {'target': target, 'kind': entry['kind'], 'source_commit': SOURCE, 'run_id': RUN,
                   'producing_job_id': owner['id'], 'producing_attempt': owner['run_attempt'],
                   'artifact_id': api['id'], 'name': api['name'], 'bytes': len(raw), 'sha256': digest(raw),
                   'created_at': api['created_at'], 'expires_at': api['expires_at'],
                   'local_archive': local_record, 'artifact_api': reference(folder / (entry['kind'] + '-api.json')),
                   'original_upload_id_size_digest_lines_found_once': True,
                   'original_job_api': reference(folder / ('validator-job-api.json' if owner['id'] == 138483772246 else 'producer-job-api.json')),
                   'acquisition_receipt': reference(folder / 'retrieval-receipt.json'),
                   'scope': 'Reconciles retained authenticated acquisition and original job/log identities; API has no standalone producing-attempt field.'}
            artifacts.append(row)
            archives[(target, entry['kind'])] = path

    native_rows, failure = {}, None
    for target, folder in folders.items():
        observed = parser.native({'target': target, 'archive': str(archives[(target, 'native')])}, repo)
        with zipfile.ZipFile(archives[(target, 'native')]) as z:
            last = parser.decode(z.read('Testing/Temporary/LastTest.log'))
        names = [r['name'] for r in observed['testcases']]
        require(set(names) == set(contract['expected_names'][target]) and len(names) == EXPECTED_COUNTS[target],
                'Exact source-derived native names differ: ' + target)
        last_cases = parser.lasttest_cases(last)
        new_rows = [r for r in observed['testcases'] if r['name'] in native_contract.NEW]
        require(len(new_rows) == 4 and all(r['result'] == 'passed' for r in new_rows), 'New native regression failed')
        audit = next(r for r in observed['testcases'] if r['name'] == 'cs80_library_audit_tests')
        require(audit['result'] == 'passed' and 'library audit passed: 301 curated + 222 original circuit programs'
                in last_cases['cs80_library_audit_tests']['output'], 'LibraryAudit did not complete all 523')
        if target != 'macos-x86_64':
            skip_path = BASE / 'verification/a53/phase1-complete' / target / 'skip-review.json'
            review = read(skip_path)
            native_contract.check_observation(observed, contract['expected_names'][target], last, review, target)
            accepted_content = True
            rejection = None
        else:
            require(observed['counts'] == {'registered': 277, 'passed': 274, 'failed': 1,
                    'formal_skipped': 2, 'disabled': 0, 'unknown': 0}, 'Intel original failure counts differ')
            require(observed['failures'] == ['cs80_ui_smoke'], 'Intel original failed case differs')
            require(jobs[target]['conclusion'] == 'failure', 'Intel producer failure not retained')
            try:
                native_contract.check_observation(observed, contract['expected_names'][target], last, {}, target)
            except ValueError as exc:
                rejection = str(exc)
                require(rejection == 'Native observation is not green', 'Unexpected frozen checker refusal')
            else:
                raise ValueError('Frozen gate wrongly accepted failed Intel observation')
            accepted_content = False
            ui = last_cases['cs80_ui_smoke']['output']
            failed_lines = [line for line in ui.splitlines() if line.startswith('FAIL:')]
            require(failed_lines == ['FAIL: source screenshot meter reflects real rendered audio through its normal GUI timer'],
                    'Intel assertion identity differs')
            failure = {'source_commit': SOURCE, 'run_id': RUN, 'producing_attempt': 1,
                       'producer_job_id': PRODUCERS[target], 'target': target,
                       'status': 'original_failed_attempt_preserved', 'counts': observed['counts'],
                       'nested_python_skips': observed['nested_python_skips'],
                       'failed_test': 'cs80_ui_smoke', 'assertion_lines': failed_lines,
                       'relevant_output_lines': [x for x in ui.splitlines() if x.startswith(('source screenshot fixture:', 'UI SMOKE TEST:'))],
                       'ui_output_sha256': digest(ui.encode()), 'library_audit': audit,
                       'library_audit_output': last_cases['cs80_library_audit_tests']['output'],
                       'original_ctest_numeric_exit': 8,
                       'exit_basis': 'Retained producer log explicitly reports Process completed with exit code 8 after the one-failure CTest summary.',
                       'downstream_steps': [{k: step[k] for k in ('name', 'status', 'conclusion')}
                                            for step in jobs[target]['steps'] if step['number'] in range(8, 17)],
                       'frozen_native_gate_refusal': rejection, 'root_cause_established': False,
                       'flake_classification_established': False, 'successful_retry_observed': False,
                       'limits': ['No timer/product root cause inferred from this bounded failure review.',
                                  'Any later attempt retains a separate original job and artifact identity.',
                                  'Diagnostic or single-test success cannot erase this failed full producer attempt.']}
            require('##[error]Process completed with exit code 8.' in logs[target], 'Original CTest nonzero exit absent')
        native_rows[target] = {
            'target': target, 'source_commit': SOURCE, 'run_id': RUN, 'producing_attempt': 1,
            'producer_job_id': PRODUCERS[target], 'original_producer_conclusion': jobs[target]['conclusion'],
            'counts': observed['counts'], 'formal_skips': observed['formal_skips'],
            'nested_python_skips': observed['nested_python_skips'], 'failures': observed['failures'],
            'exact_source_derived_name_set_equal': True,
            'ordered_actual_names_sha256': digest(('\n'.join(names) + '\n').encode()),
            'native_content_check_passed': accepted_content, 'frozen_native_gate_refusal': rejection,
            'all_four_new_tests_passed': True, 'library_audit': audit,
            'junit': observed['inputs']['junit'], 'lasttest': observed['inputs']['lasttest'],
            'ctest_seconds_junit': observed['ctest_seconds_junit'],
            'native_execution_acceptance_claimed_by_a58': False,
        }
    native_rows['linux-x86_64'] = {'target': 'linux-x86_64', 'expected_registered': 613,
                                 'counts': None, 'status': 'actual_native_archive_not_in_this_handoff',
                                 'native_execution_acceptance_claimed_by_a58': False}

    arm_path = CI / 'macos-arm64/platform-qualification.json'
    arm = read(arm_path)
    require(arm['platform_qualified'] is True and arm['source_commit'] == SOURCE and arm['source_tree'] == TREE
            and (arm['run_id'], arm['run_attempt'], arm['job_id']) == (RUN, 1, PRODUCERS['macos-arm64']),
            'ARM platform acceptance context differs')
    require(arm['native_counts'] == native_rows['macos-arm64']['counts'], 'ARM accepted native counts differ')
    win_path = CI / 'windows-x86_64/original-ci-binding.json'
    win = read(win_path)
    require(win['source'] == SOURCE and win['tree'] == TREE and (win['run_id'], win['attempt']) == (RUN, 1),
            'Windows original binding context differs')
    require(win['original_ctest_exit_zero_verified'] is True, 'Windows original CTest exit binding missing')
    validator_rows = []
    for target, external, ref in [('macos-arm64', arm['validator_outcomes'], arm_path),
                                  ('windows-x86_64', win['validators']['lanes'], win_path)]:
        observed = parser.validators({'target': target, 'archive': str(archives[(target, 'validator')])}, repo)
        require(observed['status'] == 'observed_logs_green_unbound', 'Validator text failure')
        for lane in observed['lanes']:
            matching = [r for r in external if r['console']['sha256'] == lane['console']['sha256']]
            require(len(matching) == 1, 'Original validator binding not unique')
            actual = matching[0]
            require(actual['report']['sha256'] == lane['report']['sha256']
                    and actual['original_normal_exit'] == 0, 'Original validator process/report binding differs')
            validator_rows.append({'target': target, 'lane': lane['lane'],
                'required_workflow_step': lane['required_workflow_step'], 'producing_attempt': 1,
                'original_job_id': PRODUCERS[target] if target == 'macos-arm64' else 138483772246,
                'source_commit': SOURCE, 'run_id': RUN, 'pluginval_version': '1.0.4',
                'seed': lane['console']['summary']['seeds'][0], 'strictness': 10, 'programs': 523,
                'gui': lane['gui_required'], 'original_normal_exit': actual['original_normal_exit'],
                'exit_basis': actual['exit_basis'], 'validation_invocations': lane['validation_invocations'],
                'validation_retries': lane['validation_retries'], 'warnings': lane['console']['summary']['warnings'],
                'explicit_skips': lane['console']['summary']['explicit_skips'],
                'preserved_failed_attempts': lane['preserved_failed_attempts'],
                'original_process_binding': reference(ref),
                'console': {k: lane['console'][k] for k in ('path', 'bytes', 'sha256')},
                'report': {k: lane['report'][k] for k in ('path', 'bytes', 'sha256')}})
    require(len(validator_rows) == 7, 'Partial validator lane count differs')

    packages = read(BASE / 'verification/a57/installation-matrix-phase1.json')
    for target in ('macos-arm64', 'windows-x86_64'):
        entry = packages['targets'][target]
        artifact = next(r for r in artifacts if r['target'] == target and r['kind'] == 'install')
        require(entry['artifact_id'] == artifact['artifact_id'] and entry['outer_zip']['bytes'] == artifact['bytes']
                and entry['outer_zip']['sha256'] == artifact['sha256']
                and entry['package_status'] == 'actual_package_crosscheck_passed', 'A57 installation identity differs')
    for path in [CI / 'windows-x86_64/payload-binding.json', CI / 'windows-x86_64/install-acceptance.json',
                 CI / 'windows-x86_64/docs-and-notices.json', CI / 'macos-arm64/archive-payload-binding.json',
                 CI / 'macos-arm64/install-acceptance.json']:
        reference(path)
    get_path = CI / 'public-downloads/macos-arm64-full-get-0001.json'
    public = read(get_path)
    arm_install = next(r for r in artifacts if r['target'] == 'macos-arm64' and r['kind'] == 'install')
    require(public['source'] == SOURCE and public['run_id'] == RUN and public['artifact_id'] == arm_install['artifact_id'],
            'Public GET context differs')
    require(public['method'] == 'GET' and public['credentials_supplied'] is False and public['status'] == 200
            and public['public_route_verified'] is True, 'Public GET not verified')
    require(public['actual_bytes'] == arm_install['bytes'] and public['actual_sha256'] == arm_install['sha256']
            and not any(k.lower() == 'content-range' for k in public['headers']), 'Public full body differs')

    review_counts = {k: sum(r['counts'][k] for r in native_rows.values() if r['counts'] is not None)
                     for k in ('registered', 'passed', 'failed', 'formal_skipped', 'disabled', 'unknown')}
    summary = {
        'schema': 1, 'status': 'bounded_partial_reconciliation_not_release_acceptance',
        'source_commit': SOURCE, 'source_tree': TREE, 'factory_revision': '2026.11.7', 'run_id': RUN,
        'recorded_at_utc': datetime.now(timezone.utc).isoformat(), 'documentation_head_at_inspection': git('rev-parse', 'HEAD'),
        'source_protection': {'tracked_changes': changed, 'untracked_protected_inputs': untracked},
        'native_expected_registered': EXPECTED_COUNTS, 'native_expected_registered_total': 1422,
        'native_observed_partial_totals': review_counts,
        'native_observed_partial_nested_python_skips': sum(r['nested_python_skips']['total'] for r in native_rows.values() if r['counts'] is not None),
        'native_targets_observed': 3, 'all_four_native_accepted': False,
        'accepted_external_platforms_at_this_snapshot': ['macos-arm64'],
        'windows_component_checks_reconciled': True, 'windows_final_platform_receipt_in_this_handoff': None,
        'validator_lanes_reconciled': 7, 'validator_lanes_required': 15,
        'validator_invocations_reconciled': sum(r['validation_invocations'] for r in validator_rows),
        'validator_retries_reconciled': sum(r['validation_retries'] for r in validator_rows),
        'installation_packages_crosschecked_externally': 2, 'installation_packages_required': 4,
        'audition_prepared_contract': audition_contract['expected'], 'audition_actual_observed': None,
        'public_gets_reconciled': 1, 'public_gets_required': 5,
        'earliest_expiry_across_all_five': None,
        'all_60_review_completion_accepted': False, 'final_signoff_accepted': False,
        'cross_platform_qualified': False, 'release_qualified': False,
        'blocked_gates': ['Intel attempt 1 native/UI failure; no accepted replacement attempt supplied.',
                          'Linux original native/validator/install evidence absent from this handoff.',
                          'Actual audition ZIP and content/CI acceptance absent.',
                          'Four remaining actual public GETs absent.',
                          'Final provenance/skip/package reconciliation, 60-review ledger and final signoff pending.'],
        'limits': ['This zero-exit reviewer records the observed product failure; it is not a green product result.',
                   'Expected totals are contract requirements, not observed successful executions.',
                   'Three-target partial totals include the failed Intel attempt and must not be published as qualified native totals.',
                   'All archive producing attempts are taken from individual original job/upload records.',
                   'No new presets, product run, upload, network request, shared documentation edit, Git index edit or remote mutation.'],
    }
    for row in list(inputs.values()):
        path = Path(row['path'])
        path = path if path.is_absolute() else repo / path
        raw = path.read_bytes()
        require(len(raw) == row['bytes'] and digest(raw) == row['sha256'], 'Input changed before snapshot closed')
    out.mkdir(parents=True)
    dump('snapshot.json', summary)
    dump('native-observations.json', native_rows)
    dump('validator-lanes.json', validator_rows)
    dump('artifact-attempt-matrix.json', artifacts)
    dump('intel-original-failure.json', failure)
    dump('input-records.json', {'schema': 1, 'unchanged_at_close': True, 'files': list(inputs.values())})
    dump('reproducer.json', {'script': reference(Path(__file__).resolve()),
                           'argv': sys.argv, 'scope': 'Bounded offline read-only reconciliation; no product execution.'})
    print(json.dumps({k: summary[k] for k in ('status', 'native_observed_partial_totals',
          'validator_lanes_reconciled', 'public_gets_reconciled', 'release_qualified')}, indent=2))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, KeyError, OSError) as exc:
        print('ERROR: ' + str(exc), file=sys.stderr)
        sys.exit(2)
