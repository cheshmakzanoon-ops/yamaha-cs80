#!/usr/bin/env python3
"""Replay the complete Q decision from aggregate sufficient statistics; no timing, subprocess, or native binary."""
from pathlib import Path
import gzip, hashlib, json, math, re
SCENES=('idle','release_equal','release_varied','held_varied','wake_varied')
SRC=Path(__file__).with_name('sufficient.json.gz')
PIN='bc3693f582112c9bb21e86e112f9766248a6fc63dcddb7b1df7beef0182022bf'
def need(test,why):
    if not test:raise ValueError(why)
def avg(seq):return math.fsum(seq)/len(seq)
def pct(x):return 100*math.expm1(x)
def load():
    b=SRC.read_bytes();need(hashlib.sha256(b).hexdigest()==PIN,'recorded file digest mismatch')
    return json.loads(gzip.decompress(b))
def replay(data):
    need(data['schema']=='native-cs80-cloud-Q-compact-v1' and data['stage']=='Q','not the recorded Q stage')
    t=data['terminal'];b=data['stage_budgets'];slots=data['slots'];cells=data['cells'];pub=data['original_decision']
    need(t['complete'] and t['numerical_gates_pass'] and t['candidate_runs']==0 and not t['candidate_stage_authorized'],
         'unexpected terminal')
    need(data['stage_seal_sha256']==t['seal_sha256']=='48e3499addacc08e363f43c75e5888ceee98b52c76978f450191e7f4994dd8ae'
         and data['stock_sha256']=='0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2','wrong sealed comparator')
    need(len(cells)==t['scenes']==240 and len(slots)==t['completed_slots']==48 and t['retained_rows']==240*1152,'missing slot or scene')
    need(t['elapsed_ns']<=b['wall_seconds']*10**9 and t['pooled_worker_cpu_ns']<=b['pooled_worker_cpu_seconds']*10**9,'stage resource cap')
    sums={};err=[];gates={};oldend=0
    def eq(x,y):
        err.append(abs(x-y));need(math.isclose(x,y,abs_tol=1.1e-12,rel_tol=1e-10),'original numerical result disagrees')
    for i,slot in enumerate(slots):
        e=i//4;j=((0,1,3,2)[i%4]+e)%4;r=4*e+j
        need((slot['epoch'],slot['cpu_cell'],slot['launch_first'],slot['pcpu'])==
             (e,j,(e+j//2)%2,data['cpu_ids'][j]),'wrong planned slot, role order or affinity')
        need(slot['public_bytes_compared_by_lane']=={'0':24947518,'1':24947518} and slot['kind']=='same-physical-stock','missing identity/byte receipts')
        need(oldend<=slot['start_ns']<slot['end_ns']<=t['elapsed_ns'] and slot['end_ns']-slot['start_ns']<=b['slot_seconds']*10**9,'slot envelope')
        if i and i%4==0:need(slot['start_ns']-oldend>=b['post_epoch_seconds']*10**9,'missing post-epoch spacing')
        oldend=slot['end_ns']
        for k in range(5):
            scene=('idle','held_varied','release_equal','wake_varied','release_varied')[(r+k*(-1 if (r//5)%2 else 1))%5]
            row=cells[5*i+k];need((row['scene'],row['epoch'],row['cpu_cell'])==(scene,e,j),'missing or shuffled musical cells')
            cs=row['real_thread_ns']
            keys={f'{tag}_{q}_{x}' for tag in 'AB' for q in (1,4) for x in (0,1)}
            need(set(cs)==keys and all(type(x) is int and x>0 for x in cs.values()),'bad CPU sums')
            need(re.fullmatch('[a-f0-9]{64}',row['six_original_csv_hashes_sha256']) and
                 row['q1_gap_ns_twice_median']>0 and row['q1_wall_ns_twice_pooled_median']>0 and
                 row['empty_ns_sum']>=0,'bad raw-file or control receipts')
            def val(tag,qs=(1,4),xx=(0,1)):return sum(cs[f'{tag}_{q}_{x}'] for q in qs for x in xx)
            aa=val('A',xx=(0,));bb=val('B',xx=(0,))
            sums[scene,e,j]=[math.log(bb/aa),math.log(val('B')/aa),math.log(val('A')/aa),
                             math.log(val('B',(1,),(0,))/val('A',(1,),(0,))),
                             math.log(val('B',(4,),(0,))/val('A',(4,),(0,))),
                             (row['empty_ns_sum']/256)/(sum(cs.values())/1152),
                             row['q1_gap_ns_twice_median']/row['q1_wall_ns_twice_pooled_median']]
    for s in SCENES:
        m=[[sums[s,e,j] for j in range(4)] for e in range(12)];official=pub['scenes'][s]
        est=[]
        for contrast in range(5):
            ep=[avg([c[contrast] for c in row]) for row in m];mu=avg(ep)
            se=(math.fsum((v-mu)**2 for v in ep)/(11*12))**.5
            est.append((pct(mu),pct(mu-2.200985160082949*se),pct(mu+2.200985160082949*se),ep))
        twin,pos,aextra,q1,q4=est
        for a,k in ((twin,'twins'),(pos,'extra_work')):
            orig=official[k]
            for x,y in zip([a[0],a[1],a[2],*a[3]],[orig['point_pct'],*orig['ci95_pct'],*orig['epoch_logs']]):eq(x,y)
        halves=[pct(avg(twin[3][:6])),pct(avg(twin[3][6:]))]
        cores=[pct(avg([row[j][0] for row in m])) for j in range(4)]
        for x,y in zip([*halves,*cores,q1[0],q4[0],aextra[0]],[*official['half_points'],*official['cpu_points'],*official['q1_q4_twin_points'],official['within_A_extra_pct']]):eq(x,y)
        for label,v in [('empty-clock',all(c[5]<.02 for row in m for c in row)),('q1-handoff',all(c[6]<3 for row in m for c in row)),
                        ('twins-equivalence',twin[1]>-2.5 and twin[2]<2.5),('extra-work-interval',pos[1]>7.5 and pos[2]<17.5),
                        ('extra-work-point',10<pos[0]<15),('order-quantum',max(abs(q1[0]),abs(q4[0]))<3),
                        ('cpu-time-strata',max(map(abs,halves+cores))<4)]:gates[f'{s}:{label}']=v
    need(gates==pub['gates'] and len(gates)==35 and all(gates.values()),'one or more gates differ or fail')
    return {'Q_only':True,'verified_gates':len(gates),'numeric_values_checked':len(err),'largest_absolute_difference':max(err),
            'timed_candidate_runs':0}
if __name__=='__main__':print(json.dumps(replay(load()),indent=2))
