#!/usr/bin/env python3
"""Classify complete Brass inventory. Input capture --inventory JSONs."""
import csv,json,hashlib,struct,sys,collections
from pathlib import Path
out=Path(__file__).resolve().parent
j=json.load(open(sys.argv[1]));old=json.load(open(sys.argv[2]));ids=j['parameter_ids'];before={p['host_index']:p for p in old['programs']}
recipes=['Velvet Brass','Covered Horn','Distant Brass','Fanfare Brass','Muted Trumpet','Horn Section','Contour Brass','Brass Bite','Low Brass','Brass Crescendo','Bright Brass Poly','Reed Brass','Brass Ensemble 16','Covered Horn','Brass Swell I','Noir Swell Brass','Brass Pad Swell','Sforzando Brass','Synth Horns','Brass Octaves','Swell And Fall','Brass Stab']
source={22:'Noir Swell Brass',23:'Brass Pad Swell',24:'Covered Horn',25:'Ring Breath Brass',**dict(enumerate(recipes,44))}
acoustic={'Horn Section','Covered Horn','Muted Trumpet','Trombone Glide','Low Brass','Tuba'}
rows=[]
for p in j['programs']:
 if p['category']!='Brass':continue
 v=dict(zip(ids,p['values']));recipe=source.get(p['host_index'],p['name']);models=[v[f'body{n}_model'] for n in (1,2)]
 kind='acoustic-inspired' if recipe in acoustic else 'electronic' if models==[0,0] else 'hybrid'
 row={'host_index':p['host_index'],'name':p['name'],'origin':'original' if p['host_index']<222 else 'curated','recipe':recipe,'class':kind,'sha256_float32':hashlib.sha256(struct.pack('<'+'f'*len(p['values']),*p['values'])).hexdigest(),'unchanged_from_baseline':ids==old['parameter_ids'] and p['values']==before[p['host_index']]['values'],'description':p['description'],'tags':'|'.join(p['tags'])}
 for key in ['mix','detune','analog_noise','portamento','init_bend','sub_at_vco','sub_at_vcf','ring_amount','chorus_on','fx1_type','fx1_mix','fx2_type','fx2_mix']:
  if key in v:row[key]=v[key]
 for n in (1,2):
  for key in ['saw','pulse','sine','noise','pw','pwm_depth','pwm_speed','feet','hpf_on','lpf_on','f_il','f_al','f_a','f_d','f_r','vcf_level','hpf_cutoff','lpf_cutoff','hpf_res','lpf_res','v_a','v_d','v_s','v_r','v_level','init_brill','init_level','after_brill','after_level']:
   if f'ch{n}_{key}' in v:row[f'ch{n}_{key}']=v[f'ch{n}_{key}']
  for key in ['model','mix','brightness','size']:row[f'body{n}_{key}']=v[f'body{n}_{key}']
 rows.append(row)
with (out/'brass-inventory.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
summary={'count':len(rows),'classes':dict(collections.Counter(r['class'] for r in rows)),'origins':dict(collections.Counter(r['origin'] for r in rows)),'all_vectors_unchanged':all(r['unchanged_from_baseline'] for r in rows),'class_names':{c:[r['name'] for r in rows if r['class']==c] for c in ('acoustic-inspired','hybrid','electronic')},'feet_pairs':dict(collections.Counter(str((r['ch1_feet'],r['ch2_feet'])) for r in rows)),'definition':'Intent classification follows the named source recipe; acoustic-inspired is not an acoustic-fidelity claim. Electronic has both body models off; hybrid covers remaining modeled brass with circuit/effect/mixed-instrument intent.'}
(out/'classification.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps({k:v for k,v in summary.items() if k!='class_names'},indent=2))
