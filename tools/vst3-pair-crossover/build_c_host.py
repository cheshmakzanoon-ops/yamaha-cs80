#!/usr/bin/env python3
"""Build the separately authorized exact FIR/stock worker; never execute native DSP."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import build_q_host as qb

HERE=Path(__file__).resolve().parent
def source():
    src=qb.source()
    key='#include "q_clocks.h"'
    if src.count(key)!=1:raise ValueError("Q clock adapter changed")
    return src.replace(key,'#include "c_identity.h"')
def build(sdk,out):
    if out.exists():raise ValueError("fresh output required")
    repo=HERE.parents[1];sdk=sdk.resolve()
    with tempfile.TemporaryDirectory(prefix="cs80-c-stage-") as t:
        cpp=Path(t)/"c_worker.cpp";cpp.write_text(source())
        subprocess.run([os.environ.get("CXX","c++"),"-std=gnu++20","-O3","-DNDEBUG","-fno-fast-math","-ffp-contract=off",
            "-I"+str(HERE),"-I"+str(repo/"src"),"-I"+str(repo/"tests"),"-I"+str(sdk),str(cpp),
            str(sdk/"pluginterfaces/base/coreiids.cpp"),str(sdk/"pluginterfaces/base/funknown.cpp"),
            str(sdk/"public.sdk/source/vst/vstinitiids.cpp"),"-ldl","-o",str(out)],check=True)
if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("sdk",type=Path);p.add_argument("new_binary",type=Path)
    a=p.parse_args();build(a.sdk,a.new_binary)
