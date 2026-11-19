#!/usr/bin/env python3
"""A real, untimed native VST3 sample/state/FP comparison, Linux x86-64."""
import argparse
import hashlib
import gzip
import json
import os
import shutil
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCENES = {'fresh-idle': (17408, 1), 'held8': (5120, 1), 'post-release': (25600, 1), 'wake': (16896, 16)}

def require(ok, message):
    if not ok: raise ValueError(message)

def sha(f):
    h=hashlib.sha256()
    with f.open('rb') as stream:
        for part in iter(lambda:stream.read(65536), b''):h.update(part)
    return h.hexdigest()

def compare(recs):
    require(len(recs)==2 and recs[0]['host_hash']==recs[1]['host_hash'], 'unchanged host identity')
    rows=[]
    for a,b in zip(recs[0]['scenes'], recs[1]['scenes']):
        scene=a['scene']; require(scene==b['scene'] and scene in SCENES,'scene identity')
        count, repeats=SCENES[scene]; require(a['call_count']==b['call_count']==count,'callback count')
        require(a['sha256']==b['sha256'] and a['bytes']==b['bytes'],'public bytes differ: '+scene)
        require(a['metadata']==b['metadata'],'public receipt differs: '+scene)
        m=a['metadata'];require(m['audible_admission'] and m['quiet_admission'] and m['period_us']==0, 'public admission')
        require(a['bytes']['all.pcm']==512*count and a['bytes']['callback-status.bin']==52*count,'all phases must be observed')
        require(sum(f.startswith('state-') for f in a['bytes'])==4*repeats,'phase snapshot count')
        rows.append({'scene':scene,'calls':count,'stereo_frames':count*64,'matched_bytes':sum(a['bytes'].values()),'pcm_sha256':a['sha256']['all.pcm']})
    require(len(rows)==len(SCENES) and {r['scene'] for r in rows}==set(SCENES),'four distinct scenes required')
    return {'scenes':rows,'all_calls':sum(r['calls'] for r in rows),'stereo_frames':sum(r['stereo_frames'] for r in rows),'matched_bytes':sum(r['matched_bytes'] for r in rows)}

def capture(args):
    repo=args.source.resolve();sdk=args.sdk.resolve();out=args.output.resolve();out.mkdir() # never overwrite
    tool=repo/'tools/native-vst3-profile'; host=out/'host.cpp'; probe=out/'probe';shutil.copyfile(tool/'host.cpp',host)
    require(sha(host)=='2c053909bf58c1832f542ff580e5f0b75db0eefd29673648cf5964ea5f3a4b90', 'unsupported host source revision')
    require(sha(HERE/'public-probe.patch')=='82bf1aa59b4576c40c8ba88cc4d3523513c47a1d6a9df3f1110863c8186e2157','probe patch identity')
    with (HERE/'public-probe.patch').open('rb') as f: subprocess.run(['patch','--batch','-p1','--fuzz=0'],cwd=out,stdin=f,check=True)
    require(sha(host)=='a1d4a3cacd2f62c29282ae7e7fd1c202ea185004c7a29227e3a4d436ac5dad62','unexpected proof source')
    subprocess.run([args.cxx,'-O2','-g','-DNDEBUG','-std=gnu++20','-I'+str(tool),'-I'+str(repo/'src'),'-I'+str(repo/'tests'),'-I'+str(sdk),host,sdk/'pluginterfaces/base/coreiids.cpp',sdk/'pluginterfaces/base/funknown.cpp',sdk/'public.sdk/source/vst/vstinitiids.cpp','-ldl','-pthread','-o',probe],check=True)
    recs=[]
    for arm,mod in (('stock',args.stock.resolve()),('candidate',args.candidate.resolve())):
        module_start=sha(mod);rows=[]
        for scene,(count,repeats) in SCENES.items():
            folder=out/arm;folder.mkdir(exist_ok=True);home=folder/('home-'+scene);home.mkdir(); dest=folder/scene
            env=dict(os.environ,HOME=str(home),LC_ALL='C',XDG_CONFIG_HOME=str(home/'config'),XDG_CACHE_HOME=str(home/'cache'),XDG_DATA_HOME=str(home/'data'))
            subprocess.run([probe,mod,scene,'0',dest],check=True,env=env,timeout=300)
            sampler=json.loads((dest/'sampler.json').read_text());require(all(sampler[k]==0 for k in ('recorded','outside','dropped','timer_overruns','foreign_signals')),'timer/measurement enabled')
            files=sorted([dest/'scene.json',dest/'all.pcm',dest/'callback-status.bin',*dest.glob('state-*')]);require(len(files)==3+4*repeats,'state roster')
            rows.append({'scene':scene,'call_count':count,'sha256':{p.name:sha(p) for p in files},'bytes':{p.name:p.stat().st_size for p in files},'metadata':json.loads((dest/'scene.json').read_text())})
        require(sha(mod)==module_start,'module changed during execution')
        rec={'module_hash':module_start,'host_hash':sha(probe),'scenes':rows};(folder/'receipt.json').write_text(json.dumps(rec,indent=2)+'\n');recs.append(rec)
    result=compare(recs); (out/'comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

def replay():
    payload=json.loads(gzip.decompress((HERE/'recorded.json.gz').read_bytes()))
    r=compare([payload['control'],payload['combined']]); identity=payload['identity'];
    require(identity['baseline']['module']==payload['control']['module_hash'] and identity['combined']['module']==payload['combined']['module_hash'], 'module identity')
    require((r['all_calls'],r['stereo_frames'],r['matched_bytes'])==(65024,4161536,37760474),'incomplete scenes')
    cells=payload['automation']['cells'];require(len(cells)==6 and {(a['rate'],a['mode']) for a in cells}=={(f,m) for f in (44100,48000,96000) for m in ('realtime','offline')},'unmatched automation cells')
    for a in cells:
        require(a['matched_files']==len(a['control_sha256'])==36 and a['combined_sha256']==a['control_sha256'] and a['matched_bytes']==1179648,'automation bytes')
    print(f"{r['all_calls']} callbacks, {r['stereo_frames']} stereo frames and {r['matched_bytes']} public bytes match. 216 automation capture files match.")

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);s=p.add_subparsers(dest='action',required=True);s.add_parser('replay');c=s.add_parser('compare')
    for flag in ('source','sdk','stock','candidate','output'):c.add_argument('--'+flag,type=Path,required=True)
    c.add_argument('--cxx',default='c++');arg=p.parse_args()
    if arg.action=='replay':replay()
    else:capture(arg)
