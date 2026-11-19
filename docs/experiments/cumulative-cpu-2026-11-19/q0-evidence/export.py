#!/usr/bin/env python3
"""Reconcile a complete archived original-only Q0 campaign, writing aggregate evidence, never audio or timings.
Usage: python3 -B export.py <archived-campaign-directory> <new-sufficient.json.gz>
All inputs are read from the supplied directory, not from machine-specific paths in the original seal.
"""
import argparse
import csv
import gzip
import hashlib
import io
import json
import statistics
from pathlib import Path

STUDY = 'c1401da9002d94d853ec7612177fcafcd1660130e4627c5835c93d3d7ada899d'
SEAL = '167fad0697b438996ae27dddabecd794fd3a3bdfcf7977e50e70a3c3b7cf5638'
ORIGINAL = '8d4078be629d097e6470ec90fb12ccfdb7ad486663f14bb8e3aaef3076bd8725'
FINAL = '25281f677330f7e73a0fc695f43cdfc6eaf902ffb059218e0b3ac36188edae5a'
HOST = 'f4eec9685fc16fd592d3061e730724f5f1418e05ef2ddfaf284d852d5d27a70e'
SC = ('idle','release_equal','release_varied','held_varied','wake_varied')
FILES = ('all.fp','all.pcm','guards.csv','midi.csv','states.bin')
NS = 10**9

def need(b, msg):
    if not b:
        raise ValueError(msg)

def sha(b): return hashlib.sha256(b).hexdigest()
def H(p): return sha(p.read_bytes())
def J(p): return json.loads(p.read_bytes())
def G(p): return json.loads(gzip.decompress(p.read_bytes()))
def table(p):
    with p.open(encoding='ascii', newline='') as f: return list(csv.reader(f))
def compact(obj):
    b = json.dumps(obj, sort_keys=True, separators=(',',':'), allow_nan=False).encode('utf-8')
    # Set mtime=0 and OS byte to 255 regardless of Python/zlib platform.
    with io.BytesIO() as buf:
        with gzip.GzipFile(filename='', mode='wb', fileobj=buf, mtime=0, compresslevel=9) as fp: fp.write(b)
        return buf.getvalue()

