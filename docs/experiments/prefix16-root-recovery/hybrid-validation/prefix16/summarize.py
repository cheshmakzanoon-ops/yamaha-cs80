import csv,json,statistics,math
from pathlib import Path
p=Path(__file__).parent;out=[]
for pair in (1,2,3):
 runs={v:list(csv.DictReader(open(p/f'{v}-{pair}.csv'))) for v in ('old','new')}
 for rows in runs.values():
  assert len(rows)==558
  assert len({(r['scenario'],r['round'],r['variant']) for r in rows})==558
  assert all(r['groups']=='512' and math.isfinite(float(r['ns_per_group'])) for r in rows)
 for s in range(6):
  e={'pair':pair,'scenario':s}
  for variant in ('baseline_scalar','repaired_scalar','repaired_wrapper'):
   vals={}
   for v,rows in runs.items():
    a=[r for r in rows if int(r['scenario'])==s and r['variant']==variant];assert len(a)==31
    assert len({r['checksum'] for r in a})==1
    vals[v]=statistics.median(float(r['ns_per_group']) for r in a)
   e[variant]={'old_ns':vals['old'],'new_ns':vals['new'],'new_over_old':vals['new']/vals['old']}
  out.append(e)
(p/'summary.json').write_text(json.dumps(out,indent=2)+'\n')
for e in out:print(e['pair'],e['scenario'],*[round(e[v]['new_over_old'],3) for v in ('baseline_scalar','repaired_scalar','repaired_wrapper')])
