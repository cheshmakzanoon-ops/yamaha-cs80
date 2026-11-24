#!/usr/bin/env python3
"""A19 numerical authored-patch analysis. No acoustic-equivalence/listening score."""
import argparse, csv, hashlib, json, pathlib
import numpy as np

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--work', type=pathlib.Path, required=True)
p.add_argument('--output', type=pathlib.Path, required=True)
p.add_argument('--baseline-inventory', type=pathlib.Path, required=True)
a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
RATE = 48000
PATCHES = {'Flute': [60, 72, 84], 'Clarinet': [48, 60, 72], 'Oboe': [60, 72, 84],
           'Bassoon': [48, 60, 72], 'Shakuhachi': [48, 60, 72]}
def db(power): return float(10 * np.log10(max(float(power), 1e-30)))
def power(x, start=2.0, stop=2.9): return float(np.mean(x[int(start*RATE):int(stop*RATE)]**2))
def path(rev, name, note, mode, gesture='held'):
    return a.work / rev / 'pcm' / f'{name.replace(" ", "_")}_{note}_{mode}_{gesture}.f32'
def read(rev, name, note, mode, gesture='held'):
    return np.fromfile(path(rev, name, note, mode, gesture), dtype='<f4').reshape(-1, 2).astype(np.float64)
