#!/usr/bin/env python3
"""Independent artifact recomputation only; never executes the benchmark."""
import collections,csv,fractions,gzip,hashlib,io,itertools,json,math,pathlib,subprocess
F=fractions.Fraction
here=pathlib.Path(__file__).resolve().parent
repo=pathlib.Path(subprocess.check_output(['git','-C',str(here),'rev-parse','--show-toplevel'],text=True).strip())
p=here/'results';meta=json.loads((p/'metadata.json').read_text());primary=json.loads((p/'summary.json').read_text())
sha=lambda b:hashlib.sha256(b).hexdigest()
assert sha((here/'premeasurement-sha256.txt').read_bytes())==meta['source_manifest_sha256']
manifest=[]
for line in (here/'premeasurement-sha256.txt').read_text().splitlines():
 expected,name=line.split(maxsplit=1);path=pathlib.Path(name);path=path if path.is_absolute() else repo/path
 assert sha(path.read_bytes())==expected,name
 manifest.append(name)
compressed=(p/'callback-times.csv.gz').read_bytes();rawdata=gzip.decompress(compressed)
assert sha(compressed)==meta['raw_gzip_sha256'] and sha(rawdata)==meta['raw_uncompressed_sha256']
assert len(rawdata)==meta['raw_uncompressed_bytes'] and compressed[4:8]==b'\0'*4
assert meta['probe_exit']==0 and meta['cost_gate'] is None and primary['acceptance_gate'] is None
raw=list(csv.DictReader(io.StringIO(rawdata.decode())));rows=list(csv.DictReader((p/'results.csv').open()));numeric=list(csv.DictReader((p/'numerical-differences.csv').open()))
assert len(raw)==33792 and len(rows)==132 and len(numeric)==66
conditions=[(0,'cold'),(0,'retired'),(1,'held'),(8,'held')]
configs=[(rate,block,scene,'static',v,k) for rate,block,scene,(v,k) in itertools.product([48000,96000],[128,256],['source_both','panel_both'],conditions)]
configs.append((96000,128,'source_panel','moving',1,'held'))
groups=collections.defaultdict(list);flags=collections.Counter()
for ordinal,r in enumerate(raw):
 c,a,b,o,n,before,after=(int(r[x]) for x in ['cell','arm','callback','order','nanoseconds','mxcsr_before','mxcsr_after'])
 assert (c,b,o)==(ordinal//1024,(ordinal//4)%256,ordinal%4) and a==(c+b+o)%4 and n>0
 assert before&0x3f==0 and before&0x8040==(0x8040 if a&1 else 0)
 assert before&0x7f80==0x1f80 and after&~0x3f==before&~0x3f
 groups[c,a].append((n,after))
 for bit,name in [(1,'invalid'),(2,'denormal_operand'),(4,'divide_by_zero'),(8,'overflow'),(16,'underflow'),(32,'inexact')]:
  flags[a,name]+=bool(after&bit)
assert len(groups)==132 and all(len(v)==256 for v in groups.values())
stats={};rowmap={};overruns=collections.Counter();maximum=collections.defaultdict(F)
for ordinal,r in enumerate(rows):
 c=int(r['cell']);engine=0 if r['engine']=='baseline' else 1;flush=0 if r['fp_policy']=='FTZ_DAZ_OFF' else 1;a=engine*2+flush
 assert (c,a)==divmod(ordinal,4)
 rate,block,scene,controls,voices,kind=configs[c]
 assert (int(r['rate']),int(r['block']),r['scene'],r['controls'],int(r['voices']),r['idle_kind'])==configs[c]
 assert int(r['callbacks'])==256 and int(r['active'])==voices and int(r['effective'])==engine and int(r['rejected'])==0
 ns=sorted(n for n,_ in groups[c,a]);med=F(ns[127]+ns[128],2);count=sum(n*rate>10**9*block for n in ns)
 assert med==F(r['median_ns']) and ns[253]==int(r['p99_ns']) and ns[-1]==int(r['max_ns']) and count==int(r['overruns'])
 assert int(r['denormal_operand_callbacks'])==sum(bool(after&2) for _,after in groups[c,a])
 assert int(r['underflow_callbacks'])==sum(bool(after&16) for _,after in groups[c,a])
 for field in ['audio_checksum','state_checksum','filter_eg_checksum']:assert math.isfinite(float(r[field]))
 if engine:
  prefix=int(r['kernel_samples'])-576*block
  if kind!='retired':assert prefix==0
  else:assert prefix>=8704 and (prefix-8192)%512==0
 else:assert int(r['kernel_samples'])==0
 stats[c,a]=med;rowmap[c,a]=r;overruns[a]+=count;maximum[a]=max(maximum[a],F(ns[-1]*rate,10**9*block))
# Numerical counts are harness observations, not independently recomputable audio.
totals=collections.Counter()
for ordinal,r in enumerate(numeric):
 c=int(r['cell']);e=0 if r['engine']=='baseline' else 1;assert (c,e)==divmod(ordinal,2)
 assert int(r['samples'])==configs[c][1]*2*256;totals[e]+=int(r['samples'])
 for name in ['bit_differences','nonzero_differences','signed_zero_differences','off_subnormal_outputs','on_subnormal_outputs']:assert int(r[name])==0
 for name in ['max_absolute_difference','rms_difference']:assert float(r[name])==0
 for n,agg in [('state_checksum','state_checksum'),('filter_eg','filter_eg_checksum')]:
  assert r[n+'_off']==rowmap[c,e*2][agg] and r[n+'_on']==rowmap[c,e*2+1][agg]
  assert r[n+'_off']==r[n+'_on']
 assert rowmap[c,e*2]['audio_checksum']==rowmap[c,e*2+1]['audio_checksum']
result={}
for e,name in enumerate(['baseline','candidate']):
 ratios=[stats[c,e*2+1]/stats[c,e*2] for c in range(33)];ref=primary['engines'][name]
 assert [float(min(ratios)),float(max(ratios))]==ref['on_off_median_ratio_range']
 assert float(sorted(ratios)[16])==ref['median_of_ratios']
 for c,x in enumerate(ref['on_off_ratios']):assert x['cell']==c and x['on_divided_by_off_median']==float(ratios[c])
 policies={}
 for flush,label in enumerate(['FTZ_DAZ_OFF','FTZ_DAZ_ON']):
  a=e*2+flush;v={'overruns':overruns[a],'maximum_RTF':float(maximum[a]),'denormal_operand_callbacks':flags[a,'denormal_operand'],'underflow_callbacks':flags[a,'underflow']}
  for key,value in v.items():assert math.isclose(value,ref['policies'][label][key],rel_tol=1e-15)
  policies[label]=v
 assert totals[e]==ref['harness_reported_output_samples_compared']==3211264
 result[name]={'median_ratio_range':[float(min(ratios)),float(max(ratios))],'median_of_ratios':float(sorted(ratios)[16]),'policies':policies,'harness_reported_samples_compared':totals[e],'harness_reported_bit_differences':0}
first=here.parent/'first-measurement';firstSummary=json.loads((first/'summary.json').read_text())
assert sha((first/'results.csv').read_bytes())==firstSummary['sha256']['results.csv']
assert sha((first/'callback-times.csv.gz').read_bytes())==firstSummary['sha256']['callback-times.csv.gz']
report={'status':'PASS independent artifact recomputation; no measurement rerun','cells':33,'rows':132,'raw_observations':33792,'all_raw_order_timing_and_mxcsr_flags_verified':True,'all_current_manifest_sources_objects_executable_verified':len(manifest),'raw_sha256':sha(compressed),'results_sha256':sha((p/'results.csv').read_bytes()),'source_manifest_sha256':meta['source_manifest_sha256'],'engines':result,'all_measured_denormal_and_underflow_flags_zero':all(flags[a,n]==0 for a in range(4) for n in ['denormal_operand','underflow']),'first_measurement_hashes_unchanged':True,'limits':'No acceptance gate; no replacement first result. Numerical difference counts are source-auditable harness observations, not independently recomputed audio (samples not persisted). Equal aggregate state checksums are not full-state equality. No causal attribution of earlier outliers or DAW deadline qualification.'}
print(json.dumps(report,indent=2))
