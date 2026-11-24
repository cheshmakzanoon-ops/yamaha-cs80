#!/usr/bin/env python3
"""Analyze A16 raw interleaved float32 PCM; no listening inference."""
import numpy as np,json,csv,hashlib,sys
from pathlib import Path
work=Path(sys.argv[1]);out=Path(__file__).resolve().parent;rate=48000
names=['French_Horn','Muted_Trumpet','Trombone_Glide','Pressure_Swell_Brass','Synth_Horns','Delay_Brass']
def read(rev,name,mode,gesture='held'):return np.fromfile(work/rev/'pcm'/f'{name}_{mode}_{gesture}.f32',dtype='<f4').reshape(-1,2).astype(float)
def db(x):return 10*np.log10(max(float(x),1e-30))
def power(a,start=1.5,end=2.9):return np.mean(a[int(start*rate):int(end*rate)]**2)
def spectrum(a):
 x=a[int(1.5*rate):int(2.9*rate)].mean(axis=1);w=np.hanning(len(x));y=np.fft.rfft(x*w);f=np.fft.rfftfreq(len(x),1/rate);p=2*abs(y)**2/(len(x)*sum(w*w));return f,p
def nonharm(a,n,excl=20):
 f,p=spectrum(a);f0=261.6255653*(.5 if n=='Trombone_Glide' else 1);mask=(f>=1000)&(f<=12000)&(abs(f/f0-np.round(f/f0))*f0>excl);return sum(p[mask])
rows=[];hashes={};nonfinite=0;peak=0
for rev in ['before','after','frozen','noair','dry-current','dry-noair']:
 for path in sorted((work/rev/'pcm').glob('*.f32')):
  a=np.fromfile(path,dtype='<f4');nonfinite+=int((~np.isfinite(a)).sum());peak=max(peak,float(np.max(np.abs(a))));hashes[str(path.relative_to(work))]=hashlib.sha256(path.read_bytes()).hexdigest()
for n in names:
 b0=read('before',n,'standard');b1=read('before',n,'hq');a0=read('after',n,'standard');a1=read('after',n,'hq');ao=read('after',n,'offline');ap0=read('after',n,'standard','pressure');ap1=read('after',n,'hq','pressure')
 row={'name':n.replace('_',' '),'before_nonharm_hq_standard_db':db(nonharm(b1,n)/nonharm(b0,n)),'after_nonharm_hq_standard_db':db(nonharm(a1,n)/nonharm(a0,n)),'after_nonharm40_hq_standard_db':db(nonharm(a1,n,40)/nonharm(a0,n,40)),'before_rms_hq_standard_db':db(power(b1)/power(b0)),'after_rms_hq_standard_db':db(power(a1)/power(a0)),'pressure_level_standard_db':db(power(ap0)/power(a0)),'pressure_level_hq_db':db(power(ap1)/power(a1)),'hq_offline_equal':bool(np.array_equal(a1,ao)),'standard_before_after_equal':bool(np.array_equal(a0,b0))}
 if (work/'noair/pcm'/f'{n}_standard_held.f32').exists():
  d0=a0-read('noair',n,'standard');d1=a1-read('noair',n,'hq');do=ao-read('noair',n,'offline')
  row.update(air_delta_standard_dbfs=db(power(d0)),air_delta_hq_dbfs=db(power(d1)),air_delta_hq_standard_db=db(power(d1)/max(power(d0),1e-30)),air_relative_full_standard_db=db(power(d0)/power(a0)),air_relative_full_hq_db=db(power(d1)/power(a1)),air_offline_equal=bool(np.array_equal(d1,do)),air_delta_standard_peak=float(np.max(abs(d0))))
 if 'air_delta_standard_peak' in row and row['air_delta_standard_peak']==0:
  for k in list(row):
   if k.startswith('air_') and k.endswith(('db','dbfs')):row[k]=None
 rows.append(row)
bends=[]
for n in names[:3]:
 a=read('after',n,'standard','bend');frozen=read('frozen',n,'standard','bend');d=a-frozen
 # Find the fundamental after settled bend, then compare normalized upper harmonic energy.
 f,p=spectrum(a);target=261.6255653*(.5 if n=='Trombone_Glide' else 1)*2**(2/12);ix=np.where(abs(f-target)<5)[0];pi=int(ix[np.argmax(p[ix])]);lp=np.log(np.maximum(p[pi-1:pi+2],1e-30));shift=.5*(lp[0]-lp[2])/(lp[0]-2*lp[1]+lp[2]);peakf=float(f[pi]+shift*(f[1]-f[0]))
 ff,pp=spectrum(frozen);ratios=[]
 for h in range(2,13):
  m=abs(f-peakf*h)<4;fund=abs(f-peakf)<4;ratios.append(db(sum(p[m])/sum(p[fund]))-db(sum(pp[m])/sum(pp[fund])))
 bends.append({'name':n.replace('_',' '),'expected_hz':target,'measured_peak_hz':peakf,'pitch_error_cents_parabolic_fft':1200*np.log2(peakf/target),'before_bend_correction_relative_db':db(power(d,.7,.95)/power(a,.7,.95)),'after_bend_correction_relative_db':db(power(d,1.5,2.9)/power(a,1.5,2.9)),'max_harmonic_balance_change_db':max(abs(x) for x in ratios),'harmonic_2_12_changes_db':ratios})
dry={}
if (work/'dry-current/pcm/Delay_Brass_offline_held.f32').exists() and (work/'dry-noair/pcm/Delay_Brass_offline_held.f32').exists():
 d0=read('dry-current','Delay_Brass','standard')-read('dry-noair','Delay_Brass','standard');d1=read('dry-current','Delay_Brass','hq')-read('dry-noair','Delay_Brass','hq')
 dry={'diagnostic':'Delay Brass only: FX1/2 wet mix zero on temporary copies; original filters/VCA retained','air_hq_standard_db':db(power(d1)/power(d0)),'full_hq_standard_db':db(power(read('dry-current','Delay_Brass','hq'))/power(read('dry-current','Delay_Brass','standard')))}
summary={'dry_diagnostic':dry,'render_count':len(hashes),'audio_seconds':len(hashes)*3.3,'nonfinite_samples':nonfinite,'maximum_peak':peak,'rows':rows,'bends':bends,'method':'48 kHz, MIDI60, velocity .8, 3s held + .3s tail, 113-frame blocks; pressure .8; bend +1 at 1s uses authored 2-semitone range. Complete factory vectors retained except quality. Off-harmonic power 1-12kHz, Hann 1.5-2.9s, +/-20 and 40Hz harmonic exclusion. Isolated noair difference is causal brass-air response through original filters/VCA; not perceptual air audibility. Frozen-formant control removes only brass from tracksFormants().'}
(out/'measurements.json').write_text(json.dumps(summary,indent=2)+'\n');(out/'pcm-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
with (out/'quality-summary.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
print(json.dumps(summary,indent=2))
