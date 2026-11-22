import json, math, subprocess, itertools, concurrent.futures
from pathlib import Path
import numpy as np
from scipy.io import wavfile
ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'build-w02-winds'
BINARY=ROOT/'build-probe/capture'
INV=json.loads((ROOT/'build-baseline/inventory.json').read_text())
IDS=INV['parameter_ids']
PROGRAMS={p['name']:dict(zip(IDS,p['values'])) for p in INV['programs']}
CANDIDATES={
 'Flute':dict(ch2_noise=.82,ch2_hpf_cutoff=950,ch2_vcf_level=.95,ch2_v_level=.32,ch2_init_level=.44,ch2_after_level=.40,ch2_v_s=.82,mix=.22),
 'Breathy Flute':dict(ch2_noise=.75,ch2_hpf_cutoff=800,ch2_vcf_level=.98,ch2_v_level=.30,ch2_init_level=.55,ch2_after_level=.50,ch2_v_s=.88,mix=.32),
}

def run(task):
 name,note,vel,pressure,version=task
 stem=f'{name.replace(" ","-")}-n{note}-v{vel}-p{pressure}-{version}'
 d=OUT/stem
 override=CANDIDATES[name] if version=='candidate' else {}
 for rank in (0,1,2):
  dest=d/str(rank)
  args=[str(BINARY),'--out',str(dest),'--preset',name,'--wav','--note',str(note),'--velocity',str(vel),'--pressure',str(pressure),'--hold','.8','--tail','.3']
  for k,v in override.items():args+=['--set',f'{k}={v}']
  if rank: args+=['--rank',str(rank)]
  if not list(dest.glob('*.wav')):subprocess.run(args,check=True,stdout=subprocess.DEVNULL)
 audio=[]
 for rank in (0,1,2):
  sr,a=wavfile.read(next((d/str(rank)).glob('*.wav')))
  audio.append(a.astype(np.float64))
 mix=override.get('mix',PROGRAMS[name]['mix'])
 full=audio[0];body=audio[1]*(1-mix);air=audio[2]*mix
 def rms(x):return math.sqrt(float(np.mean(x*x)))
 def db(v):return 20*math.log10(max(v,1e-15))
 def win(x,a,b):return x[round(a*sr):round(b*sr)]
 data=dict(name=name,note=note,velocity=vel,pressure=pressure,version=version,
  peak_dbfs=db(np.max(abs(full))),finite=bool(np.isfinite(full).all()),
  reconstruction_error_db=db(rms(full-body-air)/rms(full)),
  windows=[dict(start=a,end=b,body_dbfs=db(rms(win(body,a,b))),air_dbfs=db(rms(win(air,a,b))),air_body_db=db(rms(win(air,a,b))/rms(win(body,a,b)))) for a,b in [(0.004,.020),(.03,.08),(.3,.5),(.6,.78),(.98,1.1)]])
 return data

if __name__=='__main__':
 import sys
 if '--matrix' in sys.argv:tasks=list(itertools.product(CANDIDATES,(55,67,79),(.3,.9),(0,1),('baseline','candidate')));label='matrix'
 else:tasks=list(itertools.product(CANDIDATES,(67,),(.8,),(0,1),('baseline','candidate')));label='center'
 with concurrent.futures.ThreadPoolExecutor(max_workers=2) as ex:
  results=list(ex.map(run,tasks))
 (OUT/f'{label}.json').write_text(json.dumps({'rate':48000,'hold':.8,'tail':.3,'candidates':CANDIDATES,'results':results},indent=2)+'\n')
 for x in results:
  print(x['name'],x['note'],x['velocity'],x['pressure'],x['version'], 'air/body',round(x['windows'][2]['air_body_db'],2),'peak',round(x['peak_dbfs'],2),'recon',round(x['reconstruction_error_db'],1))
