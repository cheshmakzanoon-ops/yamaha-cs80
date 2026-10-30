#!/usr/bin/env python3
"""Independent read-only first-run recomputation; does not run the benchmark.
Optional argument: preserved replay workspace, to verify actual tree/binary hashes.
Uses only the standard library; deliberately does not import the primary analyzer.
"""
import collections,csv,datetime,fractions,gzip,hashlib,io,itertools,json,math,pathlib,subprocess,sys
F=fractions.Fraction
here=pathlib.Path(__file__).resolve().parent
repo=pathlib.Path(subprocess.check_output(['git','-C',str(here),'rev-parse','--show-toplevel'],text=True).strip())
measurement=here/'first-measurement'
sha=lambda b:hashlib.sha256(b).hexdigest()
metadata=json.loads((measurement/'metadata.json').read_text())
criteria=json.loads((here/'preregistered-criteria.json').read_text())
primary=json.loads((measurement/'summary.json').read_text())
published=metadata['published_experiment_commit'];baseline=metadata['baseline_commit']
assert baseline==criteria['baseline_commit']=='428e4b2bb0b4bcee0a8f3bb8826449f699bdb618'
assert subprocess.check_output(['git','-C',str(repo),'rev-parse',published+'^{tree}'],text=True).strip()==metadata['published_experiment_tree']
commit_time=int(subprocess.check_output(['git','-C',str(repo),'show','-s','--format=%ct',published],text=True))
assert commit_time<datetime.datetime.fromisoformat(metadata['started_utc']).timestamp()
assert datetime.datetime.fromisoformat(metadata['completed_utc'])>datetime.datetime.fromisoformat(metadata['started_utc'])
assert sha((here/'preregistered-criteria.json').read_bytes())==metadata['criterion_sha256']
assert sha((here/'premeasurement-source-sha256.txt').read_bytes())==metadata['source_manifest_sha256']
# Verify all published repo manifest entries and all pristine baseline files.
# Optional actual workspace additionally verifies every candidate source and executable.
work=pathlib.Path(sys.argv[1]) if len(sys.argv)>1 else None
entries=[];binary_checked=False
for line in (here/'premeasurement-source-sha256.txt').read_text().splitlines():
 expected,name=line.split(maxsplit=1);group,relative=name.split('/',1)
 if group=='repo':
  p=here/relative;assert sha(p.read_bytes())==expected,name
  committed=subprocess.check_output(['git','-C',str(repo),'show',published+':'+str(p.relative_to(repo))]);assert sha(committed)==expected,name
 elif group=='base':
  b=subprocess.check_output(['git','-C',str(repo),'show',baseline+':'+relative]);assert sha(b)==expected,name
 if work:
  path=work/relative if group=='executable' else work/group/relative if group in ('base','candidate') else here/relative
  assert sha(path.read_bytes())==expected,name
  if group=='executable':binary_checked=True
 entries.append(name)
compressed=(measurement/'callback-times.csv.gz').read_bytes();data=gzip.decompress(compressed)
assert sha(compressed)==metadata['raw_gzip_sha256']==primary['sha256']['callback-times.csv.gz']
assert sha(data)==metadata['raw_csv_uncompressed_sha256'] and len(data)==metadata['raw_csv_uncompressed_bytes']
assert compressed[4:8]==b'\0'*4
assert sha((measurement/'results.csv').read_bytes())==primary['sha256']['results.csv']
rows=list(csv.DictReader((measurement/'results.csv').open()))
raw=list(csv.DictReader(io.StringIO(data.decode())))
assert len(rows)==288 and len(raw)==73728
assert primary['rows']==288 and primary['raw_observations']==73728 and primary['all_raw_aggregates_verified']
assert metadata['probe_exit']==0 and not metadata['production_integrated'] and not criteria['production_integrated']
arms=['baseline_428','candidate_enabled'];conditions=[(0,'cold'),(0,'retired'),(1,'held'),(8,'held')]
configs=list(itertools.product([44100,48000,96000],[128,256],['source_both','source_panel','panel_both'],['static','moving'],conditions))
assert len(configs)==144
samples=collections.defaultdict(list)
for ordinal,r in enumerate(raw):
 p,a,b,o,n=(int(r[x]) for x in ['pair','arm','callback','order','nanoseconds'])
 assert p==ordinal//512 and b==(ordinal//2)%256 and o==ordinal%2
 assert a==(p+b+o)%2 and n>0
 samples[p,a].append(n)
