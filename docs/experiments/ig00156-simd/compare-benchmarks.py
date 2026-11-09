#!/usr/bin/env python3
"""Run paired original/scalar/SIMD executables in deterministic rotating order."""
import argparse,csv,os,statistics,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('baseline');p.add_argument('candidate');p.add_argument('output');p.add_argument('--blocks',type=int,default=384);p.add_argument('--repeats',type=int,default=7);p.add_argument('--cpu',type=int);a=p.parse_args()
cases=[(0,8,15,0),(0,8,15,1),(1,0,15,0),(1,1,15,0),(1,2,15,0),(1,4,1,0),(1,8,0,0),(1,8,5,0),(1,8,10,0),(1,8,15,0),(1,8,15,1),(2,8,15,0),(2,8,15,1)]
head=['policy','voices','bypass','effective_hp_ranks','effective_lp_ranks','moving','accelerated','blocks','wall_seconds','cpu_seconds','checksum']
results=[]
with open(a.output,'w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=['variant','repeat']+head);w.writeheader()
 for case in cases:
  for repeat in range(a.repeats):
   variants=[('baseline',a.baseline,'0'),('scalar',a.candidate,'0'),('simd',a.candidate,'1')]
   # Balanced cyclic+reverse order; startup/warmup are outside each measurement.
   shift=repeat%3;variants=variants[shift:]+variants[:shift]
   if repeat%2:variants.reverse()
   for name,binary,enabled in variants:
    cmd=[binary,*map(str,case),enabled,str(a.blocks)]
    if a.cpu is not None:cmd=['taskset','-c',str(a.cpu),*cmd]
    fields=subprocess.check_output(cmd,text=True).strip().split(',');row=dict(zip(head,fields,strict=True));row.update(variant=name,repeat=repeat);w.writerow(row);f.flush();results.append(row)
for case in cases:
 rows=[r for r in results if tuple(int(r[k])for k in ('policy','voices','bypass','moving'))==case]
 assert len(set(r['checksum']for r in rows))==1,('checksum changed',case)
 times={v:[float(r['cpu_seconds'])for r in rows if r['variant']==v]for v in ('baseline','scalar','simd')}
 med={v:statistics.median(t)for v,t in times.items()}
 print(case,'median cpu',med,'scalar/original',med['scalar']/med['baseline'],'SIMD/original',med['simd']/med['baseline'])
