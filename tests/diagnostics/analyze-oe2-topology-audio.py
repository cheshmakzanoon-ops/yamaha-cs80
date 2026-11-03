#!/usr/bin/env python3
"""Convert unchanged float samples to WAV and report descriptive before/after metrics."""
from pathlib import Path
import argparse,csv,hashlib,json,struct
import numpy as np
p=argparse.ArgumentParser();p.add_argument('input',type=Path);p.add_argument('output',type=Path);args=p.parse_args()
args.output.mkdir(parents=True,exist_ok=True)
rate=96000;names=['off','chorus','tremolo','both'];rows=[];files=[]
def stats(x):
    y=x.astype(np.float64);mid=(y[:,0]+y[:,1])/2;side=(y[:,0]-y[:,1])/2
    centered=y-y.mean(axis=0);cov=centered.T@centered/len(y);den=float(np.sqrt(cov[0,0]*cov[1,1]))
    return {'rms':float(np.sqrt(np.mean(y*y))),'peak':float(np.max(np.abs(y))),
        'mid_rms':float(np.sqrt(np.mean(mid*mid))),'side_rms':float(np.sqrt(np.mean(side*side))),
        'lr_correlation':float(cov[0,1]/den) if den>0 else None}
for mode,name in enumerate(names):
    pair=[]
    for variant in ['legacy','experimental']:
        source=args.input/f'{mode}-{variant}.f32';raw=source.read_bytes();x=np.frombuffer(raw,dtype='<f4').reshape(-1,2)
        if len(x)!=6*rate or not np.isfinite(x).all():raise ValueError('invalid audio shape or values')
        # IEEE float WAV, stereo, unchanged sample amplitude and duration.
        header=b'RIFF'+struct.pack('<I',50+len(raw))+b'WAVEfmt '+struct.pack('<IHHIIHHH',18,3,2,rate,rate*8,8,32,0)+b'fact'+struct.pack('<II',4,len(x))+b'data'+struct.pack('<I',len(raw))
        dest=args.output/f'{name}-{variant}.wav';dest.write_bytes(header+raw)
        files.append({'file':dest.name,'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'sample_sha256':hashlib.sha256(raw).hexdigest()})
        pair.append(x)
    for segment,a,b in [('whole',0,6),('multitone',0,2),('asymmetric-transients',2,4),('settled-silence',4.25,6)]:
        old,new=[x[int(a*rate):int(b*rate)] for x in pair];delta=new.astype(np.float64)-old.astype(np.float64)
        row={'mode':name,'segment':segment,'difference_rms':float(np.sqrt(np.mean(delta*delta))),'difference_peak':float(np.max(np.abs(delta)))}
        for variant,x in zip(['legacy','experimental'],[old,new]): row.update({variant+'_'+key:value for key,value in stats(x).items()})
        rows.append(row)
with (args.output/'metrics.csv').open('w') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
(args.output/'manifest.json').write_text(json.dumps({'analysis_script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'renderer_source_sha256':hashlib.sha256(Path(__file__).with_name('Oe2TopologyAudio.cpp').read_bytes()).hexdigest(),'rate':rate,'channels':2,'format':'IEEE float32 WAV','seconds':6,'normalization':'none','baseline_scope':'historicalGrouping experimental comparator; OFF/CHORUS/TREMOLO verified against frozen main; BOTH did not exist on main',
    'stimulus':'0-2s mono multitone; 2-4s asymmetric decaying transients; 4-6s silence',
    'speed_hz':4,'depth':.9,'initial_state':'same prepare/reset seeds','files':files,'metrics':rows},indent=2,allow_nan=False)+'\n')
for r in rows:
    if r['segment'] in ['whole','settled-silence']:
        print(r['mode'],r['segment'],'difference_rms',format(r['difference_rms'],'.8g'),'difference_peak',format(r['difference_peak'],'.8g'),
              'legacy_lr',r['legacy_lr_correlation'],'experimental_lr',r['experimental_lr_correlation'])
