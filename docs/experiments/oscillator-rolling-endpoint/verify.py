"""Replay the public rolling-endpoint study. Reads files only; no plug-in execution."""
from pathlib import Path
import csv, gzip, hashlib, itertools, json, math, random, re, statistics
ROOT=Path(__file__).resolve().parent
def chk(v, label):
    if not v: raise ValueError(label)
def sha(b): return hashlib.sha256(b).hexdigest()
def hashval(x): return type(x) is str and re.fullmatch('[a-f0-9]{64}',x) is not None
def near(a,b): return math.isclose(a,b,rel_tol=1e-12,abs_tol=1e-13)
def pct(sorted_v,p):
    pos=(len(sorted_v)-1)*p; lo=int(pos)
    return sorted_v[lo]+(sorted_v[min(lo+1,len(sorted_v)-1)]-sorted_v[lo])*(pos-lo)
def stats(comparisons,aa,clocks):
    def block(a):
        chk(len(a)==4 and all(math.isfinite(x) and x>0 for x in a),'positive balanced blocks')
        return (math.log(a[0])+math.log(a[3])-math.log(a[1])-math.log(a[2]))/2
    g=lambda x: 1-math.exp(-x)
    eff=list(map(block,comparisons)); aeff=list(map(block,aa)); rnd=random.Random(20261117)
    bs=sorted(statistics.mean(rnd.choices(eff,k=6)) for _ in range(10000))
    lo,hi=g(pct(bs,.025)),g(pct(bs,.975)); noise=max(abs(g(x)) for x in aeff); over=max(clocks); floor=max(noise,3*over)
    return dict(gain_fraction=g(statistics.mean(eff)),gain_ci95=[lo,hi],aa_noise_fraction=noise,
                clock_overhead_fraction_max=over,claim_floor_fraction=floor,
                gain_above_noise=lo>floor,noninferiority_lower95_at_least_minus_3pct=lo>=-0.03)
