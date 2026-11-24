#!/usr/bin/env python3
"""A59 causal audit of the 28 VCA-baseline-only programs; no production writes."""
from pathlib import Path
import csv
import hashlib
import json
import math
import statistics
import struct
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
TARGET = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
BASELINE = ROOT / 'docs/preset-evidence/2026.11.5/baseline/inventory.json'
CURRENT = ROOT / 'docs/preset-evidence/2026.11.6/verification/a15/inventory.json'
A56 = ROOT / 'docs/preset-evidence/2026.11.6/verification/a56/development-inventory-audit.json'
LIBRARY = ROOT / 'build-preset/libcs80_core.a'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def bits(value):
    return struct.pack('<f', value)


def run(command, log):
    result = subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (HERE / log).write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(f'{log}: exit {result.returncode}')
    return result.stdout


def main():
    before, after, classification = (json.loads(p.read_text()) for p in (BASELINE, CURRENT, A56))
    assert sha(BASELINE) == '0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611'
    assert len(before['parameter_ids']) == 200 and len(after['parameter_ids']) == 204
    assert after['parameter_ids'][:200] == before['parameter_ids']
    ids = after['parameter_ids']
    names = [p['name'] for p in classification['conservative_classification']['programs']]
    assert len(names) == len(set(names)) == 28
    old_by_name = {p['name']:p for p in before['programs']}
    new_by_name = {p['name']:p for p in after['programs']}
    cases, analytical = [], []
    for name in names:
        old, new = old_by_name[name], new_by_name[name]
        assert all(old[k] == new[k] for k in ('index','host_index','category'))
        ov, nv = old['values'] + [0,0,0,0], new['values']
        changed = [k for k,a,b in zip(ids,ov,nv) if bits(a) != bits(b)]
        assert set(changed) <= {'volume','ch1_v_level','ch2_v_level'}
        assert any(k != 'volume' for k in changed)
        current_values = dict(zip(ids,nv))
        assert current_values['source_performance'] == 1 and current_values['parity_mode'] == 0
        assert current_values['velocity_curve'] == current_values['pressure_curve'] == 0
        note = 48 if new['category'] == 'Bass' else 60
        cases.append(json.dumps(name) + ' ' + str(note) + ' ' + ' '.join(map(str, ov+nv)))
        ranks = []
        for rank in (1,2):
            b0, b1 = ov[ids.index(f'ch{rank}_v_level')], nv[ids.index(f'ch{rank}_v_level')]
            initial, pressure = current_values[f'ch{rank}_init_level'], current_values[f'ch{rank}_after_level']
            weight = 1-current_values['mix'] if rank == 1 else current_values['mix']
            # Isolated stationary touch sum, excluding time/key/matrix offsets;
            # full Engine renders below include those routes and both ranks.
            ratio = lambda base: 20*math.log10((base+.85*initial)/(base+.25*initial)) if base+.25*initial > 0 else None
            ranks.append({'rank':rank,'weight':weight,'baseline_before':b0,'baseline_after':b1,
                          'initial_touch':initial,'after_touch':pressure,
                          'isolated_touch_velocity_contrast_before_db':ratio(b0),
                          'isolated_touch_velocity_contrast_after_db':ratio(b1)})
        analytical.append({'name':name,'category':new['category'],'host_index':new['host_index'],
                           'note':note,'changed_controls':changed,'ranks':ranks})
    binding = json.loads((ROOT/'docs/preset-evidence/2026.11.6/verification/a15/capture-binding.json').read_text())
    source_hashes = {}
    for path, expected in binding['core_source_hashes'].items():
        committed = subprocess.check_output(['git','show',TARGET+':'+path],cwd=ROOT)
        assert hashlib.sha256(committed).hexdigest() == expected and sha(ROOT/path) == expected, path
        source_hashes[path] = expected
    (HERE/'cases.txt').write_text('\n'.join(cases)+'\n')
    with tempfile.TemporaryDirectory(prefix='cs80-a59-') as tmp:
        executable = Path(tmp)/'response-probe'
        command = ['c++','-std=c++20','-O2','-I'+str(ROOT/'src'),str(HERE/'response-probe.cpp'),str(LIBRARY),'-o',str(executable)]
        run(command,'compile.log')
        output = run([str(executable),str(HERE/'cases.txt')],'response.csv')
        executable_sha = sha(executable)
    rows = list(csv.DictReader(output.splitlines()))
    assert len(rows) == 28*2*4
    results = []
    for record in analytical:
        name = record['name']
        group = [r for r in rows if r['name'] == name]
        assert len(group) == 8
        result = dict(record)
        result['measurements'] = group
        contrasts = {}
        for version in ('published_controls','current'):
            r = [x for x in group if x['version'] == version]
            get = lambda v,p: next(float(x['held_rms_dbfs']) for x in r if abs(float(x['velocity'])-v)<1e-6 and abs(float(x['pressure'])-p)<1e-6)
            contrasts[version] = {'firm_minus_soft_db':get(.85,0)-get(.25,0),
                                  'pressure_0_8_minus_zero_db':get(.5,.8)-get(.5,0)}
        result['contrasts'] = contrasts
        result['velocity_contrast_change_db'] = contrasts['current']['firm_minus_soft_db']-contrasts['published_controls']['firm_minus_soft_db']
        result['pressure_contrast_change_db'] = contrasts['current']['pressure_0_8_minus_zero_db']-contrasts['published_controls']['pressure_0_8_minus_zero_db']
        results.append(result)
    changes = [p['velocity_contrast_change_db'] for p in results]
    summary = {'schema':1,'scope':'Same current Engine; published .4 parameter values versus .6, all at common master 0.5. Isolates preset VCA baseline edits, not old-to-new DSP differences. Local 2-second held-note measurements; no acoustic reference or listening qualification.',
               'release_qualified':False,'perceptual_quality_qualified':False,'source_commit':TARGET,
               'source_files_matching_target':source_hashes,'library_sha256':sha(LIBRARY),
               'executable_sha256':executable_sha,'inputs':{str(p.relative_to(ROOT)):sha(p) for p in (BASELINE,CURRENT,A56,HERE/'cases.txt',HERE/'response-probe.cpp',HERE/'response.csv')},
               'sample_rate':48000,'render_block_frames':128,'held_seconds':2,'clips':len(rows),
               'common_master_volume':.5,'programs':results,
               'summary':{'programs':len(results),'velocity_contrast_increased':sum(x>0 for x in changes),
                          'velocity_contrast_change_db':{'min':min(changes),'median':statistics.median(changes),'max':max(changes)},
                          'peak':max(float(r['peak']) for r in rows),
                          'all_clips_finite_and_within_full_scale':True,
                          'all_routes_panel':all(r['source1']=='13' and r['source2']=='13' for r in rows)}}
    (HERE/'response-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary['summary'],indent=2))
    print('\n'.join(f"{p['name']}: velocity {p['contrasts']['published_controls']['firm_minus_soft_db']:.6f} -> {p['contrasts']['current']['firm_minus_soft_db']:.6f}; pressure delta {p['pressure_contrast_change_db']:.6f}" for p in results))


if __name__ == '__main__':
    main()
