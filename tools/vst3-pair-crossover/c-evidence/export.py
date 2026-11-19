#!/usr/bin/env python3
"""Verify a completed C archive from original native rows and export a portable, compact proof. No timers or subprocesses."""
import argparse, csv, gzip, hashlib, json, statistics
from pathlib import Path

SCENES=('idle','release_equal','release_varied','held_varied','wake_varied')
STOCK='0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2'
FIR='74f2507db4a7042a23a96b9a8c2249cc2f76d1f7f417ebb4838498d7f0578895'
SEAL='418a8a72ac1c6af1a590c98f04006135d42bd1451b04f4003059839b1d09970a'
QSEAL='48e3499addacc08e363f43c75e5888ceee98b52c76978f450191e7f4994dd8ae'
def need(ok,msg):
    if not ok:raise ValueError(msg)
def read(p):return json.loads(p.read_bytes())
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def gzread(p):return json.loads(gzip.decompress(p.read_bytes()))
def gzdata(v):return gzip.compress(json.dumps(v,sort_keys=True,separators=(',',':'),allow_nan=False).encode(),mtime=0)
def timeline(e,c,s):
    k=SCENES.index(s);rows=[]
    for b in range(16):
        q=(1,4)[(b+e+c+k)%2]
        for j in range(36):
            i=36*b+j
            rows.append((i,q,(b//2+e+c//2+k+j//q)%2,int(i%9==(i//9+3*e+c+k)%9)))
    return rows
def load_csv(p):
    with p.open(newline='') as f:return list(csv.reader(f))
def snapshot(cap,scene,num,fn):
    a=load_csv(cap/scene/fn);need(a[0]==['index','cpu0','cpu1','wall0','wall1'] and len(a)==1+num,'incomplete clock array')
    v=[tuple(map(int,z)) for z in a[1:]]
    need(all(len(z)==5 and z[0]==i and z[2]>=z[1]>=0 and z[4]>z[3] for i,z in enumerate(v)) and
         all(x[2]<=y[1] and x[4]<=y[3] for x,y in zip(v,v[1:])),'invalid clock endpoints')
    return v
def one_scene(w,e,c,s,row,origin,start,end,last,tag):
    al=(e//2+c)%2; raw=[None,None];empty=[];res=[0]*4;fp=hashlib.sha256();col={};nextcpu=[];nextwall=0
    plan=timeline(e,c,s);need(sum(x[3] for x in plan)==64,'fixed extras')
    need([tuple(map(int,y)) for y in load_csv(w.parent/'plan'/(s+'.csv'))[1:]]==plan,'changed callback quantum/extra timeline')
    for l in range(2):
        cap=w/f'peer-{l}/capture'
        for n in ('clocks.csv','empty.csv','resources.csv'):fp.update(sha(cap/s/n).encode()+b'\n')
        tables=load_csv(cap/s/'resources.csv');need(len(tables)==2,'missing thread-scene resources')
        ru=dict(zip(tables[0],map(int,tables[1])));need(ru.keys()=={'wall0','wall1','voluntary_switches','involuntary_switches','minor_faults','major_faults'}
             and all(x>=0 for x in ru.values()) and start<=ru['wall0']<ru['wall1']<=end,'bad resources')
        need({k:v-(origin if k in ('wall0','wall1') else 0) for k,v in ru.items()}==row['scene_rusage'][l],'resource receipt differs')
        res=[a+ru[b] for a,b in zip(res,('voluntary_switches','involuntary_switches','minor_faults','major_faults'))]
        rr=snapshot(cap,s,576,'clocks.csv');em=snapshot(cap,s,128,'empty.csv');raw[l]=rr
        need(all(x[2]>x[1] for x in rr) and last[l]<=rr[0][1] and last[2]<=rr[0][3] and ru['wall0']<=rr[0][3] and
             rr[-1][4]<=em[0][3] and rr[-1][2]<=em[0][1] and em[-1][4]<=ru['wall1'],'cross-window ordering')
        v=[x[2]-x[1] for x in em];empty+=v
        need(v==row['empty_cpu_ns'][l] and [[i,c0,c1,w0-origin,w1-origin] for i,c0,c1,w0,w1 in em]==row['empty_endpoints'][l],'empty raw mismatch')
        who='A' if l==al else 'B'
        for q in (1,4):
            for x in (0,1):col[f'{who}_{q}_{x}']=sum(rr[i][2]-rr[i][1] for i,qu,who0,xx in plan if (qu,xx)==(q,x))
        nextcpu.append(em[-1][2]);nextwall=max(nextwall,em[-1][4])
    ordered=[];i=0
    while i<576:
        _,q,first,x=plan[i]
        for l in (first,first^1):
            for j in range(i,i+q):
                _,c0,c1,w0,w1=raw[l][j];ordered.append([j,l,c0,c1,w0-origin,w1-origin])
        i+=q
    need((row['epoch'],row['cpu_cell'],row['scene'],row['role'],row['a_sha256'],row['b_sha256'])==(e,c,s,tag,STOCK,STOCK if tag=='S' else FIR)
         and ordered==row['clocks'] and all(a[5]<=b[4] for a,b in zip(ordered,ordered[1:])), 'real raw rows/role/order differ')
    i1=[i for i,q,_,_ in plan if q==1]
    gaps=[abs(raw[0][i][3]-raw[1][i][3]) for i in i1]
    walls=[raw[l][i][4]-raw[l][i][3] for i in i1 for l in (0,1)]
    return {'scene':s,'epoch':e,'cpu_cell':c,'real_thread_ns':col,'empty_ns_sum':sum(empty),
            'q1_gap_ns_twice_median':int(2*statistics.median(gaps)),'q1_wall_ns_twice_pooled_median':int(2*statistics.median(walls)),
            'resources_both_workers':res,'six_original_csv_hashes_sha256':fp.hexdigest()},[*nextcpu,nextwall]
def export(folder,out):
    P=folder/'C-seal.json';M=read(P);R=folder/'C-run';T=read(R/'terminal.json'); lg=[json.loads(l) for l in (R/'journal.jsonl').read_text().splitlines()]
    need(sha(P)==SEAL==T['seal_sha256']==lg[0]['seal'] and len(lg)==193 and T['complete'] is True and T['completed_pairs']=={'C':48,'S':48},
         'not the complete sealed C campaign')
    F=Path(M['q_dir']); dried=Path(M['dry_dir']);D=read(dried/'dry.json'); charges=M['dry_charges']
    need(sha(F/'seal.json')==M['Q']['seal_sha256']==QSEAL and M['Q']['gate_count']==M['Q']['gates_passed']==35 and M['Q']['raw_replay_cells']==240,
         'missing separate stock qualification')
    ids=M['image_identities'];need([sha(Path(p)) for p in M['image_paths']]==[x[3] for x in ids]==[STOCK,FIR] and
        ids[0][:2]!=ids[1][:2] and sha(Path(M['host_path']))==M['host_identity'][3],'wrong actual modules or host')
    need(D['complete'] is True and D['callback_clock_reads']==D['empty_clock_reads']==0 and T['native_dry_clock_reads']==0 and
         [sha(p/'dry.json') for p in [dried,*map(Path,M['prior_dry_dirs'])]]==charges['dry_receipt_hashes'] and len(M['prior_dry_dirs'])==1,
         'missing zero-clock admission attempts')
    official={r:read(R/f'decision-{r}.json') for r in 'SC'}
    origin=lg[0]['timed_stage_origin_ns'];dryorigin=lg[0]['first_dry_origin_ns'];need(dryorigin==charges['first_dry_start']<=origin,'bad stage origin')
    outdata={'schema':'native-CS80-cloud-C-compact-v1','stage_seal_sha256':SEAL,'stock_sha256':STOCK,'fir_sha256':FIR,
             'host_sha256':M['host_identity'][3],'build_ids':M['build_ids'],'Q':M['Q'],'sealed_sources':M['sources'],
             'admission':{k:v for k,v in charges.items() if k!='first_dry_start'},'before_timing_ns':origin-dryorigin,
             'terminal':T,'stage_budgets':M['stage_budget'],'cpu_ids':M['four_cpus'],'supervisor_cpu':M['supervisor_cpu'],
             'source_sha256':{**{f'decision_{k}':sha(R/f'decision-{k}.json') for k in 'SC'},
                              **{f'combined_{k}':sha(R/f'complete-{k}.json.gz') for k in 'SC'},'journal':sha(R/'journal.jsonl')},
             'original_decisions':official,'pairs':[],'cells':{'S':[],'C':[]}}
    need(sorted(p.name for p in R.glob('slot-*'))==[f'slot-{i:02d}' for i in range(48)],'extra or missing slot')
    prevend=epstart=0;cpu_sum=0;previous_done=origin
    for i in range(48):
        e=i//4;c=((0,1,3,2)[i%4]+e)%4;aa=(e//2+c)%2;launch=(e+c//2)%2;order=('SC' if ((e//2+c//2)%2==0) else 'CS')
        sd=R/f'slot-{i:02d}';seq=('idle','held_varied','release_equal','wake_varied','release_varied');f=4*e+c
        scenes=[seq[(f+j*(-1 if (f//5)%2 else 1))%5] for j in range(5)]
        need(scenes==(sd/'plan/scenes.txt').read_text().splitlines() and M['schedule']['slots'][i]['scene_order']==scenes,'changed scene order')
        for j,k in enumerate(order):
            w=sd/f'{j}-{k}';at,done=lg[4*i+2*j+1:4*i+2*j+3];vx=read(w/'verified.json');ex=vx['external'];expected=[int(k=='C' and l!=aa) for l in range(2)]
            s0,s1=ex['start'],ex['end'];hashnorm=sha(w/'normalized.json.gz');norm=gzread(w/'normalized.json.gz')
            need((at['event'],at['slot'],at['pair'],at['epoch'],at['cell'],done['event'],done['slot'],done['pair'])==('attempt',i,k,e,c,'verified',i,k)
                 and previous_done<=at['t']<=s0<s1<=done['t'] and 0<s1-s0<=100*10**9 and done['normalized_sha256']==hashnorm,'attempt/order/hash mismatch')
            if i%4==j==0:
                if i:need(at['epoch_start_ns']>=epstart+120*10**9 and at['epoch_start_ns']>=prevend+10*10**9,'epoch spacing')
                epstart=at['epoch_start_ns']
            need(at['epoch_start_ns']==epstart<=at['t'] and s0>=prevend,'overlap')
            prevend=s1;previous_done=done['t'];cpu_sum+=ex['pooled_worker_cpu_ns']
            need(ex['launch_first']==launch and ex['a_lane']==vx['a_lane']==aa and ex['pair']==vx['pair']==k and
                 ex['image_indexes_by_lane']==vx['image_indexes_by_lane']==expected and vx['compared_public_bytes']=={'0':24947518,'1':24947518},
                 'bad physical pair or parity receipt')
            for l,img in enumerate(expected):
                cap=w/f'peer-{l}/capture';id0=read(cap/'identity.json')
                need(vx['workers'][l]=={**id0,'verified_sha256':ids[img][3]} and [id0[t] for t in ('dev','ino','size')]==ids[img][:3] and
                     [id0[t] for t in ('lane','cpu','visits','callback_clock_reads','q_empty_clock_reads')]==[l,M['four_cpus'][c],1840,11520,2560],
                     'actual worker identity/clock/affinity receipt mismatch')
                need(sha(cap/'controls.csv')==D['controls_sha256']==vx['controls_sha256'] and
                     sha(cap/'baton.csv')==sha(F/f'Q-run/slot-{i:02d}'/f'peer-{l}/capture/baton.csv'),'changed controller or socket visit evidence')
            outdata['pairs'].append({'epoch':e,'cpu_cell':c,'pair':k,'cpu':M['four_cpus'][c],'a_lane':aa,'first_launch':launch,'indexes':expected,
                'start_ns':s0-origin,'end_ns':s1-origin,'epoch_start_ns':epstart-origin,'pooled_worker_cpu_ns':ex['pooled_worker_cpu_ns'],
                'normalized_sha256':hashnorm,'bytes_per_worker':24947518})
            last=[0,0,0];sumcalls=0
            for sn,rown in zip(scenes,norm,strict=True):
                need(sha(sd/'plan'/(sn+'.csv'))==M['lane_schedule_sha256'][f'{e}/{c}/{sn}'],'lane schedule hash')
                row,last=one_scene(w,e,c,sn,rown,origin,s0,s1,last,k);outdata['cells'][k].append(row);sumcalls+=sum(row['real_thread_ns'].values())
            need(sumcalls<=ex['pooled_worker_cpu_ns']+2000000,'CPU envelopes')
    need(cpu_sum==T['sum_of_measured_pair_external_worker_cpu_ns'] and T['original_genuine_clock_rows']==552960 and T['original_empty_brackets']==122880,'stage receipt totals')
    for k in 'SC':
        bulk=gzread(R/f'complete-{k}.json.gz');need((bulk['role'],bulk['a_sha256'],bulk['b_sha256'])==(k,STOCK,STOCK if k=='S' else FIR) and len(bulk['cells'])==240,'whole role file')
        off=0
        for i in range(48):
            w,=list((R/f'slot-{i:02d}').glob(f'*-{k}'));need(gzread(w/'normalized.json.gz')==bulk['cells'][off:off+5],'whole/slot normalized mismatch');off+=5
        del bulk
    with out.open('xb') as f:f.write(gzdata(outdata))
    print(sha(out))
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('sealed_stage',type=Path);ap.add_argument('new_file',type=Path)
    a=ap.parse_args();export(a.sealed_stage,a.new_file)
