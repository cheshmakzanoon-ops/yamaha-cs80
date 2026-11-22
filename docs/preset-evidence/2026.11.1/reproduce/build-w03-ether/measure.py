from pathlib import Path
import subprocess, json, hashlib, concurrent.futures
import numpy as np
from scipy.io import wavfile
ROOT=Path(__file__).resolve().parent.parent
OUT=Path(__file__).resolve().parent
CAP=ROOT/'build-probe/capture'
OVERRIDES={'ch2_noise':.95, 'mix':.28}
inv=json.loads((ROOT/'build-baseline/inventory.json').read_text())
preset=next(p for p in inv['programs'] if p['name']=='Ether Pad')
oldmix=preset['values'][inv['parameter_ids'].index('mix')]
commands=[]
for bank in ['baseline','candidate']:
 for note in [48,60,76]:
  for pressure in [0,.5,1]:
   for rank in [0,1,2]:
    folder=OUT/f'{bank}-n{note}-p{pressure:g}-r{rank}'
    cmd=[str(CAP),'--out',str(folder),'--preset','Ether Pad','--wav','--rate','48000','--note',str(note),'--velocity','.8','--pressure',str(pressure),'--hold','3','--tail','1.5']
    if rank:cmd+=['--rank',str(rank)]
    if bank=='candidate':
     for k,v in OVERRIDES.items():cmd+=['--set',f'{k}={v}']
    commands.append(cmd)
def run(cmd):
 p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(run,commands))
def rms(a):return float(np.sqrt(np.mean(a*a)))
def db(a):return float(20*np.log10(max(float(a),1e-24)))
def read(folder):
 sr,a=wavfile.read(next(folder.glob('*.wav')))
 assert sr==48000 and a.shape[1]==2 and np.isfinite(a).all()
 return a.astype(np.float64)
def weights(m):
 m=np.float32(m);return [float(np.float32(1)-m),float(m)]
def spectrum(a,note):
 x=a[round(1.5*48000):round(2.5*48000)]
 win=np.hanning(len(x));s=np.abs(np.fft.rfft(x*win[:,None],axis=0))**2
 p=np.mean(s,axis=1); f=np.fft.rfftfreq(len(x),1/48000);p[0]=0
 ideal=440*2**((note-69)/12)
 band=(f>ideal*.97)&(f<ideal*1.03)
 return {'fundamental_band_power_fraction':float(np.sum(p[band])/np.sum(p)), 'dominant_bin_hz':float(f[np.argmax(p)]), 'expected_note_hz':ideal}
rows=[]
for bank in ['baseline','candidate']:
 w=weights(oldmix if bank=='baseline' else OVERRIDES['mix'])
 for note in [48,60,76]:
  for pressure in [0,.5,1]:
   full,body,air=[read(OUT/f'{bank}-n{note}-p{pressure:g}-r{rank}') for rank in [0,1,2]]
   body*=w[0];air*=w[1]
   row={'bank':bank,'note':note,'velocity':.8,'pressure':pressure,'weights':w,
        'peak_dbfs':db(np.max(np.abs(full))),'nonfinite':int(np.sum(~np.isfinite(full))), 'samples_over_full_scale':int(np.sum(np.abs(full)>1)),
        'reconstruction_relative_db':db(rms(full-body-air)/rms(full)),'spectrum':spectrum(full,note),'windows':[]}
   for begin,end in [(0,.2),(.3,.5),(.8,1.3),(1.5,2.5),(2.5,2.9),(4,4.5)]:
    lo,hi=round(begin*48000),round(end*48000)
    row['windows'].append({'begin':begin,'end':end,'air_body_db':db(rms(air[lo:hi])/rms(body[lo:hi])), 'body_dbfs':db(rms(body[lo:hi])),'air_dbfs':db(rms(air[lo:hi])),'full_dbfs':db(rms(full[lo:hi]))})
   rows.append(row)
report={'baseline_source_commit':'7f55a95a7534b389927defe46866456560b3a878','capture_binary_sha256':hashlib.sha256(CAP.read_bytes()).hexdigest(),'diagnostic_source_sha256':hashlib.sha256((ROOT/'build-probe/diagnostic_capture.cpp').read_bytes()).hexdigest(),'candidate_overrides':OVERRIDES,'commands':commands,'rows':rows,'notes':'Unnormalized stereo float captures. Only noise amount and rank mix overridden; exact factory effects and volumes retained. Weighted isolated ranks reconstruct full signal as causal check. No calibrated acoustic reference or listening-panel claim.'}
(OUT/'measurements.json').write_text(json.dumps(report,indent=2)+'\n')
base={(r['note'],r['pressure']):r for r in rows if r['bank']=='baseline'}
lines=[]
for r in rows:
 if r['bank']!='candidate':continue
 b=base[r['note'],r['pressure']];wr=r['windows'][3];wb=b['windows'][3]
 lines.append(f"n{r['note']:2} p{r['pressure']:.1f}: air/body {wb['air_body_db']:.3f}→{wr['air_body_db']:.3f}, delta {wr['air_body_db']-wb['air_body_db']:+.3f}dB; peak {r['peak_dbfs']:.2f}; reconstruct {r['reconstruction_relative_db']:.2f}; fundamental {r['spectrum']['fundamental_band_power_fraction']:.6f}")
(OUT/'summary.txt').write_text('\n'.join(lines)+'\n');print('\n'.join(lines))
