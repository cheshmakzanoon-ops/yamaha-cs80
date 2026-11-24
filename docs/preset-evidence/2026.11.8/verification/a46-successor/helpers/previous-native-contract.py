#!/usr/bin/env python3
"""Check .7 native content against the frozen full roster; never qualify a release.

Reuse the unchanged .6 A52 parser/binder. --describe derives the contract from
frozen Git objects. --inputs checks actual native ZIP/API/skip-review inputs.
API authenticity, original producing attempt and process exits remain external.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
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
PREVIOUS = '18711f93a39aa49f3d6f9a41784be456224facbe'
BASELINE = '078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
CONTEXT = dict(factory_revision='2026.11.7', source=SOURCE, tree=TREE,
               run_id=45397189410, attempt=1)
TARGETS = {'linux-x86_64': (587, 613), 'windows-x86_64': (229, 255),
           'macos-arm64': (251, 277), 'macos-x86_64': (251, 277)}
NEW = {'cs80_piano_partial_decay_tests', 'cs80_body_noise_rate_tests',
       'cs80_string_body_formant_tests', 'cs80_low_choir_spectrum_tests'}
SKIPPABLE = {'cs80_filter_batch_cap_tests', 'cs80_stereo_pra_exact_tests'}
HELPER_ROOT = 'docs/preset-evidence/2026.11.6/verification/a52/'
PINS = {'parse-native-evidence.py': 'f192bf2ff70d4501f73875eb5bb6416b9adc3b77912e165da4305302724cd846',
        'bind-parser-archives.py': '343cbc293d388fcf72484e05eaf52933e1bdbed3e13f194e59799af2cdf58b11'}
CMAKE_PINS = {BASELINE: 'e0e8bd350318c1eacca266b2397878172c74a72a0af41019227f728997b8369d',
              PREVIOUS: '136e8b7b6d75057eeefc291246a0a4ad2a198a5d0b0e8ffd7335327823c4db33',
              SOURCE: '27e3314f93f9cab0b61633b295021b9bffb274044b663a53faa74b201d65bd92'}
SOURCE_PINS = {'.github/workflows/build.yml': '9cb87b71aefd18f0e753f203d8f2fa2b537935fedde33fdc87f49eb64b04fccb',
               'src/tools/ui_snapshot.cpp': '6ec9948ff25756e98bd52ee5e7945ecc781b79ec6c9a004cc973d70817ca175e',
               'tests/InstrumentBodyHostTests.h': 'bb3dc27350b2e911f91edcea4665bcccf8f5bf11013adf83f55772d10f91733c'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def load_helpers(repo):
    loaded = []
    for name, digest in PINS.items():
        path = repo / HELPER_ROOT / name
        require(sha(path.read_bytes()) == digest, 'Historical helper changed: ' + name)
        spec = importlib.util.spec_from_file_location('a29_' + name.replace('-', '_'), path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        loaded.append(module)
    return loaded


def derive_contract(repo):
    require(git(repo, 'rev-parse', SOURCE + '^{tree}').decode().strip() == TREE, 'Frozen tree differs')
    for commit, digest in CMAKE_PINS.items():
        require(sha(git(repo, 'show', commit + ':CMakeLists.txt')) == digest, 'Frozen CMake differs')
    for path, digest in SOURCE_PINS.items():
        require(sha(git(repo, 'show', SOURCE + ':' + path)) == digest, 'Frozen source input differs: ' + path)
    additions = []
    for before, after, count in ((BASELINE, PREVIOUS, 22), (PREVIOUS, SOURCE, 4)):
        diff = git(repo, 'diff', '--unified=0', before, after, '--', 'CMakeLists.txt').decode()
        added = re.findall(r'^\+\s*add_test\(NAME (cs80_\w+)', diff, re.M)
        require(not re.search(r'^-\s*add_test\(', diff, re.M), 'Unexpected removed registration')
        require(len(added) == len(set(added)) == count, 'Registration delta differs')
        additions.append(sorted(added))
    require(set(additions[1]) == NEW, '.7 additions differ')
    rosters, records = {}, {}
    for target, (old_count, expected_count) in TARGETS.items():
        path = 'docs/preset-evidence/2026.11.4/ci/source-078c098/' + target + '-native-observations.json'
        raw = git(repo, 'show', SOURCE + ':' + path)
        rows = json.loads(raw)['native']
        require(len(rows) == 1 and rows[0]['target'] == target, 'Historical target differs')
        old = [row['name'] for row in rows[0]['testcases']]
        require(len(old) == len(set(old)) == old_count, 'Historical roster differs')
        names = old + additions[0] + additions[1]
        require(len(names) == len(set(names)) == expected_count, 'Derived full roster differs')
        rosters[target] = sorted(names)
        records[target] = dict(path=path, bytes=len(raw), sha256=sha(raw), historical_registered=old_count)
    return dict(schema=1, status='contract_prepared_not_native_acceptance', **CONTEXT,
                release_qualified=False, baseline_source=BASELINE, previous_source=PREVIOUS,
                common_additions_through_6=additions[0], common_additions_7=additions[1],
                expected_names=rosters, expected_registered={t: len(n) for t, n in rosters.items()},
                historical_observation_records=records, helper_sha256=PINS,
                cmake_sha256=CMAKE_PINS, frozen_source_sha256=SOURCE_PINS,
                roster_condition='Same native CI configuration/platform conditions as retained .4; all 26 added registrations share CS80_BUILD_TESTS with no additional platform guard.')


def check_context(value):
    require(isinstance(value, dict) and value == CONTEXT
            and all(type(value[key]) is type(expected) for key, expected in CONTEXT.items()),
            'Source/tree/revision/run/attempt claim differs from the .7 contract')


def check_observation(observed, names, lasttest, review, target):
    require(observed['target'] == target, 'Native target differs')
    rows = observed['testcases']
    actual = [row['name'] for row in rows]
    require(len(actual) == len(set(actual)) and set(actual) == set(names),
            'Full native roster differs; missing=' + repr(sorted(set(names) - set(actual)))
            + '; unexpected=' + repr(sorted(set(actual) - set(names))))
    require(observed['status'] == 'observed_tests_green', 'Native observation is not green')
    require(all(observed['counts'][key] == 0 for key in ('failed', 'unknown', 'disabled')),
            'Failed, unknown or disabled native case')
    require(all(row['result'] in ('passed', 'formal_skipped') for row in rows), 'Unaccepted native result')
    skipped = {row['name'] for row in rows if row['result'] == 'formal_skipped'}
    require(skipped <= SKIPPABLE, 'Non-capability test was skipped')
    require(all(row['result'] == 'passed' for row in rows if row['name'] in NEW), 'A new .7 gate did not pass')
    # The .6 parser checks each case, but does not require full-session framing.
    markers = re.findall(r'^(\d+)/(\d+) Test: (.+)$', lasttest, re.M)
    require(len(markers) == len(names) and {int(x[0]) for x in markers} == set(range(1, len(names) + 1))
            and {int(x[1]) for x in markers} == {len(names)}, 'Incomplete LastTest numbered roster')
    require(len(re.findall(r'^Start testing:', lasttest, re.M)) == 1
            and len(re.findall(r'^End testing:', lasttest, re.M)) == 1
            and lasttest.rfind('End testing:') > lasttest.rfind('<end of output>'), 'Incomplete LastTest session')
    check_context(review.get('provenance'))
    require(review.get('target') == target and review.get('reviewed') is True
            and isinstance(review.get('review_note'), str) and bool(review['review_note'].strip()),
            'Missing explicit target skip review')
    require(review.get('formal_skips') == observed['formal_skips'], 'Formal skip review differs')
    require(review.get('nested_python_skips') == observed['nested_python_skips'], 'Nested Python skip review differs')


def check_target(item, base, parser, binder, contract):
    target = item['target']
    require(set(item) == {'target', 'archive', 'api', 'skip_review'}, 'Expected exact native ZIP/API/review fields')
    paths = {key: (base / item[key]).resolve() for key in ('archive', 'api', 'skip_review')}
    raw = binder.snapshot(paths['archive'], binder.MAX_ZIP)
    api_raw = binder.snapshot(paths['api'], 32 * 1024**2)
    api = binder.parse(api_raw)
    binding = binder.check_receipt(api, raw, binder.ARTIFACTS[target][0], SOURCE, CONTEXT['run_id'], binder.REPOSITORY)
    require(api.get('expired') is False, 'Native artifact is expired or expiry status is missing')
    expires = datetime.fromisoformat(api['expires_at'].replace('Z', '+00:00'))
    require(expires.tzinfo is not None and expires > datetime.now(timezone.utc), 'Native artifact retention has elapsed')
    observed = parser.native(dict(target=target, archive=str(paths['archive'])), base)
    binder.match_record(observed.get('archive'), raw, 'Native parser archive')
    binder.member_bindings(raw, observed, 'native')
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        lasttest = parser.decode(archive.read('Testing/Temporary/LastTest.log'))
    review_raw = binder.snapshot(paths['skip_review'], 32 * 1024**2)
    review = binder.parse(review_raw)
    check_observation(observed, contract['expected_names'][target], lasttest, review, target)
    last_cases = parser.lasttest_cases(lasttest)
    require(all(case['lasttest_status'] in ('Passed', 'Skipped') for case in last_cases.values()),
            'LastTest contains a failed or unknown result')
    ui = last_cases['cs80_ui_smoke']['output']
    markers = [r'^UI SMOKE START: ', r'^UI SMOKE PHASE: instrument body host identity and recall$',
               r'^UI SMOKE TIMING: phase=instrument body host identity and recall seconds=', r'^UI SMOKE TEST PASSED$']
    found = [list(re.finditer(pattern, ui, re.M)) for pattern in markers]
    require(all(len(matches) == 1 for matches in found)
            and [matches[0].start() for matches in found] == sorted(matches[0].start() for matches in found),
            'Missing, repeated or unordered complete UI/body identity evidence')
    fields = dict(line.split('=', 1) for line in observed['juce_provenance']['text'].splitlines() if '=' in line)
    expected = dict(expected_sha='c352e2489000ffeb9e397051377d75eef2efe641',
                    actual_sha='c352e2489000ffeb9e397051377d75eef2efe641', expected_version='8.0.11',
                    actual_version='8.0.11', source_clean='true', verification='PASS',
                    workflow_commit=SOURCE, workflow_run=str(CONTEXT['run_id']))
    require(all(fields.get(key) == value for key, value in expected.items()), 'JUCE/source provenance text differs')
    return dict(target=target, counts=observed['counts'], native_content_contract_verified=True,
                api_zip_binding=binding, parsed_member_records=observed['inputs'],
                formal_skips=observed['formal_skips'], nested_python_skips=observed['nested_python_skips'],
                skip_review_sha256=sha(review_raw), api_input_sha256=sha(api_raw),
                ui_body_identity_phase_complete=True, original_attempt_binding_verified=False,
                original_ctest_process_exit_verified=False, native_results_accepted=False,
                platform_qualified=False, release_qualified=False)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--repo', type=Path, required=True)
    mode = cli.add_mutually_exclusive_group(required=True)
    mode.add_argument('--describe', action='store_true')
    mode.add_argument('--inputs', type=Path)
    cli.add_argument('--target', choices=TARGETS, help='Explicit preliminary single-target scope; omit for all four')
    cli.add_argument('--out', type=Path, required=True)
    args = cli.parse_args()
    require(not args.out.exists(), 'Refusing an existing output path')
    parser, binder = load_helpers(args.repo)
    contract = derive_contract(args.repo)
    result = contract
    if args.inputs:
        manifest = binder.parse(binder.snapshot(args.inputs, 32 * 1024**2))
        check_context(manifest.get('provenance'))
        items = manifest.get('native', [])
        targets = [item['target'] for item in items]
        wanted = {args.target} if args.target else set(TARGETS)
        require(len(targets) == len(set(targets)) and set(targets) == wanted, 'Missing, duplicate or unexpected native target')
        rows = [check_target(item, args.inputs.resolve().parent, parser, binder, contract) for item in items]
        result = dict(schema=1, status='native_content_contract_verified_external_gates_pending', **CONTEXT,
                      native=rows, all_four_native_targets_checked=set(targets) == set(TARGETS),
                      native_results_accepted=False, platform_qualified=False, release_qualified=False,
                      checker_sha256=sha(Path(__file__).read_bytes()), helper_sha256=PINS,
                      limitation='API authenticity, exact original job/upload attempt and CTest exits are not proved; validators, payloads, packaging, auditions and distribution remain independent.')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open('x') as stream:
        json.dump(result, stream, indent=2, allow_nan=False)
        stream.write('\n')
    print(json.dumps(dict(status=result['status'], release_qualified=False)))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError, zipfile.BadZipFile) as error:
        print('ERROR: ' + str(error), file=sys.stderr)
        sys.exit(2)