assert len(samples)==288 and all(len(v)==256 for v in samples.values())
by_key={};stats={};overruns=[];prefixes=collections.defaultdict(set)
for ordinal,r in enumerate(rows):
 pair=int(r['pair']);a=arms.index(r['arm']);assert (pair,a)==divmod(ordinal,2)
 rate,block,scene,controls,(voices,idle)=configs[pair]
 assert [int(r['rate']),int(r['block']),r['scene'],r['controls'],int(r['voices']),r['idle_kind']]==[rate,block,scene,controls,voices,idle]
 assert int(r['callbacks'])==256 and int(r['active'])==voices and int(r['effective'])==a and int(r['rejected'])==0
 for name in ['audio_checksum','state_checksum','filter_eg_before','filter_eg_after']:assert math.isfinite(float(r[name]))
 assert float(r['audio_checksum'])>=0
 ns=sorted(samples[pair,a]);med=F(ns[127]+ns[128],2);p99=ns[253];maximum=ns[255]
 count=sum(x*rate>10**9*block for x in ns)
 assert med==F(r['median_ns']) and p99==int(r['p99_ns']) and maximum==int(r['max_ns']) and count==int(r['overruns'])
 stats[pair,a]={'median':med,'median_RTF':med*rate/(10**9*block),'p99':p99,'max':maximum,'overruns':count}
 by_key[pair,a]=r
 for callback,n in enumerate(samples[pair,a]):
  if n*rate>10**9*block:overruns.append({'pair':pair,'arm':arms[a],'callback':callback,'nanoseconds':n,'RTF':float(F(n*rate,10**9*block))})
 if a:
  prefix=int(r['kernel_samples'])-2*(32+256)*block
  if idle!='retired':assert prefix==0
  else:
   assert prefix>=2*(4096+256) and (prefix-2*4096)%512==0
   prefixes[rate,scene].add(prefix)
   if scene=='panel_both':assert float(r['filter_eg_before'])!=float(r['filter_eg_after'])
 else:assert int(r['kernel_samples'])==0
assert all(len(v)==1 for v in prefixes.values())
# Exact rational gate comparisons, independent of primary float computations.
eight=[];idle=[]
for pair,(_,_,_,_,(voices,_)) in enumerate(configs):
 if voices==8:eight.append(stats[pair,1]['median']/stats[pair,0]['median'])
 if configs[pair][0]==48000 and voices==0:idle.append(stats[pair,1]['median_RTF'])
assert len(eight)==36 and len(idle)==24
assert len(primary['eight_active_pairs'])==36 and len(primary['48000_zero_audible_rows'])==24
for item in primary['eight_active_pairs']:
 pair=item['pair'];assert configs[pair][-1][0]==8
 assert item['ratio']==float(stats[pair,1]['median']/stats[pair,0]['median'])
 assert item['passed']==(F(str(item['ratio']))<=F('1.35'))
for item in primary['48000_zero_audible_rows']:
 pair=item['pair'];assert configs[pair][0]==48000 and configs[pair][-1][0]==0
 assert item['kind']==configs[pair][-1][1]
 assert math.isclose(item['median_RTF'],float(stats[pair,1]['median_RTF']),rel_tol=1e-15)
 assert item['passed']==(stats[pair,1]['median_RTF']<=F('.35'))
limits=criteria['cost_gates']
eight_limit=F(str(limits['all_36_eight_active_pairs_candidate_median_divided_by_baseline_median_at_most']))
idle_limit=F(str(limits['all_24_48000Hz_zero_audible_candidate_rows_median_RTF_at_most']))
passed=all(x<=eight_limit for x in eight) and all(x<=idle_limit for x in idle)
assert passed==primary['cost_gates_passed'] and len(overruns)==primary['overrun_count']==143
assert [float(min(eight)),float(max(eight))]==primary['eight_active_ratio_range']
assert all(math.isclose(x,y,rel_tol=1e-15) for x,y in zip([float(min(idle)),float(max(idle))],primary['48000_idle_median_RTF_range']))
assert len(primary['retained_overruns'])==len(overruns)
for ours,theirs in zip(overruns,primary['retained_overruns']):
 for key in ['pair','arm','callback','nanoseconds']:assert ours[key]==theirs[key]
 assert math.isclose(ours['RTF'],theirs['RTF'],rel_tol=1e-15)
assert max(r['RTF'] for r in overruns)==primary['maximum_RTF']
report={
 'status':'PASS; independent recomputation only, no benchmark rerun',
 'published_commit':published,'baseline_commit':baseline,'published_commit_precedes_run':True,
 'source_manifest_entries':len(entries),'actual_workspace_verified':bool(work),'executable_hash_verified':binary_checked,
 'sha256':{name:sha((measurement/name).read_bytes()) for name in ['callback-times.csv.gz','results.csv','metadata.json','summary.json']},
 'criteria_sha256':metadata['criterion_sha256'],'source_manifest_sha256':metadata['source_manifest_sha256'],
 'pairs':144,'aggregate_rows':288,'raw_observations':73728,'every_raw_order_and_aggregate_verified':True,
 'eight_active_pairs':36,'eight_active_ratio_range':[float(min(eight)),float(max(eight))],'eight_active_limit':float(eight_limit),
 '48000_zero_audible_rows':24,'48000_zero_audible_median_RTF_range':[float(min(idle)),float(max(idle))],'idle_limit':float(idle_limit),
 'gates_passed':passed,'total_overruns':len(overruns),
 'overruns_by_arm':dict(collections.Counter(x['arm'] for x in overruns)),
 'maximum_RTF_by_arm':{arm:max(x['RTF'] for x in overruns if x['arm']==arm) for arm in arms},
 'rows_p99_over_budget_by_arm':{arm:sum(stats[p,a]['p99']*configs[p][0]>10**9*configs[p][1] for p in range(144)) for a,arm in enumerate(arms)},
 'retired_prefix_internal_samples':{str(k):next(iter(v)) for k,v in prefixes.items()},
 'clock_scope':'Cold/held totals exact. Retired prefix matches256-frame preparation granularity and is identical across block/control variants per rate/scene; frozen probe also asserts exact post-warmup delta at runtime. Before-counter values are not separately stored.',
 'limits':'Single shared-host wall-clock run; no outlier removal; no machine WCET, DAW hard-realtime, hardware-accuracy or audio-equivalence claim.'
}
print(json.dumps(report,indent=2))
