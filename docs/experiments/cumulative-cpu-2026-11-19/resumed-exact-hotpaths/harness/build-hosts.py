#!/usr/bin/env python3
"""Build the 16/32/64 matched VST3 client-request work drivers."""
from pathlib import Path
import argparse, hashlib, os, shutil, subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('checkout',type=Path);p.add_argument('sdk',type=Path);p.add_argument('vgincludes',type=Path);p.add_argument('out',type=Path);p.add_argument('--frames',nargs='+',type=int,choices=[16,32,64],default=[16,32,64]);p.add_argument('--align-musical-window',action='store_true',help='Match 2048 warm and 256 counted stereo frames at all buffer lengths.');a=p.parse_args()
root=a.checkout.resolve();sdk=a.sdk.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True);cxx=os.environ.get('CXX','c++');fixture=root/'docs/experiments/heterogeneous-source-cutoff/native-short/guest-host.cpp';factory=root/'docs/experiments/eight-stage-filter/native-cost/host';
def hashfile(p):return hashlib.sha256(p.read_bytes()).hexdigest()
if hashfile(fixture)!='4d2df17fd14fffc0466f7fa7e2ca20f4d1d162abf8efc94c11774921e403100a':raise ValueError('changed historical client fixture')
def rep(s,key,val):
    if s.count(key)!=1:raise ValueError('missing/ambiguous exact edit: '+key)
    return s.replace(key,val)
for n in a.frames:
    if n==64:host=fixture;includes=factory
    else:
        srcdir=out/f'generated{n}';srcdir.mkdir();includes=srcdir
        shutil.copyfile(factory/'factory8_base.cpp',srcdir/'factory8_base.cpp');scaf=(factory/'sdk_scaffold.cpp').read_text();scaf=rep(scaf,'{ testProcessMode, kSample32, 64, testSampleRate };','{ testProcessMode, kSample32, '+str(n)+', testSampleRate };');(srcdir/'sdk_scaffold.cpp').write_text(scaf)
        host=srcdir/'guest-host.cpp';s=fixture.read_text()
        for key,val in [('data.numSamples=64;','data.numSamples='+str(n)+';'),('ctx.projectTimeSamples=int64(b)*64;','ctx.projectTimeSamples=int64(b)*'+str(n)+';'),('for(int k=0;k<64;++k)for(float s:','for(int k=0;k<'+str(n)+';++k)for(float s:')]:s=rep(s,key,val)
        if a.align_musical_window:
            prefix=256//n;warm=2048//n;s=rep(s,'constexpr unsigned warm=32,w=5,total=warm+w+1;',f'constexpr unsigned warm={warm},w={prefix+1},total=warm+w+1;');s=rep(s,'?b<warm+4:b==warm+4',f'?b<warm+{prefix}:b==warm+{prefix}');s=rep(s,'?4u:kind=="all5"?5u:1u',f'?{prefix}u:kind=="all5"?{prefix+1}u:1u');
        host.write_text(s)
    target=out/f'guest-host-{n}';cmd=[cxx,'-O3','-DNDEBUG','-std=gnu++20','-I'+str(a.vgincludes.resolve()),'-I'+str(root/'src'),'-I'+str(root/'tests'),'-I'+str(includes),'-I'+str(sdk),str(host),*(str(sdk/k) for k in ('pluginterfaces/base/coreiids.cpp','pluginterfaces/base/funknown.cpp','public.sdk/source/vst/vstinitiids.cpp')),'-ldl','-o',str(target)];subprocess.run(cmd,check=True); print(n,hashfile(target))
