#!/usr/bin/env python3
"""Outcome-blind, deterministic Williams schedule; no clocks or processes."""
import hashlib,json,collections
from pathlib import Path
SEED='cs80-eight-native-average-v1/0x80305124'
LABELS=('S0','S1','C','S+')
ROWS=((0,1,3,2),(1,2,0,3),(2,3,1,0),(3,0,2,1))
class Rng:
 def __init__(self):self.counter=0
 def randbelow(self,n):
  limit=2**256-(2**256%n)
  while True:
   value=int.from_bytes(hashlib.sha256((SEED+'/'+str(self.counter)).encode()).digest(),'big');self.counter+=1
   if value<limit:return value%n
 def shuffle(self,values):
  result=list(values)
  for i in range(len(result)-1,0,-1):
   j=self.randbelow(i+1);result[i],result[j]=result[j],result[i]
  return result
def make_schedule():
 rng=Rng();blocks=[];slots=[]
 for replicate in range(6):
  assignment=rng.shuffle(LABELS);rows=rng.shuffle(range(4))
  for row in rows:
   block=len(blocks);labels=[assignment[j] for j in ROWS[row]]
   blocks.append({'block':block,'replicate':replicate,'row':row,'assignment':assignment,'labels':labels,'slot_ids':list(range(4*block,4*block+4))})
   for position,label in enumerate(labels):slots.append({'slot':len(slots),'block':block,'replicate':replicate,'position':position,'treatment':label,'module_role':'candidate' if label=='C' else 'stock','warm_callbacks':1024,'measured_callbacks':544 if label=='S+' else 512,'normalization_callbacks':512})
 assert len(blocks)==24 and len(slots)==96
 assert collections.Counter(x['treatment'] for x in slots)=={k:24 for k in LABELS}
 assert all(collections.Counter(x['treatment'] for x in slots if x['position']==p)=={k:6 for k in LABELS} for p in range(4))
 within=collections.Counter((a,b) for block in blocks for a,b in zip(block['labels'],block['labels'][1:]))
 assert within=={(a,b):6 for a in LABELS for b in LABELS if a!=b}
 # Every temporal half is itself balanced: three complete Williams replicates.
 for lo,hi in [(0,12),(12,24)]:
  assert all(collections.Counter(x['treatment'] for x in slots if lo<=x['block']<hi and x['position']==p)=={k:3 for k in LABELS} for p in range(4))
 return {'schema':1,'seed':SEED,'rng':'SHA256(seed + slash + decimal counter), unbiased rejection and descending Fisher-Yates','base_rows':ROWS,'randomization':'independent treatment bijection and row permutation within each of six complete Williams sets','blocks':blocks,'slots':slots,'balance':{'each_treatment_per_position':6,'each_directed_distinct_within_block_predecessor_pair':6,'between_block_carryover':'retained and reported, not forced balanced; fresh process per treatment'}}
if __name__=='__main__':
 p=Path(__file__).resolve().parent/'schedule.json'
 with p.open('x') as f:json.dump(make_schedule(),f,indent=2);f.write('\n')
