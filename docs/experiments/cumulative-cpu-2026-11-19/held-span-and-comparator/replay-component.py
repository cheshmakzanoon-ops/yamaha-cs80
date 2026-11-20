#!/usr/bin/env python3
"""Fixed guest-work fixture, not a native CPU timer. Requires matching GNU and Callgrind."""
import argparse, json, os, shutil, subprocess
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('repo',type=Path)
parser.add_argument('new_work',type=Path)
parser.add_argument('--cxx',default='g++-14')
parser.add_argument('--valgrind',default='valgrind')
parser.add_argument('--valgrind-includes',type=Path)
parser.add_argument('--extra-debuginfo-path',type=Path)
parser.add_argument('--baseline',default='9208eb6323207b5054055dd2e0549e45324c539a',help='Git source revision for the stock oscillator')
args=parser.parse_args()
repo=args.repo.resolve()
work=args.new_work.resolve()
work.mkdir(parents=True,exist_ok=False)
src=['ReconstructedOscillator.h','WscActiveSegments.inc','EngineWscHeld.cpp','EngineWscHeld.h']
base=args.baseline
stock_core=set(subprocess.check_output(['git','-C',str(repo),'ls-tree','--name-only',base+':src/core'],text=True).splitlines())
here=Path(__file__).resolve().parent
def run(cmd,**kw):
    return subprocess.run(cmd,cwd=work,check=True,**kw)
rows={}
for arm in ['stock','candidate']:
    d=work/arm/'core'
    d.mkdir(parents=True)
    for name in src:
        p=d/name
        if arm=='stock':
            p.write_bytes(subprocess.check_output(['git','-C',str(repo),'show',base+':src/core/'+name]))
        else: shutil.copy(repo/'src/core'/name,p)
    for name in ['WscNextBranchEvent.inc','WscAdvanceSegment.inc']:
        if arm=='stock' and name in stock_core:
            (d/name).write_bytes(subprocess.check_output(['git','-C',str(repo),'show',base+':src/core/'+name]))
        if arm=='candidate' and (repo/'src/core'/name).is_file():
            shutil.copy(repo/'src/core'/name,d/name)
    flags=[args.cxx,'-O3','-DNDEBUG','-std=gnu++20','-fPIC','-DCS80_ENABLE_BORROWED_HELD=1']
    run(flags+['-c',str(d/'EngineWscHeld.cpp'),'-o',arm+'.o'])
    if args.valgrind_includes: flags+=['-I'+str(args.valgrind_includes.resolve())]
    run(flags+['-I'+str(d.parent),str(here/'component-work.cpp'),arm+'.o','-o',arm+'.cost'])
    rows[arm]=[]
    for index,n in enumerate([256,257,256]):
        log=arm+str(index)+'.cg'
        v=[args.valgrind,'--tool=callgrind','--instr-atstart=no','--cache-sim=yes','--branch-sim=no','--quiet','--callgrind-out-file='+log]
        if args.extra_debuginfo_path: v+=['--extra-debuginfo-path='+str(args.extra_debuginfo_path.resolve())]
        native=run(['./'+arm+'.cost',str(n)],capture_output=True,text=True).stdout.strip()
        guest=run(v+['./'+arm+'.cost',str(n)],stdout=subprocess.PIPE,text=True).stdout.strip()
        if native!=guest:raise ValueError('native/guest checksum differs: '+arm)
        cg=(work/(log+'.1')).read_text().splitlines()
        labels=next(l for l in cg if l.startswith('events:')).split()[1:]
        counts=next(l for l in cg if l.startswith('summary:')).split()[1:]
        row={'n':n,'returns':native}
        row.update({k:int(v) for k,v in zip(labels,counts) if k in ['Ir','Dr','Dw']})
        rows[arm].append(row)
    if rows[arm][0]!=rows[arm][2]: raise ValueError('A/A differs: '+arm)
if [r['returns'] for r in rows['stock']] != [r['returns'] for r in rows['candidate']]:
    raise ValueError('A/B output differs')
(work/'work.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(rows,indent=2))
