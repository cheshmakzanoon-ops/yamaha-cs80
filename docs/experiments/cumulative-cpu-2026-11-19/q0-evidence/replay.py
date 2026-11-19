#!/usr/bin/env python3
"""Standalone offline replay of the already finished, negative original-Q0 qualification. No plug-in or native runner."""
import gzip
import hashlib
import json
import math
import re
from pathlib import Path

FILE=Path(__file__).with_name('sufficient.json.gz')
PIN='955e65ab2e6a6696ddcbaf9cf60c5561f7dee3b5cc6c1a50e410cee0bbe70968'
STUDY='c1401da9002d94d853ec7612177fcafcd1660130e4627c5835c93d3d7ada899d'
SEAL='167fad0697b438996ae27dddabecd794fd3a3bdfcf7977e50e70a3c3b7cf5638'
A='8d4078be629d097e6470ec90fb12ccfdb7ad486663f14bb8e3aaef3076bd8725'
B='25281f677330f7e73a0fc695f43cdfc6eaf902ffb059218e0b3ac36188edae5a'
SCENES=('idle','release_equal','release_varied','held_varied','wake_varied')
F={'idle:empty-clock','release_varied:twins-equivalence','release_varied:extra-work-interval','release_varied:extra-work-point',
   'release_varied:order-quantum','release_varied:cpu-time-strata'}
NANO=10**9

def need(ok, why):
    if not ok:
        raise ValueError(why)
def digest(v): return isinstance(v,str) and re.fullmatch('[0-9a-f]{64}',v) is not None
def mean(v): return math.fsum(v)/len(v)
def pct(v): return 100*math.expm1(v)
def load():
    b=FILE.read_bytes();need(hashlib.sha256(b).hexdigest()==PIN,'compact digest mismatch');return json.loads(gzip.decompress(b))

