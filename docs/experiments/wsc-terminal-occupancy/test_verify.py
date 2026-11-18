#!/usr/bin/env python3
"""Small independent semantic mutations, including a tamper with updated raw hash."""
import hashlib,json,tempfile,shutil
from pathlib import Path
from verify import run
P=Path(__file__).resolve().parent;PROFILE=P.parent/'shipping-core-profile'
def cases(profile=PROFILE):
  def mutate(r,key):
    e=json.loads((r/'evidence.json').read_text())
    if key=='missing-stream':e['stream_receipts'].pop(0)
    elif key=='changed-audio':e['stream_receipts'][0]['observed_sha256']='ff'*32
    elif key=='reset-unconditional':e['real_reset_control']['rows'][1]['terminal_packs']=1
    elif key=='report-not-sensitive':e['reporting_control']['detected']=False
    elif key=='profile-label':
      p=r/'profile-scope.json';d=json.loads(p.read_text());d['rows'][0]['scene']='not-its-scene';p.write_text(json.dumps(d))
    elif key=='duplicate-group-rehashed':
      p=r/'observed-counts.csv';ls=p.read_text().splitlines();ls[2]=ls[1];p.write_text('\n'.join(ls)+'\n');e['observed_counts_sha256']=hashlib.sha256(p.read_bytes()).hexdigest()
    (r/'evidence.json').write_text(json.dumps(e))
  keys=['missing-stream','changed-audio','reset-unconditional','report-not-sensitive','profile-label','duplicate-group-rehashed']
  for key in keys:
    with tempfile.TemporaryDirectory() as t:
      r=Path(t);[shutil.copyfile(P/f,r/f) for f in ['evidence.json','observed-counts.csv','census.cpp','profile-scope.json']];mutate(r,key)
      try:run(r,profile)
      except ValueError:continue
      raise AssertionError(key+' was not detected')
  print(str(len(keys))+' evidence mutations rejected; no DSP executed.')
if __name__=='__main__':cases()
