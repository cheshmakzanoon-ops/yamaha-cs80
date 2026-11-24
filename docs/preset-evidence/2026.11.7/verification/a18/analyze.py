#!/usr/bin/env python3
"""Reduce actual A18 preset captures; spectra are descriptive, not auditions."""
import collections,csv,hashlib,json,pathlib
import numpy as np
HERE=pathlib.Path(__file__).resolve().parent; TMP=pathlib.Path('/tmp/cs80-a18'); RATE=48000
def dump(name,value): (HERE/name).write_text(json.dumps(value,indent=2)+'\n')
def db(x):return float(20*np.log10(max(float(x),1e-15)))
def rms(x):return float(np.sqrt(np.mean(x*x)))
def window(x,a,b):return x[int(a*RATE):int(b*RATE)]
def table(name,rows):
 with (HERE/name).open('w') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
current=json.loads((TMP/'current-inventory.json').read_text());baseline=json.loads((TMP/'baseline-inventory.json').read_text())
ids=current['parameter_ids'];previous={p['name']:p for p in baseline['programs']}
rows=[];params={}
for p in current['programs']:
 if p['category']!='Strings':continue
 v=dict(zip(ids,p['values']));params[p['name']]=v
 m=[v[f'body{r}_model'] if v[f'body{r}_mix']>0 else 0 for r in (1,2)]
 kind='circuit-only synthetic' if m==[0,0] else 'plucked accent' if any(x in (6,7) for x in m) else 'solo bowed' if p['name'] in ('Solo Violin','Solo Cello','Soft Strings 36','Midnight Strings 45') else 'detached bowed' if v['ch1_v_s']==0 else 'modeled ensemble/hybrid'
 keys=['mix','volume','chorus_on','chorus_depth','detune','kbd_brilliance_low','kbd_brilliance_high','kbd_level_low','kbd_level_high']
 for r in (1,2):
  keys += [f'body{r}_{k}' for k in ('model','mix','brightness','decay','size','variant','material')]
  keys += [f'ch{r}_{k}' for k in ('saw','pulse','sine','noise','feet','hpf_cutoff','lpf_cutoff','v_a','v_d','v_s','v_r','init_level','after_level','after_brill')]
 rows.append(dict(index=p['index'],name=p['name'],bank='original' if p['index']<222 else 'curated',classification=kind,description=p['description'],unchanged_complete_identity_and_values=p==previous[p['name']],vector_sha256=hashlib.sha256(np.array(p['values'],dtype='<f4').tobytes()).hexdigest(),recipe={k:v[k] for k in keys}))
assert len(rows)==59 and all(x['unchanged_complete_identity_and_values'] for x in rows)
dump('structural-coverage.json',{'count':len(rows),'classes':dict(collections.Counter(x['classification'] for x in rows)),'all_complete_identities_and_vectors_unchanged':True,'programs':rows})
metadata={}
for side in ('current','baseline'):
 for r in csv.DictReader((HERE/f'{side}-probe.csv').open()):
  stem=r['name'].replace(' ','_')+'_'+r['quality']+'_'+r['scene'];metadata[side,stem]=r
assert len(metadata)==104, len(metadata)
audio={};metrics=[];manifest=[];wins={'onset':(.015,.20),'held':(.8,1.3),'returned':(1.6,1.9),'tail':(2.8,3.0)}
for (side,stem),meta in metadata.items():
 path=TMP/side/'gestures'/f'{stem}.f32';raw=path.read_bytes();x=np.frombuffer(raw,dtype='<f4').reshape(-1,2).astype(float);audio[side,stem]=x
 assert x.shape==(144000,2)
 row=dict(side=side,**meta,nonfinite=int((~np.isfinite(x)).sum()),over_full_scale=int((np.abs(x)>=1).sum()),peak_dbfs=db(np.max(np.abs(x))),rms_dbfs=db(rms(x)))
 for name,(a,b) in wins.items():row[name+'_rms_dbfs']=db(rms(window(x,a,b)))
 metrics.append(row);manifest.append(dict(side=side,stem=stem,sha256=hashlib.sha256(raw).hexdigest(),bytes=len(raw)))
table('window-metrics.csv',metrics);dump('pcm-manifest.json',manifest)
official=[]
for side in ('baseline','current'):
 for r in csv.DictReader((TMP/side/'capture/metrics.csv').open()):official.append(dict(side=side,**r))
