#!/usr/bin/env python3
"""Recompute targeted FP timing/flag aggregates; no acceptance gate."""
import csv, gzip, hashlib, itertools, json, math, pathlib, statistics, sys
p=pathlib.Path(sys.argv[1])
with gzip.open(p/'callback-times.csv.gz','rt',newline='') as f: raw=list(csv.DictReader(f))
with (p/'results.csv').open(newline='') as f: rows=list(csv.DictReader(f))
with (p/'numerical-differences.csv').open(newline='') as f: numeric=list(csv.DictReader(f))
assert len(raw)==33792 and len(rows)==132 and len(numeric)==66
groups={}
for i,r in enumerate(raw):
    cell,arm,b,order,ns,before,after=(int(r[k])for k in ('cell','arm','callback','order','nanoseconds','mxcsr_before','mxcsr_after'))
    assert (cell,b,order)==(i//1024,(i%1024)//4,i%4)
    assert arm==(cell+b+order)%4 and ns>0
    assert before&0x3f==0 and (before&0x8040)==(0x8040 if arm%2 else 0)
    assert before&~0x3f==after&~0x3f
    assert before&0x7f80==0x1f80
    groups.setdefault((cell,arm),[]).append((b,ns,after&0x3f))
expected=set(itertools.product((48000,96000),(128,256),('source_both','panel_both'),('static',),((0,'cold'),(0,'retired'),(1,'held'),(8,'held'))))
expected.add((96000,128,'source_panel','moving',(1,'held')))
seen=set(); by={}; overruns=[]
for r in rows:
    cell=int(r['cell']);arm=(2 if r['engine']=='candidate' else 0)+(r['fp_policy']=='FTZ_DAZ_ON')
    values=groups[(cell,arm)];assert sorted(b for b,_,_ in values)==list(range(256))
    sorted_ns=sorted(ns for _,ns,_ in values)
    assert float(r['median_ns'])==statistics.median(sorted_ns)
    assert int(r['p99_ns'])==sorted_ns[253] and int(r['max_ns'])==sorted_ns[-1]
    budget=1e9*int(r['block'])/int(r['rate'])
    assert int(r['overruns'])==sum(ns>budget for _,ns,_ in values)
    assert int(r['denormal_operand_callbacks'])==sum(bool(flags&2)for _,_,flags in values)
    assert int(r['underflow_callbacks'])==sum(bool(flags&16)for _,_,flags in values)
    assert int(r['rejected'])==0 and int(r['effective'])==(arm>=2) and int(r['active'])==int(r['voices'])
    config=(int(r['rate']),int(r['block']),r['scene'],r['controls'],(int(r['voices']),r['idle_kind']))
    seen.add(config);by[(cell,r['engine'],r['fp_policy'])]=r
    for b,ns,flags in values:
        if ns>budget:overruns.append(dict(cell=cell,arm=arm,callback=b,nanoseconds=ns,RTF=ns/budget,mxcsr_exception_flags=flags))
assert seen==expected and len(groups)==132
engines={}
for engine in ('baseline','candidate'):
    ratios=[]
    for cell in range(33):
        off,on=(by[(cell,engine,fp)]for fp in ('FTZ_DAZ_OFF','FTZ_DAZ_ON'))
        assert all(off[k]==on[k]for k in ('rate','block','scene','controls','voices','idle_kind','active','kernel_samples'))
        ratios.append(dict(cell=cell,on_divided_by_off_median=float(on['median_ns'])/float(off['median_ns'])))
    n=[r for r in numeric if r['engine']==engine]
    assert len(n)==33
    for r in n:
        cell=int(r['cell']);off,on=(by[(cell,engine,fp)]for fp in ('FTZ_DAZ_OFF','FTZ_DAZ_ON'))
        assert int(r['samples'])==256*int(off['block'])*2
        for suffix,row in (('off',off),('on',on)):
            assert float(r['state_checksum_'+suffix])==float(row['state_checksum'])
            assert float(r['filter_eg_'+suffix])==float(row['filter_eg_checksum'])
    policies={}
    for fp in ('FTZ_DAZ_OFF','FTZ_DAZ_ON'):
        selected=[r for r in rows if r['engine']==engine and r['fp_policy']==fp]
        policies[fp]={'overruns':sum(int(r['overruns'])for r in selected),
            'denormal_operand_callbacks':sum(int(r['denormal_operand_callbacks'])for r in selected),
            'underflow_callbacks':sum(int(r['underflow_callbacks'])for r in selected),
            'maximum_RTF':max(int(r['max_ns'])*int(r['rate'])/(1e9*int(r['block']))for r in selected)}
    engines[engine]={'on_off_ratios':ratios,'on_off_median_ratio_range':[min(x['on_divided_by_off_median']for x in ratios),max(x['on_divided_by_off_median']for x in ratios)],
        'median_of_ratios':statistics.median(x['on_divided_by_off_median']for x in ratios),'policies':policies,
        'harness_reported_output_samples_compared':sum(int(r['samples'])for r in n),
        'harness_reported_output_bit_differences':sum(int(r['bit_differences'])for r in n),
        'harness_reported_nonzero_differences':sum(int(r['nonzero_differences'])for r in n),
        'harness_reported_maximum_absolute_difference':max(float(r['max_absolute_difference'])for r in n),
        'harness_reported_subnormal_outputs_off':sum(int(r['off_subnormal_outputs'])for r in n),
        'harness_reported_subnormal_outputs_on':sum(int(r['on_subnormal_outputs'])for r in n),
        'state_checksum_difference_cells':sum(float(r['state_checksum_off'])!=float(r['state_checksum_on'])for r in n),
        'filter_eg_checksum_difference_cells':sum(float(r['filter_eg_off'])!=float(r['filter_eg_on'])for r in n)}
out={'diagnostic_only':True,'acceptance_gate':None,'original_first_result_replaced':False,'cells':33,'rows':132,'raw_observations':len(raw),
     'all_raw_timing_and_flag_aggregates_verified':True,'engines':engines,'retained_overruns':overruns,
     'numeric_evidence_scope':'paired final output compared directly outside timers by frozen harness; numerical aggregate counters and source retained, not all audio samples; equal aggregate state checksums do not prove identical full hidden state',
     'sha256':{name:hashlib.sha256((p/name).read_bytes()).hexdigest()for name in ('results.csv','callback-times.csv.gz','numerical-differences.csv')}}
with (p/'summary.json').open('x')as f:json.dump(out,f,indent=2);f.write('\n')
print(json.dumps({engine:{k:v for k,v in info.items()if k!='on_off_ratios'}for engine,info in engines.items()},indent=2))
