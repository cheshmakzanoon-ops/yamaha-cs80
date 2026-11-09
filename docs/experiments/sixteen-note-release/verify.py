#!/usr/bin/env python3
"""Verify frozen evidence bytes and recorded outcomes, without rerunning tests.

All checks use explicit exceptions and remain enabled with python -O.
An integrity manifest is not a signature or proof of independent execution.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys

HEAD = '1a23a085657a07d9f066f7cc38aaf2572a6fe714'
BASE = 'e27c34f39320057e09a34005ad2470d1231b4f87'
NATIVE_SHA = '13c7b913f7fdb55bab4967bb355e71d2d7d54f5a18906709836d354f6c55a3e6'
PROFILES = {'gcc-generic', 'gcc-native', 'clang-generic', 'clang-native', 'sanitizer'}
STAGES = {'configure', 'build', 'cs80_sixteen_voice_release_tests', 'cs80_tests', 'cs80_tuning_midi_tests'}
MUTANTS = {'broadcast-two-engines', 'inverse-global-order', 'lost-waiter-priority',
           'ui-host-protection-omitted', 'ui-held-source-filter', 'qualify-prior-deferred-candidates',
           'accept-unqualified-candidate', 'accept-mismatched-sources', 'omit-exhaustion-guard',
           'ignore-pending-scanner', 'reuse-waiter-order-on-promotion'}


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return path.read_text(encoding='utf-8')


def data(path):
    return json.loads(read(path))


def local(root, name):
    require(isinstance(name, str), 'Non-string inventory path')
    path = (root / name).resolve()
    require(path.is_relative_to(root.resolve()) and path != root.resolve(), 'Unsafe path: ' + name)
    return path


def inventory(root, records):
    require(isinstance(records, dict) and bool(records), 'Empty inventory')
    for name, record in records.items():
        path = local(root, name)
        require(path.is_file(), 'Missing file: ' + name)
        require(path.stat().st_size == record['bytes'] and digest(path) == record['sha256'], 'Changed file: ' + name)


def terminal(text, prefix, checks, failures):
    expected = f'{prefix}: {checks} checks, {failures} failures'
    lines = text.splitlines()
    require(bool(lines) and lines[-1] == expected, 'Missing/incomplete terminal summary: ' + prefix)
    require(sum(line.startswith(prefix + ':') for line in lines) == 1, 'Duplicate summary: ' + prefix)
    return lines


def fault_result(stdout, stderr, code, failures):
    lines = terminal(stdout, 'Immediate sixteen-voice release', 1418, failures)
    require(stderr == '', 'Fault run wrote stderr')
    require(code == (1 if failures else 0), 'Fault run did not exit normally with expected status')
    require(len(lines) == failures + 1 and all(x.startswith('FAIL: ') for x in lines[:-1]), 'Fault diagnostic count/content differs')


def ctest(text, count):
    verdict = f'100% tests passed, 0 tests failed out of {count}'
    require(text.splitlines().count(verdict) == 1, 'Missing/duplicate CTest verdict')
    completed = re.findall(r'^\s*\d+/\d+ Test\s+#(\d+): (\S+)\s+\.{3,}\s+Passed\s+', text, re.M)
    require(len(completed) == count and len({i for i, _ in completed}) == count, 'Incomplete/duplicate CTest cases')
    return completed


def self_test():
    valid = 'Immediate sixteen-voice release: 1418 checks, 0 failures\n'
    fault_result(valid, '', 0, 0)
    failed = 'FAIL: controlled\nImmediate sixteen-voice release: 1418 checks, 1 failures\n'
    fault_result(failed, '', 1, 1)
    invalid = [(valid + valid, '', 0, 0), (valid + 'extra\n', '', 0, 0),
               (valid.replace('1418', '1417'), '', 0, 0), (failed, '', -11, 1),
               (failed, '', 0, 1), (failed, 'error', 1, 1),
               (failed.split('\n', 1)[1], '', 1, 1), (valid + '\n', '', 0, 0)]
    for args in invalid:
        try:
            fault_result(*args)
        except ValueError:
            continue
        raise ValueError('Self-test accepted an invalid/truncated result')
    print('Verifier parser self-test: 2 valid cases accepted, 8 invalid cases rejected. No runtime tests executed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--external', action='store_true', help='Also hash retained original workspace inputs')
    parser.add_argument('--self-test', action='store_true', help='Exercise the result parser with synthetic cases only')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    ev = here / 'evidence'
    manifest = data(here / 'manifest.json')
    require(manifest['schema'] == 1 and manifest['qualification_head'] == HEAD and manifest['published_base'] == BASE, 'Wrong frozen source identity')
    inventory(here, manifest['archive_files'])
    inventory(repo, manifest['repository_files'])
    inventory(repo, manifest['reviewed_documents'])
    actual = {str(p.relative_to(here)) for p in here.rglob('*') if p.is_file() and p.name != 'manifest.json' and '__pycache__' not in p.parts}
    require(actual == set(manifest['archive_files']), 'Archive has missing/unindexed files')
    for name in actual:
        path = local(here, name)
        require(path.suffix not in {'.o', '.a', '.so', '.exe'} and path.read_bytes()[:4] != b'\x7fELF', 'Executable/object/archive bundled: ' + name)
    require(set(manifest['changed_production_paths_against_base']) == {
        'src/core/Engine.cpp', 'src/core/Engine.h', 'src/core/ReleaseOrder.h', 'src/core/VoiceModes.h',
        'src/plugin/PluginProcessor.cpp', 'src/plugin/PluginProcessor.h'}, 'Production change scope differs')
    for name in manifest['unchanged_repository_paths_against_base']:
        record = manifest['repository_files'][name]
        require(record['sha256'] == record['base_sha256'], 'Not unchanged against base: ' + name)
    sp = data(ev / manifest['screenshot_preservation']['inventory'].removeprefix('evidence/'))
    screenshot_parent = 'b6ab4d37eb8d6c8e8d8c6361d3c9dd4c3db0fee5'
    require(sp['screenshot_parent'] == manifest['screenshot_preservation']['parent'] == screenshot_parent, 'Screenshot parent differs')
    require(sp['screenshot_parent_parent'] == BASE and sp['qualification_source_test_head'] == HEAD, 'Screenshot/source provenance differs')
    require(sp['source_tests_cmake_unchanged_after_integration'] is True, 'Screenshot integration changed qualified sources')
    require(sp['screenshot_tree'] == sp['integration_screenshot_tree'] == manifest['screenshot_preservation']['tree'], 'Screenshot tree not preserved')
    require(len(sp['files']) == manifest['screenshot_preservation']['count'] == 70, 'Screenshot preservation inventory incomplete')
    changed_screenshots = set(sp['inherited_changed_paths_against_published_base'])
    require(len(changed_screenshots) == 20 and changed_screenshots <= set(sp['files']), 'Inherited screenshot delta differs')
    require(not changed_screenshots.intersection(manifest['unchanged_repository_paths_against_base']), 'Changed screenshots misclassified as base-unchanged')
    actual_screenshots = {str(p.relative_to(repo)) for p in (repo / 'docs/screenshots').rglob('*') if p.is_file()}
    require(actual_screenshots == set(sp['files']), 'Screenshot paths differ from inherited inventory')
    for name, r in sp['files'].items():
        path = local(repo, name)
        require(not (repo / name).is_symlink() and r['git_mode'] == '100644' and not path.stat().st_mode & 0o111, 'Screenshot file mode differs: ' + name)
        require(path.stat().st_size == r['bytes'] and digest(path) == r['sha256'], 'Screenshot differs from inherited parent: ' + name)
        frozen = manifest['repository_files'][name]
        require(frozen['preservation_parent'] == screenshot_parent and frozen['git_blob_at_preservation_parent'] == r['git_blob'] and frozen['git_mode'] == r['git_mode'], 'Screenshot pin differs: ' + name)
    require(data(repo / sp['provenance_file']) == sp['provenance'] and sp['provenance']['commit'] == BASE, 'Screenshots wrongly attributed to release repair')

    integration = data(ev / 'independent-review/integration-review.json')
    require(integration['reviewed_merge_commit'] == sp['integration_commit'] and integration['qualified_source_commit'] == HEAD, 'Independent integration identity differs')
    require(integration['preserved_screenshot_commit'] == screenshot_parent and integration['screenshot_base_commit'] == BASE, 'Independent screenshot provenance differs')
    require(integration['source_tests_cmake_paths_modes_blobs_equal'] is True and integration['screenshot_paths_modes_blobs_equal'] is True, 'Independent integration preservation failed')
    require(integration['screenshot_tree_entry_count'] == 70 and integration['screenshot_png_count'] == 68 and integration['inherited_screenshot_change_count'] == 20, 'Independent screenshot scope differs')
    require(integration['runtime_repeated'] is False and integration['remote_ci_queried'] is False, 'Integration review scope was broadened')
    require(integration['screenshot_provenance'] == sp['provenance'], 'Independent screenshot source attribution differs')
    require(set(integration['screenshot_tree']) == set(sp['files']), 'Independent screenshot path inventory differs')
    for name, r in integration['screenshot_tree'].items():
        require(r['mode'] == sp['files'][name]['git_mode'] and r['object'] == sp['files'][name]['git_blob'] and r['kind'] == 'blob', 'Independent screenshot mode/blob differs: ' + name)
    for name, sha in integration['reviewed_document_sha256_unchanged'].items():
        require(manifest['reviewed_documents'][name]['sha256'] == sha, 'Integration changed reviewed document: ' + name)

    pins = data(ev / 'source-pins.json')
    require(pins['qualification_head'] == HEAD and pins['source_clean_against_pin'] is True, 'Source capture mismatch')
    require(pins['baseline_header_src_tree'] == pins['published_base_src_tree'] == 'a1ee27dbda98a04703ab01fdf8c6fc39e9ea5536', 'Old headers do not match published source')

    # Real processor baseline and unchanged candidate probe.
    for root, commands_name, failures in [(ev, 'baseline-commands.json', 4),
        (ev / 'native-build', 'candidate-77dfd74-probe-commands.json', 0)]:
        commands = data(root / commands_name)
        require(len(commands) == 3 and {x['stage'] for x in commands} == {'compile', 'link', 'run'}, 'Probe command inventory differs')
        for r in commands:
            require(r['status'] == (1 if r['stage'] == 'run' and failures else 0), 'Unexpected probe status')
            log = root / Path(r['log']).name
            require(digest(log) == r['sha256'], 'Probe log hash differs')
            if r['stage'] == 'run':
                lines = terminal(read(log), 'Sixteen release baseline', 4, failures)
                require(len(lines) == 5 and sum(x.endswith('FAIL') for x in lines[:-1]) == failures, 'Probe case count differs')
                require(sum(x.endswith('PASS') for x in lines[:-1]) == 4 - failures, 'Probe success count differs')

    # Compiler matrix includes entire linked core, not only a sanitized driver.
    require(read(ev / 'profiles-driver.status').strip() == '0', 'Profile driver failed')
    commands = data(ev / 'profiles/commands.json')
    require(len(commands) == 25 and {(r['profile'], r['stage']) for r in commands} == {(p, s) for p in PROFILES for s in STAGES}, 'Incomplete/duplicate profile matrix')
    for r in commands:
        require(r['exit_code'] == 0, 'Profile command failed')
        log = ev / 'profiles' / Path(r['log']).name
        require(digest(log) == r['sha256'], 'Profile log hash differs')
        if r['stage'] == 'configure':
            require('-DCS80_BUILD_PLUGIN=OFF' in r['argv'] and '-DCS80_ENABLE_AVX2_FILTER_BATCH=OFF' in r['argv'], 'Profile configuration changed')
            if r['profile'] == 'sanitizer':
                flags = next(x for x in r['argv'] if x.startswith('-DCMAKE_CXX_FLAGS_RELEASE='))
                require('-fsanitize=address,undefined,float-cast-overflow' in flags and '-D_GLIBCXX_ASSERTIONS' in flags, 'Incomplete sanitizer instrumentation')
        if r['profile'] == 'sanitizer':
            require(r['env'].get('ASAN_OPTIONS') == 'detect_leaks=0' and r['env'].get('UBSAN_OPTIONS') == 'halt_on_error=1', 'Sanitizer scope differs')
        if r['stage'] == 'cs80_sixteen_voice_release_tests': terminal(read(log), 'Immediate sixteen-voice release', 1418, 0)
        if r['stage'] == 'cs80_tuning_midi_tests': terminal(read(log), 'Tuning / MIDI / voice modes', 22085, 0)
        if r['stage'] == 'cs80_tests': require(read(log).splitlines()[-1] == 'ALL 11508 CHECKS PASSED', 'Incomplete existing core coverage')
    ctest(read(ev / 'core/focused-neighbors-02.log'), 11)
    require('Immediate sixteen-voice release: 1418 checks, 0 failures' in read(ev / 'core/focused-04-verbose.log'), 'Final core fixture missing')
    ctest(read(ev / 'core/focused-04-verbose.log'), 1)
    status = read(ev / 'core/full-core-statuses.txt').splitlines()
    require(status.count('build_exit=0') == 1 and status.count('ctest_exit=0') == 1, 'Full core suite has no successful terminal statuses')
    completed = ctest(read(ev / 'core/full-core-ctest.log'), 144)
    require({int(i) for i, _ in completed} == set(range(1, 145)), 'Full core case identities incomplete')
    require(HEAD in read(ev / 'core/full-core-commands.log'), 'Full core source pin missing')

    # Historical native attempts stay distinct from final registered/direct runs.
    native = ev / 'native-build'
    n = data(native / 'final-1a23a08-status.json')
    require(n['source_commit'] == HEAD and n['source_clean_at_start_and_end'] is True, 'Native source scope differs')
    require(n['binary_sha256'] == NATIVE_SHA and n['binary_identical_to_9e3f9a0'] is True, 'Native binary identity differs')
    require(set(n['stage_exit_statuses']) == {'configure', 'build', 'ctest', 'direct', 'no-work', 'dry-run', 'no-work-explained'}, 'Incomplete native stages')
    for stage, code in n['stage_exit_statuses'].items():
        require(code == 0 and read(native / f'final-1a23a08-{stage}.exit').strip() == '0', 'Native command failed')
    for stage in ['ctest', 'direct']:
        r = n['test_results'][stage]
        require((r['checks'], r['failures'], r['exit']) == (768, 0, 0), 'Native test result differs')
        require(digest(native / Path(r['log']).name) == r['log_sha256'], 'Native log hash differs')
    terminal(read(native / 'final-1a23a08-direct.log'), 'Sixteen voice release host', 768, 0)
    ctest(read(native / 'final-1a23a08-ctest.log'), 1)
    for stage in ['build', 'no-work', 'no-work-explained']:
        require('no work to do' in read(native / f'final-1a23a08-{stage}.log'), 'Actual native no-work check missing')
    neighbor = data(native / 'neighbors-9e3f9a0-status.json')
    require(neighbor['binary_hash_verified'] == NATIVE_SHA and neighbor['ctest_exit'] == 0 and neighbor['ctests_run'] == 5 and neighbor['ctests_failed'] == 0, 'Native neighbor scope differs')
    ctest(read(native / 'test-neighbors-9e3f9a0.log'), 5)
    old_failure = data(native / 'fixture-920cab9-status.json')
    require(old_failure['build_exit'] == 1 and old_failure['test_run'] is False and old_failure['test_checks'] is None, 'Native failed attempt reclassified')
    require(read(native / 'build-fixture-920cab9.exit').strip() == '1', 'Native fixture build failure missing')
    require('1326 checks, 1 failures' in read(ev / 'core/focused-01.log'), 'Core failed fixture attempt missing')

    # Old and candidate use their own headers and libraries; within-profile equality.
    require(read(ev / 'compatibility-driver.status').strip() == '0', 'Compatibility driver incomplete/failed')
    for profile in ['generic', 'native']:
        a = (ev / f'compatibility/baseline-{profile}.bin').read_bytes()
        b = (ev / f'compatibility/candidate-{profile}.bin').read_bytes()
        require(a == b and len(a) == 27552 * 4, 'Compatibility raw witness differs')
        require(all(math.isfinite(v[0]) for v in struct.iter_unpack('<f', a)), 'Nonfinite compatibility sample')
        for version in ['baseline', 'candidate']:
            require(read(ev / f'compatibility/{version}-{profile}.log').strip() == 'Unchanged MPE paths: 6 scenarios, 27552 finite serialized values, 0 failures', 'Incomplete raw capture')
    for verdict in ['legacy default: byte-identical to d56911a', 'legacy tails: byte-identical to d56911a',
                    'v1 circuit numerics: byte-identical to 22d6a32', 'v2 circuit-engine audio: byte-identical to 00ceccf']:
        require(read(ev / 'compatibility-driver.log').splitlines().count(verdict) == 1, 'Missing/duplicate strict compatibility verdict')

    strict = data(ev / 'compatibility/strict-compatibility.json')
    ext = data(here / 'external-inputs.json')
    require(strict['driver_exit'] == 0 and strict['script_sha256'] == manifest['repository_files'][strict['script']]['sha256'], 'Strict compatibility command/source differs')
    require(set(strict['capture_pairs']) == {'legacy-default', 'legacy-tails', 'v1-circuit', 'v2-audio'}, 'Strict capture matrix incomplete')
    for pair in strict['capture_pairs'].values():
        require(pair['baseline'] == pair['candidate'] and pair['baseline']['bytes'] > 0 and pair['bundled'] is False, 'Strict captures differ or are misclassified')
        require(ext[pair['baseline_path']] == pair['baseline'] and ext[pair['candidate_path']] == pair['candidate'], 'Strict external capture identity differs')
    library = data(ev / 'compatibility/library-provenance.json')
    prior = data(repo / 'docs/experiments/mpe-member-onset/external-inputs.json')
    for r in library['baseline_library_identities'].values():
        require(r['matches_prior_record'] is True and ext[r['path']] == r['record'], 'Old library provenance changed')
        require(all(prior[r['path']][k] == r['record'][k] for k in ['bytes', 'sha256']), 'Old library no longer matches prior record')

    review = data(ev / 'independent-review/execution.json')
    require(review['reviewed_commit'] == HEAD and review['source_unchanged'] is True and review['initial_hashes'] == review['final_hashes'], 'Independent execution source scope differs')
    require(len(review['commands']) == 4 and {r['name'] for r in review['commands']} == {'configure', 'build', 'direct', 'ctest'} and all(r['returncode'] == 0 for r in review['commands']), 'Independent core execution incomplete')
    for name, sha in review['final_hashes'].items(): require(manifest['repository_files'][name]['sha256'] == sha, 'Independent source hash differs: ' + name)
    terminal(read(ev / 'independent-review/direct.log'), 'Immediate sixteen-voice release', 1418, 0)
    ctest(read(ev / 'independent-review/ctest.log'), 1)
    docs_review = data(ev / 'independent-review/review.json')
    require(docs_review['reviewed_commit'] == HEAD and docs_review['no_source_edits'] is True, 'Independent source review scope differs')
    for name, sha in docs_review['final_document_sha256'].items(): require(manifest['reviewed_documents'][name]['sha256'] == sha, 'Reviewed document changed: ' + name)

    nr = data(ev / 'independent-review/native-review-status.json')
    require(nr['source_commit'] == HEAD and nr['source_head_at_end'] == HEAD, 'Independent native source pin differs')
    require(all(nr[k] is True for k in ['source_clean_at_start', 'source_clean_at_end', 'source_inventory_matches', 'no_rebuild_performed']), 'Independent native provenance incomplete')
    require(nr['binary_sha256_before'] == nr['binary_sha256_after'] == NATIVE_SHA, 'Independent native binary changed')
    require(len(nr['commands']) == 2 and {r['name'] for r in nr['commands']} == {'native-direct', 'native-ctest'}, 'Independent native command scope differs')
    for r in nr['commands']:
        require(r['exit'] == 0 and read(ev / 'independent-review' / (r['name'] + '.exit')).strip() == '0', 'Independent native execution failed')
        require(digest(ev / 'independent-review' / (r['name'] + '.log')) == r['log_sha256'], 'Independent native log differs')
    terminal(read(ev / 'independent-review/native-direct.log'), 'Sixteen voice release host', 768, 0)
    ctest(read(ev / 'independent-review/native-ctest.log'), 1)
    for name, sha in nr['no_work_log_sha256'].items(): require(digest(ev / 'native-build' / name) == sha, 'Reviewed no-work record differs')

    verify_faults(ev / 'fault-controls', manifest)
    fr = data(ev / 'independent-review/fault-review.json')
    require(fr['source_test_unchanged_after'] is True and fr['frozen_source_matches_production_inputs'] is True, 'Independent fault inputs changed')
    require(fr['historical_driver_sha256'] == digest(ev / 'fault-controls/run-1/reproducer.py') and fr['current_driver_sha256'] == digest(ev / 'fault-controls/check_fault_controls.py'), 'Independent fault driver identity differs')
    require(fr['summary_sha256'] == digest(ev / 'fault-controls/run-1/summary.json'), 'Independent fault summary differs')
    require(len(fr['cases']) == 13 and all(r['hashes_commands_private_headers_private_engine_link_verified'] is True for r in fr['cases'].values()), 'Independent fault case validation incomplete')
    require(len(fr['independent_runs']) == 2 and {r['name'] for r in fr['independent_runs']} == {'broadcast-two-engines', 'accept-unqualified-candidate'}, 'Independent fault rerun scope differs')
    faults = data(ev / 'fault-controls/run-1/summary.json')
    for r in fr['independent_runs']:
        name = 'fault-' + r['name']
        require(read(ev / 'independent-review' / (name + '.exit')).strip() == '1', 'Independent fault exit differs')
        require(r['binary_sha256'] == faults['mutations'][r['name']]['binary_sha256'], 'Independent fault binary differs')
        for stream in ['stdout', 'stderr']:
            require(digest(ev / 'independent-review' / (name + '.' + stream + '.log')) == r[stream + '_sha256'], 'Independent fault output differs')
        fault_result(read(ev / 'independent-review' / (name + '.stdout.log')), read(ev / 'independent-review' / (name + '.stderr.log')), r['exit_code'], r['failures'])
    require(fr['parser_optimized_mode']['exit'] == 0 and len(fr['parser_optimized_mode']['result']['rejected_invalid_cases']) == 12, 'Optimized fault parser review incomplete')

    if args.external:
        records = data(here / 'external-inputs.json')
        require(bool(records), 'Empty external inventory')
        for name, record in records.items():
            path = Path(name)
            require(path.is_absolute() and path.is_file(), 'External input missing: ' + name)
            require(path.stat().st_size == record['bytes'] and digest(path) == record['sha256'], 'External input changed: ' + name)
    print('Verified archived hashes, frozen source inputs and complete recorded qualification coverage. No runtime tests executed.')


def verify_faults(root, manifest):
    require(read(root / 'driver.status').strip() == '0', 'Fault driver incomplete/failed')
    run = root / 'run-1'
    summary = data(run / 'summary.json')
    require(summary['commit'] == HEAD and summary['checks_expected'] == 1418 and summary['inputs_unchanged'] is True, 'Fault input scope differs')
    require(set(summary['mutations']) == MUTANTS and summary['detected_count'] == 11 and summary['gate_passed'] is True, 'Fault matrix incomplete')
    commands = data(run / 'commands.json')
    require(len({r['label'] for r in commands}) == len(commands), 'Duplicate fault command')
    lookup = {r['label']: r for r in commands}
    for r in commands:
        for stream in ['stdout', 'stderr']:
            require(digest(local(run, r[stream])) == r[stream + '_sha256'], 'Fault log hash differs')
        if not r['label'].endswith('-run'): require(r['exit_code'] == 0, 'Fault compile/inspection failed')
    cases = [('unmutated-control', summary['control'], 'passed', 0)]
    cases += [(name, r, 'detected', 1) for name, r in summary['mutations'].items()]
    cases += [('omit-router-ui-guard-only', summary['redundancy_control'], 'undetected-redundant', 0)]
    for name, r, status, code in cases:
        require(r['status'] == status and r['checks'] == 1418 and r['exit_code'] == r['expected_exit'] == code, 'Invalid fault/redundancy result: ' + name)
        require((r['failures'] > 0) if code else r['failures'] == 0, 'Fault failure count differs: ' + name)
        for stage in ['engine-compile', 'test-compile', 'link', 'run']:
            require(name + '-' + stage in lookup, 'Missing fault stage: ' + name)
        command = lookup[name + '-run']
        fault_result(read(local(run, command['stdout'])), read(local(run, command['stderr'])), command['exit_code'], r['failures'])
        require(digest(run / name / 'mutation.diff') == r['diff_sha256'], 'Mutation diff differs: ' + name)
        require(digest(run / name / 'link.map') == r['link_map_sha256'] and r['archive_engine_member_linked'] is False, 'Mutation link provenance differs')
        require('libcs80_core.a(Engine.cpp.o)' not in read(run / name / 'link.map'), 'Unmodified archive Engine linked into mutation')
    inputs = data(run / 'input-hashes.json')
    require(inputs['commit'] == HEAD, 'Fault original commit differs')
    for name, sha in inputs['source_test_sha256'].items(): require(manifest['repository_files'][name]['sha256'] == sha, 'Fault original input differs: ' + name)
    invariant = data(run / 'after-invariants.json')
    require(all(invariant[k] is True for k in ['production_and_test_hashes_unchanged', 'archive_sha256_unchanged', 'matching_build_engine_object_unchanged']), 'Fault experiment changed inputs')
    require(invariant['test_fixture_sha256'] == invariant['original_test_fixture_sha256'] == manifest['repository_files']['tests/SixteenVoiceReleaseTests.cpp']['sha256'], 'Fault fixture changed')
    audit = data(run / 'audit.json')
    require(read(root / 'audit.status').strip() == '0' and audit['passed'] is True and audit['check_count'] == 347, 'Fault post-run audit incomplete')
    require(len(audit['checks']) == 347 and all(r['passed'] is True for r in audit['checks']), 'Fault audit checks incomplete')
    parser = data(run / 'parser-self-test.json')
    require(parser['valid_cases'] == 2 and len(parser['rejected_invalid_cases']) == 12, 'Fault driver parser checks incomplete')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, IndexError, StopIteration) as exc:
        print('ERROR: ' + str(exc), file=sys.stderr)
        sys.exit(1)
