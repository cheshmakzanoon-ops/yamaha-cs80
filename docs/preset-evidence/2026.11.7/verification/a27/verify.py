#!/usr/bin/env python3
"""A27 Init/control compatibility audit. Evidence and temporary outputs only.

Stock capture executables retain each release's own ABI. The only linked
custom caller uses the complete current src headers with the current core.
No JUCE processor, host or packaged plugin is instantiated by this audit.
"""
import argparse
import array
import csv
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[4]
SOURCES = {
    'published4': '078c0981ffa1b5472342e0fb5ab8702bc3bd2889',
    'baseline6': '18711f93a39aa49f3d6f9a41784be456224facbe',
    'current7': 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201',
}
COMMANDS = []


def sha(data):
    return hashlib.sha256(data).hexdigest()


def run(args, **kwargs):
    args = [str(x) for x in args]
    result = subprocess.run(args, cwd=ROOT, capture_output=True, check=True, **kwargs)
    COMMANDS.append({'argv': args, 'returncode': result.returncode})
    return result.stdout


def source(commit, path):
    return run(['git', 'show', f'{commit}:{path}'])


def write(name, data):
    (OUT / name).write_text(json.dumps(data, indent=2, allow_nan=False) + '\n')


def bits(values):
    return struct.pack('<' + 'f' * len(values), *values)


