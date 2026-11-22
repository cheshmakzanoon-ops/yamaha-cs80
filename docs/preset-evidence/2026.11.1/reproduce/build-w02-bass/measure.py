from pathlib import Path
import subprocess,json,hashlib,concurrent.futures,argparse
import numpy as np
from scipy.io import wavfile
ROOT=Path(__file__).resolve().parent.parent;OUT=Path(__file__).resolve().parent
CAP=ROOT/'build-probe/capture'
P=argparse.ArgumentParser();P.add_argument('--matrix',action='store_true');args=P.parse_args()
NAMES=['Sine Bass','Upright Pluck'];NOTES=[36,48,60] if args.matrix else [48];VELOCITIES=[.3,.9] if args.matrix else [.9]
WINDOWS=[(0,.01),(.004,.02),(.02,.06),(.2,.4),(.6,.8)]
CANDIDATES={
 'Sine Bass':{'baseline':{},'gain':{'ch2_vcf_level':1.,'mix':.23},'woody':{'ch2_vcf_level':.95,'mix':.21,'ch2_hpf_cutoff':450}},
 'Upright Pluck':{'baseline':{},'gain':{'ch2_vcf_level':.98,'mix':.22},'woody':{'ch2_vcf_level':.95,'mix':.19,'ch2_hpf_cutoff':300}}
}
if args.matrix:
 CANDIDATES={n:{k:v for k,v in vs.items() if k in ['baseline','woody']} for n,vs in CANDIDATES.items()}
inv=json.loads((ROOT/'build-baseline/inventory.json').read_text());presets={p['name']:p for p in inv['programs']};mid=inv['parameter_ids'].index('mix')
commands=[]
for name in NAMES:
 for label,overrides in CANDIDATES[name].items():
  for note in NOTES:
   for velocity in VELOCITIES:
    for rank in [0,1,2]:
     folder=OUT/f"{name.replace(' ','-')}-{label}-n{note}-v{velocity:g}-r{rank}"
     cmd=[str(CAP),'--out',str(folder),'--wav','--rate','48000','--note',str(note),'--velocity',str(velocity),'--hold','.8','--tail','.4','--preset',name]
     for key,value in overrides.items():cmd+=['--set',f'{key}={value}']
     if rank:cmd+=['--rank',str(rank)]
     commands.append(cmd)
def run(cmd):
 p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
 return p.stdout
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(run,commands))
def rms(a):return float(np.sqrt(np.mean(a*a)))
def db(a):return float(20*np.log10(max(float(a),1e-24)))
def read(name,label,note,velocity,rank):
 folder=OUT/f"{name.replace(' ','-')}-{label}-n{note}-v{velocity:g}-r{rank}"
 path=next(folder.glob(f"{presets[name]['index']}-*.wav"));sr,a=wavfile.read(path)
 assert sr==48000 and a.shape==(57600,2) and np.isfinite(a).all()
 return a.astype(np.float64)
rows=[]
for name in NAMES:
 for label,overrides in CANDIDATES[name].items():
  m=np.float32(overrides.get('mix',presets[name]['values'][mid]));weights=(float(np.float32(1)-m),float(m))
  for note in NOTES:
   for velocity in VELOCITIES:
    full=read(name,label,note,velocity,0);body=read(name,label,note,velocity,1)*weights[0];strike=read(name,label,note,velocity,2)*weights[1]
    row={'name':name,'version':label,'note':note,'velocity':velocity,'overrides':overrides,'weights':weights,'reconstruction_error_relative_db':db(rms(full-body-strike)/rms(full)),'peak_dbfs':db(np.max(np.abs(full))),'release_last100ms_dbfs':db(rms(full[-4800:])),'windows':[]}
    for begin,end in WINDOWS:
     lo,hi=round(begin*48000),round(end*48000);b,s,f=body[lo:hi],strike[lo:hi],full[lo:hi]
     row['windows'].append({'begin':begin,'end':end,'full_dbfs':db(rms(f)),'body_dbfs':db(rms(b)),'strike_dbfs':db(rms(s)),'strike_body_db':db(rms(s)/rms(b)),'reconstruction_error_relative_db':db(rms(f-b-s)/rms(f))})
    rows.append(row)
report={'source_commit':'7f55a95a7534b389927defe46866456560b3a878','probe_binary_sha256':hashlib.sha256(CAP.read_bytes()).hexdigest(),'commands':commands,'condition':'Float WAV, no normalization; baseline master levels, 0.8s key-held +0.4s release; actual float32 factory blend; causal rank reconstruction. Engineering contribution metric, not perceptual inaudibility claim.','rows':rows}
(OUT/('matrix.json' if args.matrix else 'center.json')).write_text(json.dumps(report,indent=2)+'\n')
for r in rows:print(r['name'],r['version'],r['note'],r['velocity'],'4-20ms',round(r['windows'][1]['strike_body_db'],2),'late',round(r['windows'][4]['strike_body_db'],2),'peak',round(r['peak_dbfs'],2),'reconstruction',round(r['reconstruction_error_relative_db'],1))
