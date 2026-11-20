#!/usr/bin/env python3
"""Run the existing real oscillator and renderer interruptions against two matching unstripped Linux VST3 ELFs."""
import argparse, hashlib, subprocess
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('repo',type=Path)
parser.add_argument('stock_so',type=Path)
parser.add_argument('candidate_so',type=Path)
parser.add_argument('new_work',type=Path)
parser.add_argument('--cxx',default='g++-14')
a=parser.parse_args()
repo,old,new,w=(p.resolve() for p in [a.repo,a.stock_so,a.candidate_so,a.new_work])
w.mkdir(parents=True,exist_ok=False)
exp=repo/'docs/experiments'
cc=[a.cxx,'-O3','-DNDEBUG','-std=gnu++20','-I'+str(repo/'src')]
def build(src,extra,exe):
    subprocess.run([*cc,str(src),*extra,'-o',str(w/exe)],check=True)
def run(args,log):
    proc=subprocess.run(args,cwd=w,text=True,capture_output=True,check=True)
    (w/log).write_text(proc.stdout+proc.stderr)
    print(proc.stdout.splitlines()[-1])
for name in ['output','elapsed']:
    exe='trap-'+name
    build(exp/'oscillator-endpoint-copy'/(name+'_trap.cpp'),['-ldl'],exe)
    for tag,module in [('aa',old),('ab',new)]:run([w/exe,old,module],tag+'-'+name+'.log')
for tag,module in [('stock',old),('candidate',new)]:
    exe=tag+'-render'
    build(exp/'wsc-terminal-staging/render-traps.cpp',['-fno-access-control',str(module)],exe)
    run([w/exe,tag+'.bin',tag+'.csv'],tag+'.log')
buf=(w/'stock.bin').read_bytes()
if buf!=(w/'candidate.bin').read_bytes():raise RuntimeError('renderer interrupted/continuation records differ')
def meta(path):
    return [v[:3]+v[4:] for l in path.read_text().splitlines() if (v:=l.split(','))]
if meta(w/'stock.csv')!=meta(w/'candidate.csv'):raise RuntimeError('trap metadata differs beyond code addresses')
print(len(buf),hashlib.sha256(buf).hexdigest())
