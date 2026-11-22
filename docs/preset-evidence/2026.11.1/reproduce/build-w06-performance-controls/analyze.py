from pathlib import Path
import csv,hashlib,json,subprocess
import numpy as np
from scipy import signal
D=Path(__file__).resolve().parent
RATE=48000
rows=list(csv.DictReader((D/'captures.csv').open()))
def db(x): return float(20*np.log10(max(float(x),1e-15)))
def rms(a,a0,a1):return float(np.sqrt(np.mean(np.square(a[round(a0*RATE):round(a1*RATE)],dtype=np.float64))))
def load(slug,kind):return np.fromfile(D/f'{slug}_{kind}.f32',dtype='<f4').reshape(-1,2).astype(np.float64)
def trace(slug,kind):return np.genfromtxt(D/f'{slug}_{kind}.csv',delimiter=',',names=True)
def fit_amplitude(t,x,f):
 t=t-t[0]
 design=np.column_stack([np.ones(len(t)),t,np.sin(2*np.pi*f*t),np.cos(2*np.pi*f*t)])
 coef=np.linalg.lstsq(design,x,rcond=None)[0]
 return float(np.hypot(coef[2],coef[3]))
def audio_pitch(a):
 # Demodulate around MIDI60; the lowpass isolates the fundamental from higher harmonics.
 mono=a.mean(axis=1);f0=440*2**((60-69)/12);t=np.arange(len(mono))/RATE
 z=signal.sosfiltfilt(signal.butter(5,70,fs=RATE,output='sos'),mono*np.exp(-2j*np.pi*f0*t))
 hz=f0+np.gradient(np.unwrap(np.angle(z)))*RATE/(2*np.pi)
 cents=1200*np.log2(np.maximum(hz,1)/f0)
 return t,cents
def centroid(a):
 f,t,z=signal.stft(a.mean(axis=1),fs=RATE,nperseg=4096,noverlap=3856,boundary=None)
 mask=(f>=150)&(f<=8000);p=abs(z[mask])**2
 return t,(p*f[mask,None]).sum(axis=0)/np.maximum(p.sum(axis=0),1e-30)
results={};checks=[]
def check(name,truth):
 checks.append({'check':name,'pass':bool(truth)})
for row in rows:
 for k in ('nonfinite','over_full_scale','final_voices'):check(row['capture']+': '+k,int(row[k])==0)
 check(row['capture']+': output below full scale',float(row['peak'])<1)
for slug in ('pressure_lead','wah_lead','vibrato_lead'):
 p,c=load(slug,'performed'),load(slug,'control');pt,ct=trace(slug,'performed'),trace(slug,'control')
 pre=float(np.max(abs(p[:round(.5*RATE)]-c[:round(.5*RATE)])))
 measure={'pre_pressure_max_abs_audio_difference':pre,'oscillator_hz_max_abs_difference_first_hold':float(max(np.max(abs(pt[k][pt['time']<3.2]-ct[k][ct['time']<3.2])) for k in ('osc1','osc2'))),'initial_unpressed_rms_dbfs':db(rms(p,.15,.45)),
          'full_pressure_rms_dbfs':db(rms(p,1.2,1.9)),'zero_pressure_control_same_window_rms_dbfs':db(rms(c,1.2,1.9)),
          'returned_zero_pressure_held_rms_dbfs':db(rms(p,2.9,3.1)),
          'returned_zero_pressure_control_rms_dbfs':db(rms(c,2.9,3.1)),
          'zero_pressure_release_tail_rms_dbfs':db(rms(p,4.2,4.4)),
          'full_pressure_release_tail_rms_dbfs':db(rms(p,6.3,6.5)),
          'final_200ms_rms_dbfs':db(rms(p,7.8,8.0))}
 check(slug+': matched audio before pressure',pre==0)
 for kind,tr,a in [('performed',pt,p),('control',ct,c)]:
  mask=(tr['time']>=1.2)&(tr['time']<=1.9)
  f=3.8 if slug=='wah_lead' else 5.0
  x=1200*np.log2(tr['osc1'][mask]/(440*2**((60-69)/12)))
  t,y=audio_pitch(a);m=(t>=1.2)&(t<=1.9)
  tc,yc=centroid(a);mc=(tc>=1.2)&(tc<=1.9)
  measure[kind]={'osc1_peak_to_peak_cents':float(np.ptp(x)),
       'osc1_modulation_fit_peak_to_peak_cents':2*fit_amplitude(tr['time'][mask],x,f),
       'lpf1_min_hz':float(tr['lpf1'][mask].min()),'lpf1_max_hz':float(tr['lpf1'][mask].max()),
       'lpf1_modulation_fit_peak_to_peak_octaves':2*fit_amplitude(tr['time'][mask],np.log2(tr['lpf1'][mask]),f),
       'audio_fundamental_modulation_fit_peak_to_peak_cents':2*fit_amplitude(t[m][::48],y[m][::48],f),
       'audio_centroid_mean_hz':float(yc[mc].mean()),
       'audio_centroid_modulation_fit_peak_to_peak_hz':2*fit_amplitude(tc[mc],yc[mc],f),
       'fitted_modulation_hz':f}
 if slug=='pressure_lead':
  check('Pressure Lead: unpressed silence',rms(p,.15,.45)<1e-6)
  check('Pressure Lead: pressure speaks',rms(p,1.2,1.9)>1e-3)
  check('Pressure Lead: easing pressure fades held note',rms(p,2.9,3.1)<.03*rms(p,1.2,1.9))
 if slug=='wah_lead':
  check('Wah Lead: no pressure-induced oscillator vibrato',measure['performed']['osc1_peak_to_peak_cents']<1 and measure['oscillator_hz_max_abs_difference_first_hold']==0)
  check('Wah Lead: filter modulation develops',measure['performed']['lpf1_modulation_fit_peak_to_peak_octaves']>measure['control']['lpf1_modulation_fit_peak_to_peak_octaves']+.2)
  check('Wah Lead: audio brightness oscillation develops',measure['performed']['audio_centroid_modulation_fit_peak_to_peak_hz']>measure['control']['audio_centroid_modulation_fit_peak_to_peak_hz']+20)
 if slug=='vibrato_lead':
  check('Aftertouch Vibrato Lead: oscillator excursion develops',measure['performed']['osc1_modulation_fit_peak_to_peak_cents']>30)
  check('Aftertouch Vibrato Lead: control stable',measure['control']['osc1_peak_to_peak_cents']<1)
  check('Aftertouch Vibrato Lead: audible fundamental excursion follows',measure['performed']['audio_fundamental_modulation_fit_peak_to_peak_cents']>30)
 results[slug]=measure
