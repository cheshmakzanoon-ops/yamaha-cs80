from pathlib import Path
import subprocess,json,hashlib,concurrent.futures
import numpy as np
from scipy.io import wavfile
ROOT=Path(__file__).resolve().parent.parent
OUT=Path(__file__).resolve().parent
CAP=ROOT/'build-probe/capture'
NAMES=['Harp','Kalimba']
NOTES=[48,60,76]; VELOCITIES=[.25,.9]
WINDOWS=[(0,.01),(.004,.02),(.02,.06),(.2,.4)]
CANDIDATES={'Harp':{'ch2_sine':.46,'mix':.24},'Kalimba':{'ch2_sine':.54,'mix':.23}}
inv=json.loads((ROOT/'build-baseline/inventory.json').read_text())
presets={p['name']:p for p in inv['programs']}; mid=inv['parameter_ids'].index('mix')
commands=[]
for name in NAMES:
 for note in NOTES:
  for velocity in VELOCITIES:
   for rank in [0,1,2]:
    folder=OUT/f'{name}-n{note}-v{velocity:g}-r{rank}'
    cmd=[str(CAP),'--out',str(folder),'--preset',name,'--wav','--rate','48000','--note',str(note),'--velocity',str(velocity),'--hold','.5','--tail','0']
    if rank:cmd+=['--rank',str(rank)]
    for k,v in CANDIDATES[name].items():cmd+=['--set',f'{k}={v}']
    commands.append(cmd)
for name in NAMES:
 for bank in ['baseline','candidate']:
  for rank in [0,1,2]:
   folder=OUT/f'{name}-{bank}-long-r{rank}'
   cmd=[str(CAP),'--out',str(folder),'--preset',name,'--wav','--rate','48000','--note','60','--velocity','.9','--hold','2','--tail','1']
   if rank:cmd+=['--rank',str(rank)]
   if bank=='candidate':
    for k,v in CANDIDATES[name].items():cmd+=['--set',f'{k}={v}']
   commands.append(cmd)
def run(cmd):
 p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
 return p.stdout
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(run,commands))
def rms(a):return float(np.sqrt(np.mean(a*a)))
def db(a):return float(20*np.log10(max(float(a),1e-24)))
def read(folder,name):
 path=next(folder.glob(f"{presets[name]['index']}-*.wav"));sr,a=wavfile.read(path)
 assert sr==48000 and a.shape[1]==2 and np.isfinite(a).all()
 return a.astype(np.float64)
def weights(m):
 m=np.float32(m);return [float(np.float32(1)-m),float(m)]
base=json.loads((ROOT/'build-w02-strikes/measurements.json').read_text())
base={(r['name'],r['note'],r['velocity']):r for r in base['rows']}
rows=[]
for name in NAMES:
 w=weights(CANDIDATES[name]['mix'])
 for note in NOTES:
  for velocity in VELOCITIES:
   full,body,strike=[read(OUT/f'{name}-n{note}-v{velocity:g}-r{rank}',name) for rank in [0,1,2]]
   body*=w[0];strike*=w[1]
   b=base[name,note,velocity]
   row={'name':name,'note':note,'velocity':velocity,'candidate':CANDIDATES[name],'weights':w,
     'reconstruction_error_relative_db':db(rms(full-body-strike)/rms(full)),'peak_dbfs':db(np.max(np.abs(full))),'nonfinite':int(np.sum(~np.isfinite(full))),
     'over_full_scale':int(np.sum(np.abs(full)>1)),'windows':[]}
   for j,(begin,end) in enumerate(WINDOWS):
    lo,hi=round(begin*48000),round(end*48000);bodyR=rms(body[lo:hi]);strikeR=rms(strike[lo:hi]);fullR=rms(full[lo:hi])
    rel=db(strikeR/bodyR);old=b['windows'][j]['strike_body_db']
    row['windows'].append({'begin':begin,'end':end,'baseline_strike_body_db':old,'candidate_strike_body_db':rel,'improvement_db':rel-old,'body_dbfs':db(bodyR),'strike_dbfs':db(strikeR),'full_dbfs':db(fullR)})
   rows.append(row)
long=[]
for name in NAMES:
 for bank in ['baseline','candidate']:
  w=weights(CANDIDATES[name]['mix'] if bank=='candidate' else presets[name]['values'][mid])
  full,body,strike=[read(OUT/f'{name}-{bank}-long-r{rank}',name) for rank in [0,1,2]]
  body*=w[0];strike*=w[1]
  row={'name':name,'bank':bank,'reconstruction_error_relative_db':db(rms(full-body-strike)/rms(full)), 'peak_dbfs':db(np.max(np.abs(full))),'windows':[]}
  for begin,end in [(0,.02),(.2,.4),(.8,1.2),(1.5,1.9),(2.5,2.9)]:
   lo,hi=round(begin*48000),round(end*48000)
   row['windows'].append({'begin':begin,'end':end,'body_dbfs':db(rms(body[lo:hi])),'strike_dbfs':db(rms(strike[lo:hi])), 'strike_body_db':db(rms(strike[lo:hi])/max(rms(body[lo:hi]),1e-24)), 'full_dbfs':db(rms(full[lo:hi]))})
  long.append(row)
report={'baseline_source_commit':'7f55a95a7534b389927defe46866456560b3a878','capture_binary_sha256':hashlib.sha256(CAP.read_bytes()).hexdigest(),'diagnostic_source_sha256':hashlib.sha256((ROOT/'build-probe/diagnostic_capture.cpp').read_bytes()).hexdigest(),'candidates':CANDIDATES,'commands':commands,'rows':rows,'long':long,'notes':'Unnormalized stereo float WAV; overrides before rank isolation. Same frozen library and engine. Candidate changes only upper sine amplitude and mix. Baseline onset results reused from build-w02-strikes, raw WAVs retained. Master volume is not recalibrated here.'}
(OUT/'measurements.json').write_text(json.dumps(report,indent=2)+'\n')
lines=[]
for r in rows:
 lines.append(f"{r['name']:8} n{r['note']:3} v{r['velocity']:.2f} "+' / '.join(f"{w['baseline_strike_body_db']:.2f}->{w['candidate_strike_body_db']:.2f} ({w['improvement_db']:+.2f})" for w in r['windows'])+f" reconstruction {r['reconstruction_error_relative_db']:.2f} peak {r['peak_dbfs']:.2f}")
for r in long:
 lines.append(f"{r['name']:8} {r['bank']:9} long strike/body "+' / '.join(f"{w['strike_body_db']:.2f}" for w in r['windows']))
(OUT/'summary.txt').write_text('\n'.join(lines)+'\n');print('\n'.join(lines))
