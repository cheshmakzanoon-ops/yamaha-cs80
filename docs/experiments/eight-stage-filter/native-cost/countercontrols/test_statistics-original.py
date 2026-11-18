#!/usr/bin/env python3
"""Synthetic statistics/journal controls only. No clocks, process launch or DSP."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
from study_statistics import (analyze,validate_rows,bootstrap,quantile,summary,set_sensitivity,
    evaluate_gates,PLAN,METRICS,decide,schedule_audit,Rng)
from analyze_campaign import check_ledger,check_terminal,RECEIPT_KEYS,strict,ACTION


def fixtures(schedule):
    return [{**s,'cpu_ns':91_000_000 if s['treatment']=='C' else 106_250_000 if s['treatment']=='S+' else 100_000_000,
             'pid':1000+s['slot'],'module_sha256':('b' if s['module_role']=='candidate' else 'a')*64,'integrity_pass':True} for s in schedule['slots']]


def ledger_fixture(schedule):
    ph,fh,ah='a'*64,'b'*64,'c'*64
    terminal={'schema':1,'status':'complete','completed_slots':96,'error':None,'wall_begin_ns':1,
              'wall_end_ns':1000001,'cpu_elapsed_ns':200000,'no_resume':True,'analysis_performed':False}
    ledger=[{'event':'start','action':ACTION,'authorization_sha256':ah,'plan_sha256':ph,'freeze_sha256':fh,'wall_ns':1}]
    receipts=[];pins=[]
    for i,s in enumerate(schedule['slots']):
        r={k:None for k in RECEIPT_KEYS};r.update(schema=1,slot=s,pid=1000+i,integrity='PASS',exit=0,error=None,stderr='',
            wall_begin_ns=10+i*10000,wall_end_ns=9000+i*10000,resource_user_ns=1000,resource_system_ns=10)
        pin=f'{i:064x}';receipts.append(r);pins.append(pin)
        ledger.extend([{'event':'slot-start','slot':i,'wall_ns':r['wall_begin_ns']},
                       {'event':'slot-end','slot':i,'receipt_sha256':pin,'integrity':'PASS'}])
    ledger.append({'event':'terminal',**terminal})
    return ledger,terminal,receipts,pins,ph,fh,ah


def run(schedule):
    records=[]
    def test(name,fn):
        try:fn()
        except Exception as error:records.append({'test':name,'pass':False,'error':repr(error)})
        else:records.append({'test':name,'pass':True})
    def rejected(fn):
        try:fn()
        except (ValueError,KeyError,TypeError):return
        raise AssertionError('invalid synthetic input admitted')
    rows=fixtures(schedule);base=analyze(rows,schedule)
    test('complete96 synthetic positive all gates and6-set sensitivity',lambda:assertion(base['admission'] and base['calibration_pass'] and len(base['blocks'])==24 and len(base['set_sensitivity']['sets'])==6))
    test('known extra total +6.25 not normalized away',lambda:assertion(abs(base['main']['metrics']['extra']['point_percent']-6.25)<1e-10))
    test('candidate savings sign9%, not reciprocal speedup',lambda:assertion(abs(base['main']['metrics']['candidate_pooled']['point_percent']-9)<1e-10))
    test('schedule positions and ordered carryovers balance',lambda:assertion(schedule_audit(schedule)['each_half_balanced']))
    for name,mutate in [
        ('missing process',lambda r:r.pop()),('extra process',lambda r:r.append(copy.deepcopy(r[0]))),
        ('duplicate temporal slot',lambda r:r[1].update(slot=0)),('wrong label role',lambda r:r[0].update(treatment='C')),
        ('same pid reused',lambda r:r[1].update(pid=r[0]['pid'])),('zero CPU',lambda r:r[0].update(cpu_ns=0)),
        ('negative CPU',lambda r:r[0].update(cpu_ns=-1)),('fractional normalized CPU',lambda r:r[0].update(cpu_ns=1.5)),
        ('bool CPU',lambda r:r[0].update(cpu_ns=True)),('failed integrity',lambda r:r[0].update(integrity_pass=False)),
        ('physical stock twin swapped',lambda r:next(x for x in r if x['treatment']=='S1').update(module_sha256='c'*64)),
        ('candidate actually stock',lambda r:[x.update(module_sha256='a'*64) for x in r if x['treatment']=='C']),
        ('fake known extra count',lambda r:next(x for x in r if x['treatment']=='S+').update(measured_callbacks=512)),
        ('known extra divided by544',lambda r:next(x for x in r if x['treatment']=='S+').update(normalization_callbacks=544)),
        ('warm count changed',lambda r:r[0].update(warm_callbacks=0)),
        ('warm data substituted for measured',lambda r:r[0].update(measured_callbacks=1024)),
        ('process order swapped',lambda r:r.__setitem__(slice(0,2),[r[1],r[0]]))]:
        def fn(mutate=mutate):
            r=copy.deepcopy(rows);mutate(r);rejected(lambda:validate_rows(r,schedule))
        test(name,fn)
    def gate_mutation(name,mutate,expected=None):
        def fn():
            a,b,c=copy.deepcopy([base['main'],base['set_sensitivity'],base['subgroups']]);mutate(a,b,c)
            failures=[x for x in evaluate_gates(a,b,c) if not x['pass']]
            assertion(bool(failures) and (expected is None or any(expected in x['gate'] for x in failures)))
        test(name,fn)
    gate_mutation('placebo interval overlapping zero is not equivalence',lambda a,b,c:a['metrics']['placebo'].update(bootstrap_95_percent=[-2.01,1]),'placebo')
    gate_mutation('negative placebo sign boundary',lambda a,b,c:a['metrics']['placebo'].update(bootstrap_95_percent=[-2.001,-.5]),'placebo')
    gate_mutation('known extra CI not merely positive point',lambda a,b,c:a['metrics']['extra'].update(bootstrap_95_percent=[2.99,8]),'known-extra')
    gate_mutation('known extra upper bound fails',lambda a,b,c:b['metrics']['extra'].update(set_t95_percent=[5,10.01]),'known-extra')
    gate_mutation('primary lower exactly3 is not above3',lambda a,b,c:a['metrics']['candidate_pooled'].update(bootstrap_95_percent=[3,9]),'candidate_pooled')
    gate_mutation('individual stock twin contradiction',lambda a,b,c:b['metrics']['candidate_S1'].update(set_t95_percent=[2.9,9]),'candidate_S1')
    gate_mutation('six-set correlation sensitivity mandatory',lambda a,b,c:b['metrics']['candidate_pooled'].update(set_t95_percent=[2.9,12]),'six-set-t')
    gate_mutation('first half interval includes zero',lambda a,b,c:c['first12']['metrics']['candidate_pooled'].update(bootstrap_95_percent=[0,10]),'split.first12')
    gate_mutation('split drift exceeds3pp',lambda a,b,c:c['first12']['metrics']['candidate_pooled'].update(point_percent=13),'point-gap')
    gate_mutation('candidate position reversal',lambda a,b,c:c['C-position-0']['metrics']['candidate_pooled'].update(point_percent=-1),'C-position-0')
    gate_mutation('candidate order spread exceeds5pp',lambda a,b,c:c['C-position-0']['metrics']['candidate_pooled'].update(point_percent=15),'position-range')
    gate_mutation('placebo order cancellation cannot hide bias',lambda a,b,c:c['S1-before-S0']['metrics']['placebo'].update(point_percent=2.01),'S1-before')
    def inclusive():
        a,b,c=copy.deepcopy([base['main'],base['set_sensitivity'],base['subgroups']])
        for z,key in [(a,'bootstrap_95_percent'),(b,'set_t95_percent')]:
            z['metrics']['placebo'][key]=[-2,2];z['metrics']['extra'][key]=[3,10]
        assertion(all(g['pass'] for g in evaluate_gates(a,b,c)))
    test('inclusive placebo and positive boundaries',inclusive)
    test('declared rounding makes3% strict boundary stable',lambda:assertion(decide(3.0000000000000004)==3))
    def joint():
        vectors=[[x,2*x,3*x,4*x,5*x] for x in (-2,-1,0,1,2,3)]
        intervals=bootstrap(vectors,'C-position-0',draws=101)
        for k,ci in enumerate(intervals):
            assertion(all(abs(ci[j]-(k+1)*intervals[0][j])<1e-12 for j in range(2)))
        assertion(bootstrap(vectors,'C-position-0',draws=101)==intervals)
    test('joint resampling preserves contrasts and deterministic seed',joint)
    test('linear percentile interpolation convention',lambda:assertion(quantile([0,10,20,30],.25)==7.5))
    def correlation():
        blocks=[]
        for i in range(24):
            gain=.30 if i//4==5 else .05
            blocks.append({'block':i,'replicate':i//4,'contrasts':[0,math.log(1.0625),gain,gain,gain]})
        main=summary(blocks,'all');sets=set_sensitivity(blocks)
        assertion(main['metrics']['candidate_pooled']['bootstrap_95_percent'][0]>3)
        assertion(sets['metrics']['candidate_pooled']['set_t95_percent'][0]<3)
        assertion(len(sets['leave_one_complete_set'])==6)
    test('actual20000-draw block interval can pass while df5 set gate fails',correlation)
    fixture=ledger_fixture(schedule)
    test('complete96 synthetic journal',lambda:check_ledger(*fixture))
    mutations=[('missing finish',lambda l,t,r,p:l.pop(2)),('extra retry',lambda l,t,r,p:l.insert(3,copy.deepcopy(l[1]))),
        ('wrong attempt order',lambda l,t,r,p:r[1]['slot'].update(slot=0)),('reused pid',lambda l,t,r,p:r[1].update(pid=r[0]['pid'])),
        ('interrupted process',lambda l,t,r,p:r[0].update(error='interrupted',integrity='FAIL')),
        ('overlapping temporal process',lambda l,t,r,p:r[1].update(wall_begin_ns=1)),
        ('wrong finish digest',lambda l,t,r,p:l[2].update(receipt_sha256='f'*64)),
        ('child CPU cap breach',lambda l,t,r,p:r[0].update(resource_user_ns=4_000_000_001)),
        ('child wall cap breach',lambda l,t,r,p:r[0].update(wall_end_ns=7_000_000_000)),
        ('incomplete terminal',lambda l,t,r,p:t.update(completed_slots=95)),('interrupted terminal',lambda l,t,r,p:t.update(status='failed',error='signal')),
        ('analysis before completed collection',lambda l,t,r,p:t.update(analysis_performed=True)),
        ('whole CPU cap breach',lambda l,t,r,p:t.update(cpu_elapsed_ns=240_000_000_001)),
        ('whole wall cap breach',lambda l,t,r,p:t.update(wall_end_ns=360_000_000_002)),
        ('global CPU omitted children',lambda l,t,r,p:t.update(cpu_elapsed_ns=1))]
    for name,mutate in mutations:
        def fn(mutate=mutate):
            l,t,r,p,ph,fh,ah=copy.deepcopy(fixture);mutate(l,t,r,p);rejected(lambda:check_ledger(l,t,r,p,ph,fh,ah))
        test('journal '+name,fn)
    test('duplicate JSON key rejected',lambda:rejected(lambda:strict('{"a":1,"a":2}')))
    test('nonfinite JSON rejected',lambda:rejected(lambda:strict('{"a":NaN}')))
    return {'schema':1,'scope':'synthetic statistics and journal checks only; no timing data',
        'passed':all(r['pass'] for r in records),'tests':len(records),'records':records,
        'new_DSP_runs':0,'measurement_clock_reads':0,'native_loads':0,'bootstrap_draws_in_correlation_test':20000}


def assertion(value):
    if not value:raise AssertionError('synthetic assertion failed')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path);a=p.parse_args()
    root=Path(__file__).resolve().parents[1];result=run(json.loads((root/'schedule.json').read_text()))
    result['source_sha256']={x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in sorted(Path(__file__).parent.glob('*.py'))}
    if a.output:a.output.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(json.dumps({'passed':result['passed'],'tests':result['tests'],'failures':[r for r in result['records'] if not r['pass']]},indent=2))
    raise SystemExit(0 if result['passed'] else 1)
