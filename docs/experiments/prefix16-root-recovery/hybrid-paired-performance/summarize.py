import csv,statistics,json
from pathlib import Path
r=Path(__file__).parent
key=lambda x:tuple(x[k]for k in ['rate','block','isolation','scenario','initial_selector','phase','mode'])
out=[]
for n in [1,2,3]:
 a={key(x):x for x in csv.DictReader((r/f'{n}-baseline/results.csv').open())};b={key(x):x for x in csv.DictReader((r/f'{n}-candidate/results.csv').open())};assert a.keys()==b.keys() and len(a)==100
 row={'pair':n,'rows':100,'different_checksums':sum(a[k]['checksum']!=b[k]['checksum']for k in a),'nonfinite':sum(int(x['nonfinite_count'])for z in[a,b]for x in z.values())}
 for phase in ['warmed_held','release_tail']:
  for scope in ['all','source_performance']:
   ratios=[float(b[k]['median_us'])/float(a[k]['median_us'])for k in a if k[5]==phase and(scope=='all'or k[6]==scope)]
   row[scope+'_'+phase]={'case_count':len(ratios),'median_case_ratio':statistics.median(ratios),'candidate_faster_cases':sum(v<1 for v in ratios)}
 out.append(row)
(r/'summary.json').write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2))
