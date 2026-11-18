import csv,json,math,pathlib,random,statistics
r=pathlib.Path(__file__).resolve().parent
names=['held-low','held-middle','held-high','moving-pitch-pwm','dense-controls','idle'];keys=[(n,c) for c in [16,32] for n in names];active=[k for k in keys if k[0]!='idle']
def load(phase,rounds):
 rows=list(csv.DictReader((r/(phase+'.csv')).open()));assert len(rows)==12*rounds*2
 groups={};i=0
 for k in keys:
  groups[k]=[];checksums=set()
  for n in range(rounds):
   pair={}
   for order in range(2):
    x=rows[i];i+=1;version='baseline' if (order+n)%2==0 else 'candidate'
    assert (x['scenario'],int(x['oscillators']))==k and int(x['round'])==n and int(x['order'])==order and x['version']==version
    assert int(x['frames'])==32768 and int(x['failed'])==0 and int(x['cpu_start'])==int(x['cpu_end'])==0
    assert int(x['elapsed_ns'])>0;pair[version]=x;checksums.add(x['checksum'])
   assert pair['baseline']['checksum']==pair['candidate']['checksum'];groups[k].append(pair)
  assert len(checksums)==1
 return groups
A=load('aa',8);B=load('ab',16)
def blocks(g,k):
 return [math.sqrt(int(g[k][i]['candidate']['elapsed_ns'])*int(g[k][i+1]['candidate']['elapsed_ns'])/(int(g[k][i]['baseline']['elapsed_ns'])*int(g[k][i+1]['baseline']['elapsed_ns']))) for i in range(0,len(g[k]),2)]
aa={k:blocks(A,k) for k in keys};ab={k:blocks(B,k) for k in keys}
for k in keys:assert A[k][0]['baseline']['checksum']==B[k][0]['baseline']['checksum']
rng=random.Random(20261117);resamples=[[rng.randrange(8) for _ in range(8)] for _ in range(10000)]
def gm(v):return math.exp(sum(math.log(x)for x in v)/len(v))
def stats(v):
 boot=sorted(1-gm([v[i] for i in ix]) for ix in resamples)
 return {'ratio':gm(v),'gain':1-gm(v),'gain_ci95':[boot[249],boot[9749]],'ratios_by_block':v}
pv=[gm([ab[k][i] for k in active]) for i in range(8)];primary=stats(pv);noise=max(abs(1-gm([aa[k][i] for k in active])) for i in range(4));floor=max(.03,noise)
cases={str(k):{**stats(ab[k]),'aa_ratios_by_block':aa[k]} for k in keys}
advance=primary['gain_ci95'][0]>floor and all(x['gain_ci95'][0]>=-.03 for x in cases.values())
result={'scope':'isolated oscillator thread CPU; not native plugin callback or platform qualification','decision':'ADVANCE_COMPONENT_SIGNAL' if advance else 'NO_ADVANCE_GAIN_OR_UNCERTAIN_GUARD','bootstrap_unit':'whole ABBA block index shared across all ten active cases','bootstrap_samples':10000,'seed':20261117,'primary':primary,'aggregate_aa_max_absolute_gain':noise,'single_active_case_aa_max_absolute_gain':max(abs(1-x) for k in active for x in aa[k]),'primary_floor':floor,'case_guard_lower_gain_minimum':-.03,'cases':cases}
assert result==json.loads((r/'summary.json').read_text()), 'stored component summary mismatch';print('576 component rows match all original statistics.')
