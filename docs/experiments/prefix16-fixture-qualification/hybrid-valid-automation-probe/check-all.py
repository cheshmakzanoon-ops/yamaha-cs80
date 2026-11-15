import json
from pathlib import Path
p=Path(__file__).parent;results=[]
for rate in [44100,48000,96000]:
 for mode in ['realtime','offline']:
  logs={}
  for v in ['baseline','candidate']:
   n=f'{v}-{rate}-{mode}';code=int((p/f'{n}-exit.txt').read_text());s=(p/f'{n}.log').read_text();assert code==0 and 'FAIL:' not in s and 'Generated normalized roster count=120' in s
   assert 'CONTROL invalid_program error=0' in s
   assert f'VST3 protocol configuration: rate={rate} mode={mode}' in s
   logs[v]=s
  results.append({'rate':rate,'mode':mode,'baseline_exit':0,'candidate_exit':0,'logs_identical':logs['baseline']==logs['candidate']})
(p/'all-results.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
