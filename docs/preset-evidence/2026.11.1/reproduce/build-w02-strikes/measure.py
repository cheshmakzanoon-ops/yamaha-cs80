from pathlib import Path
import subprocess,json,hashlib,concurrent.futures
import numpy as np
from scipy.io import wavfile
ROOT=Path(__file__).resolve().parent.parent
OUT=Path(__file__).resolve().parent
CAP=ROOT/'build-baseline/cs80_preset_capture'
NAMES=['Electric Piano I','Guitar I','Harp','Kalimba']
NOTES=[48,60,76]
VELOCITIES=[.25,.9]
WINDOWS=[(0,.01),(.004,.02),(.02,.06),(.2,.4)]
inv=json.loads((ROOT/'build-baseline/inventory.json').read_text())
presets={p['name']:p for p in inv['programs']}
mid=inv['parameter_ids'].index('mix')
commands=[]
for note in NOTES:
 for velocity in VELOCITIES:
  for rank in [0,1,2]:
   folder=OUT/f'n{note}-v{velocity:g}-r{rank}'
   cmd=[str(CAP),'--out',str(folder),'--wav','--rate','48000','--note',str(note),'--velocity',str(velocity),'--hold','.5','--tail','0']
   if rank:cmd+=['--rank',str(rank)]
   for name in NAMES:cmd+=['--preset',name]
   commands.append(cmd)
def run(cmd):
 p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
 return p.stdout
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
 list(pool.map(run,commands))
def rms(a):return float(np.sqrt(np.mean(a*a)))
def db(a):return float(20*np.log10(max(float(a),1e-24)))
def read(note,velocity,rank,name):
 folder=OUT/f'n{note}-v{velocity:g}-r{rank}'
 path=next(folder.glob(f"{presets[name]['index']}-*.wav"))
 sr,a=wavfile.read(path)
 assert sr==48000 and a.shape==(24000,2)
 assert np.isfinite(a).all()
 return a.astype(np.float64)
rows=[]
for name in NAMES:
 m=np.float32(presets[name]['values'][mid]);weights=(float(np.float32(1)-m),float(m))
 for note in NOTES:
  for velocity in VELOCITIES:
   full=read(note,velocity,0,name)
   body=read(note,velocity,1,name)*weights[0]
   strike=read(note,velocity,2,name)*weights[1]
   recon=body+strike
   row={'name':name,'note':note,'velocity':velocity,'weights':weights,
    'reconstruction_error_relative_db':db(rms(full-recon)/rms(full)),
    'peak_dbfs':db(np.max(np.abs(full))),'windows':[]}
   for begin,end in WINDOWS:
    lo,hi=round(begin*48000),round(end*48000)
    b,s,f=body[lo:hi],strike[lo:hi],full[lo:hi]
    row['windows'].append({'begin':begin,'end':end,'full_dbfs':db(rms(f)),
     'body_dbfs':db(rms(b)),'strike_dbfs':db(rms(s)),'strike_body_db':db(rms(s)/rms(b)),
     'reconstruction_error_relative_db':db(rms(f-b-s)/rms(f))})
   rows.append(row)
report={'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
 'capture_binary_sha256':hashlib.sha256(CAP.read_bytes()).hexdigest(),'commands':commands,
 'notes':'Actual factory rank balance applied in float32; all outputs float WAV, no normalization; first .5 seconds of key-held note. Reconstruction gates causal interpretation. Four windows compare onset to later body; no perceptual-inaudibility claim.',
 'rows':rows}
(OUT/'measurements.json').write_text(json.dumps(report,indent=2)+'\n')
print('name | note | velocity | strike/body dB 0-10 / 4-20 / 20-60 / 200-400 ms | reconstruct dB')
for r in rows:print(f"{r['name']:18s} {r['note']:3} {r['velocity']:.2f} "+' / '.join(f"{w['strike_body_db']:6.1f}" for w in r['windows'])+f" | {r['reconstruction_error_relative_db']:.1f}")