table('official-native-metrics.csv',official)
dump('official-pcm-manifest.json',[dict(side=side,path=str(p),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for side in ('baseline','current') for p in sorted((TMP/side/'capture').glob('*.wav'))])
comparisons=[];bands=[];art=[]
for stem in sorted({s for _,s in audio}):
 meta=metadata['current',stem];old=audio['baseline',stem];new=audio['current',stem]
 row=dict(**meta,exact_pcm=bool(np.array_equal(old,new)),difference_relative_db=db(rms(new-old)/max(rms(new),1e-15)),peak_change_db=db(np.max(np.abs(new)))-db(np.max(np.abs(old))))
 for name,(a,b) in wins.items():row[name+'_rms_change_db']=db(rms(window(new,a,b)))-db(rms(window(old,a,b)))
 comparisons.append(row)
 if meta['scene']!='held':continue
 p=params[meta['name']];note=int(meta['note']);f0=440*2**((note-69)/12)*(.5 if p['ch1_feet']==0 else 1)
 for side in ('baseline','current'):
  for name,(a,b) in {'onset':(.015,.2),'held':(.8,1.3)}.items():
   x=window(audio[side,stem],a,b).mean(axis=1);w=np.hanning(len(x));f=np.fft.rfftfreq(len(x),1/RATE);power=np.abs(np.fft.rfft(x*w))**2/(len(x)*(w*w).sum());power[1:-1]*=2
   broad=(f>=1000)&(f<=12000);harmonic=np.abs(f/f0-np.round(f/f0))*f0
   bdict=dict(side=side,name=meta['name'],quality=meta['quality'],window=name,f0=f0,centroid_hz=float((f*power).sum()/power.sum()))
   for exclusion in (20,40):
    sel=broad&(harmonic>exclusion);bdict[f'nonharmonic_{exclusion}_dbfs']=db(np.sqrt(power[sel].sum()))
   for lo,hi,label in ((40,400,'low'),(400,2000,'mid'),(2000,12000,'high')):bdict[label+'_fraction']=float(power[(f>=lo)&(f<hi)].sum()/power.sum())
   bands.append(bdict)
for name in ('Solo Violin','Solo Cello','Cellos','PWM Strings'):
 for quality in ('native','hq'):
  stem=name.replace(' ','_')+'_'+quality+'_';held=audio['current',stem+'held'];press=audio['current',stem+'pressure'];early=audio['current',stem+'earlyoff']
  p=params[name];f0=440*2**((60-69)/12)*(.5 if p['ch1_feet']==0 else 1)
  pitches={}
  for scene,expected,a,b in [('low',f0/2,.8,1.3),('held',f0,.8,1.3),('high',f0*2,.8,1.3),('bend',f0*2**(p['pitch_bend_range']/12),.8,1.3),('bend_return',f0,1.6,1.9)]:
   source='bend' if scene=='bend_return' else scene;x=window(audio['current',stem+source],a,b).mean(axis=1);nfft=2**int(np.ceil(np.log2(len(x)*16)));spec=np.abs(np.fft.rfft(x*np.hanning(len(x)),n=nfft));f=np.fft.rfftfreq(nfft,1/RATE);ix=np.flatnonzero((f>expected*.96)&(f<expected*1.04));k=ix[np.argmax(spec[ix])];z=np.log(np.maximum(spec[k-1:k+2],1e-30));delta=.5*(z[0]-z[2])/(z[0]-2*z[1]+z[2]);estimate=(k+delta)*RATE/nfft;pitches[scene]={'expected_hz':expected,'estimated_hz':estimate,'cents':float(1200*np.log2(estimate/expected))}
  art.append(dict(name=name,quality=quality,pressure_rms_change_db=db(rms(window(press,.8,1.3)))-db(rms(window(held,.8,1.3))),earlyoff_vs_held_db=db(rms(window(early,.8,1.3)))-db(rms(window(held,.8,1.3))),pitches=pitches))
table('comparisons.csv',comparisons);table('spectral-windows.csv',bands);dump('articulation.json',art)
summary={'structural_programs':len(rows),'actual_probe_captures':len(metrics),'official_native_captures':8,'finite':all(x['nonfinite']==0 for x in metrics),'unclipped':all(x['over_full_scale']==0 for x in metrics),'peak_dbfs':max(x['peak_dbfs'] for x in metrics if x['side']=='current'),'max_peak_change_db':max(abs(x['peak_change_db']) for x in comparisons),'native_exact':sum(x['exact_pcm'] for x in comparisons if x['quality']=='native'),'control_exact':sum(x['exact_pcm'] for x in comparisons if x['name']=='PWM Strings'),'max_late_tail_dbfs':max(x['tail_rms_dbfs'] for x in metrics if x['side']=='current'),'max_abs_pitch_error_cents':max(abs(v['cents']) for r in art for v in r['pitches'].values())}
dump('summary.json',summary);print(json.dumps(summary,indent=2))
