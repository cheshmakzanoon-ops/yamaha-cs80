import json,subprocess,math
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import numpy as np
from scipy.io import wavfile
root=Path.cwd();out=root/'build-w02-stereo'
settings={'Noir Swell Brass':[-.24,.34],'Brass Ensemble 16':[-.12,.38],'Neo-Noir Pad':[-.18,.40],'Arctic Pad':[-.12,.40],'Synthwave Chords':[-.24,.32]}
jobs=[]
for name,pans in settings.items():
 for note in [48,72]:
  for pressure in [0,.85]:
   jobs.append((name,'candidate',note,pans,pressure))
def run(job):
 name,variant,note,pans,pressure=job
 folder=out/variant/name.replace(' ','-')/(str(note)+'-p'+str(pressure))
 cmd=[str(root/'build-probe/capture'),'--out',str(folder),'--wav','--preset',name,'--note',str(note),'--hold','3','--tail','.5','--velocity','.8','--pressure',str(pressure)]
 if variant=='candidate':
  cmd+=['--set',f'ch1_pan={pans[0]}','--set',f'ch2_pan={pans[1]}']
 subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL)
 sr,x=wavfile.read(next(folder.glob('*.wav')));x=x.astype(float)
 a=x[int(sr*1):int(sr*2.8)];l,r=a.T;m=(l+r)/2;s=(l-r)/2
 db=lambda x:20*math.log10(max(x,1e-15))
 rms=lambda x:float(np.sqrt(np.mean(x*x)))
 stereo=rms(a);mono=rms(m)
 # fundamental power under stereo versus mono within +/-10% of played register.
 f=440*2**((note-69)/12);f*=.5 if name=='Brass Ensemble 16' else 1
 w=np.hanning(len(a));freq=np.fft.rfftfreq(len(a),1/sr);bins=(freq>f*.9)&(freq<f*1.1)
 sp=(abs(np.fft.rfft(l*w))**2+abs(np.fft.rfft(r*w))**2)/2;mp=abs(np.fft.rfft(m*w))**2
 return {'name':name,'variant':variant,'note':note,'pressure':pressure,'pan':pans if variant=='candidate' else [0,0], 'side_mid_db':db(rms(s)/rms(m)),'lr_balance_db':db(rms(l)/rms(r)),'mono_loss_db':db(stereo/mono),'fundamental_mono_loss_db':10*math.log10(float(sp[bins].sum()/mp[bins].sum())), 'correlation':float(np.corrcoef(l,r)[0,1]),'peak_dbfs':db(float(abs(x).max())),'nonfinite':int((~np.isfinite(x)).sum())}
with ThreadPoolExecutor(max_workers=2) as pool: rows=list(pool.map(run,jobs))
(out/'registers.json').write_text(json.dumps({'conditions':{'rate':48000,'velocity':.8,'pressure':0,'hold':3,'tail':.5,'window':[1,2.8]},'measurements':rows},indent=2)+'\n')
for r in rows:print(r)