def wav_data(path):
    data = path.read_bytes()
    assert data[:4] == b'RIFF' and data[8:12] == b'WAVE'
    at = 12
    while at + 8 <= len(data):
        kind, size = struct.unpack_from('<4sI', data, at)
        if kind == b'data':
            return data[at + 8:at + 8 + size]
        at += 8 + size + size % 2
    raise AssertionError(f'No WAV data: {path}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--current-build', type=Path, default='/tmp/cs80-build-1cee5a')
    parser.add_argument('--baseline6-build', type=Path, default='/tmp/cs80-reference-1cee5a')
    parser.add_argument('--published4-build', type=Path, default='/tmp/cs80-published-4-build-1cee5a')
    parser.add_argument('--temporary', type=Path, default='/tmp/cs80-a27-1cee5a')
    args = parser.parse_args()
    tmp = args.temporary.resolve()
    tmp.mkdir(parents=True, exist_ok=True)
    builds = {'published4': args.published4_build.resolve(),
              'baseline6': args.baseline6_build.resolve(), 'current7': args.current_build.resolve()}
    run(['git', 'diff', '--exit-code', SOURCES['current7'], '--', 'src', 'tests'])
    inventories, metadata, artifact_hashes = {}, {}, {}
    emitter = ROOT / 'docs/preset-evidence/2026.11.6/development/a08-parameter-contract.cpp'
    for label, build in builds.items():
        target = tmp / label
        target.mkdir(exist_ok=True)
        capture = build / 'cs80_preset_capture'
        run([capture, '--inventory', target / 'inventory.json', '--library', 'all'])
        inventories[label] = json.loads((target / 'inventory.json').read_text())
        (target / 'Params.h').write_bytes(source(SOURCES[label], 'src/core/Params.h'))
        run(['c++', '-std=c++20', '-O2', '-I', target, emitter, '-o', target / 'metadata'])
        metadata[label] = run([target / 'metadata'])
        artifact_hashes[label] = {name: sha((build / name).read_bytes())
                                 for name in ('cs80_preset_capture', 'libcs80_core.a')}
    current, before6, before4 = [inventories[x] for x in ('current7', 'baseline6', 'published4')]
    assert [len(inventories[x]['parameter_ids']) for x in ('published4', 'baseline6', 'current7')] == [200, 204, 204]
    assert all(len(d['programs']) == 523 for d in inventories.values())
    identity = lambda d: [(p['index'], p['host_index'], p['name'], p['category']) for p in d['programs']]
    assert identity(current) == identity(before6) == identity(before4)
    assert current['parameter_ids'] == before6['parameter_ids']
    assert current['parameter_ids'][:200] == before4['parameter_ids']
    assert (tmp / 'current7/inventory.json').read_bytes() == (OUT.parent / 'a12/inventory.json').read_bytes()
    rows = {k: [line.split('\t') for line in v.decode().splitlines()] for k, v in metadata.items()}
    assert metadata['baseline6'] == metadata['current7']
    # Numeric and normalized contracts are independent of human-facing labels.
    assert all(a[:2] + a[5:] == b[:2] + b[5:]
               for a, b in zip(rows['published4'], rows['current7'][:200]))
    label_differences = [{'index': int(a[0]), 'id': a[1], 'published4': a[2:5], 'current7': b[2:5]}
                         for a, b in zip(rows['published4'], rows['current7'][:200]) if a[2:5] != b[2:5]]
    append = [{'index': int(r[0]), 'id': r[1], 'minimum': struct.unpack('<f', bytes.fromhex(r[5])[::-1])[0],
               'maximum': struct.unpack('<f', bytes.fromhex(r[6])[::-1])[0],
               'default': struct.unpack('<f', bytes.fromhex(r[7])[::-1])[0]} for r in rows['current7'][200:]]
    init_rows = []
    for index, name in ((43, 'Init Patch'), (522, 'Init Circuit')):
        p4, p6, p7 = [inventories[x]['programs'][index] for x in ('published4', 'baseline6', 'current7')]
        assert p7['name'] == name and p7['category'] == 'Init'
        assert p4['values'] == p7['values'][:200] and p6['values'] == p7['values']
        assert p7['values'][190:] == [0, 0, .5, 1, 0, 0, .5, 1, 1, 1, 0, 0, 0, 0]
        assert bits(p4['values']) == bits(p7['values'][:200])
        assert bits(p6['values']) == bits(p7['values'])
        init_rows.append({'host_index': index, 'name': name, 'category': 'Init',
                          'published4_prefix_200_sha256': sha(bits(p4['values'])),
                          'current204_sha256': sha(bits(p7['values'])),
                          'changed_vs_published4': [], 'changed_vs_baseline6': []})
    write('controls.json', {'program_count_each': 523, 'catalogue_identity_exact': True,
        'new_programs': 0, 'parameter_counts': {k: len(v['parameter_ids']) for k, v in inventories.items()},
        'historic200_id_range_default_skew_and_257_normalization_pairs_exact': True,
        'baseline6_all204_metadata_exact': True, 'label_differences_from_published4': label_differences,
        'metadata_sha256': {k: sha(v) for k, v in metadata.items()}, 'appended_after_published4': append,
        'init_entries': init_rows,
        'all_init_controls': [{'index': i, 'id': id, 'init_patch': current['programs'][43]['values'][i],
                               'init_circuit': current['programs'][522]['values'][i]}
                              for i, id in enumerate(current['parameter_ids'])]})

    # This caller is linked only to the current core and current complete headers.
    probe = tmp / 'core_probe'
    run(['c++', '-std=c++20', '-O3', '-DNDEBUG', '-I', ROOT / 'src', OUT / 'core_probe.cpp',
         builds['current7'] / 'libcs80_core.a', '-o', probe])
    completed = json.loads(run([probe]))
    assert completed['failures'] == 0
    write('core-completion.json', completed)

    # Two small playing cases at each supported capture sample rate.
    audio = []
    for rate in (44100, 48000, 96000):
        for note, velocity, pressure in ((48, .25, 0), (72, .85, .75)):
            case = f'{rate}-n{note}-v{velocity}-p{pressure}'
            files, metrics = {}, {}
            for label, build in builds.items():
                destination = tmp / label / case
                run([build / 'cs80_preset_capture', '--out', destination, '--library', 'all',
                     '--preset', 'Init Patch', '--preset', 'Init Circuit', '--rate', rate,
                     '--note', note, '--velocity', velocity, '--pressure', pressure,
                     '--hold', .5, '--tail', .5, '--wav'])
                files[label] = {43: destination / '43-Init-Patch.wav', 522: destination / '522-Init-Circuit.wav'}
                metrics[label] = {int(r['index']): r for r in csv.DictReader((destination / 'metrics.csv').open())}
            for index in (43, 522):
                payload = {label: wav_data(value[index]) for label, value in files.items()}
                values = array.array('f'); values.frombytes(payload['current7'])
                assert len(values) == rate * 2 and all(math.isfinite(x) for x in values)
                peak = max(abs(x) for x in values)
                assert 0.001 < peak < 1
                audio.append({'name': current['programs'][index]['name'], 'host_index': index,
                    'rate': rate, 'note': note, 'velocity': velocity, 'pressure': pressure,
                    'hold_seconds': .5, 'tail_seconds': .5, 'stereo_float_samples': len(values),
                    'sample_sha256': {k: sha(v) for k, v in payload.items()},
                    'exact_vs_baseline6': payload['current7'] == payload['baseline6'],
                    'exact_vs_published4': payload['current7'] == payload['published4'],
                    'peak_dbfs_current': float(metrics['current7'][index]['peak_dbfs'])})
            # The comparison distinguishes the two intentionally distinct Init recipes.
            assert wav_data(files['current7'][43]) != wav_data(files['current7'][522])
    write('audio.json', {'scope': 'fresh deterministic stock core captures; no listening/host/plugin claim',
                         'cases': audio, 'all_exact_vs_baseline6': all(r['exact_vs_baseline6'] for r in audio),
                         'all_exact_vs_published4': all(r['exact_vs_published4'] for r in audio)})

    tests = []
    for name in ('cs80_instrument_body_variant_tests', 'cs80_material_body_tests', 'cs80_instrument_body_engine_tests'):
        executable = builds['current7'] / name
        output = run([executable])
        (OUT / f'{name}.log').write_bytes(output)
        tests.append({'name': name, 'exit_code': 0, 'executable_sha256': sha(executable.read_bytes()),
                      'stdout_sha256': sha(output)})
    paths = ['src/core/Params.h', 'src/core/Engine.cpp', 'src/core/Engine.h', 'src/core/InstrumentBody.h',
             'src/core/FactoryPresets.cpp', 'src/core/OriginalCircuitLibrary.inc', 'src/core/CuratedLibrary.inc',
             'src/core/InstrumentBodyVariants.h', 'src/core/InstrumentBodyMaterials.h',
             'src/plugin/PluginProcessor.cpp', 'src/plugin/PluginProcessorLibrary.cpp', 'src/plugin/PresetBrowser.cpp',
             'src/plugin/PresetFileValues.h', 'tests/InstrumentBodyHostTests.h']
    unchanged_state_paths = ['src/core/Params.h', 'src/core/Engine.cpp', 'src/plugin/PluginProcessor.cpp',
                             'src/plugin/PluginProcessorLibrary.cpp', 'src/plugin/PresetBrowser.cpp',
                             'src/plugin/PresetFileValues.h']
    assert all((ROOT / path).read_bytes() == source(SOURCES['baseline6'], path) for path in unchanged_state_paths)
    write('receipt.json', {'sources': SOURCES, 'current_worktree_head': run(['git', 'rev-parse', 'HEAD']).decode().strip(),
        'source_files_match_current_sound_commit': True,
        'state_paths_exactly_unchanged_from_baseline6': unchanged_state_paths,
        'source_sha256': {p: sha((ROOT / p).read_bytes()) for p in paths},
        'local_artifact_sha256': artifact_hashes, 'inventory_sha256': {
            label: sha((tmp / label / 'inventory.json').read_bytes()) for label in builds},
        'metadata_emitter': str(emitter.relative_to(ROOT)), 'metadata_emitter_sha256': sha(emitter.read_bytes()),
        'existing_tests': tests, 'commands': COMMANDS,
        'host_deserialization_executed': False, 'packaged_plugin_qualified': False,
        'passed': all(r['exact_vs_baseline6'] and r['exact_vs_published4'] for r in audio)})
    print(json.dumps({'audio_cases': len(audio), 'exact_vs_baseline6': all(r['exact_vs_baseline6'] for r in audio),
                      'exact_vs_published4': all(r['exact_vs_published4'] for r in audio),
                      'core_checks': completed['checks'], 'existing_tests_passed': len(tests)}))


if __name__ == '__main__':
    main()