def spectrum(x):
    x = x[2*RATE:29*RATE//10].mean(axis=1)
    win = np.hanning(len(x)); y = np.fft.rfft(x*win)
    return np.fft.rfftfreq(len(x), 1/RATE), 2*abs(y)**2/(len(x)*sum(win*win))
def frequency(x, expected):
    x = x[2*RATE:29*RATE//10].mean(axis=1); win = np.hanning(len(x))
    nfft = 1 << int(np.ceil(np.log2(4*len(x))))
    z = abs(np.fft.rfft(x*win, n=nfft))**2; f = np.fft.rfftfreq(nfft, 1/RATE)
    ids = np.flatnonzero(abs(f-expected) < .04*expected)
    k = int(ids[np.argmax(z[ids])]); lp = np.log(np.maximum(z[k-1:k+2], 1e-30))
    delta = .5*(lp[0]-lp[2])/(lp[0]-2*lp[1]+lp[2])
    return float(f[k] + delta*(f[1]-f[0]))
def offharm(x, f0, exclusion):
    f, z = spectrum(x)
    keep = (f>=1000) & (f<=12000) & (abs(f/f0-np.round(f/f0))*f0>exclusion)
    return float(sum(z[keep]))
def metrics(x, expected):
    f0 = frequency(x, expected); f, z = spectrum(x)
    harmonic = np.array([sum(z[abs(f-h*f0)<min(8., f0/8)]) for h in range(1,25)])
    # Release completion: the last 10-ms RMS window above -60 dB relative to held RMS.
    # A trailing window exceeding threshold means the finite one-second tail is censored.
    tail = x[3*RATE:4*RATE].reshape(100, RATE//100, 2)
    tail_power = np.mean(tail**2, axis=(1,2)); active = np.flatnonzero(tail_power > power(x)*1e-6)
    end = float((active[-1]+1)*10) if len(active) else 0.
    return {'rms_dbfs': db(power(x)), 'peak': float(np.max(abs(x))),
            'nonfinite': int(np.count_nonzero(~np.isfinite(x))),
            'measured_f0_hz': f0, 'pitch_error_cents': float(1200*np.log2(f0/expected)),
            'centroid_hz': float(sum(f*z)/sum(z)),
            'odd_upper_over_even_db': db(sum(harmonic[2::2])/max(sum(harmonic[1::2]),1e-30)),
            'h2_h1_db': db(harmonic[1]/max(harmonic[0],1e-30)),
            'release_below_minus60_ms': end if end<1000 else None,
            'last_100ms_dbfs': db(power(x,3.9,4.0)),
            'offharm_20_dbfs': db(offharm(x,f0,20.)),
            'offharm_40_dbfs': db(offharm(x,f0,40.)) if f0>80 else None}
def csvwrite(name, rows):
    with (a.output/name).open('w') as out:
        w = csv.DictWriter(out, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)

current = json.loads((a.work/'current-inventory.json').read_text())
old = json.loads(a.baseline_inventory.read_text()); ids=current['parameter_ids']
old_by_name={p['name']:p for p in old['programs']}
recipe_keys=['ch1_feet','ch2_feet','body1_model','body1_mix','body1_brightness','body1_decay','body1_size',
             'body2_model','body2_mix','body2_brightness','body2_decay','body2_size','mix','init_bend','portamento',
             'ch1_sine','ch1_pulse','ch1_noise','ch1_hpf_cutoff','ch1_lpf_cutoff','ch1_v_a','ch1_v_s','ch1_v_r','ch1_v_level',
             'ch1_init_brill','ch1_init_level','ch1_after_brill','ch1_after_level',
             'ch2_sine','ch2_pulse','ch2_noise','ch2_hpf_cutoff','ch2_lpf_cutoff','ch2_v_a','ch2_v_s','ch2_v_r','ch2_v_level',
             'ch2_init_brill','ch2_init_level','ch2_after_brill','ch2_after_level',
             'sub_at_vco','kbd_level_low','kbd_level_high','drift','voice_dispersion']
recipes=[]
for program in current['programs']:
    if program['category']!='Winds': continue
    q=dict(zip(ids, program['values'])); old_program=old_by_name[program['name']]
    recipes.append({'name':program['name'],'host_index':program['host_index'],
                    'all_204_values_equal_baseline':program['values']==old_program['values'],
                    'metadata_equal_baseline':all(program[k]==old_program[k] for k in ('name','category','description','tags','host_index')),
                    **{key:q[key] for key in recipe_keys}})
assert len(recipes)==14
csvwrite('recipes.csv', recipes)
(a.output/'recipes.json').write_text(json.dumps(recipes,indent=2)+'\n')

captures=[]; hashes={}; by_key={}; max_peak=0.; nonfinite=0
for rev in ('before','after'):
    for name, notes in PATCHES.items():
        for note in notes:
            expected=440*2**((note-69-(12 if name=='Bassoon' else 0))/12)
            for mode in ('rt2x','hq8x','offline8x'):
                for gesture in ('held','soft','pressure'):
                    fpath=path(rev,name,note,mode,gesture)
                    if not fpath.exists(): continue
                    x=read(rev,name,note,mode,gesture); m=metrics(x,expected)
                    row={'revision':rev,'name':name,'midi_note':note,'expected_f0_hz':expected,'mode':mode,'gesture':gesture,**m}
                    captures.append(row); by_key[(rev,name,note,mode,gesture)]=row
                    hashes[str(fpath.relative_to(a.work))]=hashlib.sha256(fpath.read_bytes()).hexdigest()
                    max_peak=max(max_peak,m['peak']); nonfinite+=m['nonfinite']
csvwrite('captures.csv',captures)

quality=[]; expressivity=[]; references=[]
for name, notes in PATCHES.items():
    for note in notes:
        r=read('after',name,note,'rt2x'); h=read('after',name,note,'hq8x')
        mr=by_key['after',name,note,'rt2x','held']; mh=by_key['after',name,note,'hq8x','held']
        quality.append({'name':name,'midi_note':note,
                        'hq_minus_rt_rms_db':db(power(h)/power(r)),
                        'hq_minus_rt_offharm20_db':mh['offharm_20_dbfs']-mr['offharm_20_dbfs'],
                        'hq_minus_rt_offharm40_db':mh['offharm_40_dbfs']-mr['offharm_40_dbfs'] if mh['offharm_40_dbfs'] is not None else None,
                        'hq_minus_rt_pitch_cents':mh['pitch_error_cents']-mr['pitch_error_cents'],
                        'hq_minus_rt_centroid_hz':mh['centroid_hz']-mr['centroid_hz'],
                        'rt_release_below_minus60_ms':mr['release_below_minus60_ms'],
                        'hq_release_below_minus60_ms':mh['release_below_minus60_ms']})
    note=notes[1]
    ref={ 'name':name,'middle_midi_note':note }
    for mode in ('rt2x','hq8x'):
        hard=read('after',name,note,mode); soft=read('after',name,note,mode,'soft'); pressure=read('after',name,note,mode,'pressure'); before=read('before',name,note,mode)
        mh=by_key['after',name,note,mode,'held']; ms=by_key['after',name,note,mode,'soft']; mp=by_key['after',name,note,mode,'pressure']; mb=by_key['before',name,note,mode,'held']
        expressivity.append({'name':name,'midi_note':note,'mode':mode,
                             'velocity_085_over_030_db':db(power(hard)/power(soft)),
                             'velocity_centroid_change_hz':mh['centroid_hz']-ms['centroid_hz'],
                             'pressure_08_over_zero_db':db(power(pressure)/power(hard)),
                             'pressure_centroid_change_hz':mp['centroid_hz']-mh['centroid_hz'],
                             'prepressure_samples_equal':bool(np.array_equal(pressure[:6*RATE//5],hard[:6*RATE//5])),
                             'pressure_release_below_minus60_ms':mp['release_below_minus60_ms']})
        ref[mode+'_before_after_pcm_equal']=bool(np.array_equal(before,hard))
        ref[mode+'_before_after_rms_db']=mh['rms_dbfs']-mb['rms_dbfs']
        ref[mode+'_before_after_offharm20_db']=mh['offharm_20_dbfs']-mb['offharm_20_dbfs']
    ref['hq_offline_pcm_equal']=bool(np.array_equal(read('after',name,note,'hq8x'),read('after',name,note,'offline8x')))
    before_rt=by_key['before',name,note,'rt2x','held']; before_hq=by_key['before',name,note,'hq8x','held']
    ref['before_hq_minus_rt_offharm20_db']=before_hq['offharm_20_dbfs']-before_rt['offharm_20_dbfs']
    references.append(ref)
csvwrite('quality.csv',quality); csvwrite('expressivity.csv',expressivity); csvwrite('baseline.csv',references)
summary={'current_source': 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201', 'library_revision':current['library_revision'],
         'baseline_source':'18711f93a39aa49f3d6f9a41784be456224facbe','recipe_count':len(recipes),
         'recipes_all_204_equal_baseline':all(x['all_204_values_equal_baseline'] for x in recipes),
         'recipes_metadata_equal_baseline':all(x['metadata_equal_baseline'] for x in recipes),
         'render_count':len(captures),'total_audio_seconds':len(captures)*4,
         'maximum_peak':max_peak,'nonfinite_samples':nonfinite,
         'quality':quality,'expressivity':expressivity,'baseline':references,
         'method': '48 kHz; exact 113-frame render blocks truncated at pressure 1.2s and note-off 3s, 1s tail; factory vectors unchanged except quality; velocity .85/.30; pressure step .8. Sustained 2.0-2.9s RMS stereo; Hann PSD mono, 1-12kHz off-harmonic energy excluding +/-20Hz (40Hz cross-check) about measured f0. Dominant local fundamental via four-times-zero-padded Hann FFT and log-power parabolic interpolation. -60dB relative release is last 10ms RMS window above held power x 1e-6; null denotes tail censored. Odd/even metric excludes fundamental. Full-patch off-harmonic power includes independent circuit air, modulation skirts, and other residuals; it does not isolate the modeled body or certify acoustic similarity.'}
(a.output/'measurements.json').write_text(json.dumps(summary,indent=2)+'\n')
(a.output/'pcm-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
print(json.dumps(summary,indent=2))
