#!/usr/bin/env python3
"""A25 FX evidence: immutable inventory, reused gates, targeted actual captures.

Run from the repository root. WAVs stay in the explicitly selected scratch
directory; only receipts, small metric tables and the report enter the repo.
No production preset is modified and no old Engine is linked to new headers.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

import numpy as np
from scipy.io import wavfile

HERE = Path(__file__).resolve().parent
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--repo', type=Path, default=HERE.parents[4])
ap.add_argument('--tmp', type=Path, default=Path('/tmp/cs80-a25-1cee5a'))
ap.add_argument('--current', type=Path, default=Path('/tmp/cs80-build-1cee5a/cs80_preset_capture'))
ap.add_argument('--baseline', type=Path, default=Path('/tmp/cs80-reference-1cee5a/cs80_preset_capture'))
ap.add_argument('--render', action='store_true')
args = ap.parse_args()
ROOT, TMP = args.repo, args.tmp
CURRENT = 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201'
BASELINE = '18711f93a39aa49f3d6f9a41784be456224facbe'
DEVELOPMENT = ROOT / 'docs/preset-evidence/2026.11.7/development'
checks = []


def sha(blob):
    return hashlib.sha256(blob).hexdigest()


def ref(path):
    path = Path(path)
    return {'path': str(path), 'bytes': path.stat().st_size, 'sha256': sha(path.read_bytes())}


def git_blob(commit, path):
    return subprocess.check_output(['git', '-C', str(ROOT), 'show', commit + ':' + path])


def check(ok, label):
    checks.append({'pass': bool(ok), 'label': label})


inventory_path = ROOT / 'docs/preset-evidence/2026.11.7/verification/a12/inventory.json'
inventory = json.loads(inventory_path.read_bytes())
baseline = json.loads(git_blob(BASELINE, 'docs/preset-evidence/2026.11.6/verification/a15/inventory.json'))
published = json.loads(git_blob(BASELINE, 'docs/preset-evidence/2026.11.5/baseline/inventory.json'))
ids = inventory['parameter_ids']
family = [p for p in inventory['programs'] if p['category'] == 'FX']
before = {p['host_index']: p for p in baseline['programs']}
old = {p['host_index']: p for p in published['programs']}
by_name = {p['name']: p for p in family}
values = {p['name']: dict(zip(ids, p['values'])) for p in family}
check(len(family) == 46, 'all 46 FX category records selected')
check(ids == baseline['parameter_ids'], 'all 204 parameter identities equal .6')
check(all(p == before[p['host_index']] for p in family), 'all 46 complete FX records exactly equal authenticated .6')
check(len({tuple(v for k, v in zip(ids, p['values']) if k != 'volume') for p in family}) == 46,
      '46 distinct non-master-volume parameter vectors')
mechanisms_path = ROOT / 'docs/preset-evidence/2026.11.7/verification/a12/mechanism-coverage.json'
mechanisms = json.loads(mechanisms_path.read_text())
eligible = {p['name']: p['eligible_mechanisms'] for p in mechanisms['programs']}
origins = ['Machine Hum', 'Wind', 'Deep Space', 'Telemetry', 'Robot Voice', 'Clockwork', 'Rain',
           'Laser Zap', 'Sci-Fi Drone', 'Space Sweep', 'Metal Scrape', 'Thunder', 'Surf', 'Bubbles',
           'Ring Mallet', 'Siren', 'UFO Landing', 'Alarm', 'Random Filter', 'Sweep Riser']
structural = []
for p in family:
    v = values[p['name']]
    changed = [k for k, a, b in zip(ids, p['values'], old[p['host_index']]['values'])
               if k != 'volume' and np.float32(a) != np.float32(b)]
    check(bool(changed), p['name'] + ': retained cumulative non-master changes versus .4')
    check(all(v[f'fx{n}_type'] == v[f'fx{n}_mix'] == 0 for n in (1, 2, 3)), p['name'] + ': rack effects and delay off')
    check(v['body1_material'] == v['body2_material'] == 0, p['name'] + ': material selectors remain neutral')
    origin = ('Ring Mallet' if p['host_index'] == 40 else 'Space Sweep' if p['host_index'] == 41
              else origins[p['host_index'] - 180] if 180 <= p['host_index'] <= 199 else p['name'])
    structural.append({'host_index': p['host_index'], 'name': p['name'], 'source_recipe': origin,
                       'description': p['description'], 'tags': p['tags'], 'full_record_equal_6': True,
                       'cumulative_4_changed_nonmaster_controls': changed,
                       'new_7_eligible_mechanisms': eligible[p['name']],
                       'ring': {k: v[k] for k in ('ring_speed', 'ring_depth', 'ring_attack', 'ring_decay', 'ring_amount')},
                       'rank_envelopes_and_noise': [{k: v[k] for k in (f'ch{n}_noise', f'ch{n}_v_a', f'ch{n}_v_d',
                           f'ch{n}_v_s', f'ch{n}_v_r', f'ch{n}_f_a', f'ch{n}_f_d', f'ch{n}_f_r')} for n in (1, 2)],
                       'body_values': {k: x for k, x in v.items() if k.startswith('body')},
                       'rack_delay_enabled': False})

source_paths = ['src/core/Engine.cpp', 'src/core/Engine.h', 'src/core/Params.h', 'src/core/FactoryPresets.cpp',
                'src/core/FactoryPresets.h', 'src/core/InstrumentBody.h', 'src/core/InstrumentBodyMaterials.h',
                'src/core/CuratedLibrary.inc', 'src/core/OriginalCircuitLibrary.inc', 'src/core/curated/Fx.inc',
                'src/core/curated/Bells.inc', 'src/core/RingModulatorCircuit.h', 'src/core/ModulationLfo.h',
                'src/tools/preset_capture.cpp', 'tests/FxRefinementTests.cpp']
source_receipts = []
for path in source_paths:
    local, frozen, prior = (ROOT / path).read_bytes(), git_blob(CURRENT, path), git_blob(BASELINE, path)
    check(local == frozen, path + ': local bytes equal frozen .7 source')
    source_receipts.append({'path': path, 'sha256': sha(local), 'matches_current_commit': local == frozen,
                            'unchanged_since_6': local == prior, 'baseline_sha256': sha(prior)})

test_path = DEVELOPMENT / 'cs80-focused.xml'
test = next(t for t in ET.parse(test_path).getroot().iter('testcase') if t.get('name') == 'cs80_fx_refinement_tests')
output = test.findtext('system-out')
check(test.find('failure') is None and '30 checks, 0 failures' in output, 'reuse current FX refinement gate: 30 checks, zero failures')
(HERE / 'reused-fx-refinement.txt').write_text(output)
bank = {}
for label in ('baseline', 'published-4', 'current'):
    path = DEVELOPMENT / (label + '-bank-metrics.csv')
    rows = [r for r in csv.DictReader(path.open()) if r['category'] == 'FX']
    check(len(rows) == 46, label + ': matched bank has all 46 FX records')
    check(all(int(r['nonfinite']) == int(r['samples_over_full_scale']) == 0 for r in rows), label + ': all FX bank captures finite and below full scale')
    bank[label] = {'input': ref(path), 'rows': rows,
                   'peak_dbfs_min': min(float(r['peak_dbfs']) for r in rows),
                   'peak_dbfs_max': max(float(r['peak_dbfs']) for r in rows)}
check(bank['current']['rows'] == bank['baseline']['rows'], 'all 46 current bank metric rows exactly equal .6 at default 48 kHz/note 60/velocity .8/pressure 0')

binaries_before = {k: ref(p) for k, p in [('current', args.current), ('baseline', args.baseline)]}
matched = json.loads((DEVELOPMENT / 'matched-published-current-bank.json').read_text())
current_hash = next(r['capture_sha256'] for r in matched['runs'] if r['label'] == 'current')
check(current_hash == binaries_before['current']['sha256'], 'current capture executable matches matched-bank receipt')

long_names = ['Laser Zap', 'Metal Scrape', 'Space Sweep', 'Sweep Riser', 'Downer', 'UFO Landing', 'Distant FX 139']
scenarios = [{'label': 'long', 'build': 'current', 'names': long_names, 'rate': 48000, 'note': 60, 'velocity': .85,
              'pressure': .5, 'hold': 10, 'tail': 16},
             {'label': 'long-repeat', 'build': 'current', 'names': ['Space Sweep'], 'rate': 48000, 'note': 60,
              'velocity': .85, 'pressure': .5, 'hold': 10, 'tail': 16}]
for build in ('current', 'baseline'):
    for velocity in (.25, .85):
        scenarios.append({'label': build + '-strike-v' + str(velocity), 'build': build,
                          'names': ['Ring Bell', 'Amber FX 151'], 'rate': 44100, 'note': 60, 'velocity': velocity,
                          'pressure': .5, 'hold': 3, 'tail': 3})
    for pressure in (0, 1):
        scenarios.append({'label': build + '-low-robot-p' + str(pressure), 'build': build,
                          'names': ['Robot Voice', 'Hollow FX 141'], 'rate': 44100, 'note': 36, 'velocity': .85,
                          'pressure': pressure, 'hold': 3, 'tail': 3})
scenarios.append({'label': 'robot-repeat', 'build': 'current', 'names': ['Robot Voice', 'Hollow FX 141'],
                  'rate': 44100, 'note': 36, 'velocity': .85, 'pressure': 1, 'hold': 3, 'tail': 3})

TMP.mkdir(parents=True, exist_ok=True)
for s in scenarios:
    out = TMP / s['label']
    executable = args.current if s['build'] == 'current' else args.baseline
    cmd = [str(executable), '--out', str(out), '--library', 'all', '--wav']
    for k in ('rate', 'note', 'velocity', 'pressure', 'hold', 'tail'):
        cmd += ['--' + k, str(s[k])]
    for name in s['names']:
        cmd += ['--preset', name]
    s['command'] = cmd
    if args.render:
        if out.exists():
            raise SystemExit('Refusing to overwrite capture directory: ' + str(out))
        print('Capture: ' + s['label'], flush=True)
        with (TMP / (s['label'] + '.log')).open('w') as log:
            subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True)


def db(a):
    return float(20 * np.log10(max(float(a), 1e-15)))


def rms(x):
    return float(np.sqrt(np.mean(np.asarray(x, dtype=np.float64)**2)))


def spectral(x, rate):
    x = np.asarray(x, dtype=np.float64)
    if x.shape[0] < 16:
        return {}
    # Stereo-average power avoids phase cancellation; the Hann window is
    # an analysis weight only and is never written back to the audio.
    spectrum = np.fft.rfft(x * np.hanning(len(x))[:, None], axis=0)
    power = np.mean(np.abs(spectrum)**2, axis=1)
    freq = np.fft.rfftfreq(len(x), 1 / rate)
    total = float(power.sum())
    return {'centroid_hz': float((freq * power).sum() / max(total, 1e-30)),
            'power_above_1500_share': float(power[freq > 1500].sum() / max(total, 1e-30))}


def window(x, rate, a, b):
    y = x[round(a * rate):round(b * rate)]
    return {'start': a, 'end': b, 'rms_dbfs': db(rms(y)), 'peak_dbfs': db(np.max(np.abs(y))), **spectral(y, rate)}


captures = []
audio = {}
for s in scenarios:
    out = TMP / s['label']
    rows = list(csv.DictReader((out / 'metrics.csv').open()))
    check([r['name'] for r in rows] == s['names'], s['label'] + ': exact requested named capture set')
    # Retain small original metrics, not duplicate the generated WAVs.
    (HERE / (s['label'] + '-metrics.csv')).write_bytes((out / 'metrics.csv').read_bytes())
    for r in rows:
        name = r['name']
        host = by_name[name]['host_index']
        filename = str(host) + '-' + ''.join(c if c.isascii() and c.isalnum() else '-' for c in name) + '.wav'
        path = out / filename
        rate, x = wavfile.read(path)
        check(rate == s['rate'] and x.dtype == np.float32 and x.shape == ((s['hold'] + s['tail']) * rate, 2),
              s['label'] + '/' + name + ': unnormalized stereo float32 WAV and exact duration')
        check(np.isfinite(x).all() and np.max(np.abs(x)) < 1, s['label'] + '/' + name + ': held/released output finite and below full scale')
        audio[s['label'], name] = x
        bounds = [(0, .05), (.05, .25), (.25, 1), (1, 2), (s['hold'] - .5, s['hold']),
                  (s['hold'], s['hold'] + .5), (s['hold'] + .5, s['hold'] + 1.5),
                  (s['hold'] + s['tail'] - 1, s['hold'] + s['tail'])]
        if s['hold'] == 10:
            bounds += [(2, 3), (6, 8), (10.05, 10.4), (12, 14)]
        captures.append({'scenario': s['label'], 'build': s['build'], 'name': name, 'host_index': host,
                         'wav': ref(path), 'metrics': r, 'windows': [window(x, rate, a, b) for a, b in bounds]})

comparisons = []
for kind, names, settings in [('strike', ['Ring Bell', 'Amber FX 151'], [.25, .85]),
                             ('low-robot', ['Robot Voice', 'Hollow FX 141'], [0, 1])]:
    for value in settings:
        suffix = kind + ('-v' if kind == 'strike' else '-p') + str(value)
        for name in names:
            a, b = audio['current-' + suffix, name], audio['baseline-' + suffix, name]
            current_power, old_power = rms(a), rms(b)
            comparisons.append({'name': name, 'scenario_suffix': suffix, 'equal_float32_samples': bool(np.array_equal(a, b)),
                                'maximum_sample_difference': float(np.max(np.abs(a.astype(np.float64) - b))),
                                'difference_rms_db_relative_baseline': db(rms(a.astype(np.float64) - b) / old_power),
                                'rms_change_db': db(current_power / old_power),
                                'window_1_2_seconds_current': window(a, 44100, 1, 2),
                                'window_1_2_seconds_baseline': window(b, 44100, 1, 2)})

touch = []
for name in ('Ring Bell', 'Amber FX 151'):
    soft, firm = audio['current-strike-v0.25', name], audio['current-strike-v0.85', name]
    difference = db(rms(firm[:round(.15 * 44100)]) / rms(soft[:round(.15 * 44100)]))
    touch.append({'name': name, 'control': 'velocity .25 -> .85; pressure fixed .5; MIDI 60',
                  'window': [0, .15], 'rms_change_db': difference})
    check(difference > 0, name + ': firm velocity raises measured onset')
for name in ('Robot Voice', 'Hollow FX 141'):
    zero, full = audio['current-low-robot-p0', name], audio['current-low-robot-p1', name]
    difference = db(rms(full[44100:88200]) / rms(zero[44100:88200]))
    touch.append({'name': name, 'control': 'pressure 0 -> 1; velocity fixed .85; MIDI 36',
                  'window': [1, 2], 'rms_change_db': difference})
    check(difference > 0, name + ': full pressure raises low sustained voice')

repeats = []
for original, repeated, names in [('long', 'long-repeat', ['Space Sweep']),
                                  ('current-low-robot-p1', 'robot-repeat', ['Robot Voice', 'Hollow FX 141'])]:
    for name in names:
        equal = bool(np.array_equal(audio[original, name], audio[repeated, name]))
        check(equal, name + ': independent fresh-process repetition exactly equals original float32 output')
        repeats.append({'name': name, 'original_scenario': original, 'repeat_scenario': repeated, 'equal_float32_samples': equal})

binaries_after = {k: ref(p) for k, p in [('current', args.current), ('baseline', args.baseline)]}
check(binaries_before == binaries_after, 'capture executable bytes unchanged during work')
check(all(sha((ROOT / r['path']).read_bytes()) == r['sha256'] for r in source_receipts), 'reviewed source bytes unchanged during work')
bank_receipts = {label: {k: v for k, v in data.items() if k != 'rows'} | {'row_count': len(data['rows'])}
                 for label, data in bank.items()}
bank_receipts['current']['all_metric_rows_equal_6'] = bank['current']['rows'] == bank['baseline']['rows']
result = {'source': CURRENT, 'baseline': BASELINE,
          'scope': 'All-family structural audit; reused full-bank short-note metrics and current 30-check gate; 11 actual programs in supplemental captures. No subjective listening, native plugin qualification or field-recording match claim.',
          'inputs': {'inventory': ref(inventory_path), 'mechanisms': ref(mechanisms_path), 'focused_xml': ref(test_path),
                     'prior_fx_review': ref(ROOT / 'docs/preset-evidence/2026.11.6/reviews/a29.md'),
                     'script': ref(Path(__file__))},
          'source_receipts': source_receipts, 'executables': binaries_before,
          'family_count': len(family), 'original_count': sum(p['host_index'] < 222 for p in family),
          'curated_count': sum(p['host_index'] >= 222 for p in family), 'structural': structural,
          'reused_fx_test_output': output, 'matched_bank': bank_receipts, 'scenarios': scenarios, 'captures': captures,
          'current_baseline_comparisons': comparisons, 'touch_controls': touch, 'reproducibility': repeats,
          'checks': checks, 'pass': all(c['pass'] for c in checks)}
(HERE / 'review.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({'pass': result['pass'], 'checks': len(checks), 'failed': [c for c in checks if not c['pass']],
                  'capture_count': len(captures), 'unique_programs': len({c['name'] for c in captures}),
                  'max_capture_peak_dbfs': max(float(c['metrics']['peak_dbfs']) for c in captures),
                  'touch_controls': touch, 'comparisons': comparisons}, indent=2))
raise SystemExit(0 if result['pass'] else 1)