def plan(epoch, cell, scene):
    k = SC.index(scene)
    out = []
    for i in range(576):
        block,pos = divmod(i,36)
        q = 4 if (block+epoch+cell+k)%2 else 1
        out.append((i,q,(block//2+epoch+cell//2+k+pos//q)%2,int(i%9==(i//9+3*epoch+cell+k)%9)))
    return out

def batons(scenes, plans, lane):
    expected = [['serial','scene','placement','phase','index','count','first','lane']]
    def add(s, place, phase, i, num, first):
        expected.append([str(v) for v in (len(expected)-1,s,place,phase,i,num,first,lane)])
    for s in scenes:
        k=SC.index(s);wake=s=='wake_varied'
        for place in range(16 if wake else 1):
            add(s,place,0,0,1024+(8192 if s.startswith('release_') else 0),(place+k)%2)
            i=36*place if wake else 0; end=i+(36 if wake else 576)
            while i<end:
                _,q,first,_=plans[s][i]
                add(s,place,1,i,q,first);i+=q
            add(s,place,2,0,33,(place+k+1)%2)
    return expected

def endpoints(p, n, origin, positive):
    rows=table(p);need(rows[0]==['index','cpu0','cpu1','wall0','wall1'] and len(rows)==n+1,'wrong endpoint header/count')
    out=[];prev_c=prev_w=0
    for i,row in enumerate(rows[1:]):
        need(len(row)==5 and all(x.isascii() and x.isdecimal() for x in row),'bad CSV integer')
        k,c0,c1,w0,w1=map(int,row)
        w0-=origin;w1-=origin
        need(i==k and c0>0 and prev_c<=c0<=c1 and (not positive or c0<c1) and prev_w<=w0<w1,'index, order or sign of endpoint')
        out.append([i,c0,c1,w0,w1]);prev_c,prev_w=c1,w1
    return out

def reconcile(campaign):
    sp=campaign/'Q0-stage';r=sp/'Q0-run';dry=campaign/'Q0-dry'
    need(H(campaign/'study.json')==STUDY and H(sp/'seal.json')==SEAL,'wrong prospective study or stage')
    study=J(campaign/'study.json');seal=J(sp/'seal.json');attempt=J(campaign/'run.attempt.json');md=J(dry/'preflight.json')
    term=J(r/'terminal.json');decision=J(r/'numerical-gates.json')
    need(study['guest_only'] and not study['automatic_candidate_launch'] and study['stock_identity']==seal['stock_identity']
         and study['release_pin']['final_release_elf_sha256']==FINAL and study['stock_identity'][3]==ORIGINAL,'wrong study comparator or disposition')
    need(seal['study_sha256']==STUDY and seal['scope']=='same-physical-original-stock-only' and seal['host_identity'][3]==HOST
         and attempt['state']=='run' and attempt['study_sha256']==STUDY and attempt['details']['Q0_seal_sha256']==SEAL and attempt['details']['exclusive_window'],'admission')
    need(term['schema']=='cs80-origin-cumulative-Q0-terminal-v1' and term['complete'] is True and term['numerical_gates_pass'] is False
         and term['study_sha256']==STUDY and term['seal_sha256']==SEAL and term['candidate_runs']==0 and term['candidate_stage_authorized'] is False,'not terminal negative Q0')
    need(md['schema']=='cs80-original-Q0-full-dry-v1' and md['study_sha256']==STUDY and md['complete'] is True and H(dry/'preflight.json')==seal['dry_receipt_sha256']
         and md['stock_sha256']==ORIGINAL and md['host_sha256']==HOST and md['callback_clock_reads']==md['timed_candidate_runs']==md['timed_placebo_runs']==0,'wrong zero-clock rehearsal')
    physical=seal['stock_identity'][:3];physical_tag=sha((':'.join(map(str,physical))).encode('ascii'))
    dry_checks={s:{k:md['public_capture_per_worker'][s]['artifacts'][k] for k in FILES} for s in SC}
    public_bytes=sum(v['bytes'] for a in dry_checks.values() for v in a.values())
    need(public_bytes==24947518 and md['visits_per_worker']==1840,'missing reference')
    for worker,lane in [('isolated',-1),('peer-0',0),('peer-1',1)]:
        cap=dry/worker/'capture';native=J(cap/'identity.json')
        need([native[k] for k in ('dev','ino','size')]==physical and [native[k] for k in ('lane','callback_clock_reads','q_empty_clock_reads','visits')]==[lane,0,0,1840],'dry object/clock disagreement')
        need(H(cap/'controls.csv')==md['controls_sha256'],'dry controller mismatch')
        for s in SC:
            for k,v in dry_checks[s].items():
                p=cap/s/k;need(p.stat().st_size==v['bytes'] and H(p)==v['sha256'],'changed original dry capture')
    journal=[json.loads(x) for x in (r/'journal.jsonl').read_text().splitlines()]
    need(len(journal)==1+48*2 and journal[0]['event']=='start' and journal[0]['seal']==SEAL and journal[0]['t']==md['started_at_monotonic_ns'],'slot ledger')
    origin=journal[0]['t'];combined=G(r/'complete-normalized.json.gz')
    need(combined.keys()=={'role','study_family','study_sha256','a_sha256','b_sha256','cells'} and combined['role']=='Q'
         and combined['study_family']=='cs80-origin-cumulative-Q0-v1' and combined['study_sha256']==STUDY and combined['a_sha256']==combined['b_sha256']==ORIGINAL
         and len(combined['cells'])==240 and len(seal['schedule']['slots'])==48,'wrong combined file')
    evidence={'schema':'cs80-original-only-Q0-compact-v1','scope':'shared-guest-original-only','study_sha256':STUDY,'stage_seal_sha256':SEAL,
       'original':{'commit':study['original_commit'],'tree':study['original_tree'],'elf_sha256':ORIGINAL,'build_id':study['stock_build_id'],
                   'size':physical[2],'actual_file_dev_inode_size_tag_sha256':physical_tag},
       'never_timed_final':{'commit':study['release_pin']['final_git_commit'],'tree':study['release_pin']['final_git_tree'],'elf_sha256':FINAL,'build_id':study['final_build_id']},
       'host_sha256':HOST,'host_build_id':seal['host_build_id'],'cpu_ids':seal['four_cpus'],'supervisor_cpu':seal['supervisor_cpu'],
       'sealed_study_endpoints':study['registered_endpoints'],'budgets':seal['stage_budget'],'terminal':term,'original_decision':decision,
       'digest_receipts':{'dry':seal['dry_receipt_sha256'],'raw_journal':H(r/'journal.jsonl'),'complete_normalized':H(r/'complete-normalized.json.gz'),
           'actual_decision':H(r/'numerical-gates.json'),'terminal':H(r/'terminal.json'),'raw_run_latch':H(campaign/'run.attempt.json'),
           'dry_latch':H(campaign/'dry.attempt.json'),'method_schedule':study['method_schedule_sha256'],'all_planned_lane_csvs':study['lane_csvs_sha256']},
       'reference':{'dry_genuine_or_empty_clock_reads':0,'dry_workers':3,'visits_per_worker':1840,'public_bytes_per_worker':public_bytes,
           'controller_sha256':md['controls_sha256'],'dry_capture_files':dry_checks},'slots':[],'cells':[]}
    for n,v in enumerate(seal['schedule']['slots']):
        w=r/f'slot-{n:02d}';e,c=v['epoch'],v['cpu_cell'];cpu=seal['four_cpus'][c]
        expected_c=((0,1,3,2)[n%4]+e)%4;t=4*e+c;base=('idle','held_varied','release_equal','wake_varied','release_varied')
        scenes=[base[(t+i*(-1 if (t//5)%2 else 1))%5] for i in range(5)]
        need((e,c)==(n//4,expected_c) and v['scene_order']==scenes==(w/'plan/scenes.txt').read_text().splitlines(),'wrong fixed slot order')
        plans={s:plan(e,c,s) for s in scenes};a_lane=(e//2+c)%2;first=(e+c//2)%2
        need(v['a_lane']==a_lane and v['launch_first']==first,'wrong original role assignment')
        for s in scenes:
            p=w/'plan'/(s+'.csv');expected=[['index','quantum','first_lane','extra']]+[[str(vv) for vv in row] for row in plans[s]]
            need(table(p)==expected and H(p)==seal['lane_schedule_sha256'][f'{e}/{c}/{s}'],'changed planned callback assignments')
        at,ok=journal[2*n+1:2*n+3];meta=J(w/'verified.json');ext=meta['external']
        need((at['event'],ok['event'],at['slot'],ok['slot'],at['epoch'],at['cell'])==('attempt','verified',n,n,e,c)
             and [meta['epoch'],meta['cpu_cell'],meta['genuine_callbacks'],meta['empty_brackets']]==[e,c,5760,1280],'slot identity')
        digest=H(w/'normalized.json.gz');need(digest==ok['normalized_sha256'],'normalized digest ledger')
        need(meta['public_bytes_compared_by_lane']=={'0':public_bytes,'1':public_bytes} and meta['public_reference_sha256']==md['controls_sha256'] and ext['launch_first']==first,'public parity receipts/order')
        slotnorm=G(w/'normalized.json.gz');need(len(slotnorm)==5,'missing scene')
        sl={'epoch':e,'cpu_cell':c,'cpu':cpu,'a_lane':a_lane,'launch_first':first,'normalized_sha256':digest,'verified_sha256':H(w/'verified.json'),
            'epoch_start_ns':at['epoch_start_ns']-origin,'attempt_ns':at['t']-origin,'start_ns':ext['start']-origin,'end_ns':ext['end']-origin,
            'verified_ns':ok['t']-origin,'worker_cpu_ns':ext['pooled_worker_cpu_ns'],'public_bytes_per_lane':[public_bytes,public_bytes],
            'actual_object_tags':[],'identity_and_baton_sha256':[]}
        for l in (0,1):
            cap=w/f'peer-{l}'/'capture';ident=J(cap/'identity.json')
            need(ident==meta['actual_worker_identities'][l] and [ident[k] for k in ('dev','ino','size')]==physical
                 and [ident[k] for k in ('lane','cpu','callback_clock_reads','q_empty_clock_reads','visits')]==[l,cpu,11520,2560,1840],'wrong native object/affinity/count')
            need(table(cap/'baton.csv')==batons(scenes,plans,l) and H(cap/'controls.csv')==md['controls_sha256'],'actual controller/visit history')
            sl['actual_object_tags'].append(sha((':'.join(str(ident[k]) for k in ('dev','ino','size'))).encode('ascii')))
            sl['identity_and_baton_sha256'].append([H(cap/'identity.json'),H(cap/'baton.csv')])
        evidence['slots'].append(sl);last_cpu=[0,0];last_wall=0;sum_callback=0
        for k,s in enumerate(scenes):
            actual=[];empty=[];rs=[];h=hashlib.sha256()
            for l in (0,1):
                cap=w/f'peer-{l}'/'capture'/s;reals=endpoints(cap/'clocks.csv',576,origin,True);empties=endpoints(cap/'empty.csv',128,origin,False)
                for name in ('clocks.csv','empty.csv','resources.csv'):h.update((H(cap/name)+'\n').encode('ascii'))
                rt=table(cap/'resources.csv');need(len(rt)==2 and rt[0]==['wall0','wall1','voluntary_switches','involuntary_switches','minor_faults','major_faults']
                     and len(rt[1])==6 and all(x.isascii() and x.isdecimal() for x in rt[1]),'bad raw scene resources')
                z=dict(zip(rt[0],map(int,rt[1])));z['wall0']-=origin;z['wall1']-=origin
                need(sl['start_ns']<=z['wall0']<=reals[0][3] and last_wall<=reals[0][3] and last_cpu[l]<=reals[0][1] and reals[-1][4]<=empties[0][3]
                     and reals[-1][2]<=empties[0][1] and empties[-1][4]<=z['wall1']<=sl['end_ns'],'cross-lane/cross-scene/enclosing order')
                actual.append(reals);empty.append(empties);rs.append(z);last_cpu[l]=empties[-1][2]
            last_wall=max(row[-1][4] for row in empty);pl=plans[s];i=0;clocks=[]
            while i<576:
                idx,q,f,_=pl[i]
                for l in (f,1-f):
                    for j in range(i,i+q):clocks.append([j,l,*actual[l][j][1:]])
                i+=q
            need(all(x[5]<=y[4] for x,y in zip(clocks,clocks[1:])),'raw clocks violate bilateral baton order')
            en=[[x[2]-x[1] for x in row] for row in empty]
            raw={'role':'Q','epoch':e,'cpu_cell':c,'scene':s,'a_sha256':ORIGINAL,'b_sha256':ORIGINAL,'clocks':clocks,
                 'empty_endpoints':empty,'empty_cpu_ns':en,'scene_rusage':rs}
            need(raw==slotnorm[k]==combined['cells'][5*n+k],'raw original CSV vs per-slot vs complete normalized mismatch')
            tim={f'{t}_{q}_{x}':0 for t in 'AB' for q in (1,4) for x in (0,1)};gaps=[];wall=[]
            for i,q,_,x in pl:
                for l in (0,1):
                    _,t0,t1,w0,w1=actual[l][i];tag='A' if l==a_lane else 'B';tim[f'{tag}_{q}_{x}']+=t1-t0;sum_callback+=t1-t0
                    if q==1:wall.append(w1-w0)
                if q==1:gaps.append(abs(actual[0][i][3]-actual[1][i][3]))
            evidence['cells'].append({'scene':s,'epoch':e,'cpu_cell':c,'thread_ns':tim,'empty_ns_sum':sum(map(sum,en)),
                'q1_gap_ns_twice_median':int(2*statistics.median(gaps)),'q1_wall_ns_twice_pooled_median':int(2*statistics.median(wall)),
                'six_original_csv_hashes_sha256':h.hexdigest()})
        need(sum_callback<=sl['worker_cpu_ns']+2_000_000,'callback CPU exceeds child CPU envelope')
    return evidence

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('archived_campaign',type=Path);p.add_argument('new_output',type=Path);a=p.parse_args()
    encoded=compact(reconcile(a.archived_campaign))
    with a.new_output.open('xb') as f: f.write(encoded)
    print(f'{len(encoded)} bytes; SHA256 {sha(encoded)}')
