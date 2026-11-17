from pathlib import Path
import json,hashlib,re
import numpy as np
root=Path(__file__).parent;rows=[];identical=True
for rate in (44100,48000,88200,96000):
 for scene in range(6):
  n=f'{rate}-{scene}.f32';arms={m:np.fromfile(root/'captures'/m/n,dtype='<f4').reshape(-1,2).astype(float) for m in ('stock','off','draft','hq')}
  eq=(root/'captures/stock'/n).read_bytes()==(root/'captures/off'/n).read_bytes();identical &= eq
  hqfactor=max(2,8//(2 if rate>=88200 else 1));lat={'stock':67,'off':67,'draft':8,'hq':63+8//hqfactor}
  windows={m:a[rate//4+lat[m]:rate//2-67+lat[m]].mean(axis=1) for m,a in arms.items()}
  metrics={}
  for m,a in arms.items():
   w=windows[m];spec=np.abs(np.fft.rfft((w-w.mean())*np.hanning(len(w))))**2;freq=np.fft.rfftfreq(len(w),1/rate);energy=float(spec.sum());bands={}
   for lo,hi in ((0,1000),(1000,5000),(5000,10000),(10000,rate/2+1)):
    bands[f'{lo}-{hi}Hz']=float(spec[(freq>=lo)&(freq<hi)].sum()/energy) if energy else None
   metrics[m]={'finite':bool(np.isfinite(a).all()),'peak':float(np.max(np.abs(a))),'rms':float(np.sqrt(np.mean(a*a))),'dc':float(a.mean()),'release_rms':float(np.sqrt(np.mean(a[rate//2:]**2))),'max_adjacent_delta':float(np.max(np.abs(np.diff(a,axis=0)))),'bands_power_fraction':bands,'largest_spectral_bin_hz':float(freq[int(spec.argmax())]),'latency':lat[m]}
  comparisons={}
  for reference in ('stock','hq'):
   diff=windows['draft']-windows[reference];den=np.sqrt(np.mean(windows[reference]**2));comparisons[reference]={'aligned_difference_rms':float(np.sqrt(np.mean(diff**2))),'difference_relative_rms':float(np.sqrt(np.mean(diff**2))/den) if den else None}
  rows.append({'rate':rate,'scene':scene,'stock_off_byte_identical':eq,'metrics':metrics,'draft_comparisons':comparisons})
report={'scope':'24 named PANEL scenes, descriptive only, no CPU or quality acceptance claim','stock_off_all_exact':bool(identical),'rows':rows}
(root/'quality-summary.json').write_text(json.dumps(report,indent=2)+'\n');assert identical
print('stock/off24 exact; all96 finite:',all(m['finite'] for r in rows for m in r['metrics'].values()))
print('draft/stock aligned difference relativeRMS range:',min(r['draft_comparisons']['stock']['difference_relative_rms'] for r in rows),max(r['draft_comparisons']['stock']['difference_relative_rms'] for r in rows))
print('draft peak range:',min(r['metrics']['draft']['peak'] for r in rows),max(r['metrics']['draft']['peak'] for r in rows))
