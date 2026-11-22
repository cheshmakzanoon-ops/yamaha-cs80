from pathlib import Path
import csv, hashlib, json, math
import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly

ROOT=Path(__file__).resolve().parent
inventory=json.loads((ROOT/'inventory.json').read_text())
programs=inventory['programs']
rows=list(csv.DictReader((ROOT/'full/metrics.csv').open()))
assert len(rows)==len(programs)==301
files={int(p.name.split('-',1)[0]):p for p in (ROOT/'full').glob('*.wav')}
assert set(files)==set(range(301))
raw_groups={}; normalized_groups={}; waves=[]; envelopes=[]; spectra=[]; summaries=[]
rate_expected=48000
# Log-frequency power bands preserve broad spectral character without claiming a
# perceptual model. Waveform correlation is screened independently.
edges=np.geomspace(20,20000,241)
for idx,program in enumerate(programs):
    rate,raw=wavfile.read(files[idx])
    assert rate==rate_expected and raw.shape==(57600,2) and raw.dtype==np.float32
    assert np.isfinite(raw).all()
    samples=raw.astype(np.float64)
    peak=float(np.max(np.abs(samples)))
    rms=float(np.sqrt(np.mean(samples*samples)))
    # Exact bits, plus a clearly labelled quantized gain-normalized screen.
    raw_hash=hashlib.sha256(raw.tobytes()).hexdigest()
    raw_groups.setdefault(raw_hash,[]).append(idx)
    unit=samples/max(peak,1e-30)
    quantized=np.rint(unit*1e6).astype('<i4')
    normalized_groups.setdefault(hashlib.sha256(quantized.tobytes()).hexdigest(),[]).append(idx)
    down=resample_poly(raw,1,4,axis=0).astype(np.float32).reshape(-1)
    length=float(np.linalg.norm(down.astype(np.float64)))
    waves.append(down/max(length,1e-30))
    env=np.sqrt(np.mean(samples.reshape(60,960,2)**2,axis=(1,2)))
    envelopes.append(env/max(float(np.linalg.norm(env)),1e-30))
    held=samples[:33600]
    spec=np.mean(np.abs(np.fft.rfft(held*np.hanning(len(held))[:,None],n=65536,axis=0))**2,axis=1)
    hz=np.fft.rfftfreq(65536,1/rate)
    binned=np.array([spec[(hz>=a)&(hz<b)].sum() for a,b in zip(edges[:-1],edges[1:])])
    spectra.append(binned/max(float(np.linalg.norm(binned)),1e-30))
    summaries.append({'index':idx,'name':program['name'],'category':program['category'],'peak_dbfs':20*math.log10(max(peak,1e-15)),'rms_dbfs':20*math.log10(max(rms,1e-15)),'audio_sha256':raw_hash})
wave=np.stack(waves)
correlation=wave@wave.T
env=np.stack(envelopes); ec=env@env.T
spectral=np.stack(spectra); sc=spectral@spectral.T
pairs=[]
for i in range(len(programs)):
    for j in range(i+1,len(programs)):
        if max(summaries[i]['rms_dbfs'],summaries[j]['rms_dbfs'])<-80:continue
        pairs.append({'a':i,'b':j,'names':[programs[i]['name'],programs[j]['name']],
                      'waveform_correlation_12khz':float(correlation[i,j]),
                      'envelope_cosine':float(ec[i,j]),'spectral_cosine':float(sc[i,j])})
pairs.sort(key=lambda p:abs(p['waveform_correlation_12khz']),reverse=True)
top=pairs[:30]
# Re-evaluate the strongest screened pairs at the original sample rate. Compute
# a fitted scalar only; timing, phase, envelopes and channel balance are retained.
for p in top:
    _,x=wavfile.read(files[p['a']]);_,y=wavfile.read(files[p['b']])
    x=x.astype(np.float64).reshape(-1);y=y.astype(np.float64).reshape(-1)
    xx=float(x@x);yy=float(y@y);xy=float(x@y)
    gain=xy/max(xx,1e-30)
    residual=float(np.sqrt(np.mean((y-gain*x)**2)))
    p.update({'waveform_correlation_48khz':xy/max(math.sqrt(xx*yy),1e-30),
              'best_fit_gain':gain,'gain_fit_residual_db':20*math.log10(max(residual/max(math.sqrt(yy/len(y)),1e-30),1e-15)),
              'changed_parameters':[id for id,a,b in zip(inventory['parameter_ids'],programs[p['a']]['values'],programs[p['b']]['values']) if a!=b]})
result={
 'schema':1,'scope':'Single fixed-fixture whole-bank audio distinctness screen, not perceptual/acoustic equivalence',
 'library_revision':inventory['library_revision'],
 'source_baseline_commit':'7f55a95a7534b389927defe46866456560b3a878',
 'candidate_note':'Current authored 2026.11.1 bank after first level-calibration pass; exact inventory and executable hashes identify the snapshot.',
 'capture_sha256':hashlib.sha256((ROOT/'capture-snapshot').read_bytes()).hexdigest(),
 'inventory_sha256':hashlib.sha256((ROOT/'inventory.json').read_bytes()).hexdigest(),
 'fixture':{'sample_rate':48000,'note':60,'velocity':0.8,'pressure':0.5,'hold_seconds':0.7,'release_excerpt_seconds':0.5,'programs':301,'total_audio_seconds':361.2},
 'exact_raw_duplicates':[[programs[i]['name'] for i in group] for group in raw_groups.values() if len(group)>1],
 'quantized_peak_normalized_duplicates_1e_minus6':[[programs[i]['name'] for i in group] for group in normalized_groups.values() if len(group)>1],
 'all_finite':True,'full_scale_exceedance_programs':[s['name'] for s in summaries if s['peak_dbfs']>0],
 'maximum_peak_dbfs':max(s['peak_dbfs'] for s in summaries),
 'top_waveform_pairs':top,
 'top_spectral_envelope_pairs':sorted(pairs,key=lambda p:p['spectral_cosine']*p['envelope_cosine'],reverse=True)[:20],
 'programs':summaries,
 'limitations':['A single held note cannot establish full musical or perceptual distinctness.','Slow swells may not reach their body within this short fixture; fixed ring carriers and modulation depend on note and phrase history.','Near-identical fundamentals alone are not duplicate sounds; envelopes, pressure response, registration and modulation are reviewed separately.']}
(ROOT/'analysis.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ['fixture','exact_raw_duplicates','quantized_peak_normalized_duplicates_1e_minus6','maximum_peak_dbfs','full_scale_exceedance_programs']},indent=2))
for p in top[:15]:print(p['names'],'corr=',round(p['waveform_correlation_48khz'],7),'residualdB=',round(p['gain_fit_residual_db'],2),'env=',round(p['envelope_cosine'],7),'spectrum=',round(p['spectral_cosine'],7))