for slug in ('velocity_keys','velocity_lead'):
 p,c=load(slug,'performed'),load(slug,'control')
 measure={'pre_hard_strike_max_abs_audio_difference':float(np.max(abs(p[:round(1.1*RATE)]-c[:round(1.1*RATE)]))),
          'hard_vs_soft_attack_gain_db':db(rms(p,1.115,1.35)/rms(c,1.115,1.35)),
          'hard_vs_soft_held_gain_db':db(rms(p,1.45,1.65)/rms(c,1.45,1.65)),
          'subsequent_soft_attack_vs_control_db':db(rms(p,2.215,2.45)/rms(c,2.215,2.45)),
          'initial_soft_held_rms_dbfs':db(rms(p,.35,.55)),
          'final_200ms_rms_dbfs':db(rms(p,3.8,4))}
 check(slug+': matched audio before hard strike',measure['pre_hard_strike_max_abs_audio_difference']==0)
 check(slug+': wide attack dynamics',measure['hard_vs_soft_attack_gain_db']>6)
 check(slug+': wide held dynamics',measure['hard_vs_soft_held_gain_db']>6)
 check(slug+': soft repeat returns',abs(measure['subsequent_soft_attack_vs_control_db'])<.5)
 results[slug]=measure
hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(D.glob('*')) if p.suffix in ('.cpp','.py','.f32','.csv','.log') and p.name != 'analysis.log'}
report={'assignment':33,'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=D.parent,text=True).strip(),'library_revision':'2026.11.1','sample_rate':RATE,'midi_note':60,'capture_count':len(rows),'seconds':sum(float(r['seconds']) for r in rows),'highest_sample_peak_dbfs':max(db(float(r['peak'])) for r in rows),'checks_passed':sum(x['pass'] for x in checks),'checks_total':len(checks),'checks':checks,'failures':[x['check'] for x in checks if not x['pass']],'measurements':results,'hashes':hashes,'input_hashes':{str(p.relative_to(D.parent)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [D.parent/'build-audit/libcs80_core.a',D.parent/'build-audit/inventory-final.json',D.parent/'src/core/curated/Keys.inc',D.parent/'src/core/curated/Leads.inc']},'method':{'pressure_events':'Two eight-second renders per pressure voice. Performed: note60 velocity0.8 at0s; channelpressure0→1 at0.5–1.0s,1→0 at2.0–2.5s, noteoff3.2s; secondnote4.5s,pressure0→1 at4.6–5.1s,noteoff5.7s whilepressure1,pressure0 at6.5s. Matchedcontrol keepspressure0.','velocity_events':'Two four-second renders per velocity voice. Note60 at0,1.1,2.2s,offs0.6,1.7,2.8s. Performedvelocities0.2,1.0,0.2;control0.2 for all.','telemetry':'Rank oscillator/filter telemetry every10ms; no parameter mutation.','audio_analysis':'Fundamental demodulation aroundMIDI60, fifth-order70Hz lowpass applied forward/backward; least-squares sinusoidal amplitude with constant/lineartrend atnominal5Hz (vibrato) or3.8Hz(wah). Spectrum centroid150–8000Hz uses4096-sampleSTFT,240-samplehop. Fit window1.2–1.9s. These fixed-frequency fits measure response amplitude, not an independent exact modulation-rate estimate.'},'limits':['Representative MIDI60 sequences; not every register, DAW, controller, or performance history.','Per-pressure roles use 0→1→0 channel pressure; polyphonic routing was tested separately in the wind assignment.','Factory parameters are unchanged; matched control differs only in event pressure or the second note velocity.','Wah filter modulation can cause small audio-phase modulation without pitch-oscillator modulation; oscillator telemetry and demodulated fundamental are reported separately.','No physical CS-80 measurement or listening-panel judgment.']}
(D/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ('capture_count','seconds','highest_sample_peak_dbfs','checks_passed','checks_total','failures','measurements')},indent=2))
if report['failures']:raise SystemExit(1)
