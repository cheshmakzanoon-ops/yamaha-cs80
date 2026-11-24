#!/usr/bin/env python3
import csv,json,pathlib
import numpy as np
OUT=pathlib.Path(__file__).resolve().parent;TMP=pathlib.Path('/tmp/cs80-a13/pedal');rows=[];by={}
def rms(a):return float(np.sqrt(np.mean(a.astype('float64')**2)))
def db(x):return float(20*np.log10(max(x,1e-15)))
for side in ('before','after'):
 for name in ('Stage-Keys','Soft-Keys','Electric-Piano-I','Reed-Piano'):
  a=[np.fromfile(TMP/side/f'{name}-m{i}.f32',dtype='float32').reshape(-1,2) for i in range(3)]
  for i,x in enumerate(a):by[(side,name,i)]=x
  held=lambda i,lo,hi: a[i][round(lo*48000):round(hi*48000)]
  before_release=db(rms(held(1,1.6,2)))
  row=dict(side=side,name=name,note=36 if name.endswith('Keys') else 60,velocity=.85,finite=bool(all(np.isfinite(x).all() for x in a)),peak_dbfs=db(max(float(np.abs(x).max()) for x in a)),pedal_vs_held_max_sample_difference=float(np.max(np.abs(a[1]-a[2]))),early_keyoff_rms_dbfs=db(rms(held(0,.8,1.5))),same_time_pedal_rms_dbfs=db(rms(held(1,.8,1.5))),key_damping_db=db(rms(held(0,.8,1.5)))-db(rms(held(1,.8,1.5))),pedal_before_up_rms_dbfs=before_release,pedal_after_up_rms_dbfs=db(rms(held(1,2.6,3))),pedal_up_attenuation_db=db(rms(held(1,2.6,3)))-before_release)
  rows.append(row)
with open(OUT/'pedal-metrics.csv','w') as f:
 w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
print(json.dumps(rows,indent=2))
# Useful direct early-release versus held ratios and touch preservation.
w=list(csv.DictReader(open(OUT/'window-metrics.csv')));tab={(x['side'],x['case'],x['host_index'],x['window']):x for x in w};touch=[]
for idx in (350,352):
 for note in (36,60):
  for side in ('before','after'):
   firm=tab[(side,f'pianos-n{note}-firm',str(idx),'onset')];soft=tab[(side,f'pianos-n{note}-soft',str(idx),'onset')]
   touch.append(dict(side=side,host_index=idx,note=note,firm_soft_rms_db=float(firm['rms_dbfs'])-float(soft['rms_dbfs']),firm_soft_upper_low_db=float(firm['upper_low_db'])-float(soft['upper_low_db'])))
for idx in (336,337,338,355):
 for side in ('before','after'):
  firm=tab[(side,'ep-n60-v0.85',str(idx),'onset')];soft=tab[(side,'ep-n60-v0.25',str(idx),'onset')]
  touch.append(dict(side=side,host_index=idx,note=60,firm_soft_rms_db=float(firm['rms_dbfs'])-float(soft['rms_dbfs']),firm_soft_upper_low_db=float(firm['upper_low_db'])-float(soft['upper_low_db'])))
with open(OUT/'touch-metrics.csv','w') as f:
 w=csv.DictWriter(f,fieldnames=touch[0]);w.writeheader();w.writerows(touch)
print('TOUCH',json.dumps(touch,indent=2))
