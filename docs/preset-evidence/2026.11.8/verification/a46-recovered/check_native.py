#!/usr/bin/env python3
"""Observe exact native ZIP/API/roster content; never prove original process exit."""
import argparse
import io
import json
from pathlib import Path
import re
import sys
import tempfile
import zipfile
import inspect_receipt as integrity


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--contract', type=Path, default=integrity.HERE / 'source-contract.json')
    for key in ('archive', 'artifact-api', 'out'):
        cli.add_argument('--' + key, type=Path, required=True)
    cli.add_argument('--artifact-id', type=int, required=True)
    cli.add_argument('--target', required=True)
    args = cli.parse_args()
    integrity.require(not args.out.exists() and not args.out.is_symlink(), 'Report exists')
    contract, contract_record = integrity.contract(args.contract)
    parser = integrity.helper('parse-native-evidence.py')
    binder = integrity.helper('bind-parser-archives.py')
    integrity.require(args.target in binder.ARTIFACTS, 'Unknown native target')
    api, api_record = integrity.api_context(args.artifact_api, binder.ARTIFACTS[args.target][0], args.artifact_id)
    raw, receipt = integrity.inspect_archive(args.archive, api, api_record)
    # Parse a private immutable copy, then bind every parsed member back to raw.
    with tempfile.TemporaryDirectory(prefix='a46-native-snapshot-') as temporary:
        frozen = Path(temporary) / 'native.zip'
        frozen.write_bytes(raw)
        observed = parser.native({'target': args.target, 'archive': str(frozen)}, Path(temporary))
    binder.match_record(observed['archive'], raw, 'Native archive')
    bound_members = binder.member_bindings(raw, observed, 'native')
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        last = parser.decode(archive.read('Testing/Temporary/LastTest.log'))
        provenance = parser.decode(archive.read('ci-juce-provenance.txt'))
    fields = {}
    for line in provenance.splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            integrity.require(key not in fields, 'Duplicate JUCE field')
            fields[key] = value
    integrity.require(all(fields.get(k) == v for k, v in {
        'verification': 'PASS', 'source_clean': 'true', 'actual_version': '8.0.11',
        'actual_sha': 'c352e2489000ffeb9e397051377d75eef2efe641', 'workflow_commit': integrity.SOURCE}.items()),
        'Native JUCE/source provenance differs')
    expected = contract['expected_native_names'][args.target]
    names = [row['name'] for row in observed['testcases']]
    integrity.require(len(names) == len(set(names)) and len(expected) == len(set(expected)), 'Duplicate test name')
    markers = re.findall(r'^(\d+)/(\d+) Test: (.+)$', last, re.M)
    framing = (len(markers) == len(names) and {int(x[0]) for x in markers} == set(range(1, len(names) + 1))
               and {int(x[1]) for x in markers} == {len(names)} and {x[2] for x in markers} == set(names)
               and len(re.findall(r'^Start testing:', last, re.M)) == 1
               and len(re.findall(r'^End testing:', last, re.M)) == 1
               and last.rfind('End testing:') > last.rfind('<end of output>'))
    missing, extra = sorted(set(expected) - set(names)), sorted(set(names) - set(expected))
    skip_names = {row['name'] for row in observed['formal_skips']}
    unexpected_skips = sorted(skip_names - set(contract['allowed_formal_skip_names_subject_to_review']))
    new_results = {row['name']: row['result'] for row in observed['testcases']
                   if row['name'] in contract['common_new_native_tests']}
    new_pass = len(new_results) == 6 and all(value == 'passed' for value in new_results.values())
    content_ok = (not missing and not extra and framing and not unexpected_skips and new_pass
                  and observed['status'] == 'observed_tests_green'
                  and all(observed['counts'][k] == 0 for k in ('failed', 'unknown', 'disabled')))
    observed['archive']['path'] = args.archive.name
    result = {'schema': 1, 'status': 'native_content_observed_not_qualified', 'release_qualified': False,
              'source': integrity.SOURCE, 'source_tree': integrity.TREE, 'run_id': integrity.RUN_ID,
              'run_attempt': integrity.ATTEMPT, 'target': args.target, 'contract': contract_record,
              'content_checks_conform': content_ok, 'receipt': receipt, 'bound_members': bound_members,
              'native': observed, 'expected_registered': len(expected), 'missing_names': missing,
              'unexpected_names': extra, 'lasttest_session_complete': framing,
              'unexpected_formal_skip_names': unexpected_skips, 'six_new_tests_passed': new_pass,
              'explicit_skip_review_completed': False, 'original_normal_exit_verified': False,
              'original_upload_attempt_verified': False,
              'remaining_gates': ['Authentic API/acquisition and exact original upload-attempt identity.',
                                 'Actual original successful Test step and unsuppressed normal CTest exit 0.',
                                 'Explicit platform/source capability review of formal and nested skips.',
                                 'All original validator lanes, complete package/payload identity, audition and delivery.']}
    integrity.write_new(args.out, result)
    print(json.dumps({'content_checks_conform': content_ok, 'counts': observed['counts'],
                      'nested_python_skips': observed['nested_python_skips']['total'], 'release_qualified': False}))
    return 0 if content_ok else 1


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as error:
        print(type(error).__name__ + ': ' + str(error), file=sys.stderr)
        sys.exit(2)
