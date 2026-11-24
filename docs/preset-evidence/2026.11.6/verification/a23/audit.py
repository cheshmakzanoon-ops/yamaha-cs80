#!/usr/bin/env python3
"""A23 full Plucked inventory and complementary current-engine captures."""
import concurrent.futures
import csv
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
CAPTURE = ROOT / 'build-preset/cs80_preset_capture'
BASE = ROOT / 'docs/preset-evidence/2026.11.5/baseline/inventory.json'
CURRENT = ROOT / 'docs/preset-evidence/2026.11.6/verification/a15/inventory.json'
SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
NEW_MATERIALS = {'Harp': 9, 'Koto': 8, 'Banjo': 7, 'Kalimba': 6}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    baseline = json.loads(BASE.read_text())
    current = json.loads(CURRENT.read_text())
    assert sha(BASE) == '0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611'
    ids = current['parameter_ids']
    programs = []
    for index, p in enumerate(current['programs']):
        if p['category'] != 'Plucked':
            continue
        old = baseline['programs'][index]
        assert all(p[k] == old[k] for k in ('index', 'host_index', 'name', 'category'))
        changes = [ids[i] for i in range(200) if p['values'][i] != old['values'][i] and ids[i] != 'volume']
        assert changes
        v = dict(zip(ids, p['values']))
        for rank in (1, 2):
            assert v[f'body{rank}_model'] > 0 and v[f'body{rank}_mix'] > 0
            assert v[f'body{rank}_material'] == NEW_MATERIALS.get(p['name'], 0)
        programs.append({'name': p['name'], 'host_index': p['host_index'], 'changed_prior_nonvolume_controls': changes,
                         'body_controls': {k: value for k, value in v.items() if k.startswith('body')},
                         'footage_indexes': [v['ch1_feet'], v['ch2_feet']]})
    assert len(programs) == 17
    paths = ['src/core/InstrumentBody.h', 'src/core/InstrumentBodyMaterials.h', 'src/core/Engine.cpp',
             'src/core/Params.h', 'src/core/FactoryPresets.cpp', 'src/core/CuratedLibrary.inc',
             'src/core/curated/Plucked.inc', 'src/core/curated/Modern.inc']
    source_hashes = {}
    for name in paths:
        raw = (ROOT / name).read_bytes()
        assert raw == subprocess.check_output(['git', 'show', SOURCE + ':' + name], cwd=ROOT)
        source_hashes[name] = hashlib.sha256(raw).hexdigest()
    before = sha(CAPTURE)
    names = [p['name'] for p in programs if p['name'] not in NEW_MATERIALS]
    assert len(names) == 13
    def capture(pair):
        note, velocity = pair
        directory = OUT / f'note-{note}-velocity-{velocity}'
        cmd = [str(CAPTURE), '--out', str(directory), '--library', 'all', '--rate', '48000',
               '--note', str(note), '--velocity', str(velocity), '--pressure', '0', '--hold', '1.2', '--tail', '.4']
        for name in names:
            cmd += ['--preset', name]
        result = subprocess.run(cmd, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (OUT / f'{directory.name}.log').write_text(result.stdout)
        assert result.returncode == 0, result.stdout
        rows = list(csv.DictReader((directory / 'metrics.csv').open()))
        assert len(rows) == 13 and {r['name'] for r in rows} == set(names)
        for row in rows:
            assert int(row['nonfinite']) == 0 and int(row['samples_over_full_scale']) == 0
        return pair, rows
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        captures = dict(pool.map(capture, [(note, vel) for note in (48, 72) for vel in (.25, .85)]))
    assert before == sha(CAPTURE)
    contrasts = []
    for note in (48, 72):
        soft = {r['name']: r for r in captures[(note, .25)]}
        for firm in captures[(note, .85)]:
            contrasts.append({'name': firm['name'], 'note': note,
                              'onset_touch_db': float(firm['early_rms_dbfs']) - float(soft[firm['name']]['early_rms_dbfs']),
                              'firm_body_dbfs': float(firm['body_rms_dbfs']),
                              'firm_tail_dbfs': float(firm['tail_rms_dbfs'])})
    summary = {'scope': 'Local development captures; no native-release or acoustic-equivalence claim',
               'matching_source': SOURCE, 'source_hashes': source_hashes,
               'capture_executable_sha256_before_and_after': before,
               'baseline_inventory_sha256': sha(BASE), 'current_inventory_sha256': sha(CURRENT),
               'program_count': len(programs), 'programs': programs,
               'capture_count': 52, 'capture_names': names, 'notes': [48, 72], 'velocities': [.25, .85],
               'sample_rate': 48000, 'hold_seconds': 1.2, 'tail_seconds': .4, 'pressure': 0,
               'generated_seconds': 52 * 1.6, 'all_finite_and_below_full_scale': True,
               'maximum_peak_dbfs': max(float(r['peak_dbfs']) for rows in captures.values() for r in rows),
               'minimum_onset_touch_db': min(c['onset_touch_db'] for c in contrasts),
               'maximum_onset_touch_db': max(c['onset_touch_db'] for c in contrasts),
               'measurements': contrasts}
    (OUT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps({k: v for k, v in summary.items() if k not in ('programs', 'measurements', 'source_hashes')}, indent=2))

if __name__ == '__main__':
    main()
