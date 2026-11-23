import csv,json,math,re,statistics,hashlib
from pathlib import Path
from collections import defaultdict
r=Path('/workspace/scratch/dde4ad85ed5b')
inv=json.loads((r/'evidence/final/inventory.json').read_text());ids={n:i for i,n in enumerate(inv['parameter_ids'])}
models={2:[],9:[]}
for p in inv['programs']:
 for m in models:
  ranks=[k for k in (1,2) if p['values'][ids[f'body{k}_model']]==m and p['values'][ids[f'body{k}_mix']]>0]
  if ranks: models[m].append({'host_index':p['host_index'],'name':p['name'],'ranks':ranks})
a={p['host_index'] for p in models[2]};b={p['host_index'] for p in models[9]}
coverage={'tine_programs':len(a),'tine_ranks':sum(len(p['ranks']) for p in models[2]),'bowed_programs':len(b),'bowed_ranks':sum(len(p['ranks']) for p in models[9]),'intersection':sorted(a&b),'distinct_programs':len(a|b),'tine_original':sum(i<222 for i in a),'tine_curated':sum(i>=222 for i in a),'bowed_original':sum(i<222 for i in b),'bowed_curated':sum(i>=222 for i in b)}
load=lambda p:list(csv.DictReader((r/p).open()))
base=load('a05/metrics.csv');cand=load('a09/metrics.csv');parity=load('a09/baseline-parity-metrics.csv')
key=lambda x:(x['name'],int(x['note']),round(float(x['velocity']),5))
assert len({key(x) for x in cand})==len(cand)==90
cmap={key(x):x for x in cand}; mismatches=[key(x) for x in parity if cmap[key(x)]['audio_fnv1a64']!=x['audio_fnv1a64']]
spans={}
for name in ['Electric Piano I','Electric Piano II','Tine Piano']:
 rows=[]
 for note in [36,60,84]:
  before=sorted([x for x in base if x['name']==name and int(x['note'])==note],key=lambda x:float(x['velocity']))
  after=sorted([x for x in cand if x['name']==name and int(x['note'])==note],key=lambda x:float(x['velocity']))
  old=float(before[-1]['rms_300_800_dbfs'])-float(before[0]['rms_300_800_dbfs']);new=float(after[-1]['rms_300_800_dbfs'])-float(after[0]['rms_300_800_dbfs']);rows.append({'note':note,'before_db':old,'after_db':new,'delta_db':new-old})
 spans[name]={'registers':rows,'before_range_db':[min(x['before_db'] for x in rows),max(x['before_db'] for x in rows)],'after_range_db':[min(x['after_db'] for x in rows),max(x['after_db'] for x in rows)]}
monotonic={}
for metric in ['peak_dbfs','rms_025_250_dbfs','rms_300_800_dbfs','fund_025_250_dbfs']:
 failures=[]
 for name,note in {(x['name'],int(x['note'])) for x in cand}:
  vals=[float(x[metric]) for x in sorted([x for x in cand if x['name']==name and int(x['note'])==note],key=lambda x:float(x['velocity']))]
  if any(y<x for x,y in zip(vals,vals[1:])): failures.append([name,note])
 monotonic[metric]={'groups':15,'nonmonotonic_groups':failures}
lines=(r/'a10/final-bowed-results.log').read_text().splitlines();spectral=defaultdict(list);cpu=defaultdict(list)
for line in lines:
 if line.startswith('spectral,'):
  q=line.split(',');spectral[(int(q[2]),int(q[3]),float(q[4]))].append((abs(float(q[6])),abs(float(q[7]))))
 match=re.match(r'CPU model=(\d+) moving=(\d+) trial=(\d+) baseline=(\S+) prototype=(\S+) ratio=(\S+)',line)
 if match: m,moving,t,o,n,z=match.groups();cpu[(m,moving)].append({'trial':int(t),'old':float(o),'new':float(n),'reported_ratio':float(z),'recomputed_ratio_from_rounded_seconds':float(n)/float(o)})
