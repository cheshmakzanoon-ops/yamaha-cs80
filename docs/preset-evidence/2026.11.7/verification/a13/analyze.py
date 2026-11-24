#!/usr/bin/env python3
"""Full-output measurements, never post-render normalizing PCM. No hearing claim."""
import csv,hashlib,json,math,pathlib
import numpy as np
from scipy.io import wavfile
OUT=pathlib.Path(__file__).resolve().parent;ROOT=OUT.parents[4];TMP=pathlib.Path('/tmp/cs80-a13')
def db(x):return float(20*np.log10(max(float(x),1e-15)))
def rms(a):return float(np.sqrt(np.mean(a.astype('float64')**2)))
def spectral(a,rate,note,lo,hi):
 s=a[round(lo*rate):round(hi*rate)].astype('float64');w=np.hanning(len(s))[:,None]
 p=(np.abs(np.fft.rfft(s*w,axis=0))**2).sum(axis=1);f=np.fft.rfftfreq(len(s),1/rate);f0=440*2**((note-69)/12)
 def power(l,h):return float(p[(f>=l)&(f<h)].sum())
 low=power(.7*f0,6.5*f0);upper=power(6.5*f0,min(30.5*f0,rate*.49))
 h6=power(5.78*f0,6.22*f0);h7=power(6.78*f0,7.22*f0)
 odd=sum(power((h-.15)*f0,(h+.15)*f0) for h in (1,3,5,7));even=sum(power((h-.15)*f0,(h+.15)*f0) for h in (2,4,6,8))
 return dict(rms_dbfs=db(rms(s)),upper_low_db=db(np.sqrt(upper/max(low,1e-30))),h7_h6_db=db(np.sqrt(h7/max(h6,1e-30))),even_odd_db=db(np.sqrt(even/max(odd,1e-30))),centroid_hz=float((f*p).sum()/max(p.sum(),1e-30)))
def writecsv(path,rows):
 with open(path,'w') as f:
  w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
manifest=json.loads((OUT/'capture-manifest.json').read_text());measurements=[];arrays={}
for j in manifest:
 for path in sorted((TMP/j['side']/j['label']).glob('*.wav')):
  rate,a=wavfile.read(path);idx=int(path.name.split('-')[0]);arrays[(j['side'],j['label'],idx)]=a
  windows=[('onset',.02,min(.2,j['hold'])),('body',.1,min(.6,j['hold']))]
  if j['hold']>=3:windows += [('settled',2.2,2.9)]
  if j['hold']>=6:windows += [('late',4.4,5.5)]
  if j['hold']>=12:windows += [('long',9,11)]
  windows += [('released',j['hold']+.6,j['hold']+min(1,j['tail']))]
  for label,lo,hi in windows:
   if hi>lo:measurements.append(dict(side=j['side'],case=j['label'],host_index=idx,note=j['note'],velocity=j['velocity'],window=label,start=lo,end=hi,**spectral(a,rate,j['note'],lo,hi)))
writecsv(OUT/'window-metrics.csv',measurements)
lookup={(r['side'],r['case'],r['host_index'],r['window']):r for r in measurements}
comparisons=[]
for key,a in arrays.items():
 side,case,idx=key
 if side!='after':continue
 b=arrays[('before',case,idx)];delta=a.astype('float64')-b
 entry=dict(case=case,host_index=idx,peak_before_dbfs=db(np.max(np.abs(b))),peak_after_dbfs=db(np.max(np.abs(a))),peak_change_db=db(np.max(np.abs(a)))-db(np.max(np.abs(b))),max_sample_difference=float(np.max(np.abs(delta))),difference_rms_dbfs=db(rms(delta)),finite=bool(np.isfinite(a).all()),over_full_scale=int((np.abs(a)>1).sum()))
 for window in ('onset','settled','late','long'):
  k=('after',case,idx,window)
  if k in lookup:
   r=lookup[k];q=lookup[('before',case,idx,window)]
   for metric in ('rms_dbfs','upper_low_db','h7_h6_db'):
    entry[window+'_'+metric+'_change']=r[metric]-q[metric]
 comparisons.append(entry)
