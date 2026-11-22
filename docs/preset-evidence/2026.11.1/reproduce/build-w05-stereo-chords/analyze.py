from pathlib import Path
import csv, hashlib, json, math, subprocess
import numpy as np
root=Path.cwd();out=root/'build-w05-stereo-chords';rate=48000
names=['Noir Swell Brass','Brass Ensemble 16','Neo-Noir Pad','Arctic Pad','Synthwave Chords']
notes=[48,55,60,64,53,57,62,69]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def db(x):return float(20*np.log10(max(float(x),1e-15)))
def power(x):return float(np.mean(x*x))
def measure(x,start,end,freqs):
 a=x[round(start*rate):round(end*rate)];l,r=a.T;m=(l+r)/2;s=(l-r)/2
 stereo=power(a);mid=power(m);side=power(s)
 w=np.hanning(len(a));f=np.fft.rfftfreq(len(a),1/rate)
 lp=abs(np.fft.rfft(l*w))**2;rp=abs(np.fft.rfft(r*w))**2;mp=abs(np.fft.rfft(m*w))**2;sp=(lp+rp)/2
 mask=np.zeros(len(f),bool)
 for hz in freqs:mask|=(f>=hz*.9)&(f<=hz*1.1)
 band=float(sp[mask].sum());monoband=float(mp[mask].sum())
 return {'start_seconds':start,'end_seconds':end,'rms_dbfs':db(np.sqrt(stereo)),
 'side_mid_db':db(np.sqrt(side/max(mid,1e-30))),
 'lr_balance_db':db(np.sqrt(power(l)/max(power(r),1e-30))),
 'mono_loss_db':db(np.sqrt(stereo/max(mid,1e-30))),
 'fundamental_union_mono_loss_db':10*math.log10(max(band,1e-30)/max(monoband,1e-30)),
 'fundamental_union_power_share':band/max(float(sp.sum()),1e-30),
 'correlation':float(np.corrcoef(l,r)[0,1]) if stereo>1e-24 else None}
rows=[]
for name in names:
 freq=[440*2**((n-69)/12)*(.5 if name=='Brass Ensemble 16' else 1) for n in notes]
 for variant in ['factory','centered-control']:
  path=out/(name.replace(' ','-')+'-'+variant+'.f32');x=np.fromfile(path,dtype='<f4').astype(float).reshape(-1,2)
  assert x.shape==(576000,2)
  windows=[measure(x,float(t),float(t+.25),freq) for t in np.arange(0,11.75001,.125)]
  active=[w for w in windows if w['rms_dbfs']>=-60]
  phases={label:measure(x,a,b,freq) for label,a,b in [('first_bloom',.8,1.4),('first_pressure_hold',2.5,3.5),('release_overlap',3.75,4.5),('second_chord',4.5,5.2),('release',5.5,6.5),('final_tail',11.5,12)]}
  rows.append({'name':name,'variant':variant,'raw_sha256':sha(path),'active_windows':len(active),'active_window_bounds':{
   'min_side_mid_db':min(w['side_mid_db'] for w in active),'max_side_mid_db':max(w['side_mid_db'] for w in active),
   'max_abs_lr_balance_db':max(abs(w['lr_balance_db']) for w in active),
   'max_mono_loss_db':max(w['mono_loss_db'] for w in active),
   'max_fundamental_union_mono_loss_db':max(w['fundamental_union_mono_loss_db'] for w in active)},
   'phases':phases,'windows':windows})
with (out/'capture.csv').open() as f: captures=list(csv.DictReader(f))
factory=[r for r in rows if r['variant']=='factory'];allfact=[c for c in captures if c['variant']=='factory']
summary={'assignment':25,'library_revision':'2026.11.1','source_base_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
 'condition':'Final calibrated factory data at 48 kHz; two staggered four-note chords; pressure 0→1→0 in 10 ms events; first release overlaps second onset, reaching 8 active voices. Twelve seconds each. Matched control changes only two rank pans to 0; full factory effects stay active.',
 'note_sets':{'first':[48,55,60,64],'second':[53,57,62,69]},'first_velocities':[.8,.85,.8,.85],'second_velocities':[1,1,1,1],
 'source_state':'Uncommitted final calibrated working-tree snapshot; frozen static-library and complete inventory hashes identify the captured bank.',
 'tail_scope':'Zero active voices at 12 seconds is a voice-retirement result, not exact digital silence: the final half-second remains near the approximately -82 dBFS output floor.',
 'events_csv':'events.csv','captured_sequences':10,'generated_seconds':120,
 'analysis':{'window_seconds':.25,'step_seconds':.125,'active_floor_dbfs':-60,'fundamental_band':'Union of ±10% bands around both chords main-rank frequencies; 16-foot main rank for Brass Ensemble 16 and 8-foot for others. Hann-windowed stereo-average and mono powers. Band share reported; not an isolated note spectrum.','mono':'(L+R)/2, compared with mean L/R channel power'},
 'bounds':{'highest_factory_peak_dbfs':max(float(c['peak_dbfs']) for c in allfact),'highest_final_halfsecond_dbfs':max(float(c['final_half_second_dbfs']) for c in allfact),
 'nonfinite_samples':sum(int(c['nonfinite']) for c in captures),'over_full_scale_samples':sum(int(c['over_full_scale']) for c in captures),
 'all_factory_reached8':all(int(c['max_active_voices'])==8 for c in allfact),'all_factory_retired':all(int(c['final_active_voices'])==0 for c in allfact),
 'max_active_abs_lr_balance_db':max(r['active_window_bounds']['max_abs_lr_balance_db'] for r in factory),
 'max_active_mono_loss_db':max(r['active_window_bounds']['max_mono_loss_db'] for r in factory),
 'max_active_fundamental_union_mono_loss_db':max(r['active_window_bounds']['max_fundamental_union_mono_loss_db'] for r in factory)},
 'hashes':{p.name:sha(p) for p in [out/'libcs80_core-final.a',out/'inventory-final.json',out/'probe.cpp',out/'probe',out/'events.csv',out/'capture.csv']},
 'limitations':'Bounded software sequence, not physical-instrument evidence or a listening judgment. Fine individual-note fundamental separation is not claimed during chord overlap; bands are unioned. The -60dBFS floor excludes essentially silent windows from stereo ratios.'}
(out/'measurements.json').write_text(json.dumps({'summary':summary,'captures':captures,'measurements':rows},indent=2)+'\n')
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
for name in names:
 fs=next(r for r in rows if r['name']==name and r['variant']=='factory');cs=next(r for r in rows if r['name']==name and r['variant']=='centered-control')
 print(name,'pressure hold side/mid',cs['phases']['first_pressure_hold']['side_mid_db'],'->',fs['phases']['first_pressure_hold']['side_mid_db'],'maxes',fs['active_window_bounds'])
