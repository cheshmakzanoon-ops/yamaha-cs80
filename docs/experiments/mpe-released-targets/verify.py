#!/usr/bin/env python3
"""Verify bundled bytes and recorded outcomes. Does not rerun product tests.

Checks remain enabled under python -O. A manifest is an integrity index, not a
signature or independent authentication. --external requires original workspace.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
sys.dont_write_bytecode = True

BASE = '2ade213bc4dec34b04352063a72e97dac4cc5667'
PRODUCTION = 'c0219bd0de8559d72fce4194a849414de5ac7e61'
FOCUSED = '300356958b3cde471f8dc14a7b505070a4cffc8c'
NATIVE_SHA = '11fbae61a11d603c50e0f8d915e50d3731c42eacdd19e0378e79d36884130cf4'
FINAL = '04597f5ee87da1f6390a8682298a1a0e656f834c'
PROFILES = {'gcc-generic', 'gcc-native', 'clang-generic', 'clang-native'}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def data(path):
    return json.loads(path.read_text(encoding='utf-8'))


def local(root, name):
    require(isinstance(name, str) and not Path(name).is_absolute(), 'Unsafe inventory name')
    path = (root/name).resolve()
    require(path != root.resolve() and path.is_relative_to(root.resolve()), 'Escaped inventory path')
    return path


def inventory(root, records):
    require(isinstance(records, dict) and bool(records), 'Empty inventory')
    for name, record in records.items():
        path = local(root, name)
        require(path.is_file(), 'Missing file: ' + name)
        require(type(record.get('bytes')) is int and record['bytes'] >= 0, 'Invalid size: ' + name)
        require(re.fullmatch('[0-9a-f]{64}', record.get('sha256', '')) is not None, 'Invalid hash: ' + name)
        require(path.stat().st_size == record['bytes'] and digest(path) == record['sha256'], 'Changed file: ' + name)


def terminal(text, prefix, checks, failures, diagnostics=False):
    lines = text.splitlines()
    expected = f'{prefix}: {checks} checks, {failures} failures'
    require(bool(lines) and lines[-1] == expected, 'Missing/incomplete terminal summary: ' + prefix)
    require(sum(line.startswith(prefix + ':') for line in lines) == 1, 'Duplicate terminal summary')
    if diagnostics:
        require(len(lines) == failures + 1 and all(s.startswith('FAIL: ') for s in lines[:-1]), 'Failure diagnostics do not match summary')


def ctest(text, count):
    require(text.count(f'100% tests passed, 0 tests failed out of {count}') == 1, 'Missing/duplicate successful CTest result')
    require('No tests were found' not in text, 'Empty CTest run')
    # Summary alone is insufficient; require every complete passed test record.
    rows = re.findall(r'^\s*(\d+)/(\d+) Test\s+#\d+:\s+([^\n]+?)\s+\.+\s+Passed\s+[^\n]+$', text, re.M)
    require(len({name for _, _, name in rows}) == count, 'Duplicate CTest names')
    require(len(rows) == count and {int(a) for a, _, _ in rows} == set(range(1, count+1)) and all(int(b) == count for _, b, _ in rows), 'Incomplete/duplicate CTest cases')


def command_logs(folder, records, expected):
    require(len(records) == len(expected) and {r['stage'] for r in records} == set(expected), 'Incomplete/duplicate command stages')
    for row in records:
        require(row.get('status', row.get('exit_code')) == expected[row['stage']], 'Unexpected command status: ' + row['stage'])
        log = folder/Path(row['log']).name
        require(log.is_file() and digest(log) == row['sha256'], 'Command log mismatch')


def integrated_preservation(pins, integration=None):
    preserved_records = dict(pins['preserved_tree_entries'])
    if integration is not None:
        original = {n:r for n,r in preserved_records.items() if n.startswith('docs/screenshots/')}
        newer = integration['screenshot_tree']
        require(integration['historical_baseline'] == BASE and integration['final_source_test_freeze'] == FINAL, 'Integration changed historical attribution')
        require(integration['runtime_repeated'] is False, 'Screenshot integration claims a runtime rerun')
        require(integration['snapshot_parent_parent_tree_equals_baseline'] is True, 'Screenshot parent baseline tree differs')
        require(set(newer) == set(original) and len(newer) == 70, 'Integration screenshot inventory changed scope')
        require(all(n.startswith('docs/screenshots/') for n in newer), 'Integration escapes screenshot scope')
        delta = sorted(n for n in newer if newer[n] != original[n])
        require(delta == integration['changed_screenshot_paths'] and len(delta) == 24, 'Integration screenshot delta differs')
        require(integration['screenshot_only_parent_delta'] == delta, 'Screenshot child changes other paths')
        require(integration['screenshot_parent'] == '4e01dcda8f0e6a721c492ef5e2ae4d952e7230c5', 'Unexpected screenshot preservation parent')
        preserved_records.update(newer)
    return preserved_records


def inspect_tree(repo, pins, integration=None):
    preserved_records = integrated_preservation(pins, integration)
    inventory(repo, pins['source_inputs'])
    inventory(repo, preserved_records)
    for records in [pins['source_inputs'], preserved_records]:
        for name, record in records.items():
            path = local(repo, name)
            content = path.read_bytes()
            blob = hashlib.sha1(b'blob ' + str(len(content)).encode() + b'\0' + content).hexdigest()
            require(record['kind'] == 'blob' and record['object'] == blob, 'Git blob differs: ' + name)
            mode = '100755' if path.stat().st_mode & 0o111 else '100644'
            require(record['mode'] == mode, 'Git file mode differs: ' + name)
    tracked = subprocess.check_output(['git', '-C', str(repo), 'ls-files', '-z']).decode().split('\0')
    relevant = {n for n in tracked if n.startswith(('src/', 'tests/', 'cmake/')) or n in {'CMakeLists.txt', 'CMakePresets.json'}}
    require(relevant == set(pins['source_inputs']), 'Source/build/test path inventory differs')
    preserved = {n for n in tracked if n.startswith(tuple(pins['preserved_prefixes'])) or (n.startswith('src/core/') and n not in {'src/core/Engine.cpp', 'src/core/Engine.h'})}
    require(preserved == set(pins['preserved_tree_entries']), 'Preserved tree path inventory differs')


def completed_outcomes(ev):
    base = data(ev/'baseline-core-commands.json')
    require(base['source_head'] == BASE, 'Baseline source differs')
    command_logs(ev, base['commands'], {'compile': 0, 'run': 1})
    terminal((ev/'baseline-core-run.log').read_text(), 'MPE released-target baseline', 48, 24, True)
    core = data(ev/'core-focused-manifest.json')
    require(core['baseline_commit'] == BASE and core['production_commit'] == 'c0219bd', 'Core source attribution differs')
    require(len(core['results']) == 4, 'Incomplete focused core records')
    for row in core['results']:
        require(digest(ev/row['log']) == row['log_sha256'], 'Focused core log differs')
    terminal((ev/'core-second-run.log').read_text(), 'MPE released targets', 7488, 0, True)
    terminal((ev/'core-baseline-full.log').read_text(), 'MPE released targets', 7488, 1076, True)
    terminal((ev/'core-baseline-unchanged.log').read_text(), 'MPE released targets unchanged paths', 842, 0, True)
    focused_last = (ev/'core-focused-ctest.log').read_text()
    require(focused_last.count('Test Passed.') == 5 and 'Test Failed.' not in focused_last and focused_last.rstrip().endswith('End testing: Nov 09 19:43 AST'), 'Incomplete focused LastTest record')
    terminal((ev/'core-first-run.log').read_text(), 'MPE released targets', 7474, 40, True)
    require(core['first_attempt']['checks'] == 7474 and core['first_attempt']['failures'] == 40, 'Initial failed fixture was lost')
    native = ev/'native'
    for folder, fail, code in [('attempt-001', 64, 1), ('candidate-probe-c0219bd-attempt-001', 0, 0)]:
        p = native/folder
        command_logs(p, data(p/'commands-statuses.json'), {'compile': 0, 'link': 0, 'run': code})
        log = (p/'run.log').read_text()
        require(f'256 checks, {fail} failures (0 fixture, {fail} retention)' in log, 'Incomplete processor baseline result')
    final = native/'final-native-3003569-attempt-002'
    rows = data(final/'commands-statuses.json')
    command_logs(final, rows, {'configure': 0, 'build': 0, 'no-work': 0, 'direct': 0, 'ctest': 0})
    require(all(row['binary_sha256'] == NATIVE_SHA for row in rows), 'Native binary changed')
    terminal((final/'direct.log').read_text(), 'MPE released target host', 6048, 0)
    ctest((final/'ctest.log').read_text(), 1)
    require('6048 checks, 0 failures' in (final/'ctest-LastTest.log').read_text(), 'Registered native output missing')
    require('no work to do' in (final/'no-work.log').read_text(), 'Actual native no-work evidence missing')
    ctest((native/'final-native-neighbors-3003569-attempt-001/ctest.log').read_text(), 6)
    stale = data(native/'final-native-3003569-attempt-001/commands-statuses.json')
    require(any(r['stage'] == 'ctest' and r['status'] == 8 for r in stale), 'Stale native registration attempt lost')
    review = data(ev/'independent-review/core-execution.json')
    require(review['head'] == FOCUSED and review['binary_before_sha256'] == review['binary_after_sha256'], 'Independent core attribution differs')
    require(len(review['runs']) == 2 and all(r['exit_code'] == 0 and r['stderr'] == '' for r in review['runs']), 'Independent core execution incomplete')
    terminal((ev/'independent-review/core-direct.log').read_text(), 'MPE released targets', 7488, 0, True)
    ctest((ev/'independent-review/core-focused-ctest.log').read_text(), 1)
    comparison = data(ev/'compatibility/comparison.json')
    require(comparison['all_passed'] is True and comparison['baseline_header_archive_source_tree_equal'] is True and comparison['candidate_core_matches_c0219bd'] is True, 'Compatibility source comparison failed')
    require(len(comparison['profiles']) == 4 and {r['profile'] for r in comparison['profiles']} == PROFILES, 'Compatibility profile matrix incomplete')
    for row in comparison['profiles']:
        require(all(row[k] is True for k in ['source_matches_frozen', 'compiler_versions_match', 'compile_flags_match', 'baseline_partition_equal', 'candidate_partition_equal', 'partition_0_baseline_candidate_equal', 'partition_1_baseline_candidate_equal', 'passed']), 'Compatibility pair differs')
        require(row['compile_exits'] == [0, 0] and row['run_exits'] == [0, 0, 0, 0] and row['capture_bytes'] == 57339584, 'Compatibility capture incomplete')
        for side in ['baseline', 'candidate']:
            p = ev/'compatibility/CompatibilityCapture-v2'/f'{side}-{row["profile"]}'
            record = data(p/'record.json')
            require(record['source_sha256'] == comparison['source_sha256'], 'Compatibility fixture identity differs')
            require((p/'compile.log').read_text() == '', 'Unexpected final capture compiler output')
            for partition in [0, 1]:
                text = (p/f'partition-{partition}.log').read_text()
                require('6529556' in text and 'failures=0' in text and 'scenarios=270' in text, 'Incomplete compatibility run')



def fault_outcomes(ev):
    fault = ev/'fault-controls/run-3003569'
    result = data(fault/'summary.json')
    expected = {'omit-pressure-held-guard': 272, 'omit-bend-held-guard': 364,
                'omit-timbre-held-guard': 536, 'pedal-counts-as-held': 336,
                'lose-cc121-exception': 64, 'filter-manager-pressure-timbre': 52,
                'lose-shared-pressure-source': 12, 'lose-member-ribbon-route': 12,
                'lose-released-pressure-cache': 132, 'lose-released-bend-cache': 132,
                'lose-released-timbre-cache': 232}
    require(result['commit'] == FOCUSED and result['checks_expected'] == 7488, 'Fault source/fixture differs')
    require(result['gate_passed'] is True and result['inputs_unchanged'] is True and result['detected_count'] == 11, 'Fault gate incomplete')
    require(set(result['mutations']) == set(expected), 'Fault inventory differs')
    cases = dict(result['mutations'], **{'unmutated-control': result['control'], 'old-engine-control': result['baseline_control']})
    expected.update({'unmutated-control': 0, 'old-engine-control': 1076})
    commands = data(fault/'commands.json')
    require(len({r['label'] for r in commands}) == len(commands), 'Duplicate fault commands')
    by_label = {r['label']: r for r in commands}
    for command in commands:
        for stream in ['stdout', 'stderr']:
            require(digest(local(fault, command[stream])) == command[stream+'_sha256'], 'Fault command output differs')
    for name, row in cases.items():
        failures = expected[name]
        code = 1 if failures else 0
        require(row['checks'] == 7488 and row['failures'] == failures and row['exit_code'] == code and row['expected_exit'] == code, 'Fault did not finish normally')
        require(row['archive_engine_member_linked'] is False, 'Original Engine object contaminated mutation')
        require(digest(fault/name/'mutation.diff') == row['diff_sha256'], 'Mutation diff differs')
        require(digest(fault/name/'link.map') == row['link_map_sha256'], 'Mutation link map differs')
        for stage in ['engine-compile', 'test-compile', 'link', 'run']:
            command = by_label[name+'-'+stage]
            require(command['exit_code'] == (code if stage == 'run' else 0), 'Fault compile/link/run status differs')
        stdout = (fault/'logs'/f'{name}-run.stdout.log').read_text()
        stderr = (fault/'logs'/f'{name}-run.stderr.log').read_text()
        terminal(stdout, 'MPE released targets', 7488, failures, True)
        require(stderr == '', 'Fault wrote stderr')
    invariants = data(fault/'after-invariants.json')
    require(all(invariants[k] is True for k in ['production_and_test_hashes_unchanged', 'archive_sha256_unchanged', 'matching_build_engine_object_unchanged']), 'Fault source/archive invariants failed')
    require(invariants['test_fixture_sha256'] == invariants['original_test_fixture_sha256'] == data(ev/'core-focused-manifest.json')['test_sha256'], 'Focused fault fixture changed')
    parser = data(fault/'parser-self-test.json')
    require(parser['valid_cases'] == 2 and len(set(parser['rejected_invalid_cases'])) == 12, 'Fault parser validation incomplete')
    require(data(fault/'archive-member-verification.json')['matches_build_object'] is True, 'Archive Engine identity differs')



def profile_outcomes(ev, pins):
    root = ev/'profiles-final'
    inputs = data(root/'inputs.json')
    require(inputs['head'] == FINAL, 'Final profile source checkpoint differs')
    for name, sha in inputs['hashes'].items():
        require(pins['source_inputs'][name]['sha256'] == sha, 'Final profile input differs: ' + name)
    require(data(root/'source-unchanged.json') == {'unchanged': True, 'head': FINAL}, 'Profile source invariant failed')
    expected = {
        'cs80_mpe_released_target_tests': ('MPE released targets', 7488),
        'cs80_mpe_member_onset_tests': ('MPE member onset', 2501),
        'cs80_controller_reset_tests': ('Controller reset', 3513),
        'cs80_sixteen_voice_release_tests': ('Immediate sixteen-voice release', 1418),
        'cs80_tuning_midi_tests': ('Tuning / MIDI / voice modes', 22085),
        'cs80_poly_pressure_routing_tests': (None, 4504),
        'cs80_tests': (None, 11508)}
    profiles = PROFILES | {'sanitizer'}
    stages = set(expected) | {'build'}
    matrix = {(p, stage) for p in profiles for stage in stages} | {('sanitizer', 'configure')}
    commands = data(root/'commands.json')
    require(len(commands) == len(matrix) and {(r['profile'], r['stage']) for r in commands} == matrix, 'Incomplete/duplicate final profile matrix')
    for row in commands:
        require(row['exit_code'] == 0, 'Profile command failed')
        log = root/row['log']
        require(digest(log) == row['sha256'], 'Profile log hash differs')
        stage = row['stage']
        if stage in expected:
            prefix, count = expected[stage]
            if prefix:
                terminal(log.read_text(), prefix, count, 0)
            else:
                end = 'ALL 11508 CHECKS PASSED' if stage == 'cs80_tests' else '4504 checks, 0 failures'
                require(log.read_text().splitlines()[-1] == end, 'Incomplete profile test')
        if row['profile'] == 'sanitizer':
            require(row['env'].get('ASAN_OPTIONS') == 'detect_leaks=0' and row['env'].get('UBSAN_OPTIONS') == 'halt_on_error=1', 'Sanitizer environment differs')
            if stage == 'configure':
                require('-DCS80_BUILD_PLUGIN=OFF' in row['argv'] and '-DCS80_ENABLE_AVX2_FILTER_BATCH=OFF' in row['argv'], 'Sanitizer scope differs')
                flags = [x for x in row['argv'] if x.startswith('-DCMAKE_CXX_FLAGS_RELEASE=')]
                require(len(flags) == 1 and all(flag in flags[0] for flag in ['-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-D_GLIBCXX_ASSERTIONS']), 'Incomplete core sanitizer instrumentation')
    prepared = data(ev/'profiles-preparation/commands.json')
    require(len(prepared) == 8 and {(r['profile'],r['stage']) for r in prepared} == {(p,s) for p in PROFILES for s in ['configure','build-core']}, 'Incomplete compiler configuration evidence')
    for row in prepared:
        require(row['exit_code'] == 0 and digest(ev/'profiles-preparation'/row['log']) == row['sha256'], 'Compiler preparation failed')
        if row['stage'] == 'configure':
            require('-DCS80_ENABLE_AVX2_FILTER_BATCH=OFF' in row['argv'] and '-DCS80_BUILD_PLUGIN=OFF' in row['argv'], 'Profile configuration differs')
            flags = next(x for x in row['argv'] if x.startswith('-DCMAKE_CXX_FLAGS_RELEASE='))
            require('-O3 -DNDEBUG' in flags and ('-march=native' in flags) == row['profile'].endswith('-native'), 'Profile optimization identity differs')


def strict_outcomes(ev):
    root = ev/'strict-compatibility'
    command = data(root/'command.json')
    require(command['exit_code'] == 0 and command['argv'] == ['bash', 'scripts/check-compat-gates.sh', '--strict-v2'], 'Strict compatibility command failed')
    require(digest(root/'run.log') == command['log_sha256'] and (root/'driver.status').read_text().strip() == '0', 'Strict compatibility output/status differs')
    require((root/'run.log').read_text().splitlines() == [
        'legacy default: byte-identical to d56911a', 'legacy tails: byte-identical to d56911a',
        'v1 circuit numerics: byte-identical to 22d6a32', 'v2 circuit-engine audio: byte-identical to 00ceccf'], 'Strict compatibility result differs')
    artifacts = {Path(k).name: v for k,v in data(root/'artifacts.json').items()}
    for left, right in [('audio-current.bin','audio-v2.bin'), ('circuit-current.bin','circuit-v1.bin'), ('current-default.f32','legacy-default.f32'), ('current-tails.f32','legacy-tails.f32')]:
        require(artifacts[left] == artifacts[right] and artifacts[left]['size'] > 0, 'Strict reference/candidate hashes differ')


def superseded_outcomes(ev):
    root = ev/'superseded-oracles'
    rows = data(root/'baseline-final-commands.json')
    require([(r['name'], r['stage'], r['exit_code']) for r in rows] == [('PolyPressureRoutingTests','compile',0), ('PolyPressureRoutingTests','run',0), ('TuningMidiTests','compile',1)], 'Original baseline-oracle attempt differs')
    for row in rows:
        require(digest(root/row['log']) == row['log_sha256'], 'Baseline oracle log differs')
    require((root/'PolyPressureRoutingTests-baseline-final-run.log').read_text().splitlines()[-1] == '4504 checks, 0 failures', 'Old poly-pressure oracle did not pass')
    corrected = root/'tuning-corrected-command'
    command_logs(corrected, data(corrected/'commands.json'), {'compile':0, 'run':1})
    terminal((corrected/'run.log').read_text(), 'Tuning / MIDI / voice modes', 22085, 6)
    original_full = ev/'full-suite'
    command_logs(original_full, data(original_full/'commands.json'), {'build':0,'ctest':8})
    log = (original_full/'ctest.log').read_text()
    require('99% tests passed, 2 tests failed out of 145' in log, 'Original full-suite result differs')
    require('cs80_poly_pressure_routing_tests (Failed)' in log and 'cs80_tuning_midi_tests (Failed)' in log, 'Original failures misattributed')



def full_suite_outcomes(ev, pins):
    root = ev/'full-suite-final'
    command_logs(root, data(root/'commands.json'), {'build':0, 'ctest':0})
    require((root/'driver.status').read_text().strip() == '0', 'Final full-suite driver failed')
    log = (root/'ctest.log').read_text()
    ctest(log, 145)
    require('Total Test time (real) = 462.29 sec' in log, 'Final full-suite invocation differs')
    bracket = data(root/'source-bracket-and-builds.json')
    require(bracket['final_source_test_freeze'] == FINAL and bracket['post_run_head'] == FINAL, 'Full-suite checkpoint differs')
    require(bracket['source_inventory_matches_before'] is True, 'Source bracket changed')
    before = data(ev/'profiles-final/inputs.json')
    require(len(bracket['after_source_sha256']) == 411 and bracket['after_source_sha256'] == before['hashes'], 'Incomplete or differing full-suite source bracket')
    require(digest(ev/'profiles-final/inputs.json') == bracket['before_snapshot']['sha256'], 'Before snapshot differs')
    require(bracket['before_snapshot']['before_profile_first_command_utc'] < bracket['full_suite_first_command_utc'] < bracket['post_run_recorded_utc'], 'Snapshots do not bracket final execution')
    diff = bracket['tracked_source_diff']
    require(diff['exit_code'] == 0 and diff['stdout'] == '' and diff['stderr'] == '', 'Tracked final inputs differ')
    for file, key in [('commands.json','run_command_record_sha256'), ('ctest.log','ctest_console_sha256'), ('ctest-LastTest.log','ctest_last_test_sha256')]:
        require(digest(root/file) == bracket[key], 'Full-suite record hash differs')
    require(len(bracket['build_artifacts']) == 131, 'Build artifact scope differs')
    for name, sha in bracket['after_source_sha256'].items():
        require(pins['source_inputs'][name]['sha256'] == sha, 'Post-run source differs from qualification')


def verify(root, repo, external=False):
    manifest = data(root/'manifest.json')
    require(manifest['schema'] == 1, 'Unsupported manifest')
    inventory(root, manifest['files'])
    actual = {str(p.relative_to(root)) for p in root.rglob('*') if p.is_file() and p.name != 'manifest.json' and '__pycache__' not in p.parts}
    require(actual == set(manifest['files']), 'Bundle file inventory differs')
    pins = data(root/'evidence/source-pins.json')
    require(pins['baseline'] == BASE and pins['production'] == PRODUCTION and pins['focused_source_test_freeze'] == FOCUSED, 'Source attribution differs')
    require(pins['final_neighbor_test_freeze'] == FINAL and pins['production_source_equal_across_freezes'] is True, 'Final source attribution differs')
    require(pins['focused_to_final_changes'] == ['tests/PolyPressureRoutingTests.cpp', 'tests/TuningMidiTests.cpp'], 'Neighbor correction scope changed')
    integration = data(root/'evidence/integration-preservation.json') if (root/'evidence/integration-preservation.json').exists() else None
    require(pins['qualification_status'] == 'completed bounded qualification', 'This final bundle requires completed qualification')
    inspect_tree(repo, pins, integration)
    completed_outcomes(root/'evidence')
    if (root/'evidence/fault-controls/run-3003569/summary.json').exists():
        fault_outcomes(root/'evidence')
    if (root/'evidence/profiles-final/source-unchanged.json').exists():
        profile_outcomes(root/'evidence', pins)
    if (root/'evidence/strict-compatibility/driver.status').exists():
        strict_outcomes(root/'evidence')
    if (root/'evidence/full-suite/commands.json').exists():
        superseded_outcomes(root/'evidence')
    if pins['qualification_status'] == 'completed bounded qualification':
        require((root/'evidence/full-suite-final/source-bracket-and-builds.json').is_file(), 'Missing completed full-suite evidence')
        full_suite_outcomes(root/'evidence', pins)
        require((root/'evidence/profiles-final-driver.status').read_text().strip() == '0', 'Final profile driver failed')
        require((root/'evidence/fault-controls/driver-3003569.status').read_text().strip() == '0', 'Fault driver failed')
    if external:
        records = data(root/'external-inputs.json')['files']
        require(bool(records), 'External index is empty')
        for name, record in records.items():
            path = Path(name)
            require(path.is_absolute() and path.is_file(), 'Missing historical external file: ' + name)
            require(path.stat().st_size == record['bytes'] and digest(path) == record['sha256'], 'Changed external file: ' + name)
    return {'bundled_files': len(manifest['files']), 'external_checked': external, 'qualification_status': pins['qualification_status'], 'runtime_tests_repeated': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--external', action='store_true')
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--repo', type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        result = verify(root, (args.repo or root.parents[2]).resolve(), args.external)
    except (ValueError, KeyError, OSError, TypeError, subprocess.CalledProcessError) as error:
        print('FAIL: ' + str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == '__main__':
    sys.exit(main())
