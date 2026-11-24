#!/usr/bin/env python3
"""Review A43's exact-source complete-patch telemetry; no native/audio-parity claim."""
import argparse
import collections
import hashlib
import json
import math
from pathlib import Path
import statistics
import subprocess

BASE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', type=Path, default=BASE.parents[3])
parser.add_argument('--raw', type=Path, default=BASE / 'a43-sequence-phrase-raw.jsonl')
parser.add_argument('--out', type=Path, default=BASE / 'a43-sequence-review.json')
args = parser.parse_args()
REPO = args.repo
SOURCE = '6094bce18c8b19da804c2fbc9a00e31969a7f579'
RAW = args.raw
OUT = args.out
if OUT.exists():
    raise SystemExit('Refusing to overwrite prior review evidence')
rows = [json.loads(line) for line in RAW.read_text().splitlines()]
programs = [r for r in rows if r['type'] == 'program']
events = collections.defaultdict(list)
for r in rows:
    if r['type'] == 'event':
        events[r['name'], r['scenario']].append(r)
inventory_raw = subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':docs/preset-evidence/2026.11.5/inventory.json'])
inventory = json.loads(inventory_raw)
baseline = json.loads(subprocess.check_output(['git', '-C', str(REPO), 'show', SOURCE + ':docs/preset-evidence/2026.11.5/baseline/inventory.json']))
expected = {p['name']: p for p in inventory['programs'] if p['category'] == 'Sequences'}
before = {p['name']: p for p in baseline['programs']}
checks = []
def check(ok, label):
    checks.append({'passed': bool(ok), 'label': label})

normal = [p for p in programs if p['scenario'] == 'constant_tempo']
jumped = [p for p in programs if p['scenario'] == 'tempo_jump']
check(len(rows) == len(programs) + sum(map(len, events.values())), 'only recognized raw record types')
check(len(normal) == 27 and {p['name'] for p in normal} == set(expected), 'all 27 existing sequence-category programs exactly once')
check(len(jumped) == 4 and {p['name'] for p in jumped} == {'Arp Poly', 'Arp Bells', 'Arp Bass', 'Random Arp'}, 'four actual modern arp tempo controls')
keys = [(55, .65), (48, .25), (52, .95)]
comparisons = []
for p in normal:
    name = p['name']; label = name + ': '
    ordered = keys if p['mode'] == 4 else sorted(keys)
    sequence = [(note + 12 * octave, velocity) for octave in range(p['octaves']) for note, velocity in ordered]
    if p['mode'] == 1:
        sequence.reverse()
    elif p['mode'] == 2:
        sequence += sequence[-2:0:-1]
    ev = events[name, p['scenario']]
    onsets = [x for x in ev if x['note'] >= 0]
    end_frames = [x['frame'] for x in onsets]
    differences = [b-a for a,b in zip(end_frames, end_frames[1:])]
    periods = [x / 48000 for x in differences]
    reference_period = 1 / p['rate_hz']
    gate = reference_period * (.45 if p['mode'] == 3 else .58)
    widths = [(b['frame']-a['frame']) / 48000 for a,b in zip(ev, ev[1:]) if a['note'] >= 0 and b['note'] == -1 and b['frame'] < 12 * 48000]
    values = dict(zip(inventory['parameter_ids'], expected[name]['values']))
    old = dict(zip(baseline['parameter_ids'], before[name]['values']))
    identity_fields = ['arp_on','arp_rate','arp_mode','arp_octaves','ch1_feet','ch2_feet']
    check(p['index'] == expected[name]['host_index'], label + 'existing address retained')
    check(all(values[k] == old[k] for k in identity_fields), label + 'arp on/rate/mode/octaves and both footage settings equal baseline')
    check(p['rate_hz'] == values['arp_rate'] and p['mode'] == values['arp_mode'] and p['octaves'] == values['arp_octaves'], label + 'rendered settings equal committed inventory')
    check(p['frames'] == 16 * 48000 and p['block_frames'] == 64 and p['events'] == len(ev), label + 'complete sixteen-second telemetry accounted')
    check(p['nonfinite'] == 0 and p['clipped'] == 0 and p['peak_dbfs'] < 0, label + 'all complete-patch samples finite and below full scale')
    check(p['max_held'] == 1 and p['max_active'] <= 8, label + 'one generated held note and bounded physical voices')
    check(p['tail_held_observations'] == 0 and p['final_active'] == 0, label + 'ordinary final key release stops all generated notes and retires voices')
    check(p['held_rms_dbfs'] > -60, label + 'late phrase remains audible')
    check(onsets and onsets[0]['frame'] <= 64, label + 'first generated note starts within one observed block')
    check(len(onsets) >= 2 * len(sequence), label + 'at least two sequence-lengths observed')
    check(differences and max(abs(x / 48000 - reference_period) for x in differences) <= 128 / 48000, label + 'each onset spacing agrees with authored rate within two observation blocks')
    check(widths and max(abs(w-gate) for w in widths) <= 128 / 48000, label + 'each complete held gate agrees with 58% or random 45% duty within two observation blocks')
    allowed = dict(sequence)
    check(all(x['note'] in allowed and x['channel'] == 1 and abs(x['velocity']-allowed[x['note']]) < 1e-7 for x in onsets), label + 'generated pitch belongs to authored octave set and retains source channel/velocity')
    if p['mode'] == 3:
        check(len({x['note'] for x in onsets}) >= 2, label + 'random mode actually visits multiple admitted notes')
    else:
        check(all(x['note'] == sequence[i % len(sequence)][0] for i,x in enumerate(onsets)), label + 'entire ascending/descending/up-down/played-order phrase follows declared mode')
    comparisons.append({
        **p,
        'onsets':len(onsets),'complete_gate_widths':len(widths),'declared_sequence_length':len(sequence),
        'observed_note_range':[min(x['note'] for x in onsets),max(x['note'] for x in onsets)],
        'expected_mode_note_order':None if p['mode']==3 else [x[0] for x in sequence],
        'max_spacing_error_ms':1000*max(abs(x-reference_period) for x in periods),
        'max_gate_width_error_ms':1000*max(abs(x-gate) for x in widths),
        'mean_spacing_hz':1/statistics.mean(periods),
        'final_tail_below_held_db':p['final_tail_rms_dbfs']-p['held_rms_dbfs'],
    })
