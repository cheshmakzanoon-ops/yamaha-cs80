#!/usr/bin/env python3
"""Exact-source supported-MTS envelope positive, sanitizer and guard deletions."""
import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
@contextmanager
def locked(path):
    with path.open("a") as handle:
        fcntl.flock(handle,fcntl.LOCK_EX)
        try:yield
        finally:fcntl.flock(handle,fcntl.LOCK_UN)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ("source","out","lock"):p.add_argument("--"+name,type=Path,required=True)
    a=p.parse_args()
    header=a.source/"src/core/Tuning.h";test=a.source/"tests/MtsEnvelopeTests.cpp"
    cases=[
      ("seven-bit", "    for (int i = 1; i < size - 1; ++i)\n        if ((data[i] & 0x80) != 0) return false;\n", "", 1036,
       ["bulk01 high-bit byte5 rejected","bank07-nonreal structurally complete high-bit count255 rejected"]),
      ("bulk-length", "size != 6 + 16 + 128 * 3 + 2", "size < 6 + 16 + 128 * 3 + 2", 8,
       ["bulk01 excess payload1 rejected"]),
      ("counted-length", "size != header + data[header - 1] * 4 + 1", "size < header + data[header - 1] * 4 + 1", 24,
       ["single02 excess payload1 rejected","bank07-nonreal excess payload4 rejected"]),
      ("octave-length", "size != 8 + (sub == 0x08 ? 12 : 24) + 1", "size < 8 + (sub == 0x08 ? 12 : 24) + 1", 32,
       ["octave09-nonreal excess payload4 rejected"]),
      ("reserved-mask", "        if ((data[5] & 0x7C) != 0) return false; // reserved channel/options bits\n", "", 40,
       ["octave08-real reserved mask bit2 rejected","octave09-nonreal reserved mask bit6 preserves all128 pitches/mapped bits"]),
    ]
    with locked(a.lock):
        assert not subprocess.check_output(["git","status","--porcelain"],cwd=a.source)
        for path in (header,test):
            assert path.read_bytes()==subprocess.check_output(["git","show","HEAD:"+str(path.relative_to(a.source))],cwd=a.source)
        source=header.read_text();sourceHash=sha(header);testHash=sha(test)
        a.out.mkdir(parents=True,exist_ok=True)
        contract={"head":subprocess.check_output(["git","rev-parse","HEAD"],cwd=a.source,text=True).strip(),
          "source_tree":subprocess.check_output(["git","rev-parse","HEAD:src"],cwd=a.source,text=True).strip(),
          "header_sha256":sourceHash,"test_sha256":testHash,"checks":2426,
          "compiler":subprocess.check_output(["g++","--version"],text=True).splitlines()[0],
          "mutants":[{"name":n,"old":o,"new":v,"ordinary_failures":f,"required_assertions":q} for n,o,v,f,q in cases]}
        (a.out/"contract.json").write_text(json.dumps(contract,indent=2)+"\n")
    def run(name,include,flags,expected,required):
        assert sha(header)==sourceHash and sha(test)==testHash
        out=a.out/name;out.mkdir(parents=True,exist_ok=True)
        binary=out/"test"
        command=["g++","-std=c++20","-Wall","-Wextra"]+flags+["-I"+str(include),"-I"+str(a.source/"src"),str(test),"-o",str(binary)]
        started=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())
        with (out/"compile.log").open("w") as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
        env=dict(os.environ,ASAN_OPTIONS="detect_leaks=1:halt_on_error=1",UBSAN_OPTIONS="halt_on_error=1")
        result=subprocess.run([str(binary)],capture_output=True,text=True,env=env,timeout=30)
        log=result.stdout+result.stderr;(out/"test.log").write_text(log)
        marker=re.search(r"(\d+) MTS envelope checks, (\d+) failures",log)
        observed=[s for s in log.splitlines() if s.startswith("FAIL: ")]
        valid=result.returncode==(1 if expected else 0) and marker is not None and int(marker[1])==2426 and int(marker[2])==expected
        valid=valid and len(observed)==expected and all("FAIL: "+item in log for item in required)
        valid=valid and "runtime error:" not in log and "ERROR: AddressSanitizer" not in log and "DEADLYSIGNAL" not in log
        record={"name":name,"command":command,"binary_sha256":sha(binary),"exit_code":result.returncode,
          "checks":int(marker[1]) if marker else None,"ordinary_failures":len(observed),
          "started_at":started,"finished_at":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime()),"accepted":valid}
        (out/"receipt.json").write_text(json.dumps(record,indent=2)+"\n")
        print(name,record["checks"],record["ordinary_failures"],"accepted",valid,flush=True)
        if not valid:raise RuntimeError("unqualified positive/mutant: "+name)
        return record
    results=[]
    with locked(a.lock):results.append(run("positive",a.source/"src",["-O2"],0,[]))
    with locked(a.lock):results.append(run("sanitized",a.source/"src",["-O1","-g","-fsanitize=address,undefined","-fno-omit-frame-pointer"],0,[]))
    for name,old,new,failures,required in cases:
        with locked(a.lock):
            assert source.count(old)==1
            include=a.out/name;target=include/"core/Tuning.h";target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text(source.replace(old,new))
            record=run(name,include,["-O2"],failures,required)
            record["mutant_header_sha256"]=sha(target);results.append(record)
    with locked(a.lock):
        positive=a.out/"positive/test"
        assert sha(positive)==results[0]["binary_sha256"] and sha(header)==sourceHash and sha(test)==testHash
        result=subprocess.run([str(positive)],capture_output=True,text=True,timeout=30)
        (a.out/"restored-positive.log").write_text(result.stdout+result.stderr)
        assert result.returncode==0 and "2426 MTS envelope checks, 0 failures" in result.stdout
        (a.out/"summary.json").write_text(json.dumps(dict(contract,results=results,restored_positive_sha256=sha(positive)),indent=2)+"\n")
        print("Original/restored2426 checks, sanitizer2426 checks and all5 actual guards qualified.",flush=True)
if __name__=="__main__":main()
