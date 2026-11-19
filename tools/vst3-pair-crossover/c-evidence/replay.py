#!/usr/bin/env python3
"""Common-work A/A and actual-work C analyses from integer sufficient statistics. Independent of the prospective estimator; offline only."""
import gzip, hashlib, json, math, re
from pathlib import Path

PIN='bd7c5196100378af7df3f3cfb6fd45973b93bb6604170d8e579ed5199ca743eb'
STOCK='0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2'
FIR='74f2507db4a7042a23a96b9a8c2249cc2f76d1f7f417ebb4838498d7f0578895'
CSEAL='418a8a72ac1c6af1a590c98f04006135d42bd1451b04f4003059839b1d09970a'
QSEAL='48e3499addacc08e363f43c75e5888ceee98b52c76978f450191e7f4994dd8ae'
SCENES=('idle','release_equal','release_varied','held_varied','wake_varied')
def need(ok,msg):
    if not ok:raise ValueError(msg)
def load():
    v=Path(__file__).with_name('sufficient.json.gz').read_bytes();need(hashlib.sha256(v).hexdigest()==PIN,'digest mismatch')
    return json.loads(gzip.decompress(v))
def mean(z):
    v=list(z);return math.fsum(v)/len(v)
def pc(z,save=False):return (-100 if save else 100)*math.expm1(z)
def combined(s,c,both,broad):
    return {'live_sentinel_gates_pass':s,'candidate_gates_pass':c,'large_idle_win_gates_pass':s and c,
            'both_primary_above_10pct_with_all_gates':s and c and both,'broad_idle_win_with_all_gates':s and c and broad}
