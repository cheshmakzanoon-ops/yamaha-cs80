#!/usr/bin/env python3
"""Real public VST3 parity acquisition. Never interprets elapsed time as plugin cost."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, concurrent.futures, struct
SCENES=('factory','quiet','automation','pure','legacy','external','mpe','effects','pulse')
RATES=(44100,48000,96000)
def sha(p):
    with p.open('rb') as fp:return hashlib.file_digest(fp,'sha256').hexdigest()
def check(b,m):
    if not b:raise ValueError(m)
def put(p,j):p.write_text(json.dumps(j,sort_keys=True,indent=2)+'\n')
def load(p):return json.loads(p.read_text())
def read(folder):
    plan=load(folder/'plan.json');result={}
    for sr,scene in plan['cells']:
        k=f'{sr}-{scene}';cap=folder/k;row=load(cap/'outcome.json');check(row['callbacks']==3072 and row['sample_rate']==sr and row['scene']==scene,'unexpected public workload');files=set(p.name for p in cap.iterdir());expect={'outcome.json','pcm.bin','callbacks.bin','controls.csv','initial-metadata.csv','final-metadata.csv','initial.state'}|{f'after-{b}.state' for b in (1,512,513,1918,1919,2879,3071)};check(files==expect and all((cap/name).stat().st_size>0 for name in files),'public artifact inventory');check((cap/'pcm.bin').stat().st_size==row['frames']*8,'short PCM');check(row['note_ons']==(0 if scene=='quiet' else 16) and row['note_offs']==row['note_ons'],'note schedule');
        if scene in ('automation','legacy','effects'):check(row['points_sent']>0 and (cap/'initial.state').read_bytes()!=(cap/'after-3071.state').read_bytes(),'no changing state');
        if scene in ('automation','mpe'):check(row['poly_pressure']>0,'no pressure');
        records=struct.Struct('<IiqiQiII'+'IHHi'*3+'dII');raw=(cap/'callbacks.bin').read_bytes();check(len(raw)==row['callbacks']*records.size,'unexpected callback trace length');row['reported_latency_values_samples']=sorted({r[-2] for r in records.iter_unpack(raw)});row['reported_tail_values_samples']=sorted({r[-1] for r in records.iter_unpack(raw)});
        result[k]={'observations':row,'sha256':{name:sha(cap/name) for name in sorted(files)},'file_bytes':{name:(cap/name).stat().st_size for name in sorted(files)}}
    return {'plan':plan,'cells':result}
def take(a):
    mod=a.module.resolve(strict=True);host=a.host.resolve(strict=True);out=a.out.resolve();out.mkdir();plan={'module_sha256':sha(mod),'host_sha256':sha(host),'cells':[[sr,s] for sr in (a.rates or RATES) for s in (a.scenes or SCENES)],'timing':False};put(out/'plan.json',plan)
    def one(cell):
        sr,s=cell;k=f'{sr}-{s}';cap=out/k;home=out/f'home-{k}';home.mkdir();env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'config'),XDG_CACHE_HOME=str(home/'cache'),LC_ALL='C',TZ='UTC');
        with (out/('stdout-'+k)).open('wb') as log:subprocess.run([str(host),str(mod),s,str(sr),str(cap)],env=env,check=True,stdout=log,stderr=subprocess.STDOUT,timeout=240)
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:list(pool.map(one,plan['cells']))
    check(sha(host)==plan['host_sha256'] and sha(mod)==plan['module_sha256'],'inputs changed');put(out/'receipt.json',read(out))
def compare(a):
    base=read(a.baseline);cand=read(a.candidate);check(base['plan']['host_sha256']==cand['plan']['host_sha256'],'different host');cells=cand['cells'];
    for k,v in cells.items():check(v==base['cells'][k],'public samples, state, metadata, FP, events, latency, or tails differ: '+k)
    put(a.out,{'baseline_module_sha256':base['plan']['module_sha256'],'candidate_module_sha256':cand['plan']['module_sha256'],'host_sha256':cand['plan']['host_sha256'],'all_public_bytes_exact':True,'timing':False,'observations':cells,'totals_per_arm':{'histories':len(cells),'genuine_callbacks':sum(v['observations']['callbacks'] for v in cells.values()),'stereo_frames':sum(v['observations']['frames'] for v in cells.values()),'public_state_snapshots':sum(sum(k.endswith('.state') for k in v['file_bytes']) for v in cells.values()),'pcm_bytes':sum(v['file_bytes']['pcm.bin'] for v in cells.values()),'public_bytes':sum(sum(v['file_bytes'].values()) for v in cells.values())}})
    print('exact:',len(cells),'native histories')
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);p=ap.add_subparsers(dest='op',required=True);sp=p.add_parser('take');sp.add_argument('module',type=Path);sp.add_argument('out',type=Path);sp.add_argument('--host',required=True,type=Path);sp.add_argument('--jobs',type=int,default=2);sp.add_argument('--scenes',choices=SCENES,nargs='+');sp.add_argument('--rates',choices=RATES,type=int,nargs='+');sp=p.add_parser('compare');sp.add_argument('baseline',type=Path);sp.add_argument('candidate',type=Path);sp.add_argument('out',type=Path);a=ap.parse_args();take(a) if a.op=='take' else compare(a)
