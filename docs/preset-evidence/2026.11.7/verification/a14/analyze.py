#!/usr/bin/env python3
"""A14: inventory classification and four-program matched Engine PCM analysis."""
import hashlib,json,math,subprocess
from pathlib import Path
import numpy as np
from scipy.optimize import minimize_scalar
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[4]
TMP=Path('/tmp/cs80-a14')
RATE=48000
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def db(x):return 20*math.log10(max(float(x),1e-15))
def rms(x):return float(np.sqrt(np.mean(np.square(x,dtype=np.float64))))
def win(x,a,b):return x[int(a*RATE):int(b*RATE)]
before=json.loads((ROOT/'docs/preset-evidence/2026.11.7/development/baseline-inventory.json').read_text())
after=json.loads((TMP/'current-inventory.json').read_text())
ids=after['parameter_ids'];old={p['host_index']:p for p in before['programs']}
rows=[]
for p in after['programs']:
 if p['category']!='Bass':continue
 assert p['values']==old[p['host_index']]['values'],p['name']
 assert all(p[k]==old[p['host_index']][k] for k in ('index','host_index','name','category'))
 v=dict(zip(ids,p['values']));bodies=[]
 for r in (1,2):
  if v[f'body{r}_model'] and v[f'body{r}_mix']:
   m=int(v[f'body{r}_model']);bodies.append({'rank':r,'model':m,'mix':v[f'body{r}_mix'],'material':v[f'body{r}_material'],'size':v[f'body{r}_size'],'a06_affected':m in (4,5,6,7,8) and v[f'body{r}_material']==0})
 affected=any(b['a06_affected'] for b in bodies)
 role='standard bass/string body with authored circuit blend' if affected else 'brass-body/circuit hybrid; outside A06' if bodies else 'intentional electronic/circuit bass; outside A06'
 if p['name']=='Rubber Bass':role='intentional synthetic pitch scoop with bass-body accent; A06 applies to accent'
 if p['name'] in ('Pedal Bass','Velvet Bass 90'):role='intentional velocity-neutral electronic pedal registration; outside A06'
 rows.append({'host_index':p['host_index'],'name':p['name'],'role':role,'active_bodies':bodies,'a06_affected':affected,'all_values_identical_to_2026_11_6':True})
assert len(rows)==50
struct={'programs_checked':50,'body_enabled':sum(bool(r['active_bodies']) for r in rows),'a06_affected':sum(r['a06_affected'] for r in rows),'all_values_and_identities_unchanged':True,'programs':rows}
(HERE/'inventory-review.json').write_text(json.dumps(struct,indent=2)+'\n')
def pitch(x,wanted):
 # Fit the fundamental together with harmonics to reduce low-note leakage.
 y=np.mean(x,axis=1)[::4]; t=np.arange(len(y))/(RATE/4);w=np.hanning(len(y)); y=y*w
 def cost(f):
  mat=np.column_stack([fn(2*np.pi*f*h*t)*w for h in range(1,5) for fn in (np.sin,np.cos)]+[w])
  coeff=np.linalg.lstsq(mat,y,rcond=None)[0];return float(np.sum((mat@coeff-y)**2))
 fit=minimize_scalar(cost,bounds=(wanted*.985,wanted*1.015),method='bounded',options={'xatol':1e-6})
 return float(fit.x)
def partial(x,f,h):
 y=np.mean(x,axis=1); w=np.hanning(len(y));t=np.arange(len(y))/RATE
 return abs(np.sum(y*w*np.exp(-2j*np.pi*f*h*t)))*2/np.sum(w)