tempo=[]
for p in jumped:
    ref=next(x for x in normal if x['name']==p['name'])
    key=p['name']
    a=[{k:v for k,v in x.items() if k!='scenario'} for x in events[key,'constant_tempo']]
    b=[{k:v for k,v in x.items() if k!='scenario'} for x in events[key,'tempo_jump']]
    check(a==b,key+': tempo60-to180 change preserves complete observed note/gate trace')
    check(p['pcm_fnv64']==ref['pcm_fnv64'],key+': tempo60-to180 control has same complete stereo sample digest')
    check(all(p[k]==ref[k] for k in ['peak_dbfs','held_rms_dbfs','first_tail_rms_dbfs','final_tail_rms_dbfs','final_active','tail_held_observations']),key+': tempo control has same energy and lifecycle observations')
    tempo.append({'name':key,'same_trace':a==b,'same_pcm_fnv64':p['pcm_fnv64']==ref['pcm_fnv64'],'digest':p['pcm_fnv64'],'scope':'64-bit digest equality plus exact telemetry/metric equality; no retained PCM byte-comparison is asserted.'})
failed=[c for c in checks if not c['passed']]
report={
    'source':SOURCE,'preset_revision':'2026.11.5','scope':'Local complete-preset phrase/lifecycle and declared scheduling review; no native build, listening approval or acoustic-equivalence evidence.',
    'input':{'file':RAW.name,'bytes':RAW.stat().st_size,'sha256':hashlib.sha256(RAW.read_bytes()).hexdigest(),'committed_inventory_sha256':hashlib.sha256(inventory_raw).hexdigest()},
    'render_method':{'rate':48000,'hold_seconds':12,'tail_seconds':4,'block_frames':64,'observation_resolution_ms':64/48,'timing_tolerance_ms':128/48,'source_notes_velocities':keys,'parameter_overrides':[],'main_transport_bpm':60,'tempo_control':{'initial_bpm':60,'new_bpm':180,'at_seconds':6},'sample_digest':'FNV-1a64 over little-endian float32 sample representations; descriptive, not a cryptographic artifact identity'},
    'counts':{'programs':len(normal),'numbered_original_sequences':sum(' Sequences ' in p['name'] for p in normal),'other_original_sequence':sum(p['name']=='Arp Motion' for p in normal),'modern_arps':4,'renders':len(programs),'stereo_frames':sum(p['frames'] for p in programs),'checks':len(checks),'failures':len(failed)},
    'programs':comparisons,'tempo_controls':tempo,'checks':checks,'failures':failed,
    'limits':['Trace times are callback-boundary observations of held state, not sample-exact acoustic onset measurements.','Rates are free-running Hz and are not host-tempo synchronized.','Mode/order/octave and finite/headroom observations protect preserved behavior; they are not new improvements or a quality score.','Final output tails can include intentional delay and existing noise after all generated voices have retired.','Only this three-key gesture and the indicated twelve-second hold are tested.']}
OUT.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'counts':report['counts'],'max_peak_dbfs':max(p['peak_dbfs'] for p in normal),'max_spacing_error_ms':max(p['max_spacing_error_ms'] for p in comparisons),'max_gate_width_error_ms':max(p['max_gate_width_error_ms'] for p in comparisons),'failures':failed},indent=2))
raise SystemExit(bool(failed))
