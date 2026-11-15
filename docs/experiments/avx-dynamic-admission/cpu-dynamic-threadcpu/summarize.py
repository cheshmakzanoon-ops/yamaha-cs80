import csv,json,statistics,pathlib
p=pathlib.Path(__file__).parent;out=[]
for n in (1,2,3):
 if not all((p/f'{n}-{v}-exit.txt').exists() for v in ('baseline','candidate')):continue
 def read(v):return {(r['mode'],r['scenario'],r['initial_selector'],r['phase']):r for r in csv.DictReader(open(p/f'{n}-{v}/results.csv'))}
 a,b=read('baseline'),read('candidate');assert a.keys()==b.keys()
 assert all(a[k]['checksum']==b[k]['checksum'] for k in a)
 assert all(int(r['nonfinite_count'])==0 and float(r['callback_cpu_seconds'])>0 for r in list(a.values())+list(b.values()))
 groups={}
 for mode in sorted({k[0] for k in a}):
  ks=[k for k in a if k[0]==mode and k[3]=='warmed_held']
  if ks:groups[mode]={'cases':len(ks),'wall_ratio':statistics.median(float(b[k]['mean_rtf'])/float(a[k]['mean_rtf']) for k in ks),'cpu_ratio':statistics.median(float(b[k]['callback_cpu_rtf'])/float(a[k]['callback_cpu_rtf']) for k in ks),'baseline_cpu_rtf':statistics.median(float(a[k]['callback_cpu_rtf']) for k in ks),'candidate_cpu_rtf':statistics.median(float(b[k]['callback_cpu_rtf']) for k in ks)}
 idle=[{'case':k,'wall_ratio':float(b[k]['mean_rtf'])/float(a[k]['mean_rtf']),'cpu_ratio':float(b[k]['callback_cpu_rtf'])/float(a[k]['callback_cpu_rtf']),'baseline_cpu_rtf':float(a[k]['callback_cpu_rtf']),'candidate_cpu_rtf':float(b[k]['callback_cpu_rtf'])} for k in a if 'idle' in k[1]]
 out.append({'pair':n,'rows':len(a),'checksums_exact':True,'groups':groups,'idle_cases':idle})
(p/'summary.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
