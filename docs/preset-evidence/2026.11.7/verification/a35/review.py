#!/usr/bin/env python3
"""A35 inventory synthesis and one optional complete-vocal MIDI gesture pair.

This is retained review evidence, not a new production regression test or gate.
Run --render to reconstruct the two small scratch captures with matched headers.
"""
import argparse
from collections import Counter
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import subprocess
import tarfile

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[4]
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--tmp', type=Path, default=Path('/tmp/cs80-a35-1cee5a'))
ap.add_argument('--current-core', type=Path, default=Path('/tmp/cs80-build-1cee5a/libcs80_core.a'))
ap.add_argument('--baseline-core', type=Path, default=Path('/tmp/cs80-reference-1cee5a/libcs80_core.a'))
ap.add_argument('--render', action='store_true')
args = ap.parse_args()
CURRENT = 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201'
BASELINE = '18711f93a39aa49f3d6f9a41784be456224facbe'
EVIDENCE = ROOT / 'docs/preset-evidence/2026.11.7'
RATE = 48000


def sha(data):
    return hashlib.sha256(data).hexdigest()


def ref(path):
    path = Path(path)
    return {'path': str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path),
            'sha256': sha(path.read_bytes()), 'bytes': path.stat().st_size}


def git(*words):
    return subprocess.check_output(['git', '-C', str(ROOT), *words])


def db(value):
    return 20 * math.log10(max(float(value), 1e-15))


def rms(audio):
    return float(np.sqrt(np.mean(np.square(audio.astype(np.float64)))))


