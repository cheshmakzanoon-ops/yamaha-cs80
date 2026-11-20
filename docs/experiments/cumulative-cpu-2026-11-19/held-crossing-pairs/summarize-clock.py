import csv,json,math,random,statistics,sys
rows=list(csv.reader(open(sys.argv[1])))
assert len(rows)==160 and {int(r[0]) for r in rows}==set(range(1,33))
batches={i:{int(r[1]):r for r in rows if int(r[0])==i} for i in range(1,33)}
for t in batches.values():
 assert set(t)==set(range(5))
 assert len({v[-1] for k,v in t.items() if k!=4})==1 and all(int(v[3])==int(v[2])*16 for v in t.values())
def summary(arm):
 ratios=[math.log(int(t[arm][4]))-math.log(int(t[0][4])) for t in batches.values()]
 rng=random.Random(0xb047a);samples=sorted(math.exp(statistics.mean(rng.choices(ratios,k=len(ratios)))) for i in range(9999))
 return {'B_per_A':math.exp(statistics.mean(ratios)), 'CI95_B_per_A':[samples[249],samples[9749]]}
out={k:summary(i) for i,k in enumerate(('baseline','same_1','candidate','same_2','extra_12_5')) if i}
out['gates']={k: all((.975<=v<=1.025) for v in s['CI95_B_per_A']) for k,s in out.items() if k.startswith('same')}
out['gates']['positive']=(1.08<out['extra_12_5']['CI95_B_per_A'][0] and out['extra_12_5']['CI95_B_per_A'][1]<1.17)
out['gates']['candidate_upper']=(out['candidate']['CI95_B_per_A'][1]<1.)
print(json.dumps(out,indent=2))