def replay(data):
    t=data['terminal'];bud=data['budgets'];sl=data['slots'];cells=data['cells'];got=data['original_decision']
    need(data['schema']=='cs80-original-only-Q0-compact-v1' and data['scope']=='shared-guest-original-only' and data['study_sha256']==t['study_sha256']==STUDY
         and data['stage_seal_sha256']==t['seal_sha256']==SEAL and data['original']['elf_sha256']==A and data['never_timed_final']['elf_sha256']==B
         and data['original']['commit']=='db02cd8fa56892c6ea2cf7df084bcf190204968e' and data['never_timed_final']['commit']=='5ef3cfee8a7b0a25a632c2cdc6cb01b1bbfba8c4'
         and data['host_sha256']=='f4eec9685fc16fd592d3061e730724f5f1418e05ef2ddfaf284d852d5d27a70e','wrong experiment')
    need(t['role']==got['role']=='Q' and t['complete'] is True and t['numerical_gates_pass'] is False and got['numerical_only'] is True and got['numerical_gates_pass'] is False
         and t['candidate_runs']==0 and t['candidate_stage_authorized'] is False and len(cells)==t['scenes']==240 and len(sl)==t['completed_slots']==48
         and t['retained_rows']==276480,'unexpected terminal/coverage')
    need(bud==dict(wall_seconds=4500,pooled_worker_cpu_seconds=2400,retained_bytes=2**30,post_epoch_seconds=10,min_epoch_spacing_seconds=120,slot_seconds=100)
         and 0<t['elapsed_ns']<=4500*NANO and 0<t['pooled_worker_cpu_ns']<=2400*NANO and 0<t['retained_bytes_including_dry']<=2**30,'resource limits')
    ref=data['reference'];obj=data['original']['actual_file_dev_inode_size_tag_sha256'];keys={f'{k}_{q}_{x}' for k in 'AB' for q in (1,4) for x in (0,1)}
    need(digest(obj) and ref['dry_genuine_or_empty_clock_reads']==0 and ref['dry_workers']==3 and ref['visits_per_worker']==1840
         and ref['public_bytes_per_worker']==24947518 and len(data['digest_receipts'])==9 and all(map(digest,data['digest_receipts'].values()))
         and data['cpu_ids']==[0,2,4,6] and data['supervisor_cpu']==1,'identity/receipt or guest CPU assignments')
    eps=data['sealed_study_endpoints']
    need(eps['five_scenes']==list(SCENES) and eps['A_A_CI_open_pct']==[-2.5,2.5] and eps['extra_work_point_open_pct']==[10,15] and eps['extra_work_CI_open_pct']==[7.5,17.5]
         and eps['empty_fraction_below']==.02 and eps['q1_gap_below']==3 and eps['quantum_placebo_strict_abs_pct']==3 and eps['strata_half_placebo_strict_abs_pct']==4
         and eps['Q0_no_candidate_autoauthorization'] is True and eps['all_35_Q0_and_later_live_S_gates_required'] is True,'registered thresholds changed')
    rows={};err=[];gates={};report={};oldend=oldver=0
    for i,s in enumerate(sl):
        e=i//4;j=((0,1,3,2)[i%4]+e)%4
        need([s[k] for k in ('epoch','cpu_cell','cpu','a_lane','launch_first')]==[e,j,data['cpu_ids'][j],(e//2+j)%2,(e+j//2)%2]
             and s['actual_object_tags']==[obj,obj] and s['public_bytes_per_lane']==[24947518]*2,'slot order, loaded image, affinity or parity receipt')
        need(digest(s['normalized_sha256']) and digest(s['verified_sha256']) and len(s['identity_and_baton_sha256'])==2
             and all(len(x)==2 and all(map(digest,x)) for x in s['identity_and_baton_sha256']),'missing slot receipt')
        need(s['epoch_start_ns']>=0 and s['epoch_start_ns']<=s['attempt_ns'] and oldver<=s['attempt_ns']<=s['start_ns']<s['end_ns']<=s['verified_ns']<=t['elapsed_ns']
             and s['end_ns']-s['start_ns']<=100*NANO and s['worker_cpu_ns']>0,'slot or ledger resource envelope')
        if i and i%4==0:
            need(s['epoch_start_ns']>=sl[i-4]['epoch_start_ns']+120*NANO and s['epoch_start_ns']>=oldend+10*NANO,'epoch spacing')
        if i%4:need(s['epoch_start_ns']==sl[i-1]['epoch_start_ns'],'epoch counter mismatch')
        oldend=s['end_ns'];oldver=s['verified_ns'];slot_ns=0
        for n in range(5):
            r=4*e+j;scene=('idle','held_varied','release_equal','wake_varied','release_varied')[(r+n*(-1 if (r//5)%2 else 1))%5]
            row=cells[5*i+n];need((row['scene'],row['epoch'],row['cpu_cell'])==(scene,e,j),'missing or shuffled scene')
            z=row['thread_ns'];need(set(z)==keys and all(type(v) is int and v>0 for v in z.values()),'invalid integer real CPU sums')
            en=row['empty_ns_sum'];gap=row['q1_gap_ns_twice_median'];wall=row['q1_wall_ns_twice_pooled_median']
            need(type(en) is int and en>=0 and type(gap) is int and gap>0 and type(wall) is int and wall>0 and digest(row['six_original_csv_hashes_sha256']),'bad raw/diagnostic receipt')
            def get(k,qs=(1,4),xs=(0,1)):return sum(z[f'{k}_{q}_{x}'] for q in qs for x in xs)
            den=get('A',xs=(0,));slot_ns+=sum(z.values())
            rows[scene,e,j]=(math.log(get('B',xs=(0,))/den),math.log(get('B')/den),math.log(get('A')/den),
                math.log(get('B',(1,),(0,))/get('A',(1,),(0,))),math.log(get('B',(4,),(0,))/get('A',(4,),(0,))),
                (en/256)/(sum(z.values())/1152),gap/wall)
        need(slot_ns<=s['worker_cpu_ns']+2_000_000,'callback CPU above slot envelope')
    need(sum(s['worker_cpu_ns'] for s in sl)+t['charged_dry_cpu_ns']==t['pooled_worker_cpu_ns'],'pooled + dry receipt')
    def eq(x,y):
        err.append(abs(x-y));need(math.isclose(x,y,abs_tol=1.1e-12,rel_tol=1e-10),'collector/independent number disagreement')
    for scene in SCENES:
        m=[[rows[scene,e,j] for j in range(4)] for e in range(12)];ver=got['scenes'][scene];res=[]
        for k in range(5):
            epoch=[mean([u[k] for u in v]) for v in m];mu=mean(epoch)
            half=2.200985160082949*math.sqrt(math.fsum((v-mu)**2 for v in epoch)/(11*12))
            res.append((pct(mu),pct(mu-half),pct(mu+half),epoch))
        twin,pos,aw,q1,q4=res
        for v,k in ((twin,'twins'),(pos,'extra_work')):
            old=ver[k];need(len(old['ci95_pct'])==2 and len(old['epoch_logs'])==12,'missing original intervals')
            for x,y in zip([*v[:3],*v[3]],[old['point_pct'],*old['ci95_pct'],*old['epoch_logs']]):eq(x,y)
        halves=[pct(mean(twin[3][:6])),pct(mean(twin[3][6:]))];cores=[pct(mean([v[j][0] for v in m])) for j in range(4)]
        need([len(ver[r]) for r in ('half_points','cpu_points','q1_q4_twin_points')]==[2,4,2],'missing subdivisions')
        for x,y in zip([*halves,*cores,q1[0],q4[0],aw[0]],[*ver['half_points'],*ver['cpu_points'],*ver['q1_q4_twin_points'],ver['within_A_extra_pct']]):eq(x,y)
        def worst(k):
            value,e,j=max((m[e][j][k],e,j) for e in range(12) for j in range(4))
            return {'value':value,'slot':next(i for i,x in enumerate(sl) if (x['epoch'],x['cpu_cell'])==(e,j)),'epoch':e,'cpu':data['cpu_ids'][j]}
        empty,handoff=worst(5),worst(6)
        tests={'twins-equivalence':twin[1]>-2.5+1e-10 and twin[2]<2.5-1e-10,'extra-work-interval':pos[1]>7.5 and pos[2]<17.5,
               'extra-work-point':10<pos[0]<15,'order-quantum':max(abs(q1[0]),abs(q4[0]))<3,'cpu-time-strata':max(map(abs,cores+halves))<4,
               'empty-clock':empty['value']<.02,'q1-handoff':handoff['value']<3}
        gates.update({f'{scene}:{k}':v for k,v in tests.items()})
        report[scene]={'common_twin_pct_point_lo_hi':twin[:3],'extra_work_pct_point_lo_hi':pos[:3],'within_A_extra_pct':aw[0],
                       'q1_q4_twin_pct':[q1[0],q4[0]],'cpu_0_2_4_6_pct':cores,'halves_pct':halves,'worst_empty_fraction':empty,'worst_handoff_multiple':handoff}
    failed=sorted(k for k,v in gates.items() if not v)
    need(len(gates)==35 and all(type(x) is bool for x in got['gates'].values()) and gates==got['gates'] and set(failed)==F and len(err)==195,'fixed negative decision disagrees')
    return {'qualification':False,'disposition':'STOP: no candidate run or cumulative timing claim','completed_original_pairs':len(sl),'genuine_callback_rows':240*1152,
        'empty_rows':240*256,'numeric_values_checked':len(err),'largest_absolute_difference':max(err),'gates_passed':sum(gates.values()),'gates':len(gates),
        'failed':failed,'scenes':report,'candidate_runs':0,'candidate_authorized':False}

if __name__=='__main__':print(json.dumps(replay(load()),indent=2,allow_nan=False))