fields=list(dict.fromkeys(k for row in comparisons for k in row));comparisons=[{k:r.get(k,'') for k in fields} for r in comparisons];writecsv(OUT/'comparisons.csv',comparisons)
# Structurally classify every existing Keys host/MIDI address from actual saved data.
a=json.loads((TMP/'current-inventory.json').read_text());b=json.loads((ROOT/'docs/preset-evidence/2026.11.7/development/baseline-inventory.json').read_text());ids=a['parameter_ids']
models={0:'off',1:'piano',2:'tine',3:'reed',4:'clav',5:'harpsichord',7:'steel',16:'bell',17:'vibes',19:'celesta'}
recipes=['Electric Piano I','Soft Keys','Reed Piano','Stage Keys','Clavichord II','Saw Pulse Piano','Glass Keys','Electric Piano II','Tremolo Keys','Tine Piano','Harpsichord I','Lute Harpsichord','Chorus Keys','Vibes Keys','Contour Keys','Celesta','Dulcimer','Funky Clavichord']
structures=[]
for p in a['programs']:
 if p['category']!='Keys':continue
 v=dict(zip(ids,p['values']));idx=p['host_index'];cur=idx-222;addr=idx if idx<222 else cur
 recipe={38:'Stage Keys',39:'Chorus Keys'}.get(idx,recipes[idx-140] if 140<=idx<=157 else p['name'])
 role=('synthetic piano/circuit sustaining bed' if recipe=='Pressure Keys' else 'synthetic saw/pulse piano hybrid' if recipe=='Saw Pulse Piano' else 'synthetic filter-accent clav hybrid' if recipe=='Funky Clavichord' else 'acoustic-style struck strings' if v['body1_model']==1 else 'quill-string interpretation' if v['body1_model']==5 else 'tangent-string interpretation' if v['body1_model']==4 else 'hammered-steel interpretation' if v['body1_model']==7 else 'electric pickup keyboard' if v['body1_model'] in (2,3) else 'struck metal/bar interpretation')
 paths=['A05 contact (reference gain=1 at 48k/2x)']
 if any(v[f'body{r}_model']==1 for r in (1,2)):paths.insert(0,'A04 piano principal losses')
 if any(v[f'body{r}_model'] in (4,5,7) for r in (1,2)):paths.insert(0,'A06 live fixed-formant correction (also small drift)')
 s=dict(host_index=idx,name=p['name'],bank_msb=1 if idx<222 else 0,bank_lsb=addr//128,program_change=addr%128,source_recipe=recipe,intended_role=role,affected_paths='; '.join(paths))
 for r in (1,2):
  s[f'rank{r}_model']=models[int(v[f'body{r}_model'])]
  for q in ('mix','brightness','decay','size'):s[f'rank{r}_body_{q}']=v[f'body{r}_{q}']
  for q in ('feet','hpf_cutoff','lpf_cutoff','v_s','v_r','init_level','after_level'):s[f'rank{r}_{q}']=v[f'ch{r}_{q}']
 structures.append(s)
writecsv(OUT/'keys-structure.csv',structures)
summary=dict(program_count=45,piano_count=sum('A04' in s['affected_paths'] for s in structures),a06_count=sum('A06' in s['affected_paths'] for s in structures),inventory_values_and_metadata_unchanged=a['programs']==b['programs'],before_revision=b['library_revision'],after_revision=a['library_revision'],full_patch_captures=len(arrays),case_pairs=len(comparisons),pcm_frames_per_side=sum(len(v) for k,v in arrays.items() if k[0]=='after'),all_finite=all(r['finite'] for r in comparisons),over_full_scale=sum(r['over_full_scale'] for r in comparisons),max_peak_change_db=max(r['peak_change_db'] for r in comparisons),max_peak_after_dbfs=max(r['peak_after_dbfs'] for r in comparisons),ep_exact_pairs=sum(r['case'].startswith('ep') and r['max_sample_difference']==0 for r in comparisons),hashes={})
for path in ['/tmp/cs80-reference-1cee5a/cs80_preset_capture','/tmp/cs80-build-1cee5a/cs80_preset_capture','/tmp/cs80-reference-1cee5a/libcs80_core.a','/tmp/cs80-build-1cee5a/libcs80_core.a','src/core/InstrumentBody.h','src/core/curated/Keys.inc','src/core/OriginalCircuitLibrary.inc']:
 pp=pathlib.Path(path); pp=pp if pp.is_absolute() else ROOT/pp; summary['hashes'][path]=hashlib.sha256(pp.read_bytes()).hexdigest()
(OUT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
