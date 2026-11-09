#!/usr/bin/env python3
"""Validate archived evidence without running tests; explicit checks survive -O."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import math
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def local(root, name):
    require(isinstance(name, str), 'Non-string manifest path')
    path = (root / name).resolve()
    require(path.is_relative_to(root.resolve()) and path != root.resolve(), 'Unsafe path: ' + name)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--external', action='store_true', help='Also hash original workspace external inputs')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    ev = here / 'evidence'
    read = lambda p: p.read_text(encoding='utf-8')
    data = lambda p: json.loads(read(p))
    manifest = data(here / 'manifest.json')
    require(manifest['schema'] == 1, 'Unknown manifest schema')
    for group, root in [('archive_files', here), ('repository_files', repo)]:
        require(bool(manifest[group]), 'Empty file inventory: ' + group)
        for name, record in manifest[group].items():
            path = local(root, name)
            require(path.is_file(), 'Missing file: ' + str(path))
            require(path.stat().st_size == record['bytes'] and digest(path) == record['sha256'], 'File changed: ' + str(path))
    actual = {str(p.relative_to(here)) for p in here.rglob('*') if p.is_file() and p.name != 'manifest.json' and '__pycache__' not in p.parts}
    source_here = {str((repo / name).relative_to(here)) for name in manifest['repository_files'] if (repo / name).is_relative_to(here)}
    require(actual == set(manifest['archive_files']) | source_here, 'Archive has missing or unindexed files')
    for name in manifest['unchanged_source_paths_against_base']:
        require(name in manifest['repository_files'], 'Unpinned unchanged source: ' + name)
    profiles = data(ev / 'profiles/commands.json')
    expected_profiles = {'gcc-generic', 'gcc-native', 'clang-generic', 'clang-native', 'sanitizer'}
    require(len(profiles) == 20, 'Incomplete profile command matrix')
    seen = set()
    for record in profiles:
        key = (record['profile'], record['stage'])
        require(key not in seen, 'Duplicate profile command')
        seen.add(key)
        require(record['exit_code'] == 0, 'Profile command failed')
        log = ev / 'profiles' / Path(record['log']).name
        require(digest(log) == record['sha256'], 'Profile log hash mismatch')
        if record['stage'] == 'cs80_mpe_member_onset_tests':
            require(read(log).strip().splitlines()[-1] == 'MPE member onset: 2501 checks, 0 failures', 'Onset checks incomplete')
        if record['stage'] == 'cs80_tests':
            require(read(log).strip().splitlines()[-1] == 'ALL 11508 CHECKS PASSED', 'Core checks incomplete')
    require(seen == {(p,s) for p in expected_profiles for s in ['configure','build','cs80_mpe_member_onset_tests','cs80_tests']}, 'Wrong profile coverage')
    expected = dict(zip(['candidate','baseline','omit-bend','omit-pressure','omit-timbre','omit-reset','omit-mode-edge','omit-role-change','broaden-mode-scope','cache-curved-pressure'], [0,1004,410,198,606,36,8,16,9,16]))
    summary = data(ev / 'negative-controls/summary.json')
    require(summary == {name: {'checks':2501,'failures':n} for name,n in expected.items()}, 'Negative summary differs')
    commands = data(ev / 'negative-controls/commands.json')
    require(len(commands) == 28 and len({r['name'] for r in commands}) == 28, 'Incomplete negative command inventory')
    for record in commands:
        require(record['returncode'] == (1 if record['name'] in expected and record['name'] != 'candidate' else 0), 'Negative process status differs')
    for name, failures in expected.items():
        text = read(ev / 'negative-controls' / (name + '.log'))
        matches = re.findall(r'^MPE member onset: (\d+) checks, (\d+) failures$', text, re.M)
        require(matches == [('2501',str(failures))], 'Missing/duplicate negative terminal summary')
        require(text.strip().splitlines()[-1] == f'MPE member onset: 2501 checks, {failures} failures', 'Nonterminal negative summary')
        require(len(re.findall(r'^FAIL:', text, re.M)) == failures, 'Negative FAIL count differs')
    for name in ['negative-driver.status','compatibility-driver.status','full-default-build.status']:
        require(read(ev/name).strip() == '0', 'Driver failed: ' + name)
    for profile in ['generic','native']:
        a = (ev/'compatibility'/f'baseline-{profile}.bin').read_bytes()
        b = (ev/'compatibility'/f'candidate-{profile}.bin').read_bytes()
        require(a == b and len(a) == 27552 * 4, 'Compatibility bytes differ')
        require(all(math.isfinite(x[0]) for x in struct.iter_unpack('<f',a)), 'Nonfinite compatibility value')
    compat = read(ev/'compatibility-driver.log')
    for line in ['legacy default: byte-identical to d56911a','legacy tails: byte-identical to d56911a','v1 circuit numerics: byte-identical to 22d6a32','v2 circuit-engine audio: byte-identical to 00ceccf']:
        require(line in compat.splitlines(), 'Missing strict compatibility verdict')
    for stage in ['build','direct','focused-ctest','neighbors']:
        require(read(ev/'native-final'/f'{stage}.status').strip() == '0', 'Native status failed')
    require(read(ev/'native-final/direct.log').strip() == 'MPE member onset host: 976 checks, 0 failures', 'Native direct incomplete')
    for name,count in [('focused-ctest',1),('neighbors',4)]:
        require(f'100% tests passed, 0 tests failed out of {count}' in read(ev/'native-final'/f'{name}.log'), 'Native CTest incomplete')
    require(read(ev/'remaining-core-final-driver.status').strip() == '0', 'Final continuation driver failed')
    require(read(ev/'remaining-core-checked-driver.status').strip() == '1', 'First failed continuation status changed')
    core = data(ev/'remaining-core-final/summary.json')
    prior = data(ev/'remaining-core-checked/progress.json')
    require(core.get('complete') is True and len(core['registered_tests']) == 143, 'Core continuation incomplete')
    require(core['source_commit'] == manifest['qualification_head'], 'Final core checkpoint differs')
    original = read(ev/'attempts/full-default-ctest.log')
    require(digest(ev/'attempts/full-default-ctest.log') == core['original_log_sha256'], 'Original core log mismatch')
    require(digest(ev/'remaining-core-checked/progress.json') == core['prior_progress_sha256'], 'Prior progress changed')
    completed = {int(i):n for i,n in re.findall(r'^\s*\d+/143 Test\s+#(\d+): (\S+)\s+\.{3,}\s+Passed\s+',original,re.M)}
    require(len(completed) == 58 and {str(k):v for k,v in completed.items()} == prior['original_completed'], 'Original completed set differs')
    require(len(prior['remaining']) == 37, 'First continuation inventory differs')
    failed = []
    for record in prior['remaining']:
        log = local(ev/'remaining-core-checked', record['log'])
        require(digest(log) == record['sha256'], 'First continuation log changed')
        if record['passed']:
            require(record['returncode'] == 0 and record['id'] not in completed, 'Invalid first continuation pass')
            require('100% tests passed, 0 tests failed out of 1' in read(log), 'First continuation verdict missing')
            completed[record['id']] = record['name']
        else:
            failed.append(record['id'])
            require(record['returncode'] != 0, 'Prior failed test lacks failed process status')
    require(failed == [95] and len(completed) == 94, 'Prior failure/pass scope differs')
    require({str(k):v for k,v in completed.items()} == core['original_and_prior_completed'], 'Final continuation starting set differs')
    require(len(core['remaining']) == 49, 'Wrong final continuation count')
    for record in core['remaining']:
        i = record['id']
        require(i not in completed and 1 <= i <= 143, 'Duplicate/invalid continuation ID')
        require(record['returncode'] == 0 and record['passed'] is True, 'Continuation failed')
        log = local(ev/'remaining-core-final',record['log'])
        require(digest(log) == record['sha256'], 'Continuation log changed')
        require('100% tests passed, 0 tests failed out of 1' in read(log), 'Continuation verdict missing')
        completed[i] = record['name']
    require([completed[i] for i in range(1,144)] == core['registered_tests'], 'Core coverage mismatch')
    for stage in ['build','direct','baseline-build','baseline-direct']:
        require(read(ev/'tuning-fixture'/f'{stage}.status').strip() == '0', 'Corrected tuning fixture failed')
    for name in ['direct','baseline-direct']:
        require(read(ev/'tuning-fixture'/f'{name}.log').strip().splitlines()[-1] == 'Tuning / MIDI / voice modes: 22085 checks, 0 failures', 'Corrected tuning coverage missing')
    tuning_extra = data(ev/'tuning-fixture/extra-commands.json')
    require(len(tuning_extra) == 4, 'Incomplete extra tuning matrix')
    require({(r['profile'],r['stage']) for r in tuning_extra} == {(p,s) for p in ['native','sanitizer'] for s in ['build','run']}, 'Wrong extra tuning coverage')
    for record in tuning_extra:
        require(record['status'] == 0, 'Extra tuning command failed')
        log = local(ev/'tuning-fixture',record['log'])
        require(digest(log) == record['sha256'], 'Extra tuning log changed')
        if record['profile'] == 'sanitizer':
            require(record['env'] == {'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}, 'Changed tuning sanitizer scope')
        if record['stage'] == 'run':
            require(read(log).strip().splitlines()[-1] == 'Tuning / MIDI / voice modes: 22085 checks, 0 failures', 'Extra tuning checks incomplete')
    bus_commands = data(ev/'shared-pressure/commands.json')
    require(len(bus_commands) == 4, 'Incomplete supplemental bus matrix')
    require({(r['profile'],r['stage']) for r in bus_commands} == {(p,s) for p in ['generic','native'] for s in ['build','run']}, 'Wrong bus profile coverage')
    for record in bus_commands:
        require(record['status'] == 0, 'Supplemental bus command failed')
        log = local(ev/'shared-pressure',record['log'])
        require(digest(log) == record['sha256'], 'Supplemental bus log changed')
        if record['stage'] == 'run':
            require(read(log).strip() == 'MPE shared pressure input: 32 scenarios, 1853296 checks, 0 failures', 'Supplemental bus coverage differs')
    if args.external:
        for name, record in data(here/'external-inputs.json').items():
            path = Path(name)
            require(path.is_file() and path.stat().st_size == record['bytes'] and digest(path) == record['sha256'], 'External input changed/missing: ' + name)
    print('Verified archived hashes, frozen repository inputs and recorded qualification coverage. No runtime tests executed.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, IndexError) as exc:
        print('ERROR: ' + str(exc), file=sys.stderr)
        sys.exit(1)
