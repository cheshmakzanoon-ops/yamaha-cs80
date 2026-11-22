from pathlib import Path
import subprocess,json,math
import numpy as np
from scipy.io import wavfile
R=Path(__file__).resolve().parent
binary=R/'capture-snapshot'
scenarios=[
 ('bass-low',['Analog Bass','Contour Bass'],36,.8,.5,.7,.5),
 ('bass-high',['Analog Bass','Contour Bass'],84,.8,.5,.7,.5),
 ('mallet-long',['Celesta','Kalimba'],60,.8,.5,3,1),
 ('mallet-soft-high',['Celesta','Kalimba'],76,.25,0,3,1),
 ('bass-click',['Sine Bass','Sub Bass'],48,.9,0,1,.5),
]
result=[]
def db(x):return 20*math.log10(max(float(x),1e-30))
def rms(x):return float(np.sqrt(np.mean(x*x)))
for label,names,note,velocity,pressure,hold,tail in scenarios:
 out=R/'focused'/label
 cmd=[str(binary),'--out',str(out),'--wav','--note',str(note),'--velocity',str(velocity),'--pressure',str(pressure),'--hold',str(hold),'--tail',str(tail)]
 for name in names:cmd+=['--preset',name]
 with (R/'focus-capture.log').open('a') as log:subprocess.run(cmd,stdout=log,stderr=log,check=True)
 audios=[]
 for name in names:
  path=next(p for p in out.glob('*.wav') if p.stem.split('-',1)[1]==name.replace(' ','-'))
  sr,x=wavfile.read(path);audios.append(x.astype(np.float64))
 windows=[('onset',.004,.02),('early_body',.05,.15),('held_body',hold-.2,hold),('release',hold,hold+min(tail,.2))]
 metrics=[]
 for window,a,b in windows:
  x,y=[signal[round(a*sr):round(b*sr)].reshape(-1) for signal in audios]
  xx=float(x@x); yy=float(y@y); xy=float(x@y)
  gain=xy/max(xx,1e-30)
  metrics.append({'window':window,'range_seconds':[a,b],'rms_dbfs':[db(rms(x)),db(rms(y))],
    'rms_difference_b_minus_a_db':db(rms(y)/max(rms(x),1e-30)),
    'waveform_correlation':xy/max(math.sqrt(xx*yy),1e-30),
    'fit_residual_db':db(rms(y-gain*x)/max(rms(y),1e-30))})
 result.append({'label':label,'names':names,'command':cmd,'metrics':metrics})
(R/'focused.json').write_text(json.dumps(result,indent=2)+'\n')
for r in result:
 print(r['label'],r['names'])
 for m in r['metrics']:print(m['window'],'corr',round(m['waveform_correlation'],6),'RMS difference',round(m['rms_difference_b_minus_a_db'],3),'residual',round(m['fit_residual_db'],2))