results=[]
for name in ('Analog Bass','Pluck Bass','Fretless Glide','Upright Pluck'):
 for scenario in ('steady','up_return','down_return','pressure'):
  stem=name.replace(' ','_')+'_'+scenario+'.f32'
  paths={k:TMP/k/stem for k in ('baseline','current')}
  audio={k:np.fromfile(p,dtype='<f4').reshape(-1,2).astype(np.float64) for k,p in paths.items()}
  a,b=audio['baseline'],audio['current']; assert a.shape==b.shape==(144000,2)
  assert np.isfinite(a).all() and np.isfinite(b).all()
  result={'name':name,'scenario':scenario,'sha256':{k:sha(p) for k,p in paths.items()},'identical_pcm':np.array_equal(a,b),'peak_dbfs':{k:db(np.max(np.abs(x))) for k,x in audio.items()},'windows':{}}
  for label,(lo,hi) in {'onset':(.08,.30),'bend':(.60,.98),'return':(1.55,2.15),'pre_release':(2.1,2.25),'late_release':(2.8,3.)}.items():
   aa,bb=win(a,lo,hi),win(b,lo,hi)
   result['windows'][label]={'baseline_dbfs':db(rms(aa)),'current_dbfs':db(rms(bb)),'difference_db_relative_current':db(rms(bb-aa)/max(rms(bb),1e-15))}
  if scenario!='pressure':
   f0=440*2**((24-69)/12);factor=2 if scenario=='up_return' else .5 if scenario=='down_return' else 1
   for label,(lo,hi),expected in [('bend',(.60,.98),f0*factor),('return',(1.55,2.15),f0)]:
    piece=win(b,lo,hi);f=pitch(piece,expected)
    result[label+'_pitch']={'expected_hz':expected,'measured_hz':f,'error_cents':1200*math.log2(f/expected)}
   fundamental=result['bend_pitch']['measured_hz']
   result['bend_harmonic_ratios_db']={k:[db(partial(win(x,.60,.98),fundamental,h)/max(partial(win(x,.60,.98),fundamental,1),1e-15)) for h in (2,3,4)] for k,x in audio.items()}
  results.append(result)
assert all(r['identical_pcm'] for r in results if r['name']=='Analog Bass')
assert all(max(r['peak_dbfs'].values())<0 for r in results)
provenance={'baseline_commit':'18711f93a39aa49f3d6f9a41784be456224facbe','current_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'current_is_working_tree':True,'abi':'Baseline caller compiled against git archive 18711f93 src; current caller against current src. Separate matching static libraries.','libraries':{p:sha(p) for p in ('/tmp/cs80-reference-1cee5a/libcs80_core.a','/tmp/cs80-build-1cee5a/libcs80_core.a')},'probe_source_sha256':sha(HERE/'engine_probe.cpp'),'inventory_sha256':sha(TMP/'current-inventory.json'),'rate':RATE,'note':36,'velocity':.85,'hold_seconds':2.3,'tail_seconds':.7,'recipe_changes':[],'ribbon':'0.35-0.50s ramp; plateau until 1.05s; returned at 1.20s. Up +12 semitones, down -12 semitones.','pressure':'0.75 from 0.35s, no ribbon; note-local polyPressure.','raw_pcm_location':str(TMP),'main_comparison_renders':32,'human_listening':False,'results':results}
provenance['executables']={str(p):sha(p) for p in (TMP/'probe-baseline',TMP/'probe-current',Path('/tmp/cs80-build-1cee5a/cs80_bass_plucked_refinement_tests'),Path('/tmp/cs80-build-1cee5a/cs80_string_body_formant_tests'))}
provenance['source_hashes']={str(p.relative_to(ROOT)):sha(p) for p in (ROOT/'src/core/Engine.cpp',ROOT/'src/core/Engine.h',ROOT/'src/core/InstrumentBody.h',ROOT/'src/core/curated/Bass.inc',ROOT/'src/core/OriginalCircuitLibrary.inc')}
(HERE/'pcm-analysis.json').write_text(json.dumps(provenance,indent=2)+'\n')
print('inventory',len(rows),'body',struct['body_enabled'],'A06',struct['a06_affected'])
for r in results:
 print(r['name'],r['scenario'],'peak',round(r['peak_dbfs']['current'],3),'delta-bend',round(r['windows']['bend']['difference_db_relative_current'],3),'delta-return',round(r['windows']['return']['difference_db_relative_current'],3),'pitch-cents',round(r.get('bend_pitch',{}).get('error_cents',0),3),'release',round(r['windows']['late_release']['current_dbfs'],3))
