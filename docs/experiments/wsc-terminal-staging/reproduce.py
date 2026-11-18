#!/usr/bin/env python3
"""Explicit later reproduction of the public-render probe; never a benchmark.
This helper was syntax-checked, not executed, while assembling the evidence export.
"""
from pathlib import Path
import argparse, hashlib, json, os, platform, resource, subprocess

PACKAGE=Path(__file__).resolve().parent
BASE=['Engine','FactoryPresets','Ig00156Batch']
SCALAR=BASE+['EngineWscTerminalStaging','WscTerminalCohort']
VECTOR=SCALAR+['WscNodeKernelSetup','WscNodeKernelAvx2','EngineWscVectorRender']
FLAGS=['-O3','-DNDEBUG','-std=gnu++20','-fPIC','-Wall','-Wextra','-Wpedantic']
def tu_flags(name):
    return FLAGS+(['-ffp-contract=off']if name in ('Ig00156Batch','WscNodeKernelAvx2')else[])
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def limits():
    resource.setrlimit(resource.RLIMIT_CPU,(90,90))
    resource.setrlimit(resource.RLIMIT_AS,(3*1024**3,3*1024**3))
    resource.setrlimit(resource.RLIMIT_FSIZE,(128*1024**2,128*1024**2))
def normalize(exe,csv):
    syms=[]
    for line in subprocess.check_output(['nm','-S','-n',str(exe)],text=True).splitlines():
        t=line.split()
        if len(t)==4 and t[2]in'TtWw':syms.append((int(t[0],16),int(t[1],16),t[3]))
    rows=[]
    for line in csv.read_text().splitlines():
        i,sig,code,pc,csr,cw,sw,err=(int(x,0)for x in line.split(','))
        hits=[(name,pc-start)for start,length,name in syms if start<=pc<start+length]
        if sig and not hits:raise ValueError('unresolved actual trap site')
        rows.append([i,sig,code,list(hits[0])if hits else None,csr,cw,sw,err])
    if len(rows)!=22 or sum(x[1]!=0 for x in rows)!=7:raise ValueError('wrong case/signal count')
    return rows

def compare(roots):
    first=roots[0];raw=(first/'observations.bin').read_bytes();sites=json.loads((first/'sites.json').read_text())
    for root in roots[1:]:
        if (root/'observations.bin').read_bytes()!=raw or json.loads((root/'sites.json').read_text())!=sites:
            raise ValueError('public-render state/output/FP or normalized signal difference: '+root.name)
    print(json.dumps({'status':'PASS newly reproduced arm comparison','arms':len(roots),'bytes_per_arm':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'actual_signals_per_arm':sum(x[1]!=0 for x in sites),'timing':False},indent=2))

def build(args):
    if platform.system()!='Linux' or platform.machine() not in ('x86_64','amd64'):raise ValueError('this probe requires qualified Linux x86-64 signal facilities')
    if args.reverse and args.arm!='V':raise ValueError('reverse control is V-only')
    src=args.source_root.resolve();out=args.output.resolve();p=json.loads((PACKAGE/'provenance.json').read_text())
    for name,wanted in p['source_fingerprints'][args.arm].items():
        path=src/name
        if not path.is_file()or sha(path)!=wanted:raise ValueError('source fingerprint mismatch: '+name)
    out.mkdir(parents=True,exist_ok=False)
    commands=[]
    def run(label,cmd):
        with (out/(label+'.log')).open('xb')as log:
            r=subprocess.run(list(map(str,cmd)),stdout=log,stderr=subprocess.STDOUT,timeout=105,preexec_fn=limits)
        commands.append({'label':label,'exit':r.returncode})
        (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
        if r.returncode:raise ValueError('command failed: '+label)
    names={'B':BASE,'S':SCALAR,'V':VECTOR}[args.arm]
    objects=[]
    for name in names:
        obj=out/(name+'.o');run('build-'+name,[args.cxx,*tu_flags(name),'-I'+str(src/'src'),'-c',src/'src/core'/(name+'.cpp'),'-o',obj]);objects.append(obj)
    archive=out/'libresearch_core.a';run('archive',['ar','rcsD',archive,*objects])
    if args.reverse:
        linked=[out/(name+'.o')for name in ['EngineWscVectorRender','WscNodeKernelSetup','WscNodeKernelAvx2','WscTerminalCohort','EngineWscTerminalStaging','Engine','FactoryPresets','Ig00156Batch']]
    else:linked=[archive]
    exe=out/'public-render-probe'
    run('build-probe',[args.cxx,*FLAGS,'-fno-access-control','-no-pie','-I'+str(PACKAGE),'-I'+str(src/'src'),PACKAGE/'render-traps.cpp',*linked,'-o',exe])
    run('run-probe',[exe,out/'observations.bin',out/'signals.csv'])
    sites=normalize(exe,out/'signals.csv');(out/'sites.json').write_text(json.dumps(sites,indent=2)+'\n')
    if args.arm=='B':
        run('run-AA',[exe,out/'AA.bin',out/'AA.csv'])
        if (out/'AA.bin').read_bytes()!=(out/'observations.bin').read_bytes()or normalize(exe,out/'AA.csv')!=sites:raise ValueError('stock A/A mismatch')
    result={'arm':args.arm,'reverse':args.reverse,'timing':False,'native_plugin_loaded':False,'executable_sha256':sha(exe),'archive_sha256':sha(archive),'observations_sha256':sha(out/'observations.bin'),'bytes':(out/'observations.bin').stat().st_size,'actual_signals':sum(x[1]!=0 for x in sites),'compiler':subprocess.check_output([args.cxx,'--version'],text=True).splitlines()[0]}
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--arm',choices=['B','S','V']);p.add_argument('--source-root',type=Path);p.add_argument('--output',type=Path);p.add_argument('--reverse',action='store_true');p.add_argument('--cxx',default='c++');p.add_argument('--compare',nargs='+',type=Path)
    a=p.parse_args()
    try:
        if a.compare:
            if len(a.compare)<2:p.error('--compare requires at least two isolated result directories')
            compare(a.compare)
        else:
            if not(a.arm and a.source_root and a.output):p.error('--arm, --source-root and --output are required for explicit build/run')
            build(a)
    except (ValueError,OSError,subprocess.SubprocessError) as e:p.exit(1,'FAIL: '+str(e)+'\n')