spectral_out=[{'from':k[0],'to':k[1],'size':k[2],'harmonic_count':len(v),'baseline_max_abs_ratio_error_db':max(x[0] for x in v),'candidate_max_abs_ratio_error_db':max(x[1] for x in v)} for k,v in spectral.items()]
cpu_out=[{'model':int(k[0]),'moving':bool(int(k[1])),'pairs':len(v),'median_reported_pair_ratio':statistics.median(x['reported_ratio'] for x in v),'median_pair_ratio_from_rounded_times':statistics.median(x['recomputed_ratio_from_rounded_seconds'] for x in v)}for k,v in cpu.items()]
trials=load('a20/trials.csv');full=defaultdict(dict)
for row in trials:full[(row['case'],row['preset'],row['moving'],row['trial'])][row['version']]=row
assert len(trials)==112 and len(full)==56 and all(set(v)=={'baseline','candidate'} for v in full.values())
bycase=defaultdict(list)
for (case,preset,moving,trial),v in full.items():bycase[(case,preset,moving)].append(float(v['candidate']['thread_cpu_seconds'])/float(v['baseline']['thread_cpu_seconds']))
full_out=[{'case':int(k[0]),'preset':k[1],'moving':bool(int(k[2])),'pairs':len(v),'median':statistics.median(v),'min':min(v),'max':max(v)} for k,v in bycase.items()]
parity_count_a06=(18*3+1)*3*5*3*4096
parity_count_a10=20*3*sum(int(1.6*2*rate) for rate in (44100,48000,96000))
summary={'review':'A47 independent arithmetic and source-scope audit; no render reruns','final_source':'078c0981ffa1b5472342e0fb5ab8702bc3bd2889','coverage_definition':'Configured model selector 2=tine/9=bowed with body mix>0 in either rank; not listening/audibility proof.','coverage':coverage,'tine_affected':models[2],'bowed_affected':models[9],'full_preset_dynamics':{'candidate_captures':len(cand),'programs':len({x['name'] for x in cand}),'registers':3,'velocities_per_register':6,'spans':spans,'monotonic':monotonic,'parity_cases':len(parity),'parity_mismatches':mismatches,'parity_float_samples':len(parity)*96000},'direct_parity':{'a06_computed_count':parity_count_a06,'a06_comparison':'binary32 bit comparison; 18 other enabled models at3velocities plus tine at1, 3rates*5notes*3colours*4096samples','a10_computed_count':parity_count_a10,'a10_comparison':'Numeric float != and max absolute difference, not bitwise; includes Off plus19models, 3notes, 3rates,1.6seconds at2x; bowed constantpitch, allothermodels+7semitone slide'},'bowed_spectral_errors':spectral_out,'body_cpu':cpu_out,'engine_cpu':full_out,'engine_run_checks':{'runs':len(trials),'all_8_active_voices':all(int(x['active_voices'])==8 for x in trials),'all_numeric_results_finite':all(all(math.isfinite(float(x[k])) for k in ['thread_cpu_seconds','peak','rms']) for x in trials),'max_peak':max(float(x['peak']) for x in trials),'body_off_all_pairs_hash_identical':all(v['baseline']['audio_fnv1a64']==v['candidate']['audio_fnv1a64'] for k,v in full.items() if k[0]=='3')},'input_sha256':{p:hashlib.sha256((r/p).read_bytes()).hexdigest() for p in ['evidence/final/inventory.json','a05/metrics.csv','a09/metrics.csv','a09/baseline-parity-metrics.csv','a10/final-bowed-results.log','a20/trials.csv']}}
(r/'a47-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:summary[k] for k in ['coverage','full_preset_dynamics','direct_parity','bowed_spectral_errors','body_cpu','engine_cpu','engine_run_checks']},indent=2))
