#!/usr/bin/env python3
"""Review A24's explicit capture files; never glob unrelated temporary PCM."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import xml.etree.ElementTree as ET

import numpy as np

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', type=Path, default=HERE.parents[4])
parser.add_argument('--tmp', type=Path, default=Path('/tmp/cs80-a24-1cee5a'))
parser.add_argument('--out', type=Path, default=HERE / 'review.json')
args = parser.parse_args()
ROOT, TMP = args.repo, args.tmp
CURRENT = 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201'
BASELINE = '18711f93a39aa49f3d6f9a41784be456224facbe'
checks = []


def check(condition, label):
    checks.append({'passed': bool(condition), 'label': label})


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git_blob(commit, path):
    return subprocess.check_output(['git', '-C', str(ROOT), 'show', commit + ':' + path])


def file_ref(path):
    p = Path(path)
    return {'path': str(p), 'bytes': p.stat().st_size, 'sha256': sha(p.read_bytes())}


inventory_path = ROOT / 'docs/preset-evidence/2026.11.7/verification/a12/inventory.json'
inventory = json.loads(inventory_path.read_bytes())
baseline_bytes = git_blob(BASELINE, 'docs/preset-evidence/2026.11.6/verification/a15/inventory.json')
baseline = json.loads(baseline_bytes)
published_bytes = git_blob(BASELINE, 'docs/preset-evidence/2026.11.5/baseline/inventory.json')
published = json.loads(published_bytes)
family = [p for p in inventory['programs'] if p['category'] == 'Sequences']
before = {p['host_index']: p for p in baseline['programs']}
original = {p['host_index']: p for p in published['programs']}
by_name = {p['name']: p for p in family}
values = {p['name']: dict(zip(inventory['parameter_ids'], p['values'])) for p in family}
check(len(family) == 27, 'all 27 Sequences records selected by category, including Modern.inc')
check(inventory['parameter_ids'] == baseline['parameter_ids'], 'all 204 parameter identities unchanged')
check(all(p == before[p['host_index']] for p in family), 'all 27 full program records equal authenticated .6 records')
check(len({tuple(v for k, v in zip(inventory['parameter_ids'], p['values']) if k != 'volume') for p in family}) == 27,
      'all 27 non-master-volume parameter vectors are distinct')
mechanisms = json.loads((ROOT / 'docs/preset-evidence/2026.11.7/verification/a12/mechanism-coverage.json').read_text())
eligible = {p['name']: p['eligible_mechanisms'] for p in mechanisms['programs']}
source_recipes = ['Arp Poly', 'Arp Bells', 'Arp Bass', 'Random Arp', 'Arp Poly', 'Arp Bells', 'Arp Bells', 'Arp Bass',
                  'Arp Poly', 'Arp Bells', 'Arp Poly', 'Arp Bass', 'Random Arp', 'Arp Bells', 'Arp Bass', 'Arp Poly',
                  'Random Arp', 'Arp Poly', 'Arp Bells', 'Arp Bass', 'Arp Poly', 'Random Arp']
mode_names = ['ascending', 'descending', 'up/down', 'random', 'played order']
structural = []
for p in family:
    v = values[p['name']]
    changed = [k for k, a, b in zip(inventory['parameter_ids'], p['values'], original[p['host_index']]['values'])
               if k != 'volume' and a != b]
    check(bool(changed), p['name'] + ': retained cumulative non-master change versus published .4')
    check(v['arp_on'] == 1 and 2 <= v['arp_rate'] <= 9 and v['arp_mode'] in range(5),
          p['name'] + ': valid enabled Hz arpeggiator')
    body = v['body1_mix'] > 0 or v['body2_mix'] > 0
    check((body and v['body1_model'] == v['body2_model'] == 16 and v['ring_amount'] > 0
           and v['ring_depth'] == 0) or (not body and v['ring_amount'] == 0),
          p['name'] + ': intentional electronic or fixed-carrier bell topology')
    source = 'Arp Poly' if p['name'] == 'Arp Motion' else source_recipes[p['host_index'] - 200] if 200 <= p['host_index'] <= 221 else p['name']
    structural.append({
        'host_index': p['host_index'], 'name': p['name'], 'source_recipe': source,
        'mode': mode_names[int(v['arp_mode'])], 'arp_rate_hz': v['arp_rate'], 'octaves': v['arp_octaves'],
        'gate_ms': 1000 * (.45 if v['arp_mode'] == 3 else .58) / v['arp_rate'],
        'ring': {k: v[k] for k in ['ring_speed', 'ring_depth', 'ring_attack', 'ring_decay', 'ring_amount']},
        'lfos': [{k: v[k] for k in [f'lfo{n}_rate', f'lfo{n}_depth', f'lfo{n}_dest', f'lfo{n}_wave']} for n in range(1, 4)],
        'rank_envelopes': [{k: v[k] for k in [f'ch{n}_v_a', f'ch{n}_v_d', f'ch{n}_v_s', f'ch{n}_v_r',
                                            f'ch{n}_f_a', f'ch{n}_f_d', f'ch{n}_f_r', f'ch{n}_feet']} for n in [1, 2]],
        'effect_values': {k: v[k] for k in v if k.startswith('fx')},
        'body_values': {k: v[k] for k in v if k.startswith('body')},
        'new_7_eligible_mechanisms': eligible[p['name']],
        'cumulative_4_changed_nonmaster_controls': changed,
        'full_record_equal_6': p == before[p['host_index']],
    })

source_paths = ['src/core/Engine.cpp', 'src/core/Engine.h', 'src/core/Params.h', 'src/core/FactoryPresets.cpp',
                'src/core/FactoryPresets.h', 'src/core/InstrumentBody.h', 'src/core/OriginalCircuitLibrary.inc',
                'src/core/curated/Modern.inc', 'src/core/RingModulatorCircuit.h', 'src/core/ModulationLfo.h',
                'src/tools/preset_capture.cpp', 'tests/OriginalArticulationTests.cpp', 'tests/ModernPresetRefinementTests.cpp']
source_receipts = []
for path in source_paths:
    local, frozen, old = (ROOT / path).read_bytes(), git_blob(CURRENT, path), git_blob(BASELINE, path)
    check(local == frozen, path + ': local bytes equal frozen .7 source')
    source_receipts.append({'path': path, 'sha256': sha(local), 'matches_current_commit': local == frozen,
                            'unchanged_since_6': local == old, 'baseline_sha256': sha(old)})

raw = {}
events = {}
programs = {}
pcm = {}
pcm_manifest = []
for build in ['current', 'baseline']:
    raw[build] = (TMP / (build + '.jsonl')).read_bytes()
    rows = [json.loads(line) for line in raw[build].splitlines()]
    programs[build] = [r for r in rows if r['type'] == 'program']
    event_groups = collections.defaultdict(list)
    for r in rows:
        if r['type'] == 'event': event_groups[r['name'], r['host_rate'], r['scenario']].append(r)
    events[build] = event_groups
    check(len(rows) == len(programs[build]) + sum(len(r) for r in event_groups.values()), build + ': only recognized telemetry records')
    for r in programs[build]:
        path = TMP / build / r['pcm_file']
        blob = path.read_bytes()
        key = build, r['name'], r['host_rate'], r['scenario']
        pcm[key] = np.frombuffer(blob, dtype='<f4').reshape(-1, 2)
        check(len(blob) == r['frames'] * 8, str(key) + ': exact PCM size')
        pcm_manifest.append({'build': build, 'name': r['name'], 'scenario': r['scenario'], 'host_rate': r['host_rate'], **file_ref(path)})

normal48 = [p for p in programs['current'] if p['scenario'] == 'normal' and p['host_rate'] == 48000]
check(len(normal48) == 8 and {p['name'] for p in normal48} == {
    'Arp Motion', 'Distant Sequences 159', 'Slow Sequences 166', 'Neon Sequences 174', 'Arp Poly', 'Arp Bells', 'Arp Bass', 'Random Arp'},
    'fresh normal 48 kHz performances cover the declared eight presets exactly')
check(len(programs['current']) == 18 and len(programs['baseline']) == 2, 'exact declared current/control/baseline render counts')
keys = [(55, .65), (48, .25), (52, .95)]
phrase_metrics = []
for build in ['current', 'baseline']:
    for p in programs[build]:
        label = f"{build} {p['name']} {p['host_rate']} {p['scenario']}: "
        rate = p['host_rate']
        v = values[p['name']]
        ev = events[build][p['name'], rate, p['scenario']]
        onsets = [x for x in ev if x['note'] >= 0]
        periods = [(b['frame'] - a['frame']) / rate for a, b in zip(onsets, onsets[1:])]
        widths = [(b['frame'] - a['frame']) / rate for a, b in zip(ev, ev[1:])
                  if a['note'] >= 0 and b['note'] == -1 and b['frame'] < 6 * rate]
        ordered = keys if p['mode'] == 4 else sorted(keys)
        sequence = [(n + 12 * octave, velocity) for octave in range(p['octaves']) for n, velocity in ordered]
        if p['mode'] == 1: sequence.reverse()
        if p['mode'] == 2: sequence += sequence[-2:0:-1]
        admitted = dict(sequence)
        period = 1 / p['rate_hz']
        gate = period * (.45 if p['mode'] == 3 else .58)
        check(p['index'] == by_name[p['name']]['host_index'] and p['rate_hz'] == v['arp_rate']
              and p['mode'] == v['arp_mode'] and p['octaves'] == v['arp_octaves'], label + 'actual existing patch and authored scheduling settings')
        check(p['frames'] == 10 * rate and p['events'] == len(ev), label + 'complete ten-second render accounted')
        check(p['nonfinite'] == p['clipped'] == 0 and p['peak_dbfs'] < 0, label + 'finite complete output below full scale')
        check(p['max_held'] == 1 and p['max_active'] <= 8, label + 'one held generated note and bounded voices')
        check(p['tail_held_observations'] == p['final_active'] == 0, label + 'ordinary final source release stops and retires voices')
        check(p['held_rms_dbfs'] > -60 and p['final_tail_rms_dbfs'] < p['held_rms_dbfs'] - 40, label + 'held energy and final-tail reduction')
        check(onsets[0]['frame'] <= 64 and len(onsets) >= 2 * len(sequence), label + 'immediate first observed onset and at least two sequence lengths')
        spacing_error = max(abs(x - period) for x in periods)
        gate_error = max(abs(x - gate) for x in widths)
        check(spacing_error <= 128 / rate, label + 'each onset spacing within two observation blocks of authored Hz rate')
        check(gate_error <= 128 / rate, label + 'each complete held gate within two observation blocks of authored duty')
        check(all(x['note'] in admitted and x['channel'] == 1 and abs(x['velocity'] - admitted[x['note']]) < 1e-7 for x in onsets),
              label + 'allowed pitch, source channel and source velocity retained')
        check(len({x['note'] for x in onsets}) >= 2 if p['mode'] == 3 else
              all(x['note'] == sequence[i % len(sequence)][0] for i, x in enumerate(onsets)), label + 'declared random membership or full ordered pattern')
        phrase_metrics.append({'build': build, **p, 'onsets': len(onsets), 'complete_gates': len(widths),
                               'expected_order': None if p['mode'] == 3 else [x[0] for x in sequence],
                               'max_spacing_error_ms': spacing_error * 1000, 'max_gate_error_ms': gate_error * 1000,
                               'mean_rate_hz': 1 / statistics.mean(periods),
                               'final_tail_below_held_db': p['held_rms_dbfs'] - p['final_tail_rms_dbfs']})


def trace(build, name, rate, scenario):
    return [{k: v for k, v in x.items() if k != 'scenario'} for x in events[build][name, rate, scenario]]


def difference(a, b):
    a, b = a.astype(np.float64), b.astype(np.float64)
    rms = np.sqrt(np.mean((a - b) ** 2))
    return {'rms_difference': float(rms), 'rms_difference_dbfs': float(20 * np.log10(max(rms, 1e-15))),
            'relative_rms_difference': float(rms / np.sqrt(np.mean(a * a))),
            'max_absolute_difference': float(np.max(np.abs(a - b)))}


tempo = []
for name in ['Arp Poly', 'Arp Bells', 'Arp Bass', 'Random Arp']:
    a, b = pcm['current', name, 48000, 'normal'], pcm['current', name, 48000, 'tempo_jump']
    same_trace = trace('current', name, 48000, 'normal') == trace('current', name, 48000, 'tempo_jump')
    same_pcm = a.tobytes() == b.tobytes()
    check(same_trace and same_pcm, name + ': 60-to-180 BPM control preserves exact telemetry and complete PCM bytes')
    tempo.append({'name': name, 'same_trace': same_trace, 'exact_pcm_byte_equality': same_pcm, 'frames_compared': len(a)})

motion = []
for name, scenario in [('Arp Poly', 'lfo_off'), ('Arp Bass', 'lfo_off'), ('Random Arp', 'lfo_off'), ('Arp Bells', 'ring_off')]:
    a, b = pcm['current', name, 48000, 'normal'], pcm['current', name, 48000, scenario]
    same_trace = trace('current', name, 48000, 'normal') == trace('current', name, 48000, scenario)
    d = difference(a[48000:6 * 48000], b[48000:6 * 48000])
    check(same_trace and d['relative_rms_difference'] > 1e-5, name + ': named modulation route changes audio with unchanged note trace')
    log_ratios = []
    for start in range(48000, 6 * 48000 - 4800 + 1, 4800):
        def brightness(x):
            block = x[start:start + 4800].astype(np.float64)
            return np.sqrt(np.sum(np.diff(block, axis=0) ** 2) / max(np.sum(block * block), 1e-24))
        log_ratios.append(float(np.log(brightness(a) / brightness(b))))
    motion.append({'name': name, 'control': scenario, 'same_trace': same_trace, 'window_seconds': [1, 6], **d,
                   'normalized_first_difference_ratio_span': float(np.exp(max(log_ratios) - min(log_ratios))),
                   'log_spectral_ratio_standard_deviation': statistics.pstdev(log_ratios)})

rate_pairs = []
for name in ['Arp Bells', 'Slow Sequences 166']:
    a, b = pcm['current', name, 44100, 'normal'], pcm['baseline', name, 44100, 'normal']
    same_trace = trace('current', name, 44100, 'normal') == trace('baseline', name, 44100, 'normal')
    d = difference(a, b)
    check(same_trace and d['rms_difference'] > 0, name + ': .7 changes 88.2 kHz internal body audio while preserving complete scheduling trace')
    rate_pairs.append({'name': name, 'host_rate': 44100, 'internal_rate': 88200, 'same_trace': same_trace, **d})

articulation = []
for p in normal48:
    name = p['name']
    a = pcm['current', name, 48000, 'normal'].astype(np.float64)
    ev = events['current'][name, 48000, 'normal']
    ratios = []
    for onset, release, following in zip(ev, ev[1:], ev[2:]):
        if onset['note'] < 0 or release['note'] != -1 or following['note'] < 0:
            continue
        on, off, next_on = onset['frame'], release['frame'], following['frame']
        if on < 48000 or next_on >= 6 * 48000:
            continue
        early = a[int(on + .15 * (off - on)):int(on + .45 * (off - on))]
        gap = a[int(off + .65 * (next_on - off)):int(off + .90 * (next_on - off))]
        ratios.append(float(10 * np.log10(max(np.mean(gap * gap), 1e-30) / max(np.mean(early * early), 1e-30))))
    check(len(ratios) >= 10 and all(np.isfinite(ratios)), name + ': complete-patch gate/gap articulation windows measured')
    articulation.append({'name': name, 'complete_pairs_after_first_second': len(ratios),
                          'median_late_gap_to_early_gate_db': statistics.median(ratios),
                          'minimum_db': min(ratios), 'maximum_db': max(ratios)})

prior_path = 'docs/preset-evidence/2026.11.6/verification/a28/sequence-review.json'
prior_bytes = git_blob(BASELINE, prior_path)
prior = json.loads(prior_bytes)
check((ROOT / prior_path).read_bytes() == prior_bytes, 'retained .6 family review equals committed final .6 evidence')
check(prior['counts']['programs'] == 27 and prior['counts']['checks'] == 420 and not prior['failures'],
      'retained .6 complete-family evidence passes its 420 checks')
focused = ROOT / 'docs/preset-evidence/2026.11.7/development'
focused_receipt = json.loads((focused / 'focused-ctest-receipt.json').read_text())
for name, expected in focused_receipt['files'].items():
    check(sha((focused / name).read_bytes()) == expected, 'reused focused test evidence digest: ' + name)
suite_rows = []
for case in ET.parse(focused / 'cs80-focused.xml').iter('testcase'):
    if case.attrib['name'] in ['cs80_original_articulation_tests', 'cs80_modern_preset_refinement_tests']:
        check(case.find('failure') is None and case.attrib['status'] == 'run', 'reused ' + case.attrib['name'] + ' passed')
        suite_rows.append({'name': case.attrib['name'], 'seconds': float(case.attrib['time']),
                           'stdout': case.findtext('system-out'), 'reused_from': str(focused / 'cs80-focused.xml')})

normal_metrics = [r for r in phrase_metrics if r['build'] == 'current' and r['scenario'] == 'normal' and r['host_rate'] == 48000]
failed = [c for c in checks if not c['passed']]
report = {
    'source_current': CURRENT, 'source_baseline': BASELINE, 'revision': '2026.11.7',
    'scope': 'Complete structural family audit; eight current representative complete-preset phrases plus named controls; prior .6 full-family phrases reused explicitly.',
    'counts': {'structural_programs': len(family), 'exact_204_value_comparisons': len(family) * len(inventory['parameter_ids']),
               'fresh_unique_presets': len(normal48), 'current_renders': len(programs['current']), 'baseline_renders': len(programs['baseline']),
               'fresh_stereo_frames': sum(p['frames'] for build in programs for p in programs[build]),
               'fresh_seconds': sum(p['frames'] / p['host_rate'] for build in programs for p in programs[build]),
               'main_48k_generated_onsets': sum(p['onsets'] for p in normal_metrics),
               'main_48k_complete_gates': sum(p['complete_gates'] for p in normal_metrics),
               'body_noise_eligible_programs': sum('noise_rate_normalization' in eligible[p['name']] for p in family),
               'checks': len(checks), 'failures': len(failed)},
    'method': {'source_notes_velocities': keys, 'channel': 1, 'pressure': 0, 'hold_seconds': 6, 'ordinary_note_off_tail_seconds': 4,
               'block_frames': 64, 'timing_tolerance_blocks': 2, 'normal_parameter_overrides': [],
               'control_overrides': {'tempo_jump': 'transport 60 to 180 BPM at exactly 3 seconds',
                                     'lfo_off': 'lfo1_depth = 0 only', 'ring_off': 'ring_amount = 0 only'},
               'motion_metric': 'Stereo RMS difference and gain-normalized first-difference brightness ratios, 50 adjacent 100 ms windows between 1 and 6 seconds.',
               'articulation_metric': 'For complete onset/off/next-on triples after one second, compare stereo RMS in 65-90% of the released gap with RMS in 15-45% of the preceding held gate. Boundaries use public callback-end telemetry; tails are preserved.',
               'capture_cli_limit': 'preset_capture holds a single source MIDI note; arp remains enabled, but generic summary metrics alone cannot prove multi-key order, scheduled gate timing, or tempo behavior.'},
    'inventory': file_ref(inventory_path), 'baseline_inventory_sha256': sha(baseline_bytes),
    'published_4_inventory_sha256': sha(published_bytes), 'source_receipts': source_receipts,
    'core_archives': [file_ref('/tmp/cs80-build-1cee5a/libcs80_core.a'), file_ref('/tmp/cs80-reference-1cee5a/libcs80_core.a')],
    'build_setup': json.loads((TMP / 'build-setup.json').read_text()),
    'probe_source': file_ref(HERE / 'sequence-probe.cpp'),
    'probe_executables': [file_ref(TMP / 'probe-current'), file_ref(TMP / 'probe-baseline')],
    'raw_telemetry': {k: {'file': k + '.jsonl', 'sha256': sha(v)} for k, v in raw.items()},
    'pcm_manifest': pcm_manifest, 'all_27_structural_rows': structural,
    'fresh_phrase_metrics': phrase_metrics, 'tempo_controls': tempo, 'motion_controls': motion, 'baseline_rate_pairs': rate_pairs,
    'actual_patch_articulation': articulation,
    'reused_6_review': {'path': prior_path, 'sha256': sha(prior_bytes), 'counts': prior['counts'],
                       'source': prior['source'], 'not_fresh_7_renders': True},
    'reused_current_focused_tests': suite_rows,
    'fresh_lifecycle_tests': json.loads((HERE / 'lifecycle-tests.json').read_text()),
    'checks': checks, 'failures': failed,
    'limits': ['Observed gate changes use callback-end telemetry; acoustic onset timing is not measured sample-exactly.',
               'Twenty-seven structural records are covered, but only the declared eight distinct presets were freshly rendered by this assignment.',
               'The .6 full-family 31-performance result remains historical evidence and is not relabeled as a .7 rerun.',
               'Motion and ring controls establish a numerical contribution, not listener preference or a perceived-improvement rating.',
               'Bell tails and low fixed-carrier feedthrough need not be zero after generated-note ownership ends.',
               'No native-host or archive qualification is made.']
}
args.out.write_text(json.dumps(report, indent=2) + '\n')
for build, data in raw.items():
    (args.out.parent / (build + '.jsonl')).write_bytes(data)
print(json.dumps({'counts': report['counts'], 'motion_controls': motion, 'rate_pairs': rate_pairs,
                  'max_main_peak_dbfs': max(x['peak_dbfs'] for x in normal_metrics),
                  'max_main_spacing_error_ms': max(x['max_spacing_error_ms'] for x in normal_metrics),
                  'max_main_gate_error_ms': max(x['max_gate_error_ms'] for x in normal_metrics),
                  'minimum_main_tail_drop_db': min(x['final_tail_below_held_db'] for x in normal_metrics),
                  'failures': failed}, indent=2))
raise SystemExit(bool(failed))