def spectral_share(audio):
    # Same broad 2-4 kHz interpretation as A21; no vowel recognition score.
    n = 8192
    power = np.zeros(n // 2 + 1)
    for at in range(0, len(audio) - n + 1, n // 2):
        values = np.fft.rfft(audio[at:at+n] * np.hanning(n)[:, None], axis=0)
        power += np.square(np.abs(values)).sum(axis=1)
    hz = np.fft.rfftfreq(n, 1 / RATE)
    return float(power[(hz >= 2000) & (hz < 4000)].sum() / power[1:].sum())


inventory_path = EVIDENCE / 'verification/a12/inventory.json'
raw = inventory_path.read_bytes()
inventory = json.loads(raw)
baseline_raw = git('show', BASELINE + ':docs/preset-evidence/2026.11.6/verification/a15/inventory.json')
baseline = json.loads(baseline_raw)
ids = inventory['parameter_ids']
assert len(ids) == 204 and ids == baseline['parameter_ids']
old = {p['host_index']: p for p in baseline['programs']}
selected = []
for p in inventory['programs']:
    v = dict(zip(ids, p['values']))
    if any(v[f'body{r}_model'] == 15 and v[f'body{r}_variant'] in (3, 4)
           and v[f'body{r}_mix'] > 0 for r in (1, 2)):
        selected.append((p, v))
assert [p['host_index'] for p, _ in selected] == [32, 95, 98, 184, 284, 285, 487]
assert all(p == old[p['host_index']] for p, _ in selected)
assert len({tuple(x for k, x in zip(ids, p['values']) if k != 'volume') for p, _ in selected}) == 7

bank = {}
for label in ('current', 'baseline'):
    with (EVIDENCE / f'development/{label}-bank-metrics.csv').open() as f:
        bank[label] = {r['name']: r for r in csv.DictReader(f)}

programs = []
vowel_ranks = Counter()
vowel_programs = Counter()
for p, v in selected:
    ranks = []
    for r in (1, 2):
        body = {k: v[f'body{r}_{k}'] for k in ('model', 'variant', 'mix', 'size', 'brightness', 'decay', 'material')}
        vocal = body['model'] == 15 and body['variant'] in (3, 4) and body['mix'] > 0
        balance = 1 - v['mix'] if r == 1 else v['mix']
        active = balance > 0 and any(v[f'ch{r}_{k}'] > 0 for k in ('v_level', 'init_level', 'after_level'))
        assert active
        feet = {0: 16, 1: 8}[round(v[f'ch{r}_feet'])]
        nominal = []
        if vocal:
            vowel = {3: 'Ah', 4: 'Oo'}[round(body['variant'])]
            vowel_ranks[vowel] += 1
            for note in (36, 48, 60, 72):
                hz = v['tuning_ref'] * 2 ** ((note + v['master_tune'] + (v['detune']/100 if r == 2 else 0) - 69) / 12) * 8 / feet
                budget = min(48, max(24, 4400/(body['size'] * hz)))
                nominal.append({'played_midi': note, 'nominal_captured_body_hz': hz,
                                'budget': budget, 'mode_count': math.ceil(budget),
                                'last_mode_coverage': budget + 1 - math.ceil(budget),
                                'old_mode_count': 24,
                                'last_mode_frequency_hz': math.ceil(budget) * hz,
                                'coverage_target_hz': 4400/body['size'],
                                'ceiling_limits_target': budget == 48 and 48*hz < 4400/body['size']})
        else:
            vowel = None
        ranks.append({'rank': r, 'active': active, 'balance_weight': balance, 'vocal': vocal,
                      'vowel': vowel, 'feet': feet, 'body': body,
                      'controls': {k: v[f'ch{r}_{k}'] for k in ('saw','pulse','sine','noise','hpf_cutoff','lpf_cutoff',
                          'v_a','v_d','v_s','v_r','v_level','init_brill','init_level','after_brill','after_level')},
                      'nominal_budgets': nominal})
    vowels = {r['vowel'] for r in ranks if r['vocal']}
    for vowel in vowels: vowel_programs[vowel] += 1
    current_row, old_row = bank['current'][p['name']], bank['baseline'][p['name']]
    assert int(current_row['nonfinite']) == int(current_row['samples_over_full_scale']) == 0
    programs.append({'host_index': p['host_index'], 'name': p['name'], 'category': p['category'],
                     'origin': 'original address' if p['host_index'] < 222 else 'curated',
                     'description': p['description'], 'tags': p['tags'],
                     'complete_record_equals_6': True,
                     'parameter_binary32_sha256': sha(struct.pack('<204f', *p['values'])),
                     'ranks': ranks,
                     'global_controls': {k: v[k] for k in ('mix','detune','tuning_ref','master_tune','chorus_on','chorus_depth',
                         'modwheel_sub','modwheel_vcf','sub_vco','sub_vcf','sub_vca','sub_at_vco','sub_at_vcf','sub_at_speed',
                         'pitch_bend_range','mpe_on','ribbon','ribbon_cc_enable','portamento','init_bend','drift','voice_dispersion',
                         'analog_noise','ring_speed','ring_depth','ring_amount','fx1_type','fx2_type','fx3_type')},
                     'nonzero_mod_matrix_amounts': {k: v[k] for k in ids if k.startswith('mod') and k.endswith('_amount') and v[k]},
                     'nonzero_lfo_routes': [{k: v[f'lfo{n}_{k}'] for k in ('rate','depth','dest')} for n in (1,2,3) if v[f'lfo{n}_depth']],
                     'matched_bank': {'conditions': '48 kHz, played MIDI60, velocity .8, pressure0, 3-second hold, 1-second tail',
                         'current_row': current_row, 'baseline_row': old_row,
                         'metric_rows_exactly_equal': current_row == old_row,
                         'rms_delta_db': float(current_row['rms_dbfs']) - float(old_row['rms_dbfs']),
                         'peak_delta_db': float(current_row['peak_dbfs']) - float(old_row['peak_dbfs'])}})
assert vowel_ranks == {'Ah': 6, 'Oo': 6} and vowel_programs == {'Ah': 4, 'Oo': 3}

source_paths = ['src/core/InstrumentBody.h','src/core/InstrumentBodyVariants.h','src/core/InstrumentBodyFilter.h',
                'src/core/Engine.cpp','src/core/Engine.h','src/core/FactoryPresets.cpp','src/core/FactoryPresets.h',
                'src/core/Params.h','src/core/CuratedLibrary.inc','src/core/OriginalCircuitLibrary.inc',
                'src/core/curated/Pads.inc','src/core/curated/Fx.inc']
source_receipts = []
for path in source_paths:
    local = (ROOT/path).read_bytes()
    assert local == git('show', CURRENT+':'+path), path
    source_receipts.append(ref(ROOT/path))
assert source_receipts[0]['sha256'] == 'd825961c532e7bd5d27d3e19cf3bd2233736fa9c8072822cbb6cb3c0b07d40dc'

reused_paths = ['verification/a07/receipt.json','verification/a07/current.log','verification/a07/vocal-regression.log',
                'verification/a21/binding.json','verification/a21/structural-coverage.json','verification/a21/rendered-measurements.json',
                'verification/a25/review.json','verification/a25/reused-fx-refinement.txt',
                'development/matched-published-current-bank.json','development/current-bank-metrics.csv',
                'development/baseline-bank-metrics.csv','development/focused-ctest-receipt.json','verification/a12/mechanism-coverage.json']
reused = [ref(EVIDENCE/path) for path in reused_paths]
binding = json.loads((EVIDENCE/'verification/a21/binding.json').read_text())
assert sha(args.current_core.read_bytes()) == binding['cores']['current']
assert sha(args.baseline_core.read_bytes()) == binding['cores']['baseline']

args.tmp.mkdir(parents=True, exist_ok=True)
commands = []
old_headers = args.tmp/'baseline-src'
if args.render:
    old_headers.mkdir(exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(git('archive', BASELINE, 'src'))) as archive:
        archive.extractall(old_headers, filter='data')
for label, core, headers in [('current',args.current_core,ROOT/'src'),('baseline',args.baseline_core,old_headers/'src')]:
    binary = args.tmp/('gesture-'+label)
    command = ['c++','-O2','-std=c++20','-I',str(headers),str(HERE/'vocal_gesture.cpp'),str(core),'-o',str(binary)]
    capture = [str(binary),str(args.tmp/(label+'.f32'))]
    commands.extend([command,capture])
    if args.render:
        subprocess.run(command, check=True)
        with (HERE/(label+'-capture.json')).open('w') as f:
            subprocess.run(capture,check=True,stdout=f)

audio = {label: np.fromfile(args.tmp/(label+'.f32'),dtype='<f4').reshape(-1,2).astype(np.float64)
         for label in ('current','baseline')}
logs = {label: json.loads((HERE/(label+'-capture.json')).read_text()) for label in audio}
for label in audio:
    assert logs[label]['host_index'] == 95 and logs[label]['oversampling'] == 2
    assert len(audio[label]) == logs[label]['frames'] == 13*RATE
    assert np.isfinite(audio[label]).all() and (np.abs(audio[label]) <= 1).all()
    assert logs[label]['active_voices_at_end'] == 0

windows = [('before_gesture',.5,.95),('pressure_settled',1.6,1.95),('wheel_settled',2.55,2.95),
           ('up_two_semitones',3.55,3.95),('down_two_semitones',4.9,5.4),('returned_to_center',6.3,6.8),
           ('last_half_second',12.5,13)]
measurements = {}
for label, values in audio.items():
    measurements[label] = {'pcm': ref(args.tmp/(label+'.f32')),'binary': ref(args.tmp/('gesture-'+label)),
                           'peak_dbfs': db(np.max(np.abs(values))), 'rms_dbfs': db(rms(values)),
                           'nonfinite_samples': int(np.count_nonzero(~np.isfinite(values))),
                           'samples_over_full_scale': int(np.count_nonzero(np.abs(values)>1)),
                           'maximum_adjacent_sample_step': float(np.max(np.abs(np.diff(values,axis=0)))),
                           'telemetry':logs[label],'windows':[]}
    for name,start,end in windows:
        part=values[round(start*RATE):round(end*RATE)]
        measurements[label]['windows'].append({'name':name,'seconds':[start,end],'rms_dbfs':db(rms(part)),
                                               'peak_dbfs':db(np.max(np.abs(part))),
                                               'power_2k_4k_share': spectral_share(part)})
comparison=[]
for name,start,end in windows:
    a=audio['current'][round(start*RATE):round(end*RATE)]
    b=audio['baseline'][round(start*RATE):round(end*RATE)]
    comparison.append({'window':name,'rms_change_db':db(rms(a)/rms(b)),
                       'difference_rms_db_relative_baseline':db(rms(a-b)/rms(b)),
                       'upper_2k_4k_share_change_db':10*math.log10(max(spectral_share(a),1e-30)/max(spectral_share(b),1e-30))})

result={'scope':'All seven existing vocal recipes; inventory/source review, reused A07/A21/A25 and bank evidence, one ordinary complete-preset gesture pair. No listening qualification.',
        'source':CURRENT,'baseline':BASELINE,'head_at_review':git('rev-parse','HEAD').decode().strip(),
        'inventory':ref(inventory_path),'baseline_inventory_sha256':sha(baseline_raw),
        'summary':{'program_count':7,'original_address_count':4,'curated_count':3,'active_rank_count':14,'active_vocal_rank_count':12,
                   'category_program_counts':dict(Counter(p['category'] for p,_ in selected)),
                   'vowel_program_counts':dict(vowel_programs),'vowel_rank_counts':dict(vowel_ranks),
                   'unchanged_parameter_values':7*204,'all_complete_records_equal_6':True,
                   'default_bank_equal_metric_rows':4,'default_bank_changed_metric_rows':3},
        'programs':programs,'source_receipts':source_receipts,'reused_evidence':reused,
        'gesture':{'reason':'Named-preset tests and A21/A25 captures do not exercise live pitch-bend/wheel changes through an extended choir bank; A07 bend qualification is component-only.',
                   'preset':'Golden Pads 52','no_preset_overrides':True,'render_count':2,'generated_seconds':26,
                   'events':'MIDI48 velocity .85 at 0; channel pressure 0->.8 at 1-1.5s; CC1 0->1 at 2-2.5s; pitch-wheel 0->+1 at 3-3.5s, +1->-1 at 4-4.5s, -1->0 at 5.5-6s; note-off at7s, end13s.',
                   'event_period_seconds':.01,'maximum_render_block_frames':113,'saved_bend_range_semitones':2,
                   'native_sample_rate':RATE,'native_internal_rate':96000,'captured_rank_mode_counts':[33,36],
                   'baseline_uses_all_archived_src_headers':True,'commands':commands,
                   'reproduction_inputs':[ref(HERE/'review.py'),ref(HERE/'vocal_gesture.cpp'),
                                          ref(HERE/'current-capture.json'),ref(HERE/'baseline-capture.json')],
                   'current_baseline_telemetry_exactly_equal': logs['current'] == logs['baseline'],
                   'cores':{'current':ref(args.current_core),'baseline':ref(args.baseline_core)},
                   'measurements':measurements,'comparison':comparison,
                   'causal_limit':'Pressure and wheel occur within one evolving performance. Their separate gains are not isolated here; causal touch evidence comes from A21/A25. Rank telemetry proves commanded pitches, while formant tracking remains attributed to A07.',
                   'coverage_limit':'Onset budgets stay captured during the bends. A fresh lower note could admit more modes; the held note does not rebuild. No large ribbon or MPE gesture is qualified by this pair.'},
        'limitations':['Authored, physically motivated formants, not a fit to singer recordings or a phonetic corpus.',
                       'No human listening approval, perceptual preference, universal vowel-recognition or acoustic-equivalence claim.',
                       'The 48-mode ceiling leaves the deepest registers short of upper formants; footage and size change coverage.',
                       'The two original Oo finishes have full-bank and structural coverage, not a new individual low-note gesture capture.',
                       'Full-bank rows are numerical metrics, not PCM identity proofs; high-note PCM identity is separately supplied by A21.',
                       'No new native-host, CPU or all-gesture qualification.'],
        'pass_within_stated_scope':True}
(HERE/'review.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
print(json.dumps({'summary':result['summary'],'gesture_peak_dbfs':{k:v['peak_dbfs'] for k,v in measurements.items()},
                  'gesture_last_half_second_rms_dbfs':{k:v['windows'][-1]['rms_dbfs'] for k,v in measurements.items()},
                  'gesture_comparison':comparison,'review':str(HERE/'review.json')},indent=2))
