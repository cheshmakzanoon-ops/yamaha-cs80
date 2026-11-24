#!/usr/bin/env python3
"""A21 recipe audit and targeted full-program render; evidence only, no source edit."""
from collections import Counter
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import subprocess
import tarfile
import numpy as np

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[4]
TMP = Path('/tmp/cs80-a21')
CURRENT = 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201'
BASELINE = '18711f93a39aa49f3d6f9a41784be456224facbe'
RATE = 48000


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)


def db(value):
    return float(20 * math.log10(max(float(value), 1e-15)))


def rms(audio):
    return float(np.sqrt(np.mean(np.square(audio.astype(np.float64)))))


def segment(audio, start, end):
    return audio[round(start * RATE):round(end * RATE)]


def spectrum(audio):
    # Average channel power avoids stereo-sum cancellation. Welch/Hann 8192,
    # half overlap; bands characterize the complete patch, not an isolated body.
    n = 8192
    hann = np.hanning(n)[:, None]
    accumulated = np.zeros((n // 2 + 1, 2))
    count = 0
    for at in range(0, len(audio) - n + 1, n // 2):
        f = np.fft.rfft(audio[at:at+n].astype(np.float64) * hann, axis=0)
        accumulated += np.abs(f) ** 2
        count += 1
    assert count
    p = accumulated.sum(axis=1) / count
    hz = np.fft.rfftfreq(n, 1 / RATE)
    total = float(p[1:].sum())
    return dict(centroid_hz=float((hz[1:] * p[1:]).sum() / total),
                upper_2k_4k_fraction=float(p[(hz >= 2000) & (hz < 4000)].sum() / total),
                low_below_500_fraction=float(p[(hz > 0) & (hz < 500)].sum() / total))


def audit_inventory():
    raw = (ROOT / 'docs/preset-evidence/2026.11.7/verification/a12/inventory.json').read_bytes()
    oldraw = (ROOT / 'docs/preset-evidence/2026.11.7/development/baseline-inventory.json').read_bytes()
    assert oldraw == git('show', f'{BASELINE}:docs/preset-evidence/2026.11.6/verification/a15/inventory.json')
    assert raw == oldraw.replace(b'"library_revision":"2026.11.6"', b'"library_revision":"2026.11.7"', 1)
    current, baseline = json.loads(raw), json.loads(oldraw)
    ids = current['parameter_ids']
    assert len(ids) == 204
    rows = []
    for p, old in zip(current['programs'], baseline['programs']):
        if p['category'] != 'Pads':
            continue
        assert p == old and len(p['values']) == 204 and all(math.isfinite(v) for v in p['values'])
        v = dict(zip(ids, p['values']))
        ranks = []
        for r in (1, 2):
            gain = 1 - v['mix'] if r == 1 else v['mix']
            active = gain > 0 and any(v[f'ch{r}_{k}'] > 0 for k in ('v_level', 'init_level', 'after_level'))
            assert active and v[f'ch{r}_v_s'] > 0
            ranks.append(dict(rank=r, balance_gain=gain, body={k:v[f'body{r}_{k}'] for k in
                ('model', 'variant', 'material', 'mix', 'size', 'brightness', 'decay')},
                controls={k:v[f'ch{r}_{k}'] for k in ('saw','pulse','sine','noise','feet','pan',
                'hpf_cutoff','lpf_cutoff','lpf_res','f_il','f_al','f_a','f_d','f_r',
                'v_a','v_d','v_s','v_r','v_level','init_level','init_brill','after_level','after_brill',
                'pwm_depth','pwm_speed')}))
        choir = any(r['body']['model'] == 15 and r['body']['variant'] in (3,4) and r['body']['mix'] > 0 for r in ranks)
        # Mechanism-backed tags: these checks are structural, not semantic/perceptual scores.
        tag_checks = {}
        predicates = {
            'chorus': v['chorus_on'] > 0,
            'ring-mod': v['ring_amount'] > 0,
            'feet-layer': v['ch1_feet'] != v['ch2_feet'],
            'sine': v['ch1_sine'] > 0 or v['ch2_sine'] > 0,
            'noise': v['ch1_noise'] > 0 or v['ch2_noise'] > 0,
            'aftertouch': any(v[f'ch{r}_after_level'] > 0 or v[f'ch{r}_after_brill'] > 0 for r in (1,2)),
            'pwm': any(v[f'ch{r}_pulse'] > 0 and v[f'ch{r}_pwm_depth'] > 0 for r in (1,2)),
            'sub-mod': any(v[k] > 0 for k in ('sub_vco','sub_vcf','sub_vca')),
            'lfo': any(v[f'lfo{r}_depth'] > 0 and v[f'lfo{r}_dest'] > 0 for r in (1,2,3)),
        }
        for tag in p['tags']:
            if tag in predicates:
                tag_checks[tag] = predicates[tag]
        # Keep mismatches explicit: Low Drone and its Dark Pads 58 finish carry
        # a feet-layer tag despite two equal 16-foot ranks; their descriptions
        # correctly state the common footage. This is inherited metadata.
        assert all(value or (tag == 'feet-layer' and p['name'] in ('Low Drone','Dark Pads 58'))
                   for tag,value in tag_checks.items()), (p['name'], tag_checks)
        rows.append(dict(host_index=p['host_index'], name=p['name'], description=p['description'],
                         tags=p['tags'], complete_record_matches_6=True,
                         parameter_binary32_sha256=sha(struct.pack('<204f', *p['values'])),
                         vocal_body=choir, mechanism_tag_checks=tag_checks, ranks=ranks,
                         global_controls={k:v[k] for k in ('volume','output_level','mix','detune',
                         'chorus_on','chorus_mode','chorus_depth','ring_amount','ring_speed',
                         'sub_vco','sub_vcf','sub_vca','sub_speed','lfo1_rate','lfo1_depth','lfo1_dest',
                         'lfo2_rate','lfo2_depth','lfo2_dest','fx1_type','fx1_mix')}))
    assert len(rows) == 63
    assert len([r for r in rows if r['vocal_body']]) == 5
    assert {'Plate Pad', 'LFO Sweep Pad'} <= {r['name'] for r in rows}
    assert len({r['parameter_binary32_sha256'] for r in rows}) == 63
    budgets = []
    feet = [.5, 1, 2, 4, 8, 1.5]
    for p in current['programs']:
        if p['name'] not in ('Choir Pad', 'Dark Choir'):
            continue
        v = dict(zip(ids,p['values']))
        for note in (36,48,60,72):
            for r in (1,2):
                # Nominal captured frequency, including actual master tune,
                # tuning reference, rank detune and footage (excludes live drift).
                hz = v['tuning_ref'] * 2 ** ((note + v['master_tune'] + (v['detune']/100 if r==2 else 0) - 69) / 12) * feet[round(v[f'ch{r}_feet'])]
                budget = min(48,max(24,4400/(v[f'body{r}_size']*hz)))
                budgets.append(dict(preset=p['name'],note=note,rank=r,nominal_body_hz=hz,
                                    new_fractional_budget=budget,new_mode_count=math.ceil(budget),old_mode_count=24))
    write('structural-coverage.json', dict(source=CURRENT,baseline=BASELINE,
        inventory_sha256=sha(raw),baseline_inventory_sha256=sha(oldraw),
        structural_count=len(rows),original_count=sum(r['host_index']<222 for r in rows),
        curated_count=sum(r['host_index']>=222 for r in rows),unchanged_parameter_values=63*204,
        vocal_programs=[r['name'] for r in rows if r['vocal_body']],
        synthetic_program_count=sum(not r['vocal_body'] for r in rows),
        tag_counts=dict(Counter(t for r in rows for t in r['tags'])),
        examined_mechanism_tag_occurrences=sum(len(r['mechanism_tag_checks']) for r in rows),
        matched_mechanism_tag_occurrences=sum(sum(r['mechanism_tag_checks'].values()) for r in rows),
        inherited_tag_exceptions=[dict(preset=r['name'],tag=t) for r in rows
                                  for t,matched in r['mechanism_tag_checks'].items() if not matched],
        nominal_vocal_budgets=budgets,programs=rows))


def main():
    TMP.mkdir(parents=True,exist_ok=True)
    assert git('rev-parse','HEAD').decode().strip() == CURRENT
    subprocess.run(['git','diff','--exit-code',CURRENT,'--','src','tests','scripts','CMakeLists.txt'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
    audit_inventory()
    old_headers = TMP / 'baseline-src'
    old_headers.mkdir(exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(git('archive',BASELINE,'src'))) as archive:
        archive.extractall(old_headers,filter='data')
    binaries = {}
    builds = {'baseline':('/tmp/cs80-reference-1cee5a',old_headers/'src'),
              'current':('/tmp/cs80-build-1cee5a',ROOT/'src')}
    commands = []
    for label,(build,include) in builds.items():
        binary=TMP/('probe-'+label)
        command=['c++','-O2','-std=c++20','-I',str(include),str(OUT/'pads_probe.cpp'),str(Path(build)/'libcs80_core.a'),'-o',str(binary)]
        subprocess.run(command,check=True,cwd=ROOT)
        commands.append(command)
        binaries[label]=binary
    for label,binary in binaries.items():
        print('Rendering '+label,flush=True)
        with (OUT/(label+'-capture.log')).open('w') as log:
            subprocess.run([str(binary),str(TMP/label)],check=True,stdout=log,cwd=ROOT)
    rows=[]
    for file in sorted((TMP/'current').glob('*.f32')):
        stem=file.stem
        name,scene=stem.rsplit('_',1)
        name=name.replace('_',' ')
        hold = 6 if name=='Sweep Pad' else 4 if scene=='stack' else 3
        window=(hold-.75,hold-.1)
        versions={}
        audio={}
        for label in builds:
            path=TMP/label/file.name
            raw=path.read_bytes()
            a=np.frombuffer(raw,dtype='<f4').reshape(-1,2)
            assert np.isfinite(a).all()
            audio[label]=a
            held=segment(a,*window)
            versions[label]=dict(pcm_sha256=sha(raw),frames=len(a),
                peak_dbfs=db(np.max(np.abs(a))),over_full_scale_samples=int((np.abs(a)>1).sum()),
                nonfinite_samples=0,held_rms_dbfs=db(rms(held)),
                early_25_125ms_rms_dbfs=db(rms(segment(a,.025,.125))),
                final_half_second_rms_dbfs=db(rms(a[-RATE//2:])),
                tail_rms_dbfs=db(rms(a[hold*RATE:])),held_spectrum=spectrum(held),
                half_second_rms_dbfs=[db(rms(a[at:at+RATE//2])) for at in range(0,len(a),RATE//2)])
        assert audio['current'].shape==audio['baseline'].shape
        diff=audio['current'].astype(np.float64)-audio['baseline']
        reference=rms(segment(audio['baseline'],*window))
        rows.append(dict(preset=name,scene=scene,hold_seconds=hold,held_window_seconds=window,
                         sample_identical=bool(np.array_equal(audio['current'],audio['baseline'])),
                         held_difference_relative_db=db(rms(segment(diff,*window))/reference),
                         peak_delta_db=versions['current']['peak_dbfs']-versions['baseline']['peak_dbfs'],
                         held_rms_delta_db=versions['current']['held_rms_dbfs']-versions['baseline']['held_rms_dbfs'],
                         upper_2k_4k_fraction_delta_db=10*math.log10(versions['current']['held_spectrum']['upper_2k_4k_fraction']/versions['baseline']['held_spectrum']['upper_2k_4k_fraction']),
                         versions=versions))
    assert len(rows)==14
    assert all(r['sample_identical'] for r in rows if r['scene']=='high' or r['preset']=='Sweep Pad')
    assert all(not r['sample_identical'] for r in rows if r['preset']!='Sweep Pad' and r['scene']!='high')
    assert all(v['over_full_scale_samples']==0 for r in rows for v in r['versions'].values())
    expression=[]
    for name in ('Choir Pad','Dark Choir'):
        lookup={r['scene']:r for r in rows if r['preset']==name}
        for label in builds:
            expression.append(dict(preset=name,version=label,
                velocity_85_vs_25_held_gain_db=lookup['low']['versions'][label]['held_rms_dbfs']-lookup['soft']['versions'][label]['held_rms_dbfs'],
                pressure_80_vs_0_held_gain_db=lookup['pressure']['versions'][label]['held_rms_dbfs']-lookup['low']['versions'][label]['held_rms_dbfs']))
    synthetic=next(r for r in rows if r['preset']=='Sweep Pad' and r['scene']=='slow')
    a=np.fromfile(TMP/'current'/'Sweep_Pad_slow.f32',dtype='<f4').reshape(-1,2)
    bloom=rms(segment(a,4,4.5)); opening=rms(segment(a,.025,.125)); last=rms(segment(a,5.5,6))
    slow=dict(preset='Sweep Pad',hold_seconds=6,opening_window=[.025,.125],bloom_window=[4,4.5],late_window=[5.5,6],
              bloom_over_opening_db=db(bloom/opening),late_over_bloom_db=db(last/bloom),
              bloom_rms_dbfs=db(bloom),late_rms_dbfs=db(last),baseline_current_sample_identical=synthetic['sample_identical'])
    assert bloom>2*opening and last>.35*bloom and last>.001
    write('rendered-measurements.json',dict(sample_rate=RATE,oversampling='native saved quality; capture log records actual factor',
        complete_parameter_vectors_unmodified=True,preset_count=3,pair_count=len(rows),render_count=2*len(rows),
        generated_seconds=sum(r['versions']['current']['frames']/RATE for r in rows)*2,
        chord_notes=[36,43,48,52,55,60,64,67],chord_velocity=1,chord_pressure=1,pressure_onset_seconds=1.5,
        spectral_method='8192-sample Hann Welch, half overlap, summed channel power; complete patch bands, no isolated body measurement',
        expression=expression,slow_envelope=slow,renders=rows))
    evidence={str(p.relative_to(ROOT)):sha(p.read_bytes()) for p in OUT.iterdir() if p.is_file() and p.name!='binding.json'}
    write('binding.json',dict(source=CURRENT,baseline=BASELINE,scope='Read-only full-program Pads review; no listening or acoustic-equivalence claim',
        baseline_uses_all_archived_src_headers=True,commands=commands,
        binaries={label:sha(path.read_bytes()) for label,path in binaries.items()},
        cores={label:sha((Path(build)/'libcs80_core.a').read_bytes()) for label,(build,_) in builds.items()},
        source_files={p:sha((ROOT/p).read_bytes()) for p in ('src/core/Engine.cpp','src/core/Engine.h','src/core/InstrumentBody.h','src/core/FactoryPresets.cpp','src/core/FactoryPresets.h')},
        reused_tests_receipt='docs/preset-evidence/2026.11.7/development/focused-ctest-receipt.json',
        reused_tests_receipt_sha256=sha((ROOT/'docs/preset-evidence/2026.11.7/development/focused-ctest-receipt.json').read_bytes()),
        evidence_sha256=evidence))
    print(json.dumps(dict(pair_count=len(rows),sample_identical_pairs=sum(r['sample_identical'] for r in rows),
        current_worst_peak_dbfs=max(r['versions']['current']['peak_dbfs'] for r in rows),expression=expression,slow=slow),indent=2),flush=True)


if __name__=='__main__':
    main()
