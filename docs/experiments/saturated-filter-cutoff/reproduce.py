"""Rebuild and inspect the recorded SIGFPE witness without changing the checkout."""
import hashlib
import os
import platform
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

HERE=Path(__file__).resolve().parent
BASE="e9e8175f4278d2457279fcb8809cbba1ddb7be2b"
STOCK="140f703f69e856b9c117405ca5d6fff58b756f6acc54c39a1a0417fee8c17a05"
CANDIDATE="39733b08f96f3730ba054b00fb371b993f3fe8166d5bb848235b95dfa2a756f8"
def require(b,msg):
    if not b: raise SystemExit(msg)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    import sys
    repo=Path(sys.argv[1]).resolve() if len(sys.argv)==2 else Path(subprocess.check_output(
        ["git","rev-parse","--show-toplevel"],cwd=HERE,text=True).strip())
    require(platform.system()=="Linux" and platform.machine()=="x86_64","GNU/Linux x86-64 witness only")
    require(sha(repo/"src/core/Ig00156Filter.h")==STOCK,"The on-disk filter must match "+BASE)
    with tempfile.TemporaryDirectory() as tmp:
        t=Path(tmp); dirs={}
        for arm in ["stock","candidate"]:
            r=t/arm;dirs[arm]=r
            shutil.copytree(repo/"src",r/"src")
            if arm=="candidate":
                subprocess.run(["git","apply","--whitespace=nowarn",str(HERE/"candidate.patch")],cwd=r,check=True)
                require(sha(r/"src/core/Ig00156Filter.h")==CANDIDATE,"candidate header drift")
            exe=r/"trap-test"
            subprocess.run(["c++","-O3","-DNDEBUG","-std=gnu++20","-fPIC","-Wall","-Wextra","-Wpedantic",
                            "-I"+str(r/"src"),str(HERE/"traps.cpp"),"-o",str(exe)],check=True,timeout=90)
            subprocess.run([str(exe),str(r/"obs.bin"),str(r/"events.csv"),str(r/"signals.csv")],check=True,timeout=20)
        a,b=[(r/"obs.bin").read_bytes() for r in dirs.values()]; require(len(a)==len(b),"unequal traces")
        rows=[next((i for i in range(len(a)//384) if a[i*384:i*384+384]!=b[i*384:i*384+384]),None)]
        require(rows[0]==1,"this compiler did not reproduce the recorded first divergence")
        i=rows[0];xx,yy=[struct.unpack_from("<48Q",z,i*384) for z in (a,b)]
        diffs=[j for j,(x,y) in enumerate(zip(xx,yy)) if x!=y]
        require(diffs==[36] and xx[36]==0x47c35080 and yy[36]==0x447a0000,"different first divergence")
        print(f"Confirmed raw cutoff after trap: stock=100001 (0x{xx[36]:08x}), candidate=1000 (0x{yy[36]:08x}).")
if __name__=="__main__":main()
