#!/usr/bin/env python3
"""Summarize actual-recipe PCM and review all effective Bell registrations."""
import csv,hashlib,json,pathlib,datetime
import numpy as np
from scipy.io import wavfile
E=pathlib.Path(__file__).resolve().parent
ROOT=E.parents[4]
T=pathlib.Path('/tmp/cs80-a17-1cee5a')
def db(x):return float(20*np.log10(max(float(x),1e-15)))
def rms(x):return float(np.sqrt(np.mean(np.asarray(x,dtype=float)**2)))
def win(x,a,b):return x[round(a*48000):round(b*48000)]
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
cur=json.loads((T/'current-inventory.json').read_text());old=json.loads((T/'baseline-inventory.json').read_text())
oldmap={p['name']:p for p in old['programs']};bells=[p for p in cur['programs'] if p['category']=='Bells']
models={0:'circuit sine',3:'reed',16:'bell',17:'vibes',18:'wood bar',19:'celesta bar'}
materials={0:'standard',1:'tube',2:'gong plate',3:'temple shell',4:'agogo shell',5:'comb tooth'}
variants={0:'standard',1:'xylophone',2:'steelpan'}
feet={0:'16',1:'8',2:'5 1/3',3:'4',4:'2 2/3',5:'2'}
roles=[
 'long bell / short octave strike; fixed ring 307 Hz','dark bell / upper twelfth strike; ring 191 Hz',
 'same-pitch tube body / brief hard contact','high bar / tiny octave mallet','soft bar / octave accent; chorus and short release',
 'same-pitch comb / 75 ms contact','delayed plate bloom / very quiet same-pitch contact','paired shell modes / same-pitch contact',
 'wood body / fourth-harmonic knock','metal bar / fourth-harmonic circuit sine; tremolo and damper','high wood body / same-pitch dry strike',
 'bell body / octave celesta strike; ring edge','bell-sine bed / short upper edge; intentionally sustained','bell / upper twelfth strike; swept ring carrier',
 'steelpan body / octave shell accent','bell / quieter octave celesta course','low bell / short octave strike; restrained ring',
 'bell / shorter fifth bell; chorus','reed-saw body / octave bell strike; synthetic ring',
 'same-pitch compact shell / brief hard contact','two bell courses a fifth apart','wood / upper twelfth metal strike; ring',
 'bell / shorter fifth course; delay and plate']
rows=[]
for p,role in zip(bells,roles):
 v=dict(zip(cur['parameter_ids'],p['values']))
 r={'curated_index':p['index'],'host_index':p['host_index'],'name':p['name'],'identical_to_11_6':p==oldmap[p['name']],'role':role}
 for rank in (1,2):
  for suffix in ('model','mix','brightness','decay','size','variant','material'):r[f'body{rank}_{suffix}']=v[f'body{rank}_{suffix}']
  for suffix in ('feet','v_a','v_d','v_s','v_r','v_level','init_level'):r[f'ch{rank}_{suffix}']=v[f'ch{rank}_{suffix}']
  r[f'rank{rank}_identity']=models[int(v[f'body{rank}_model'])]+'/'+materials[int(v[f'body{rank}_material'])]+'/'+variants[int(v[f'body{rank}_variant'])]
  r[f'rank{rank}_feet']=feet[int(v[f'ch{rank}_feet'])]
 for k in ('mix','ring_speed','ring_depth','ring_amount','oversampling','volume'):r[k]=v[k]
 rows.append(r)
