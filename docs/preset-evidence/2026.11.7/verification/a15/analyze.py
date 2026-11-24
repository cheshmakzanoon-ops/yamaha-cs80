#!/usr/bin/env python3
"""Retained A15 actual-program integration analysis. PCM stays under /tmp.
Usage: python3 analyze.py /tmp/cs80-a15-1cee5a <evidence-directory>
Inventory files are supplied cs80_preset_capture --library curated --inventory.
"""
import csv, hashlib, json, math, pathlib, sys
import numpy as np

root,out=map(pathlib.Path,sys.argv[1:]); out.mkdir(parents=True,exist_ok=True)
names=['Guitar II','Pressure Pluck','Harp','Koto','Banjo']; notes=[48,60,72]
db=lambda x:20*math.log10(max(float(x),1e-15))
def window(x,a,b):return x[round(a*48000):round(b*48000)]
def rms(x,a,b):return float(np.sqrt(np.mean(window(x,a,b).astype(float)**2)))
def spectrum(x,a,b):
    y=window(x,a,b).mean(axis=1); n=1<<int(math.ceil(math.log2(len(y)*8)))
    p=np.abs(np.fft.rfft(y*np.hanning(len(y)),n))**2
    return np.fft.rfftfreq(n,1/48000),p
def centroid(x,a,b):
    f,p=spectrum(x,a,b);return float((f*p).sum()/p.sum())
def pitch(x,a,b,expected):
    f,p=spectrum(x,a,b);keep=np.where((f>expected*.93)&(f<expected*1.07))[0]
    i=keep[np.argmax(p[keep])];q=np.log(p[i-1:i+2]+1e-60)
    frac=.5*(q[0]-q[2])/(q[0]-2*q[1]+q[2])
    return float(f[i]+frac*(f[1]-f[0]))
def delta(a,b,start,end):return db(rms(a-b,start,end)/max(rms(b,start,end),1e-30))
checks=[]
def check(condition,label):checks.append({'pass':bool(condition),'label':label})

cur=json.loads((root/'current-inventory.json').read_text()); old=json.loads((root/'baseline-inventory.json').read_text())
selected=[p for p in cur['programs'] if p['category']=='Plucked']
check(len(selected)==17,'all 17 existing curated Plucked programs present')
old_by={p['name']:p for p in old['programs']}
expected={
 'Guitar I':(6,6,0,0,1,1,'nylon body / brief mixed finger-pick'),
 'Guitar II':(7,7,0,0,1,1,'steel body / brief mixed pick'),
 'Funky Pluck I':(4,4,0,0,1,1,'clav-circuit snap / short upper snap'),
 'Funky Pluck II':(4,7,0,0,1,1,'clav-circuit wah / short steel snap'),
 'Harp':(6,6,9,9,1,1,'long harp soundboard / same-pitch short contact'),
 'Koto':(7,7,8,8,1,1,'koto bridge body / same-pitch short contact'),
 'Banjo':(7,7,7,7,1,1,'banjo membrane body / same-pitch short contact'),
 'Mandolin':(7,7,0,0,1,1,'paired close steel courses; 1.8-cent offset'),
 'Drone Pluck':(7,7,0,0,1,1,'steel-circuit drone with pressure / short pick'),
 'Nylon Pluck':(6,6,0,0,1,1,'nylon body / soft mixed finger attack'),
 'Steel Pluck':(7,7,0,0,1,3,'8-foot steel body / shorter 4-foot course'),
 'Muted Pluck':(7,4,0,0,1,1,'rapidly damped steel / clav pick click'),
 'Kalimba':(2,2,6,6,1,1,'kalimba tine / same-pitch short contact'),
 'Zither':(7,7,0,0,1,1,'unequal steel courses; restrained chorus and detune'),
 'Dulcimer Pluck':(7,7,0,0,1,3,'8-foot steel body / short 4-foot course'),
 'Pressure Pluck':(6,7,0,0,1,1,'nylon-circuit pressure bed / short steel pick'),
 'Stereo Delay Pluck':(7,7,0,0,1,3,'8-foot steel body / short 4-foot pick into delay')}
coverage=[]
for p in selected:
    v=dict(zip(cur['parameter_ids'],p['values'])); exp=expected[p['name']]
    check(p==old_by[p['name']],p['name']+': entire inventory entry unchanged from baseline')
    actual=tuple(v[k] for k in ['body1_model','body2_model','body1_material','body2_material','ch1_feet','ch2_feet'])
    check(actual==exp[:6],p['name']+': intended models, materials and footages')
    coverage.append({'name':p['name'],'host_index':p['host_index'],'role':exp[6],
       'rank_1':{k:v[k] for k in v if k.startswith('body1_') or k.startswith('ch1_')},
       'rank_2':{k:v[k] for k in v if k.startswith('body2_') or k.startswith('ch2_')},
       'unchanged_inventory':p==old_by[p['name']]})
