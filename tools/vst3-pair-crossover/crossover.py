#!/usr/bin/env python3
"""Generate prospective per-lane ordering or evaluate a complete *numerical* gate. Never starts a timer."""
import argparse
from collections import defaultdict
import csv
import gzip
import io
import json
import math
from pathlib import Path
import statistics as st

SCENES=('idle','release_equal','release_varied','held_varied','wake_varied')
PER=576
TC=2.200985160082949
SHIPPING_CUTOFF='0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2'

def need(x,message):
    if not x:
        raise ValueError(message)

def orientation(epoch,cell):
    return {'a_lane':(epoch//2+cell)%2, 'launch_first':(epoch+cell//2)%2,
            'sentinel_first':(epoch//2+cell//2)%2==0}

def cpu_order(epoch):
    # Four-treatment Williams square, repeated in three epochs of four.
    return [(v+epoch)%4 for v in (0,1,3,2)]

def scene_order(epoch,cell):
    base=('idle','held_varied','release_equal','wake_varied','release_varied')
    k=4*epoch+cell; d=-1 if (k//5)%2 else 1
    return [base[(k+i*d)%5] for i in range(5)]

def plan(epoch,cell,scene):
    need(type(epoch) is int and 0<=epoch<12 and type(cell) is int and 0<=cell<4 and scene in SCENES,'invalid cell/scene')
    k=SCENES.index(scene);phase=(epoch+cell+k)%2;rows=[]
    for b in range(16):
        q=4 if (b+phase)%2 else 1
        first_seed=(b//2+epoch+cell//2+k)%2
        for j in range(36):
            i=36*b+j
            rows.append((i,q,(first_seed+j//q)%2,int(i%9==(i//9+3*epoch+cell+k)%9)))
    return rows

def visits(pl):
    i=0
    while i<len(pl):
        _,q,l,_=pl[i]
        for lane in (l,1-l):
            for j in range(i,i+q):
                yield j,lane
        i+=q

def cpu_ids(allowed):
    ids=sorted(set(allowed));need(len(ids)>=5,'require four test vCPUs and a spare supervisor vCPU')
    result=[ids[(len(ids)*k)//4] for k in range(4)];need(len(set(result))==4,'test CPUs must differ')
    return result,next(i for i in ids if i not in result)

def canonical(epoch,cell,scene):
    out=io.StringIO();w=csv.writer(out,lineterminator='\n')
    w.writerow(('index','quantum','first_lane','extra'));w.writerows(plan(epoch,cell,scene))
    return out.getvalue()

def reduced(record):
    """Raw cell schema; two workers' CLOCK_MONOTONIC timestamps share the same origin."""
    ep,j,s=record['epoch'],record['cpu_cell'],record['scene'];p=plan(ep,j,s)
    a=orientation(ep,j)['a_lane'];d={}
    for row in record['clocks']:
        need(len(row)==6 and all(type(x) is int for x in row),'raw compatible integer clock row required')
        i,l,c0,c1,w0,w1=row
        need(0<=i<PER and l in (0,1) and (i,l) not in d and 0<=c0<c1 and 0<=w0<w1,'bad clock endpoints/key')
        d[i,l]=c0,c1,w0,w1
    need(len(d)==2*PER,'missing native callbacks')
    last=[None,None];lastwall=None
    for i,l in visits(p):
        c0,c1,w0,w1=d[i,l]
        need((last[l] is None or last[l]<=c0) and (lastwall is None or lastwall<=w0),
             'overlap, backwards clock, or wrong baton handoff order')
        last[l]=c1;lastwall=w1
    t=defaultdict(int);gaps=[];lengths=[];empty=record['empty_cpu_ns']
    need(len(empty)==2 and all(len(r)==128 and all(type(x) is int and x>=0 for x in r) for r in empty),
         'each lane requires 128 empty brackets after the window')
    for i,q,owner,extra in p:
        for l in (0,1):
            c0,c1,w0,w1=d[i,l];v=c1-c0;tag='a' if l==a else 'b'
            t[tag]+=v;t[f'{tag}_q{q}']+=v
            t[f'{tag}_extra' if extra else f'{tag}_common']+=v
            if not extra: t[f'{tag}_common_q{q}']+=v
            if q==1: lengths.append(w1-w0)
        if q==1:gaps.append(abs(d[i,0][2]-d[i,1][2]))
    result={'epoch':ep,'cpu_cell':j,'scene':s,'role':record['role'],
            'a_sha256':record['a_sha256'],'b_sha256':record['b_sha256'],
            'empty_fraction':st.mean(v for r in empty for v in r)/(sum(t[k] for k in ('a','b'))/(2*PER)),
            'q1_gap_multiple':st.median(gaps)/st.median(lengths),**t}
    need(t['a_common']+t['a_extra']==t['a'] and t['b_common']+t['b_extra']==t['b'],'work conservation')
    return result

def meanlog(values):
    return st.mean(values)

def stats(byepoch,saving=False):
    need(len(byepoch)==12,'t intervals require 12 independent time-cluster inputs')
    center=meanlog(byepoch); hw=TC*st.stdev(byepoch)/math.sqrt(len(byepoch))
    f=(lambda x:-100*math.expm1(x)) if saving else (lambda x:100*math.expm1(x))
    lo,hi=sorted((f(center-hw),f(center+hw)))
    return {'point_pct':f(center),'ci95_pct':[lo,hi],'epoch_logs':byepoch}

def assess(cells,role,a_sha,b_sha):
    need(role in ('Q','S','C') and a_sha==SHIPPING_CUTOFF and ((a_sha==b_sha)==(role!='C')), 'wrong shipping baseline or stage roles')
    need(len(cells)==12*4*5,'require 48 complete slots, every scene')
    expected=[(e,j,s) for e in range(12) for j in cpu_order(e) for s in scene_order(e,j)]
    need([(r['epoch'],r['cpu_cell'],r['scene']) for r in cells]==expected,'incomplete or reordered acquisition')
    by={(v['epoch'],v['cpu_cell'],v['scene']):v for v in cells}
    need(len(by)==len(cells) and all(v['role']==role for v in by.values()),'duplicate or wrong-role cell')
    need(all(v.get('a_sha256')==a_sha and v.get('b_sha256')==b_sha for v in by.values()),'wrong module roles')
    checks={};scenes={}
    for s in SCENES:
        rr=[by[e,c,s] for e in range(12) for c in range(4)]
        checks[s+':empty-clock']=all(r['empty_fraction']<.02 for r in rr)
        checks[s+':q1-handoff']=all(r['q1_gap_multiple']<3 for r in rr)
        def series(num,den):
            return [math.log(r[num]/r[den]) for r in rr]
        def ep(series):return [st.mean(series[i:i+4]) for i in range(0,48,4)]
        def splits(series,save=False):
            f=(lambda x:-100*math.expm1(st.mean(x))) if save else (lambda x:100*math.expm1(st.mean(x)))
            return [f(series[:24]),f(series[24:])],[f(series[c::4]) for c in range(4)]
        if role in ('Q','S'):
            common=series('b_common','a_common');allpos=series('b','a_common')
            aa=stats(ep(common));pos=stats(ep(allpos));half,core=splits(common)
            quanta=[100*math.expm1(st.mean(series('b_common_q'+q,'a_common_q'+q))) for q in ('1','4')]
            # Each held/quiet/wake scene has its own mandatory positive-control and placebo.
            checks[s+':twins-equivalence']=aa['ci95_pct'][0]>-2.5 and aa['ci95_pct'][1]<2.5
            checks[s+':extra-work-interval']=pos['ci95_pct'][0]>7.5 and pos['ci95_pct'][1]<17.5
            checks[s+':extra-work-point']=10<pos['point_pct']<15
            checks[s+':order-quantum']=all(abs(v)<3 for v in quanta)
            checks[s+':cpu-time-strata']=all(abs(v)<4 for v in half+core)
            scenes[s]={'twins':aa,'extra_work':pos,'within_A_extra_pct':100*math.expm1(st.mean(series('a','a_common'))),
                       'q1_q4_twin_points':quanta,'half_points':half,'cpu_points':core}
        else:
            common=series('b','a');co=stats(ep(common),True);half,core=splits(common,True)
            quanta=[-100*math.expm1(st.mean(series('b_q'+q,'a_q'+q))) for q in ('1','4')]
            primary=s in ('idle','release_equal')
            # A rounded nominal 5.000000000000004% is NOT strictly more than 5%.
            checks[s+':primary' if primary else s+':nonregression']=co['ci95_pct'][0]>(5 if primary else -3)+1.e-10
            if primary:
                checks[s+':directions']=min(half+core+quanta)>0
                checks[s+':quantum-agreement']=abs(quanta[0]-quanta[1])<4
            sec=PER*64/48000
            cpu_per_audio={k:math.exp(st.mean(math.log(r[k]*1e-9/sec) for r in rr)) for k in ('a','b')}
            scenes[s]={'saving':co,'q1_q4_saving_points':quanta,'half_points':half,'cpu_points':core,
                       'geometric_cell_callback_thread_seconds_per_rendered_second':cpu_per_audio}
    return {'role':role,'numerical_gates_pass':all(checks.values()),'gates':checks,'scenes':scenes}

def decide(data):
    """Numerical/row replay. External loader, quiet/state/parity and launch attestation remain required."""
    role=data['role'];need(role in ('Q','S','C'),'invalid role')
    a,b=data['a_sha256'],data['b_sha256']
    import re
    need(all(re.fullmatch('[0-9a-f]{64}',h) for h in (a,b)) and ((a==b)==(role!='C')),'baseline/treatment aliases invalid')
    material=[reduced(r) for r in data['cells']]
    result=assess(material,role,a,b)
    result['numerical_only']=True
    if role=='C':
        result['both_primary_lower_bounds_above_10']=all(result['scenes'][k]['saving']['ci95_pct'][0]>10+1.e-10 for k in ('idle','release_equal'))
        result['varied_release_lower_bound_above_5']=result['scenes']['release_varied']['saving']['ci95_pct'][0]>5+1.e-10
        result['release_admitted_by_this_file_alone']=False
    return result

def generate():
    rows=[{'epoch':e,'cpu_cell':c,'visit':n,'scene_order':scene_order(e,c),**orientation(e,c)}
          for e in range(12) for n,c in enumerate(cpu_order(e))]
    return {'schema':'cs80-cloud-crossover-plan-v1','epochs':12,'cpu_cells':4,'scenes':list(SCENES),
            'calls_per_scene_lane':PER,'slots':rows}

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    sp=ap.add_subparsers(dest='op',required=True)
    s=sp.add_parser('lane-plan');s.add_argument('--epoch',type=int,required=True);s.add_argument('--cpu-cell',type=int,required=True)
    s.add_argument('--scene',choices=SCENES,required=True);s.add_argument('--output',type=Path,required=True)
    sp.add_parser('schedule')
    a=sp.add_parser('analyze');a.add_argument('file',type=Path)
    args=ap.parse_args()
    try:
        if args.op=='lane-plan':
            with args.output.open('x') as f:f.write(canonical(args.epoch,args.cpu_cell,args.scene))
        elif args.op=='schedule':print(json.dumps(generate(),indent=2))
        else:
            op=gzip.open if args.file.suffix=='.gz' else open
            with op(args.file,'rt') as f:print(json.dumps(decide(json.load(f)),indent=2))
    except (ValueError,IndexError,KeyError,TypeError,OSError) as e:
        ap.exit(1,'invalid input: '+str(e)+'\n')