assert len(rows)==23 and all(r['identical_to_11_6'] for r in rows)
with (E/'inventory.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
NAMES={'Tubular Bells':(238,1,2.),'Gong':(242,.5,3.153),'Temple Bell':(243,.5,2.361),'Agogo':(255,2,1.873),'Music Box':(241,2,6.2669)}
def get(label,n,stem):
 path=next((T/f'{label}_n{n}').glob(f'{stem}-*.wav'));rate,x=wavfile.read(path);assert rate==48000;return path,x
# Hann projection at the modeled ratio, permitting normal small dispersion.
def tone(x,a,b,hz):
 y=win(x,a,b).astype(float);w=np.hanning(len(y));t=np.arange(len(y))/48000
 best=(0.,hz)
 for cents in np.arange(-20,20.1,1):
  f=hz*2**(cents/1200);z=np.sum(y*w[:,None]*np.exp(-2j*np.pi*f*t[:,None]),axis=0)
  amp=2*np.sqrt(np.mean(abs(z)**2))/sum(w)
  if amp>best[0]:best=(float(amp),float(f))
 return best
metrics=[];comparisons=[]
for name,(idx,foot,ratio) in NAMES.items():
 for n in (48,60,72):
  path,x=get('current',n,idx);key=440*2**((n-69)/12)*foot
  a,b=(.65,1.5) if name=='Gong' else (.025,.25)
  amp,freq=tone(x,a,b,key*ratio)
  r={'name':name,'midi_note':n,'sounding_reference_hz':key,'characteristic_ratio':ratio,'detected_mode_hz':freq,'mode_db_relative_window_rms':db(amp/rms(win(x,a,b))),'peak_dbfs':db(np.max(np.abs(x))),'onset_10_80ms_dbfs':db(rms(win(x,.01,.08))),'body_250_750ms_dbfs':db(rms(win(x,.25,.75))),'aftersound_1_5_2s_dbfs':db(rms(win(x,1.5,2))),'nonfinite':int(np.sum(~np.isfinite(x))),'over_full_scale':int(np.sum(np.abs(x)>=1))}
  if n==60:
   oldpath,y=get('baseline',n,idx);same=np.array_equal(x,y)
   comparisons.append({'name':name,'exact_pcm_match':same,'max_abs_delta':float(np.max(np.abs(x-y))),'current_sha256':sha(path),'baseline_sha256':sha(oldpath)})
   r.update({'held_5_6s_dbfs':db(rms(win(x,5,6))),'released_8_5_9_5s_dbfs':db(rms(win(x,8.5,9.5)))})
   if name=='Gong':r['upper_bloom_db']=db((tone(x,.8,1.5,key*ratio)[0]/tone(x,.8,1.5,key)[0])/(tone(x,.025,.14,key*ratio)[0]/tone(x,.025,.14,key)[0]))
   if name=='Tubular Bells':r['octave_over_strike_fundamental_db']=db(tone(x,.04,.3,2*key)[0]/tone(x,.04,.3,key)[0])
   if name=='Music Box':r['comb_over_free_bar_overtone_db']=db(tone(x,.015,.16,6.2669*key)[0]/tone(x,.015,.16,2.756*key)[0])
  metrics.append(r)
fields=list(dict.fromkeys(k for r in metrics for k in r))
with (E/'matrix.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(metrics)
hq=[]
for name,mode in [('Agogo',0),('Agogo',1),('Agogo',2),('Gong',2)]:
 x=np.fromfile(T/'contact-current-pcm'/f'{name}_{mode}.f32',np.float32).reshape(-1,2);y=np.fromfile(T/'contact-baseline-pcm'/f'{name}_{mode}.f32',np.float32).reshape(-1,2)
 hq.append({'name':name,'scenario':('default-single','HQ-single','default-six-note-chord')[mode],'peak_dbfs':db(np.max(abs(x))),'onset_current_dbfs':db(rms(win(x,.001,.02))),'onset_baseline_dbfs':db(rms(win(y,.001,.02))),'difference_0_20ms_dbfs':db(rms(win(x-y,0,.02))),'difference_0_25_3s_max_abs':float(np.max(abs(win(x-y,.25,3)))),'exact_pcm_match':np.array_equal(x,y),'nonfinite':int(np.sum(~np.isfinite(x))),'over_full_scale':int(np.sum(abs(x)>=1))})
comp={}
for label in ('current','baseline'):
 a=np.loadtxt(E/f'component-{label}.csv',delimiter=',');comp[label]=[{'rank':int(a[i,0]),'hq_vs_reference_contact_rms_db':db(a[i+1,2]/a[i,2]),'reference_rms':float(a[i,2]),'hq_rms':float(a[i+1,2])} for i in (0,2)]
result={'scope':'Authored model/PCM validation; no listening or recorded-instrument comparison','inventory_count':len(bells),'all_inventory_entries_identical_to_11_6':True,'default_pcm_controls':comparisons,'hq_and_chord':hq,'isolated_agogo_contact_128_seeds':comp}
(E/'results.json').write_text(json.dumps(result,indent=2)+'\n')
files=['src/core/InstrumentBody.h','src/core/InstrumentBodyMaterials.h','src/core/InstrumentBodyVariants.h','src/core/curated/Bells.inc','src/core/curated/Modern.inc','src/core/FactoryPresets.h','/tmp/cs80-build-1cee5a/cs80_preset_capture','/tmp/cs80-reference-1cee5a/cs80_preset_capture','/tmp/cs80-build-1cee5a/libcs80_core.a','/tmp/cs80-reference-1cee5a/libcs80_core.a','/tmp/cs80-a15-1cee5a/baseline/src/core/InstrumentBody.h']
(E/'binding.json').write_text(json.dumps({'snapshot':'A17 development snapshot, not final release qualification','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'baseline_commit':'18711f93a39aa49f3d6f9a41784be456224facbe','abi':'Baseline probes use git-archived 18711f93 full src headers; current probes use current src. Separate matching libraries.','settings':'48 kHz host, standard 2x; .85 velocity; pressure 0; actual factory params. Five full-patch 8s hold + 2s release at MIDI60; MIDI48/72 3s held. HQ Agogo changes only oversampling=2 (8x). Chords 48,55,60,64,67,72 at velocity1, 3s held.','contact_diagnostic':'Agogo authored body parameters; silence modal count after noteOn, 128 seeds, 20ms RMS; diagnostic separate from actual Engine captures.','sha256':{p:sha(p) for p in files}},indent=2)+'\n')
print(json.dumps(result,indent=2));print('MATRIX MIDI60');print(json.dumps([r for r in metrics if r['midi_note']==60],indent=2))
