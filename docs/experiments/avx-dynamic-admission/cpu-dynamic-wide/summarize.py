import csv,json,statistics,pathlib
p=pathlib.Path(__file__).parent;out=[]
for n in ['wide','idle']:
 if not all((p/f'{n}-{v}-exit.txt').exists() for v in ['baseline','candidate']):continue
 def read(v):
  rows=list(csv.DictReader(open(p/f'{n}-{v}/results.csv')))
  return {(r['mode'],r['scenario'],r['initial_selector'],r['phase']):r for r in rows}
 a,b=read('baseline'),read('candidate');assert a.keys()==b.keys()
 assert all(a[k]['checksum']==b[k]['checksum'] for k in a),'checksum mismatch'
 assert all(int(r['nonfinite_count'])==0 for r in list(a.values())+list(b.values()))
 groups={}
 for mode in sorted({k[0] for k in a}):
  keys=[k for k in a if k[0]==mode and k[3]=='warmed_held']
  if keys:groups[mode]={'cases':len(keys),'median_candidate_over_baseline_mean_rtf':statistics.median(float(b[k]['mean_rtf'])/float(a[k]['mean_rtf']) for k in keys),'baseline_median_mean_rtf':statistics.median(float(a[k]['mean_rtf']) for k in keys),'candidate_median_mean_rtf':statistics.median(float(b[k]['mean_rtf']) for k in keys)}
 out.append({'pair':n,'rows':len(a),'checksums_exact':True,'groups':groups})
print(json.dumps(out,indent=2));(p/'summary.json').write_text(json.dumps(out,indent=2)+'\n')
