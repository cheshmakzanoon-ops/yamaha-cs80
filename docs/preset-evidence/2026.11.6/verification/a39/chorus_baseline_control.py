#!/usr/bin/env python3
"""A39 follow-up: distinguish existing chorus mono loss from a new regression."""
import argparse
import json
from pathlib import Path
import subprocess
import numpy as np
from check_stereo import sha, wav, window_metrics, WINDOWS

p=argparse.ArgumentParser()
p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--audio',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args()
prior=json.loads((a.out/'results.json').read_text())
assert sha(a.baseline)==prior['binary_sha256']['baseline']
rows=[]
for note in (48,60,72):
    for velocity in (.25,.85):
        dest=a.audio/'baseline-chorus'/f'n{note}-v{round(velocity*100)}'
        command=[str(a.baseline.resolve()),'--out',str(dest),'--wav','--library','all',
                 '--preset','Electric Piano II','--rate','48000','--note',str(note),
                 '--velocity',str(velocity),'--pressure','0','--hold','1.4','--tail','.8']
        run=subprocess.run(command,check=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        old=next(dest.glob('*-Electric-Piano-II.wav'))
        new=next((a.audio/'candidate'/f'n{note}-v{round(velocity*100)}').glob('*-Electric-Piano-II.wav'))
        original=next(x for x in prior['measurements'] if x['version']=='candidate' and x['name']=='Electric Piano II' and x['note']==note and x['velocity']==velocity)
        assert sha(new)==original['wav_sha256']
        left,right=wav(old),wav(new)
        rows.append({'note':note,'velocity':velocity,'baseline_wav_sha256':sha(old),
                     'candidate_wav_sha256':sha(new),'sample_identical':bool(np.array_equal(left,right)),
                     'maximum_absolute_sample_difference':float(np.max(np.abs(left-right))),
                     'baseline_windows':{key:window_metrics(left,*bounds) for key,bounds in WINDOWS.items()}})
        (a.out/f'chorus-baseline-n{note}-v{round(velocity*100)}.log').write_text(run.stdout)
result={'baseline_commit':prior['baseline_commit'],'candidate_commit':prior['source_commit'],
        'baseline_executable_sha256':sha(a.baseline),'additional_captures':6,'additional_seconds':13.2,
        'scope':'The preceding .5 source has the identical Electric Piano II recipe and chorus behavior.',
        'all_six_sample_identical':all(x['sample_identical'] for x in rows),'measurements':rows}
assert result['all_six_sample_identical']
(a.out/'chorus-baseline-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print('Electric Piano II: six baseline/current pairs are sample-identical.')