def replay(d):
    term=d['terminal'];bud=d['stage_budgets'];receipts=d['admission']
    need(d['schema']=='native-CS80-cloud-C-compact-v1' and term['complete'] is True and term['role']=='C-with-live-S'
         and d['stage_seal_sha256']==term['seal_sha256']==CSEAL and d['Q']['seal_sha256']==term['Q_seal_sha256']==QSEAL
         and d['Q']['gates_passed']==d['Q']['gate_count']==35 and (d['stock_sha256'],d['fir_sha256'])==(STOCK,FIR),'stage or image identity')
    need(term['completed_slots']==48 and term['completed_pairs']=={'S':48,'C':48} and len(d['pairs'])==96 and term['native_dry_clock_reads']==0 and
         term['original_genuine_clock_rows']==96*5*2*576 and term['original_empty_brackets']==96*5*2*128 and receipts['earlier_rehearsals']==1,'incomplete campaign')
    need((bud['wall_seconds'],bud['pooled_worker_CPU_seconds'],bud['slot_seconds_per_pair'],bud['retained_bytes'],bud['epoch_start_spacing_seconds'],bud['post_epoch_seconds'])==
         (9000,4800,100,2**30,120,10),'stopping limits changed')
    need(term['elapsed_ns_including_dry_and_waits']<=9000*10**9 and term['pooled_worker_cpu_ns_including_dry']<=4800*10**9 and
         term['retained_bytes_including_dry']<=2**30 and d['cpu_ids']==[0,2,4,6] and d['supervisor_cpu']==1,'resource cap or affinity')
    prove={};compared=[];keyset={f'{x}_{q}_{y}' for x in 'AB' for q in (1,4) for y in (0,1)};last_end=prev_ep=0;total=0
    for i,p in enumerate(d['pairs']):
        e=i//8;c=((0,1,3,2)[(i//2)%4]+e)%4;k=('SC' if (e//2+c//2)%2==0 else 'CS')[i%2];al=(e//2+c)%2
        need((p['epoch'],p['cpu_cell'],p['pair'],p['a_lane'],p['first_launch'],p['cpu'])==(e,c,k,al,(e+c//2)%2,d['cpu_ids'][c]) and
             p['indexes']==[int(k=='C' and l!=al) for l in (0,1)] and p['bytes_per_worker']==24947518,'wrong physical lanes/roles')
        need(last_end<=p['start_ns']<p['end_ns'] and p['end_ns']-p['start_ns']<=100*10**9 and re.fullmatch('[0-9a-f]{64}',p['normalized_sha256']),'pair envelope')
        if i%8==0:
            if i:need(p['epoch_start_ns']>=prev_ep+120*10**9 and p['epoch_start_ns']>=last_end+10*10**9,'epoch spacing')
            prev_ep=p['epoch_start_ns']
        need(p['epoch_start_ns']==prev_ep<=p['start_ns'],'epoch assignment');last_end=p['end_ns'];total+=p['pooled_worker_cpu_ns']
    need(last_end+d['before_timing_ns']<=term['elapsed_ns_including_dry_and_waits'] and total==term['sum_of_measured_pair_external_worker_cpu_ns'] and
         total+receipts['dry_cpu_ns']<=term['pooled_worker_cpu_ns_including_dry']+1000000,'total and dry resource accounting')
    def eq(a,b):
        compared.append(abs(a-b));need(math.isclose(a,b,rel_tol=1.e-10,abs_tol=1.1e-12),'decision numerical disagreement')
    for kind in 'SC':
        ctrl=kind=='S';data=d['cells'][kind];old=d['original_decisions'][kind];need(len(data)==240 and old['role']==kind,'incomplete role')
        v={};gates={};measures={}
        for i,row in enumerate(data):
            e=i//20;c=((0,1,3,2)[(i//5)%4]+e)%4;g=4*e+c
            scene=('idle','held_varied','release_equal','wake_varied','release_varied')[(g+(i%5)*(-1 if (g//5)%2 else 1))%5]
            z=row['real_thread_ns'];need((row['epoch'],row['cpu_cell'],row['scene'])==(e,c,scene) and set(z)==keyset and
                 all(type(x) is int and x>0 for x in z.values()) and row['empty_ns_sum']>=0 and row['q1_wall_ns_twice_pooled_median']>0 and
                 row['q1_gap_ns_twice_median']>0 and re.fullmatch('[0-9a-f]{64}',row['six_original_csv_hashes_sha256']),'bad or shuffled scene statistics')
            def val(label,bursts=(1,4),xp=(0,1)):return sum(z[f'{label}_{q}_{x}'] for q in bursts for x in xp)
            def ratio(b,a):return math.log(b/a)
            def selected(label,bursts=(1,4)):return val(label,bursts,(0,)) if ctrl else val(label,bursts)
            aa,bb=selected('A'),selected('B')
            v[(scene,e,c)]=[ratio(bb,aa),ratio(val('B'),aa),ratio(val('A'),aa),
                ratio(selected('B',(1,)),selected('A',(1,))),ratio(selected('B',(4,)),selected('A',(4,))),
                math.log(val('A')*1e-9/(576*64/48000)),math.log(val('B')*1e-9/(576*64/48000)),
                (row['empty_ns_sum']/256)/(sum(z.values())/1152),row['q1_gap_ns_twice_median']/row['q1_wall_ns_twice_pooled_median']]
        for s in SCENES:
            m=[[v[s,e,c] for c in range(4)] for e in range(12)];rec=old['scenes'][s]
            def summary(idx,sav):
                eps=[mean(c[idx] for c in row) for row in m];mu=mean(eps);hw=2.200985160082949*math.sqrt(math.fsum((j-mu)**2 for j in eps)/11/12)
                lo,hi=sorted([pc(mu-hw,sav),pc(mu+hw,sav)]);return pc(mu,sav),lo,hi,eps
            def check_series(calc,pub):
                for x,y in zip([calc[0],calc[1],calc[2],*calc[3]],[pub['point_pct'],*pub['ci95_pct'],*pub['epoch_logs']],strict=True):eq(x,y)
            main=summary(0,not ctrl);check_series(main,rec['twins' if ctrl else 'saving'])
            halves=[pc(mean(main[3][:6]),not ctrl),pc(mean(main[3][6:]),not ctrl)]
            cores=[pc(mean(m[e][c][0] for e in range(12)),not ctrl) for c in range(4)]
            qs=[summary(i,not ctrl)[0] for i in (3,4)]
            for x,y in zip(halves+cores+qs,rec['half_points']+rec['cpu_points']+rec['q1_q4_twin_points' if ctrl else 'q1_q4_saving_points'],strict=True):eq(x,y)
            max_empty=max(z[7] for row in m for z in row);max_gap=max(z[8] for row in m for z in row)
            gates[s+':empty-clock']=max_empty<.02;gates[s+':q1-handoff']=max_gap<3
            measures[s]={'effect_pct':main[0],'ci95_pct':[main[1],main[2]],'max_empty_pct':100*max_empty,'max_q1_gap_multiple':max_gap}
            if ctrl:
                extra=summary(1,False);check_series(extra,rec['extra_work']);eq(summary(2,False)[0],rec['within_A_extra_pct'])
                gates[s+':twins-equivalence']=main[1]>-2.5 and main[2]<2.5
                gates[s+':extra-work-interval']=extra[1]>7.5 and extra[2]<17.5;gates[s+':extra-work-point']=10<extra[0]<15
                gates[s+':order-quantum']=max(map(abs,qs))<3;gates[s+':cpu-time-strata']=max(map(abs,halves+cores))<4
                measures[s]['extra_work_point_pct']=extra[0];measures[s]['extra_work_ci95_pct']=[extra[1],extra[2]]
            else:
                primary=s in ('idle','release_equal')
                gates[s+(':primary' if primary else ':nonregression')]=main[1]>(5 if primary else -3)+1.e-10
                if primary:gates[s+':directions']=min(halves+cores+qs)>0;gates[s+':quantum-agreement']=abs(qs[0]-qs[1])<4
                ab=[math.exp(mean(z[i] for row in m for z in row)) for i in (5,6)]
                for i,lab in enumerate('ab'):eq(ab[i],rec['geometric_cell_callback_thread_seconds_per_rendered_second'][lab])
                measures[s]['absolute_cpu_per_audio_s']=ab
        need(gates==old['gates'] and all(gates.values())==old['numerical_gates_pass'] and len(gates)==(35 if ctrl else 19),'original pass/fail flags disagree')
        prove[kind]={'passing':sum(gates.values()),'gates':len(gates),'failures':[k for k in sorted(gates) if not gates[k]],'scenes':measures}
    both=all(prove['C']['scenes'][k]['ci95_pct'][0]>10+1e-10 for k in ('idle','release_equal'))
    broad=prove['C']['scenes']['release_varied']['ci95_pct'][0]>5+1e-10
    need((both,broad)==(d['original_decisions']['C']['both_primary_lower_bounds_above_10'],d['original_decisions']['C']['varied_release_lower_bound_above_5']),'candidate-only flags')
    verdict=combined(not prove['S']['failures'],not prove['C']['failures'],both,broad)
    need(all(term[k] is v for k,v in verdict.items()),'terminal combined gate disagreement')
    return {'outcome':verdict,'decision':prove,'compared_numbers':len(compared),'max_absolute_numerical_difference':max(compared)}
if __name__=='__main__':print(json.dumps(replay(load()),indent=2))