def verify(folder=ROOT):
    manifest=json.loads((folder/'manifest.json').read_text())
    chk(manifest['schema']=='rolling-endpoint-public-export-v1','unknown manifest')
    files=manifest['files']
    for f,info in files.items():
        file=folder/f; chk(file.is_file() and not file.is_symlink() and file.stat().st_size==info['bytes'] and sha(file.read_bytes())==info['sha256'],'file integrity: '+f)
    with gzip.open(folder/'campaign.json.gz','rb') as stream: raw=stream.read(32*1024**2+1)
    chk(len(raw)<=32*1024**2 and len(raw)==manifest['expanded_bytes'] and sha(raw)==manifest['expanded_sha256'],'expanded bytes')
    chk(all(t not in raw for t in (b'/'+b'workspace'+b'/',b'/'+b'home'+b'/'+b'agent',b'/'+b'tmp'+b'/')),'private path')
    data=json.loads(raw); grid=data['grid']; sched=list(itertools.product(range(3),(44100,48000,96000),range(2),(8,16),(64,256,1024)))
    chk(len(grid)==len(sched)==108,'grid length')
    grid_recs=set();grid_frames=grid_samples=0
    for i,(item,case) in enumerate(zip(grid,sched)):
        chk(item['case']==list(case),'grid position')
        s=item['arms']['stock'];c=item['arms']['candidate'];chk(s['comparison']==c['comparison'],'untimed paired equality')
        for arm in (s,c):
            rec=arm['raw_receipt_sha256'];chk(hashval(rec) and rec not in grid_recs,'grid receipt identity');grid_recs.add(rec)
            q=arm['comparison']
            chk(q['complete_parameter_count']==2378 and len(q['observe_only_readback_sha256'])==4 and hashval(q['post_A_state_sha256']),'state and scalar roster')
            chk(len(q['segments'])==4 and q['render_sha256']==[seg['fully_encoded_sha256'] for seg in q['segments']],'render identity')
            for seg in q['segments']:
                chk(seg['callback_count']==4096//case[-1] and seg['pcm_sample_count']==8192 and seg['nonzero_samples']>0 and math.isfinite(seg['peak_abs']) and hashval(seg['pcm_sha256']),'untimed coverage')
        grid_frames+=4*4096;grid_samples+=4*8192
    chk(grid_frames==1769472 and grid_samples==3538944 and len(grid_recs)==216,'untimed totals')
    schedule=[(f,b,p,s,l,'candidate' if f=='comparison' and l=='B' else 'stock') for f,n in [('aa',2),('comparison',6)] for b in range(n) for p in range(3) for s,l in enumerate('ABBA')]
    runs=data['runs'];chk(len(runs)==len(schedule)==96,'timing run count')
    samepcm={};samestate={};receipts=set();actual={};baselines={i:[] for i in range(3)};nb=ne=0;bytespcm=0
    for idx,(r,target) in enumerate(zip(runs,schedule)):
        f,b,p,s,l,arm=target
        chk([r[k] for k in ('family','block','policy','slot','label','arm')]==list(target),'timing slot/order')
        chk(r['index']==idx and r['returncode']==0 and r['watchdog_reason'] is None and r['group_empty'] and 0<r['wall_seconds']<=25,'timing execution outcome')
        rec=r['raw_receipt_sha256'];chk(hashval(rec) and rec not in receipts,'timing receipt identity');receipts.add(rec)
        segs=r['segments'];chk(len(segs)==6,'six measured histories');hs=[]
        for j,(v,frames) in enumerate(zip(segs,(4096,4096,4096,4096,65536,524288))):
            ts=v['callback_cpu_ns'];empty=v['clock_pair_ns']
            chk(v['frames']==frames and v['callback_count']==len(ts)==frames//64 and len(empty)==128,'callback schedule')
            chk(all(type(x) is int and 0<x<=25_000_000_000 for x in ts+empty),'clock value bound')
            chk(v['callback_cpu_total_ns']==sum(ts) and v['pcm_bytes']==frames*8 and v['pcm_nonzero_count']>0 and math.isfinite(v['pcm_peak']) and v['pcm_peak']>0 and math.isfinite(v['pcm_rms']) and v['pcm_rms']>0,'segment metrics')
            chk(all(hashval(v[k]) for k in ('render_sha256','pcm_sha256','protocol_sha256')),'segment digest')
            if j==5:
                chk(v['nonzero_callback_fraction']==1,'nonzero measured callbacks')
                perframe=sum(ts)/frames; overhead=len(ts)*statistics.mean(empty)/sum(ts)
                chk(near(r['measured_cpu_ns_per_frame'],perframe) and near(r['measured_clock_overhead_fraction'],overhead),'clock arithmetic')
                actual.setdefault((f,b,p),[]).append(perframe);baselines[p].append(overhead)
            hs.append(v['pcm_sha256']);nb+=len(ts);ne+=len(empty);bytespcm+=frames*8
        state=[r['post_A_state_sha256'],r['connected_metadata_sha256'],r['observe_only_readback_sha256']]
        chk(p not in samepcm or samepcm[p]==hs,'mismatched timed complete histories');samepcm[p]=hs
        chk(p not in samestate or samestate[p]==state,'mismatched post-A state/readbacks');samestate[p]=state
    observed={}
    for p in range(3):
        result=stats([actual['comparison',i,p] for i in range(6)],[actual['aa',i,p] for i in range(2)],baselines[p]);observed[p]=result;expected=data['audit']['recomputed_whole_block_statistics'][str(p)]
        for k,v in result.items():
            if type(v) is list: chk(all(near(x,y) for x,y in zip(v,expected[k])),'phase '+k)
            elif type(v) is bool: chk(v is expected[k],k)
            else: chk(near(v,expected[k]),k)
    admitted=observed[1]['gain_above_noise'] and observed[0]['noninferiority_lower95_at_least_minus_3pct'] and observed[2]['noninferiority_lower95_at_least_minus_3pct']
    chk(data['audit']['primary_circuit_and_both_nontarget_guards_pass'] is admitted,'joint decision')
    chk(nb==data['audit']['independent_raw_callback_rows']==909312 and ne==data['audit']['empty_clock_pairs']==73728 and bytespcm==data['audit']['pcm_bytes_cross_checked'] and len(receipts)==96,'all timing counters')
    print('108 exact configurations; 216 untimed receipts; 96 timing slots;',nb,'callback durations;',ne,'clock pairs; promotion criterion:',admitted)
    for p,m in observed.items():
        print(('Legacy','Circuit','Pure')[p]+':',f"{m['gain_fraction']*100:.3f}% CI [{m['gain_ci95'][0]*100:.3f}, {m['gain_ci95'][1]*100:.3f}]%; floor {m['claim_floor_fraction']*100:.3f}%")
    return admitted
if __name__=='__main__': verify()

