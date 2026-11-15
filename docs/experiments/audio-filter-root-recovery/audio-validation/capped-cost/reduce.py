import csv,json,statistics,math
from pathlib import Path
p=Path(__file__).parent
rows=list(csv.DictReader(open(p/'results.csv')))
assert len(rows)==558
assert len({(r['scenario'],r['round'],r['variant']) for r in rows})==558
out=[]
for s in range(6):
 entry={'scenario':s,'meaning':f'one capped lane {s}' if s<4 else 'four capped lanes' if s==4 else 'four converging zero-input lanes'}
 groups={}
 for v in ['baseline_scalar','repaired_scalar','repaired_wrapper']:
  a=[r for r in rows if r['scenario']==str(s) and r['variant']==v];assert len(a)==31
  assert all(r['groups']=='512' for r in a)
  assert all(int(r['vector_commits'])==(512 if v=='repaired_wrapper' and s==5 else 0) for r in a)
  assert len({r['checksum'] for r in a})==1
  x=sorted(float(r['ns_per_group']) for r in a);assert all(math.isfinite(t) and t>0 for t in x)
  groups[v]={int(r['round']):float(r['ns_per_group']) for r in a}
  entry[v]={'median_ns_per_group':statistics.median(x),'p99_chunk_average_ns_per_group':x[math.ceil(.99*len(x))-1],'max_chunk_average_ns_per_group':x[-1],'checksum':a[0]['checksum']}
 assert entry['repaired_scalar']['checksum']==entry['repaired_wrapper']['checksum']
 for a,b in [('repaired_scalar','baseline_scalar'),('repaired_wrapper','repaired_scalar')]:
  entry[a+'_over_'+b]=statistics.median(groups[a][r]/groups[b][r] for r in range(31))
 out.append(entry)
(p/'summary.json').write_text(json.dumps(out,indent=2)+'\n')
for e in out:print(e['scenario'],*[round(e[v]['median_ns_per_group'],2) for v in ['baseline_scalar','repaired_scalar','repaired_wrapper']])
