from pathlib import Path
import os,json,csv,subprocess,time,math,random,statistics,hashlib,datetime
P=Path(__file__).resolve().parent;T=P/'bench';plan=json.loads((T/'plan.json').read_text())
def check_inputs():
 for n,h in plan['sha256'].items():assert hashlib.sha256((P/n).read_bytes()).hexdigest()==h,n
check_inputs();start=time.monotonic();cpu=plan['cpu'];cases=plan['cases'];rows=[];seen={};first=datetime.datetime.now(datetime.timezone.utc).isoformat()
with (T/'raw.csv').open('x') as f:
 w=csv.writer(f);w.writerow(['family','block','case','slot','label','arm','cpu_start','cpu_end','render_thread_ns','clock_pair_ns','pcm_fnv64','nonzero_samples','active_voice_sample_blocks'])
 for fam,count in [('aa',2),('comparison',6)]:
  for k in range(count):
   for p,name in enumerate(cases):
    for slot,label in enumerate('ABBA'):
     assert time.monotonic()-start<600,'global deadline'
     arm='candidate' if fam=='comparison' and label=='B' else 'stock'
     proc=subprocess.run(['taskset','-c',str(cpu),str(T/arm),str(p)],capture_output=True,text=True,timeout=12);assert proc.returncode==0,(fam,k,p,slot,arm,proc.returncode,proc.stderr)
     q=proc.stdout.strip().split(',');assert len(q)==8 and q[0]==name and q[1]==q[2]==str(cpu);ns=int(q[3]);clk=int(q[4]);assert ns>clk>0 and (name not in seen or seen[name]==q[5:]),'observation mismatch';seen[name]=q[5:]
     assert int(q[7])==0 if p<2 else int(q[7])==8*1024,'voice occupancy'
     if p>=2:assert int(q[6])>0
     w.writerow([fam,k,name,slot,label,arm,*q[1:]]);f.flush();rows.append((fam,k,p,slot,ns/65536,clk/ns))
   print(fam,k,'slots',len(rows),flush=True)
def blocks(fam,p,n):return [(math.log(vals[0])+math.log(vals[3])-math.log(vals[1])-math.log(vals[2]))/2 for i in range(n) if (vals:=[r[4] for r in rows if r[:3]==(fam,i,p)])]
rng=random.Random(20261118);idxs=[rng.choices(range(6),k=6) for _ in range(10000)];g=lambda e:1-math.exp(-e)
def summary(v,aa,over):
 vals=sorted(sum(v[i] for i in ids)/6 for ids in idxs)
 def pct(p):i=p*9999;lo=int(i);return g(vals[lo]+(vals[min(lo+1,9999)]-vals[lo])*(i-lo))
 return {'gain':g(sum(v)/6),'gain_ci95':[pct(.025),pct(.975)],'aa_noise':max(abs(g(x)) for x in aa),'max_clock_fraction':max(over),'ab_block_gains':list(map(g,v)),'aa_block_gains':list(map(g,aa))}
per={name:summary(blocks('comparison',p,6),blocks('aa',p,2),[r[5] for r in rows if r[2]==p]) for p,name in enumerate(cases)}
pooled=lambda fam,n:[(x+y)/2 for x,y in zip(blocks(fam,0,n),blocks(fam,1,n))]
pr=summary(pooled('comparison',6),pooled('aa',2),[r[5] for r in rows if r[2]<2]);pr['floor']=max(.02,pr['aa_noise'],3*pr['max_clock_fraction'])
admit=pr['gain_ci95'][0]>pr['floor'] and all(per[z]['gain_ci95'][0]>0 for z in cases[:2]) and all(per[z]['gain_ci95'][0]>=-.03 for z in cases[2:])
check_inputs();assert len(rows)==224
s={'admit_for_independent_plugin_study':admit,'slots':len(rows),'source_and_output_identities':seen,'start_utc':first,'wall_seconds':time.monotonic()-start,'primary':pr,'cases':per}
(T/'summary.json').write_text(json.dumps(s,indent=2));print(json.dumps(s))
