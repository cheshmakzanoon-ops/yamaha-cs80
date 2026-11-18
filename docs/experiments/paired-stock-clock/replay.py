#!/usr/bin/env python3
"""Portable OFFLINE replay. No DSP, compiler, native loader, subprocess or clocks."""
import argparse
import copy
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
from contract import canonical, schedule, expected_rows, SCOPE, LIMITS, CLOCK_KEYS
from analyze import validate_rows, metric_effects, process_gates, cohort_gates

FIELDS=['slot','sequence','kind','phase','logical_chunk','side','first_block','count','extra_enabled','timed',
        'thread_begin_relative_ns','wall_begin_relative_ns','wall_end_relative_ns','thread_end_relative_ns','receipt']
INTS={'slot','sequence','logical_chunk','first_block','count','receipt'}
NULLABLE={'logical_chunk','side','first_block'}
META=['sequence','kind','phase','logical_chunk','side','first_block','count','extra_enabled','timed']
def require(ok,why):
    if not ok:raise ValueError(why)
def sha(data):return hashlib.sha256(data).hexdigest()
def load(path):return json.loads(Path(path).read_text())
def zipped(path):return json.loads(gzip.decompress(Path(path).read_bytes()))

def final_gate(protocol):
    terminal=protocol['terminal']
    require(terminal['status']=='complete' and terminal['completed_pairs']==20 and terminal['no_resume'] is True
            and terminal['failure'] is None,'final complete 20-slot terminal required')
    require(protocol['scope']==SCOPE and protocol['slots']==schedule() and protocol['limits']==LIMITS,
            'stock-only scope, complete schedule or thresholds changed')
    require(protocol['official_decision_verified'] is True and protocol['official_all_process_integrity_pass'] is True,
            'verified official complete decision required')

def decode(data,receipts):
    reader=csv.DictReader(io.StringIO(data.decode(),newline=''))
    require(reader.fieldnames==FIELDS,'CSV schema changed')
    groups={n:[] for n in range(20)}
    last_slot=0
    for flat in reader:
        require(None not in flat and all(v is not None for v in flat.values()),'CSV row width')
        for k in INTS:
            if flat[k]=='' and k in NULLABLE:flat[k]=None
            else:
                n=int(flat[k]);require(str(n)==flat[k],'noncanonical integer');flat[k]=n
        for k in ['extra_enabled','timed']:
            require(flat[k] in ['0','1'],'noncanonical boolean');flat[k]=flat[k]=='1'
        if flat['side']=='':flat['side']=None
        slot=flat['slot'];require(slot in groups and last_slot<=slot<=last_slot+1,'slot order changed');last_slot=slot
        item=copy.deepcopy(receipts[flat['receipt']])
        require(not any(k in item for k in META+['clocks']),'receipt may not override row/clock fields')
        item.update({k:flat[k] for k in META})
        clocks={}
        for key in CLOCK_KEYS:
            text=flat[key.replace('_ns','_relative_ns')]
            if flat['timed']:
                value=int(text);require(str(value)==text and value>0,'invalid relative clock');clocks[key]=value
            else:require(text=='','untimed row has clock')
        item['clocks']=clocks if flat['timed'] else None
        groups[slot].append(item)
    require(all(groups.values()),'all 20 slots required; no prefix replay')
    return groups

def calculate(groups,protocol,work):
    final_gate(protocol)
    processes=[]
    failures=[]
    for slot in schedule():
        rows=groups[slot['slot']]
        checked=validate_rows(rows,slot,work['gold'][slot['scene']],protocol['caller_fp'])
        require(not checked.failures,'row identity/work/order/FP/relative-clock validation failed: '+str(checked.failures[:2]))
        cpu=metric_effects(rows,slot,'cpu');wall=metric_effects(rows,slot,'wall')
        numerical=process_gates(cpu,slot)
        processes.append(dict(slot=slot,integrity_pass=True,cpu=cpu,wall=wall,
                              numerical_checks=numerical.checks,numerical_failures=numerical.failures))
        failures.extend({**v,'code':'slot.'+str(slot['slot'])+'.'+v['code']}for v in numerical.failures)
    cohort,cells,pooled=cohort_gates(processes);failures.extend(cohort.failures)
    return dict(schema=1,scope=SCOPE,admission=not failures,
        status='PASS frozen stock-only measurement control' if not failures else 'FAIL / no precision admission',
        processes=processes,cells=cells,pooled_sham_cycles=pooled,cohort_checks=cohort.checks,
        failures=failures,failed_threshold_checks=len(failures),
        note='All CPU numerical gates recomputed. Wall is complete descriptive evidence, never an admission metric. Full image/loader integrity is an original verified receipt, not re-executed here.')