(out/'structural-coverage.json').write_text(json.dumps(coverage,indent=2)+'\n')
rows=[];audio_meta=[]
for p,name in enumerate(names):
  for note in notes:
    blocks={}
    for rev in ['current','baseline']:
      for mode in range(4):
        path=root/f'{rev}-pcm'/f'p{p}_n{note}_m{mode}.f32'; data=path.read_bytes()
        x=np.frombuffer(data,dtype='<f4').reshape(-1,2);blocks[rev,mode]=x
        peak=float(np.max(np.abs(x)));check(np.isfinite(x).all() and 1e-6<peak<1,f'{rev} {name} {note} mode {mode}: finite audible headroom')
        audio_meta.append({'revision':rev,'name':name,'note':note,'mode':mode,'sha256':hashlib.sha256(data).hexdigest(),'peak':peak})
    x=blocks['current',0]; bent=blocks['current',1]; press=blocks['current',2]; pedal=blocks['current',3]
    row={'name':name,'note':note,'peak':max(float(np.max(np.abs(blocks['current',m]))) for m in range(4)),
      'held_decay_db':db(rms(x,.82,.98)/rms(x,.02,.15)),
      'early_centroid_hz':centroid(x,.02,.15),'late_centroid_hz':centroid(x,.82,.98),
      'key_release_db':db(rms(x,1.35,1.60)/rms(x,.82,.98)),
      'pedal_advantage_db':db(rms(pedal,1.35,1.60)/max(rms(x,1.35,1.60),1e-30)),
      'pressure_change_db':db(rms(press,.46,.66)/rms(x,.46,.66)),
      'pressure_difference_db':delta(press,x,.46,.66),
      'current_baseline_plain_difference_db':delta(x,blocks['baseline',0],.15,.65),
      'current_baseline_bend_difference_db':delta(bent,blocks['baseline',1],.15,.65),
      'baseline_bend_peak':float(np.max(np.abs(blocks['baseline',1])))}
    fundamental=440*2**((note-69)/12)
    for label,a,b,semitones in [('up',.18,.30,2),('down',.48,.60,-2),('return',.80,.95,0)]:
      ref=pitch(x,a,b,fundamental); freq=pitch(bent,a,b,fundamental*2**(semitones/12))
      row[label+'_cents']=1200*math.log2(freq/ref)
      check(abs(row[label+'_cents']-semitones*100)<8,f'{name} {note}: {label} audible pitch follows wheel within 8 cents')
    check(row['held_decay_db']<-3,f'{name} {note}: attack loses energy while held')
    check(row['late_centroid_hz']<row['early_centroid_hz'],f'{name} {note}: upper spectrum softens during held decay')
    check(row['pedal_advantage_db']>.35,f'{name} {note}: pedal postpones key damping')
    if name=='Pressure Pluck':check(row['pressure_change_db']>.3,f'{name} {note}: authored pressure route remains audible')
    else:check(np.array_equal(press,x),f'{name} {note}: zero authored aftertouch depth leaves PCM unchanged')
    if name in ['Harp','Koto','Banjo']:
      row['all_four_modes_bit_identical']=all(np.array_equal(blocks['current',m],blocks['baseline',m]) for m in range(4))
      check(row['all_four_modes_bit_identical'],f'{name} {note}: all gestures preserve .6 material PCM exactly at 48 kHz')
    rows.append(row)
fields=list(dict.fromkeys(k for row in rows for k in row))
with (out/'gesture-metrics.csv').open('w') as f:
    w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
(out/'pcm-manifest.json').write_text(json.dumps(audio_meta,indent=2)+'\n')
summary={'sample_rate':48000,'notes':notes,'duration_seconds':2.2,'velocity':.8,
 'modes':{'0':'note-off 1.0s','1':'wheel +2st .1s, -2st .35s, return .65s; note-off 1.0s',
 '2':'pressure .8 at .35s, zero .7s; note-off 1.0s','3':'pedal on 0s, note-off 1.0s, pedal off 1.7s'},
 'checks':len(checks),'failures':[c for c in checks if not c['pass']],
 'maximum_current_peak':max(r['peak'] for r in rows),
 'worst_pitch_error_cents':max(abs(r[k+'_cents']-s) for r in rows for k,s in [('up',200),('down',-200),('return',0)]),
 'pressure_pluck_gain_db':[r['pressure_change_db'] for r in rows if r['name']=='Pressure Pluck'],
 'special_material_modes_bit_identical':all(r.get('all_four_modes_bit_identical',True) for r in rows),
 'evidence_type':'Source review and offline PCM measurement; no listening performed; no acoustic recording comparison.'}
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
(out/'checks.json').write_text(json.dumps(checks,indent=2)+'\n')
print(json.dumps(summary,indent=2))
