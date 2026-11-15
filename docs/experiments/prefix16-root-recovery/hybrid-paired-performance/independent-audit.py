import csv,json,statistics,math
from pathlib import Path
p=Path(__file__).parent
key=lambda x:tuple(x[k] for k in ['rate','block','isolation','scenario','initial_selector','phase','mode'])
expected=json.load(open(p/'summary.json'));actual=[];metas=[]
for n in (1,2,3):
 runs={}
 for variant,commit in [('baseline','c51120121490854bf088c88ebfd64fca83a6d289'),('candidate','bc55c11c49d40a62dbc9dd7536cde9bfe6f47a4e')]:
  root=p/f'{n}-{variant}';rows=list(csv.DictReader(open(root/'results.csv')));d={key(x):x for x in rows}
  assert len(rows)==len(d)==100
  assert all(x['rate']=='48000' and x['block']=='256' and math.isfinite(float(x['median_us'])) and float(x['median_us'])>0 for x in rows)
  assert all(x['callbacks']=='1024' for x in rows if x['phase']=='warmed_held')
  assert json.load(open(root/'completed.json'))=={'completed':True,'cases':39,'rows':100}
  assert (p/f'{n}-{variant}-exit.txt').read_text().strip()=='0'
  m=json.load(open(root/'metadata.json'));assert m['source_commit_caller_verified']==commit
  assert m['suite']=='performance' and m['callbacks_per_held_phase']==1024 and m['hardware_equivalence_or_realtime_qualification']==False
  metas.append({k:v for k,v in m.items() if k not in ('source_commit_caller_verified','started_unix_seconds')});runs[variant]=d
 a,b=runs['baseline'],runs['candidate'];assert a.keys()==b.keys()
 row={'pair':n,'rows':100,'different_checksums':sum(a[k]['checksum']!=b[k]['checksum'] for k in a),'nonfinite':sum(int(x['nonfinite_count']) for d in (a,b) for x in d.values())}
 assert row['different_checksums']==row['nonfinite']==0
 for phase in ('warmed_held','release_tail'):
  for scope in ('all','source_performance'):
   ratios=[float(b[k]['median_us'])/float(a[k]['median_us']) for k in a if k[5]==phase and (scope=='all' or k[6]==scope)]
   row[scope+'_'+phase]={'case_count':len(ratios),'median_case_ratio':statistics.median(ratios),'candidate_faster_cases':sum(v<1 for v in ratios)}
 actual.append(row)
assert actual==expected
assert all(x==metas[0] for x in metas)
print('PASS: all six unique 100-row datasets, completions, metadata scope, checksums and nonfinite checks; summary exactly reproduced.')
for x in actual:print(x['pair'],x['source_performance_warmed_held']['median_case_ratio'],x['source_performance_release_tail']['median_case_ratio'])