def verify(root):
    manifest=load(root/'manifest.json')
    require(manifest['schema']=='paired-stock-clock-portable-v1','manifest schema')
    for name,entry in manifest['files'].items():
        require(Path(name).name==name,'nonlocal manifest path')
        content=(root/name).read_bytes()
        require(len(content)==entry['bytes'] and sha(content)==entry['sha256'],'manifest mismatch: '+name)
    protocol=load(root/'protocol.json');final_gate(protocol)
    work=zipped(root/'work-receipts.json.gz')
    groups=decode((root/'spans.csv').read_bytes(),work['receipts'])
    require(sha(canonical(groups))==protocol['normalized_records_sha256'],'normalized evidence changed')
    result=calculate(groups,protocol,work)
    require(result==zipped(root/'numerical-result.json.gz'),'exact saved effects/gates mismatch')
    return groups,protocol,work,result

def self_tests(groups,protocol,work):
    names=[]
    def rejected(name,action):
        try:action()
        except (ValueError,KeyError,IndexError,TypeError):names.append(name);return
        raise AssertionError('accepted mutant: '+name)
    p=copy.deepcopy(protocol);p['terminal']['completed_pairs']=19
    rejected('partial terminal',lambda:final_gate(p))
    p=copy.deepcopy(protocol);p['terminal']['status']='failed'
    rejected('noncomplete terminal',lambda:final_gate(p))
    p=copy.deepcopy(protocol);p['official_decision_verified']=False
    rejected('unverified official decision',lambda:final_gate(p))
    p=copy.deepcopy(protocol);p['scope']['arms']=['stock-A','candidate-C']
    rejected('candidate substitution',lambda:final_gate(p))
    p=copy.deepcopy(protocol);p['limits']['sham_abs_effect_pct']=99
    rejected('weakened threshold',lambda:final_gate(p))
    for name,change in [
        ('missing row',lambda r:r.pop()),
        ('row order',lambda r:r.reverse()),
        ('wrong work count',lambda r:r[12].__setitem__('count',1)),
        ('caller FP corruption',lambda r:r[0]['caller_after'].__setitem__('error',0)),
        ('wrong CPU',lambda r:r[0].__setitem__('cpu_after',1)),
        ('nonpositive span',lambda r:r[0]['clocks'].__setitem__('thread_end_ns',r[0]['clocks']['thread_begin_ns']))]:
        g=copy.deepcopy(groups);change(g[0]);rejected(name,lambda g=g:calculate(g,protocol,work))
    # Purely fabricated arithmetic. No measured observation is used as a fixture.
    fabricated=[]
    for slot in schedule():
        rows=expected_rows(slot)
        for row in rows:
            if row['timed']:
                duration=1_000_000*slot['chunk'] if row['kind']=='span' else 100
                if row['extra_enabled']:duration=duration*17//16
                row['clocks']={'thread_begin_ns':1,'thread_end_ns':1+duration,'wall_begin_ns':1,'wall_end_ns':1+duration}
        cpu=metric_effects(rows,slot,'cpu');gate=process_gates(cpu,slot)
        require(not gate.failures,'fabricated balanced controls should pass')
        fabricated.append(dict(slot=slot,cpu=cpu))
    require(not cohort_gates(fabricated)[0].failures,'fabricated complete cohort')
    names.append('fabricated zero-sham and exact 6.25% known-work response')
    poisoned=copy.deepcopy(fabricated);poisoned[0]['cpu']['ratio_product']={'numerator':'2','denominator':'1','root':1}
    require(process_gates(poisoned[0]['cpu'],poisoned[0]['slot']).failures,'fabricated wrong sham must fail')
    names.append('fabricated wrong-sham rejection')
    return dict(passed=True,offline_only=True,tests=names,count=len(names),DSP_calls=0,clock_reads=0)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--self-test',action='store_true');args=parser.parse_args()
    root=Path(__file__).resolve().parent;groups,protocol,work,result=verify(root)
    if args.self_test:
        tests=self_tests(groups,protocol,work);require(tests==load(root/'tests.json'),'test receipt differs');print(json.dumps(tests,indent=2))
    else:print(json.dumps(dict(verified=True,slots=20,rows=sum(map(len,groups.values())),failed_threshold_checks=result['failed_threshold_checks'],status=result['status'],scope=result['scope']),indent=2))
