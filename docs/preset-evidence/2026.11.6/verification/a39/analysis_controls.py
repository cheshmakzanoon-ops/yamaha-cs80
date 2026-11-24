#!/usr/bin/env python3
"""Known in-phase, anti-phase and unequal-channel diagnostic controls."""
import json
from pathlib import Path
import numpy as np
from check_stereo import window_metrics

t=np.arange(48000)/48000
a=.1*np.sin(2*np.pi*440*t)
controls={label:window_metrics(np.column_stack([a,right]),.1,.3)
          for label,right in [('same_phase',a),('opposite_phase',-a),('unequal_same_phase',.25*a)]}
assert abs(controls['same_phase']['mono_loss_db'])<1e-10
assert controls['opposite_phase']['mono_loss_db']>100
expected=float(10*np.log10(((1+.25**2)/2)/(.625**2)))
assert abs(controls['unequal_same_phase']['mono_loss_db']-expected)<1e-10
(Path(__file__).parent/'analysis-controls.json').write_text(json.dumps(
    {'checks':3,'failures':0,'known_unequal_channel_loss_db':expected,'controls':controls},indent=2)+'\n')
print('Analytic channel controls: 3 passed.')
