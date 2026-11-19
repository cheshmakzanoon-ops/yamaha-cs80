#!/usr/bin/env python3
"""Compact a completed *Q-only* private raw run; no native execution. Does not modify the input."""
import argparse, csv, gzip, hashlib, json, statistics
from pathlib import Path
import sys
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import crossover as design

def need(ok,msg):
    if not ok:raise ValueError(msg)
def read(path):return json.loads(path.read_bytes())
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def compress(v):
    return gzip.compress(json.dumps(v,sort_keys=True,separators=(',',':'),allow_nan=False).encode(),mtime=0)
def build(private,out):
    r=private/'Q-run';gold=read(private/'seal.json');term=read(r/'terminal.json');d=read(r/'numerical-gates.json')
    log=[json.loads(x) for x in (r/'journal.jsonl').read_text().splitlines()]
    need(term['complete'] and term['candidate_runs']==0 and term['role']=='Q' and not term['candidate_stage_authorized']
         and len(log)==97 and term['completed_slots']==48,'not a completed stock-only stage')
    need(term['seal_sha256']==sha(private/'seal.json')==log[0]['seal'],'seal mismatch')
    need(gold['stock_identity'][3]==design.SHIPPING_CUTOFF,'wrong image')
    j={'schema':'native-cs80-cloud-Q-compact-v1','stage':'Q','stock_sha256':gold['stock_identity'][3],
       'stock_build_id':gold['stock_build_id'],'host_sha256':gold['host_identity'][3],'host_build_id':gold['host_build_id'],
       'sealed_sources':gold['sources'],'dry_receipt_sha256':gold['dry_receipt_sha256'],'stage_seal_sha256':term['seal_sha256'],
       'stage_budgets':gold['stage_budget'],'cpu_ids':gold['four_cpus'],'supervisor_cpu_id':gold['supervisor_cpu'],
       'terminal':term,'source_file_sha256':{'final_normalized':sha(r/'complete-normalized.json.gz'),
          'decision':sha(r/'numerical-gates.json'),'journal':sha(r/'journal.jsonl')},
       'original_decision':d,'slots':[],'cells':[]}
    origin=log[0]['t'];combined=json.loads(gzip.decompress((r/'complete-normalized.json.gz').read_bytes()))
    for n,v in enumerate(gold['schedule']['slots']):
        e,c=v['epoch'],v['cpu_cell'];a_lane=design.orientation(e,c)['a_lane'];w=r/f'slot-{n:02d}'
        assert log[2*n+1]['slot']==log[2*n+2]['slot']==n
        comp=sha(w/'normalized.json.gz');assert comp==log[2*n+2]['normalized_sha256']
        meta=read(w/'verified.json'); ext=meta['external']
        slot={'epoch':e,'cpu_cell':c,'pcpu':gold['four_cpus'][c], 'kind':'same-physical-stock',
              'normalized_sha256':comp,'start_ns':ext['start']-origin,'end_ns':ext['end']-origin,
              'worker_cpu_ns':ext['pooled_worker_cpu_ns'],'launch_first':ext['launch_first'],
              'public_bytes_compared_by_lane':meta['public_bytes_compared_by_lane']}
        j['slots'].append(slot)
        for seq,s in enumerate(v['scene_order']):
            plan=design.plan(e,c,s);q1=[i for i,q,_,_ in plan if q==1]
            re=[];ea=[];cpu={};gaps=[];walls=[];rawhash=hashlib.sha256(); ru=[0]*4;exp=combined['cells'][5*n+seq]
            need((exp['epoch'],exp['cpu_cell'],exp['scene'])==(e,c,s),'changed musical cell')
            for l in (0,1):
                cap=w/f'peer-{l}'/'capture'/s
                for name in ('clocks.csv','empty.csv','resources.csv'):
                    rawhash.update(sha(cap/name).encode()+b'\n')
                with (cap/'clocks.csv').open() as f:
                    t=[tuple(map(int,row[0:5])) for row in list(csv.reader(f))[1:]]
                re.append(t);need(len(t)==576 and all(i==k for k,(i,*rest) in enumerate(t)),'bad real clock count')
                with (cap/'empty.csv').open() as f:
                    empt=[tuple(map(int,row[0:5])) for row in list(csv.reader(f))[1:]]
                need(len(empt)==128,'bad empty count')
                need([x[2]-x[1] for x in empt]==exp['empty_cpu_ns'][l],'changed empty clocks')
                ea.extend(x[2]-x[1] for x in empt)
                with (cap/'resources.csv').open() as f:
                    resource=next(csv.DictReader(f))
                for k,name in enumerate(('voluntary_switches','involuntary_switches','minor_faults','major_faults')):
                    ru[k]+=int(resource[name])
                tag='A' if l==a_lane else 'B'
                for q in (1,4):
                    for extra in (0,1):
                        cpu[f'{tag}_{q}_{extra}']=sum(t[i][2]-t[i][1] for i,qq,_,x in plan if qq==q and x==extra)
            raw=[[i,l,*re[l][i][1:3],re[l][i][3]-origin,re[l][i][4]-origin] for i,l in design.visits(plan)]
            need(raw==exp['clocks'] and all(x[5]<=y[4] for x,y in zip(raw,raw[1:])),'raw/normalized or pair order mismatch')
            for i in q1:
                gaps.append(abs(re[0][i][3]-re[1][i][3]))
                walls.extend(re[l][i][4]-re[l][i][3] for l in (0,1))
            j['cells'].append({'epoch':e,'cpu_cell':c,'scene':s,'real_thread_ns':cpu,
                               'empty_ns_sum':sum(ea),'q1_gap_ns_twice_median':int(2*statistics.median(gaps)),
                               'q1_wall_ns_twice_pooled_median':int(2*statistics.median(walls)),
                               'resources_both_workers':ru,'six_original_csv_hashes_sha256':rawhash.hexdigest()})
    with out.open('xb') as f:f.write(compress(j))
    print(sha(out))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('sealed_run',type=Path);p.add_argument('new_output',type=Path)
    a=p.parse_args();build(a.sealed_run,a.new_output)
