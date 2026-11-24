#!/usr/bin/env python3
"""Independent FX inventory review and optional original-bank render metrics.

Run from the repository root; use a fresh --out directory for reproduction.
No WAVs or production files are written.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--render', action='store_true')
    args = parser.parse_args()
    repo = Path.cwd()
    args.out.mkdir(parents=True, exist_ok=True)
    base_path = repo / 'docs/preset-evidence/2026.11.5/baseline/inventory.json'
    now_path = repo / 'docs/preset-evidence/2026.11.6/verification/a15/inventory.json'
    base = json.loads(base_path.read_text())
    now = json.loads(now_path.read_text())
    assert now['parameter_ids'][:200] == base['parameter_ids']
    rows = []
    vectors = set()
    for old, new in zip(base['programs'], now['programs']):
        if old['category'] != 'FX':
            continue
        for key in ('index', 'host_index', 'name', 'category'):
            assert old[key] == new[key]
        changed = [name for name, a, b in zip(base['parameter_ids'], old['values'], new['values'])
                   if name != 'volume' and a != b]
        assert changed, new['name']
        values = dict(zip(now['parameter_ids'], new['values']))
        assert values['body1_material'] == values['body2_material'] == 0
        vectors.add(tuple(v for name, v in values.items() if name != 'volume'))
        rows.append({'name': new['name'], 'host_index': new['host_index'],
                     'library': 'original' if new['host_index'] < 222 else 'curated',
                     'changed_prior_non_master_controls': changed,
                     'body_enabled': any(values[f'body{rank}_model'] != 0 and values[f'body{rank}_mix'] > 0 for rank in (1, 2)),
                     'description': new['description']})
    assert len(rows) == 46 and len(vectors) == 46
    source = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
    paths = ['src/core/Engine.cpp', 'src/core/Engine.h', 'src/core/Params.h',
             'src/core/InstrumentBody.h', 'src/core/InstrumentBodyMaterials.h',
             'src/core/FactoryPresets.cpp', 'src/core/FactoryPresets.h',
             'src/core/CuratedLibrary.inc', 'src/core/OriginalCircuitLibrary.inc',
             'src/core/curated/Fx.inc', 'tests/FxRefinementTests.cpp']
    hashes = {name: sha(repo / name) for name in paths}
    for name, digest in hashes.items():
        expected = hashlib.sha256(subprocess.check_output(['git', 'show', source + ':' + name])).hexdigest()
        assert expected == digest, name
    capture = repo / 'build-preset/cs80_preset_capture'
    fx_test = repo / 'build-preset/cs80_fx_refinement_tests'
    binaries = {str(p.relative_to(repo)): sha(p) for p in (capture, fx_test)}
    result = {'source': source, 'scope': 'Structural comparison is all 46 FX programs; basic rendering below is the 22 original-address programs only.',
              'baseline_inventory_sha256': sha(base_path), 'candidate_inventory_sha256': sha(now_path),
              'count': len(rows), 'unique_non_master_sound_vectors': len(vectors),
              'original': sum(x['library'] == 'original' for x in rows),
              'curated': sum(x['library'] == 'curated' for x in rows),
              'body_enabled': sum(x['body_enabled'] for x in rows),
              'minimum_changed_controls': min(len(x['changed_prior_non_master_controls']) for x in rows),
              'maximum_changed_controls': max(len(x['changed_prior_non_master_controls']) for x in rows),
              'programs': rows, 'source_sha256': hashes, 'executables_sha256': binaries}
    if args.render:
        out = args.out / 'original-held'
        assert not out.exists(), 'Use a fresh output directory'
        command = [str(capture), '--out', str(out), '--library', 'original', '--rate', '48000',
                   '--note', '60', '--velocity', '.85', '--pressure', '.5', '--hold', '8', '--tail', '1']
        for p in rows:
            if p['library'] == 'original':
                command += ['--preset', p['name']]
        with (args.out / 'original-capture.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        audio = list(csv.DictReader((out / 'metrics.csv').open()))
        assert len(audio) == 22
        assert all(int(x['nonfinite']) == 0 and int(x['samples_over_full_scale']) == 0 for x in audio)
        result['fresh_rendering'] = {'count': len(audio), 'seconds': len(audio) * 9,
                                     'sample_rate': 48000, 'hold_seconds': 8, 'tail_seconds': 1,
                                     'pressure': .5, 'velocity': .85, 'note': 60,
                                     'all_finite_unclipped': True,
                                     'maximum_peak_dbfs': max(float(x['peak_dbfs']) for x in audio),
                                     'metrics_sha256': sha(out / 'metrics.csv')}
    assert hashes == {name: sha(repo / name) for name in paths}
    assert binaries == {name: sha(repo / name) for name in binaries}
    result['source_and_executables_unchanged_after'] = True
    (args.out / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('programs', 'source_sha256', 'executables_sha256')}, indent=2))


if __name__ == '__main__':
    main()
