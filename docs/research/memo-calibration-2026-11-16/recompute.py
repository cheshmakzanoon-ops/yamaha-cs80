import pathlib,json,csv,hashlib,statistics,math
p=pathlib.Path(__file__).resolve().parents[1]/'extracted'
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
assert sha(p.parent/'artifact.zip')=='202b22f7300f0cbcf37c1db3f24e3765b4b67e44edcde4e9197b0dd4595f2687'
agg={'total':[],'source_performance_dry_held':[],'isolation_dry_held':[]};reference=None;absolute=[];csvhash={};case={}
for i in range(1,5):
 pair=[]
 for side in ('left','right'):
  name=f'calibration-active-{i}-{side}';folder=p/name
  assert (p/(name+'.exit')).read_text().strip()=='0'
  rows=list(csv.DictReader((folder/'results.csv').open()));assert len(rows)==36
  rows={tuple(x[k] for k in ('mode','scenario','initial_selector','phase')):x for x in rows};assert len(rows)==36
  if reference is None:reference=rows
  assert set(rows)==set(reference)
  for k,x in rows.items():
   for f in ('rate','block','checksum','callbacks','audio_seconds','callback_cpu_call_count','nonfinite_count'):assert x[f]==reference[k][f]
   assert int(x['nonfinite_count'])==0 and int(x['callbacks'])==int(x['callback_cpu_call_count'])>0 and x['callback_cpu_status']=='available'
   assert all(math.isfinite(float(x[f])) and float(x[f])>0 for f in ('callback_cpu_seconds','audio_seconds'))
  cpu=sum(float(x['callback_cpu_seconds']) for x in rows.values());audio=sum(float(x['audio_seconds']) for x in rows.values());absolute.append({'run':name,'cpu':cpu,'audio':audio,'cpu_audio':cpu/audio})
  csvhash[name]=sha(folder/'results.csv');pair.append(rows)
 a,b=pair
 agg['total'].append((sum(float(x['callback_cpu_seconds']) for x in b.values())/sum(float(x['audio_seconds']) for x in b.values()))/(sum(float(x['callback_cpu_seconds']) for x in a.values())/sum(float(x['audio_seconds']) for x in a.values())))
 for mode in ('source_performance','isolation'):
  keys=[k for k in a if k[0]==mode and k[1]=='paired_tones' and k[3]=='warmed_held'];assert len(keys)==3
  agg[mode+'_dry_held'].append(statistics.median(float(b[k]['callback_cpu_seconds'])/float(a[k]['callback_cpu_seconds']) for k in keys))
 for k in a:case.setdefault(str(k),[]).append(float(b[k]['callback_cpu_seconds'])/float(a[k]['callback_cpu_seconds']))
stored=json.loads((p/'calibration-active.json').read_text())
for k,v in agg.items():assert v==stored['aggregates'][k]['ratios']
for k,v in case.items():assert v==stored['cases'][k]['ratios']
assert not all(.98<=v<=1.02 for a in agg.values() for v in a)
assert all(.9<=v<=1.1 for a in case.values() for v in a)
bench=json.loads((p/'benchmark-hashes.json').read_text())
for arm,h in bench.items():assert sha(p/'timed-benchmarks'/('benchmark-'+arm))==h
for kind in ('history','engine','dispatch'):
 data=(p/('A-'+kind+'.bin')).read_bytes()
 for arm in 'BC':assert (p/(arm+'-'+kind+'.bin')).read_bytes()==data
assert not list(p.glob('calibration-lowload-*')) and not list(p.glob('active-*-A')) and not list(p.glob('lowload-*-A'))
assert json.loads((p/'DECISION.json').read_text())=={'candidate_timed':False,'reason':'active calibration unusable'}
result={'calibration_runs':8,'rows':288,'aggregates':agg,'E':{k:max(abs(x-1) for x in v) for k,v in agg.items()},'absolute':absolute,'csv_hashes':csvhash,'benchmark_hashes':bench,'all_nine_corpora_pairwise_exact':True,'candidate_timed':False,'loss_or_gain_conclusion':None}
(pathlib.Path(__file__).parent/'RECOMPUTED.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ('calibration_runs','rows','E','candidate_timed')},indent=2))
