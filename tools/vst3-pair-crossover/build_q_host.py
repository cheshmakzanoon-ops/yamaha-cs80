#!/usr/bin/env python3
"""Bounded seven-site overlay, compiled against the archived zero-clock source. Never runs DSP."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

HERE=Path(__file__).resolve().parent
BASE_SHA="7066b84e511726d64066e5bbc4e2b684233fe9ad87b695fcf957694926a3723e"
def source():
    s=(HERE/"host.cpp").read_text()
    if hashlib.sha256(s.encode()).hexdigest()!=BASE_SHA: raise ValueError("unknown zero-clock source revision")
    def replace(a,b):
        nonlocal s
        if s.count(a)!=1:raise ValueError("overlay anchor changed")
        s=s.replace(a,b)
    replace("namespace dry {", '#include "q_clocks.h"\nnamespace dry {')
    replace("        rec.before=fp();rec.result=instance.audio->process(data);rec.after=fp();",'''
        rec.before=fp();
        if(qclock::enabled && phase==1){
            require(index==int(qclock::count) && index<576,"out-of-order Q callback");
            auto& x=qclock::real[index];
            const int a=clock_gettime(CLOCK_MONOTONIC,&x.w0);
            const int b=clock_gettime(CLOCK_THREAD_CPUTIME_ID,&x.c0);
            rec.result=instance.audio->process(data);
            const int c=clock_gettime(CLOCK_THREAD_CPUTIME_ID,&x.c1);
            const int d=clock_gettime(CLOCK_MONOTONIC,&x.w1);
            require(a==0&&b==0&&c==0&&d==0,"native Q clock failure");
            ++qclock::count;
        }else rec.result=instance.audio->process(data);
        rec.after=fp();''')
    replace("    Capture cap(total);","    Capture cap(total);qclock::beginScene();")
    replace("            d.snapshot();\n            for(int j=0;j<tail;++j){",
            "            d.snapshot();\n            if(place+1==nplacements)qclock::emptyBrackets();\n            for(int j=0;j<tail;++j){")
    replace("    output(dir,cap,total);","    qclock::finishScene(dir);output(dir,cap,total);")
    replace('    writeControls(dir/"controls.csv",controls());','    qclock::initialize(lane,scope,st);\n    writeControls(dir/"controls.csv",controls());')
    replace(r'cs80-pair-dry-worker-v1\",\"callback_clock_reads\":0,\"lane\":',r'cs80-pair-dry-worker-v1\",\"callback_clock_reads\":"<<(qclock::enabled?5*576*4:0)<<",\"q_empty_clock_reads\":"<<(qclock::enabled?5*128*4:0)<<",\"lane\":')
    return s

def build(sdk,out):
    if out.exists():raise ValueError("fresh output required")
    repo=HERE.parents[1]; sdk=sdk.resolve()
    with tempfile.TemporaryDirectory(prefix="cs80-q-build-") as d:
        cpp=Path(d)/"q_host.cpp";cpp.write_text(source())
        subprocess.run([os.getenv("CXX","c++"),"-std=gnu++20","-O3","-DNDEBUG","-fno-fast-math","-ffp-contract=off",
                        "-I"+str(HERE),"-I"+str(repo/"src"),"-I"+str(repo/"tests"),"-I"+str(sdk),str(cpp),
                        str(sdk/"pluginterfaces/base/coreiids.cpp"),str(sdk/"pluginterfaces/base/funknown.cpp"),
                        str(sdk/"public.sdk/source/vst/vstinitiids.cpp"),"-ldl","-o",str(out)],check=True)
if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("sdk",type=Path);p.add_argument("new_binary",type=Path)
    a=p.parse_args();build(a.sdk,a.new_binary)
