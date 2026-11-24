#!/usr/bin/env python3
"""Derive the reviewed successor's source/roster contract; never test products."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SOURCE = '1b5b3c70471e369481b1f813880a6bcc762a2f57'
TREE = 'b235b67a801d33e9572076b8dfcfbb89e71fac29'
RUN_ID, ATTEMPT = 45457535944, 1
RECOVERED_SOURCE = 'ef1c472b161c37c490945f7c469086508aae6aef'
RECOVERED_TREE = 'f4e00753cde135a36172d9ea549223dfdce5cc82'
REVIEWED_CODE_BLOBS = {
    '.github/workflows/dsp-measurements.yml': 'dff5d94bbd212e7484c6232a064141abd4f5c2cf',
    'scripts/ci-sanitizer-shards.py': '3e9098ffe65a4aec63ed3649028888f0e36e8d5c',
    'scripts/test-ci-sanitizer-shards.py': 'dcb861c684e052c2c60dbb2464dc06818f6a40d9',
    'tests/SixteenVoiceReleaseTests.cpp': '779c3a87d785d797e9b9a8fd3031bb227dfee704',
}
PREVIOUS = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
EXPECTED = {'linux-x86_64': 619, 'windows-x86_64': 261, 'macos-arm64': 283, 'macos-x86_64': 283}
NEW = {'cs80_instrument_damper_tests', 'cs80_electric_pickup_tests',
       'cs80_piano_hammer_contact_tests', 'cs80_piano_treble_contact_tests',
       'cs80_crystal_string_stability_tests', 'cs80_preset_repeatability_tests'}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def record(raw):
    return {'bytes': len(raw), 'sha256': sha(raw),
            'git_blob': hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()}


def load_helper(name, records):
    raw = (HERE / 'helpers' / name).read_bytes()
    item = next(row for row in records if row['name'] == name)
    require(all(record(raw)[key] == item[key] for key in ('bytes', 'sha256', 'git_blob')), 'Recovered helper changed')
    spec = importlib.util.spec_from_file_location('recovered_' + name.replace('-', '_'), HERE / 'helpers' / name)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--out', type=Path, default=HERE / 'source-contract.json')
    args = parser.parse_args()
    require(not args.out.exists(), 'Refusing to replace a contract')

    def git(*parts):
        return subprocess.check_output(['git', '-C', str(args.repo), *parts])

    require(git('rev-parse', SOURCE + '^{tree}').decode().strip() == TREE, 'Source tree mismatch')
    require(git('rev-parse', RECOVERED_SOURCE + '^{tree}').decode().strip() == RECOVERED_TREE,
            'Recovered predecessor tree differs')
    # Read hashed commit objects directly: a shallow checkout deliberately hides
    # ancestry from merge-base even when the retained parent objects are present.
    cursor, ancestry = SOURCE, []
    while cursor != RECOVERED_SOURCE:
        require(len(ancestry) < 16, 'Unexpected successor ancestry depth')
        raw_commit = git('cat-file', 'commit', cursor)
        require(hashlib.sha1(b'commit ' + str(len(raw_commit)).encode() + b'\0' + raw_commit).hexdigest()
                == cursor, 'Commit object identity differs')
        parents = re.findall(rb'^parent ([0-9a-f]{40})$', raw_commit.split(b'\n\n', 1)[0], re.M)
        require(len(parents) == 1, 'Successor ancestry needs explicit review')
        ancestry.append({'commit': cursor, 'parent': parents[0].decode(),
                         'bytes': len(raw_commit), 'sha256': sha(raw_commit)})
        cursor = parents[0].decode()
    changed = git('diff', '--name-only', RECOVERED_SOURCE, SOURCE).decode().splitlines()
    require({p for p in changed if not p.startswith('docs/')} == set(REVIEWED_CODE_BLOBS),
            'Unreviewed non-documentation source change')
    reviewed_changes = {p: record(git('show', SOURCE + ':' + p)) for p in REVIEWED_CODE_BLOBS}
    require(all(reviewed_changes[p]['git_blob'] == pin for p, pin in REVIEWED_CODE_BLOBS.items()),
            'Reviewed successor code bytes differ')
    stable_paths = ['src', 'CMakeLists.txt', 'cmake', '.github/workflows/build.yml']
    stable_objects = {}
    for path in stable_paths:
        before = git('rev-parse', RECOVERED_SOURCE + ':' + path).decode().strip()
        after = git('rev-parse', SOURCE + ':' + path).decode().strip()
        require(before == after, 'Native/audio source binding changed: ' + path)
        stable_objects[path] = after
    helper_records = json.loads((HERE / 'helper-recovery.json').read_bytes())
    for item in helper_records:
        raw = git('show', SOURCE + ':' + item['source_path'])
        require(raw == (HERE / 'helpers' / item['name']).read_bytes(), 'Git/API helper bytes differ')
        require(all(record(raw)[key] == item[key] for key in ('bytes', 'sha256', 'git_blob')), 'Helper identity mismatch')
    previous = load_helper('previous-native-contract.py', helper_records).derive_contract(args.repo)
    require(previous['source'] == PREVIOUS, 'Unexpected predecessor')
    diff = git('diff', '--unified=0', PREVIOUS, SOURCE, '--', 'CMakeLists.txt')
    additions = re.findall(rb'^\+\s*add_test\(NAME (cs80_\w+)', diff, re.M)
    require(len(additions) == 6 and set(x.decode() for x in additions) == NEW, 'Native additions differ')
    require(not re.search(rb'^-\s*add_test\(', diff, re.M), 'Removed native registration')
    names = {target: sorted(old + sorted(NEW)) for target, old in previous['expected_names'].items()}
    require({t: len(n) for t, n in names.items()} == EXPECTED, 'Native counts differ')
    require(all(len(n) == len(set(n)) for n in names.values()), 'Duplicate native name')
    with (HERE / 'cmake-registration-delta.patch').open('xb') as stream:
        stream.write(diff)
    paths = ['CMakeLists.txt', 'cmake/CTestScheduling.cmake', '.github/workflows/build.yml',
             'scripts/capture-factory-auditions.py', 'scripts/package-ci-artifacts.py',
             'scripts/verify-ci-bundle.py', 'scripts/run-pluginval-with-core.py',
             'scripts/verify-ci-juce-source.cmake', 'src/core/FactoryPresets.h',
             'src/tools/preset_capture.cpp', 'src/tools/ui_snapshot.cpp',
             'tests/InstrumentDamperTests.cpp', 'tests/InstrumentBodyHostTests.h',
             'docs/INSTALL_AND_QUICKSTART.md', 'docs/PRESET_GUIDE.md', 'docs/CI_DISTRIBUTION_STATUS.md',
             'docs/CIRCUIT_LIBRARY_2026_11_8.md', 'docs/TEAM_REVIEW_2026_11_8.md',
             *REVIEWED_CODE_BLOBS]
    raw_sources = {path: git('show', SOURCE + ':' + path) for path in paths}
    require(re.findall(rb'^inline constexpr const char\* curatedLibraryRevision = "([^"\r\n]*)";$',
                       raw_sources['src/core/FactoryPresets.h'], re.M) == [b'2026.11.8'], 'Revision mismatch')
    ns = {'__name__': 'source_exporter', '__file__': str(args.repo / 'scripts/capture-factory-auditions.py')}
    exec(compile(raw_sources['scripts/capture-factory-auditions.py'], 'published-exporter', 'exec'), ns)
    groups = ns['selections']()
    summary = ns['roster_summary'](groups)
    require(summary == {'wav_count': 256, 'selected_program_count': 95, 'audio_seconds': 1487.2}, 'Audition totals differ')
    old_ns = {'__name__': 'old_exporter', '__file__': ns['__file__']}
    exec(compile(git('show', PREVIOUS + ':scripts/capture-factory-auditions.py'), 'old-exporter', 'exec'), old_ns)
    old_groups = old_ns['selections']()
    require(groups[:len(old_groups)] == old_groups and old_ns['roster_summary'](old_groups)['wav_count'] == 238,
            'Previous 238 requests are not preserved')
    run_raw = (HERE / 'run-api-at-preparation.json').read_bytes()
    run = json.loads(run_raw)
    require(run['id'] == RUN_ID and run['run_attempt'] == ATTEMPT and run['head_sha'] == SOURCE,
            'Authenticated preparation run receipt differs')
    require(run['head_commit']['tree_id'] == TREE and run['path'] == '.github/workflows/build.yml',
            'Native run tree/workflow differs')
    require(run['repository']['full_name'] == 'cheshmakzanoon-ops/yamaha-cs80'
            and run['repository']['id'] == 1436742091, 'Repository identity differs')
    native_parser = load_helper('parse-native-evidence.py', helper_records)
    require(sum(len(lanes) for lanes in native_parser.LANES.values()) == 15, 'Required validator lanes differ')
    canonical = json.dumps(groups, sort_keys=True, separators=(',', ':')).encode()
    result = {'schema': 1, 'status': 'successor_preparation_not_native_evidence', 'release_qualified': False,
              'source': SOURCE, 'source_tree': TREE, 'library_revision': '2026.11.8',
              'run_id': RUN_ID, 'run_attempt': ATTEMPT, 'repository': 'cheshmakzanoon-ops/yamaha-cs80',
              'repository_id': 1436742091, 'run_api_input': record(run_raw),
              'run_status_observed': run['status'], 'run_conclusion_observed': run['conclusion'],
              'reviewed_successor': {'predecessor_source': RECOVERED_SOURCE,
                                    'predecessor_tree': RECOVERED_TREE, 'changed_paths': changed,
                                    'reviewed_code_changes': reviewed_changes,
                                    'verified_commit_ancestry': ancestry,
                                    'unchanged_git_objects': stable_objects,
                                    'previous_native_evidence_reused': False,
                                    'supporting_dsp_run_id': 45457536046, 'supporting_dsp_attempt': 1},
              'helper_inputs': helper_records, 'source_inputs': {p: record(raw) for p, raw in raw_sources.items()},
              'expected_registered': EXPECTED, 'expected_native_names': names,
              'common_new_native_tests': sorted(NEW), 'native_registration_delta': record(diff),
              'previous_native_derivation': {k: v for k, v in previous.items() if k != 'expected_names'},
              'allowed_formal_skip_names_subject_to_review': ['cs80_filter_batch_cap_tests', 'cs80_stereo_pra_exact_tests'],
              'validator_lanes': native_parser.LANES,
              'audition': {'expected': summary, 'folder_count': len(groups), 'regular_file_count': 321,
                           'inventory_programs': 523, 'inventory_parameters': 204, 'selections': groups,
                           'selections_sha256': sha(canonical), 'preserved_requests': 238, 'appended_requests': 18,
                           'independent_inventory': None, 'independent_capture_receipt': None,
                           'actual_archive_checked': False,
                           'independent_inventory_interface': '--expected-inventory PATH --independent-capture-receipt PATH'},
              'scope': 'Separate successor preparation with freshly checked source objects and rederived names. '
                       'No product build/test/capture or current artifact acceptance. Earlier native evidence '
                       'is not reused for this source. Original upload attempts, normal exits, '
                       'skip review, payload equality, actual audition and public delivery remain separate gates.'}
    with args.out.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'source': SOURCE, 'tree': TREE, 'run_id': RUN_ID, 'attempt': ATTEMPT,
                      'native': EXPECTED, 'audition': summary, 'selections_sha256': sha(canonical),
                      'release_qualified': False}))


if __name__ == '__main__':
    main()
