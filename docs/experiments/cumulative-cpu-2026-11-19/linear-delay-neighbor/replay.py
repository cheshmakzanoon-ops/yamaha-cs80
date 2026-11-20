#!/usr/bin/env python3
"""Run isolated effect and trap A/B probes on Linux x86-64 (GCC). Writes JSON to stdout."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import statistics
import subprocess
import tempfile

def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stock", type=Path, help="stock checkout (contains src/core/Dsp.h)")
    parser.add_argument("candidate", type=Path, help="candidate checkout")
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--cpu", type=int, help="pin to one allowed logical CPU for optional timings")
    parser.add_argument("--timing", action="store_true", help="add descriptive time trials, not an acceptance criterion")
    args = parser.parse_args()
    if args.cpu is not None:
        os.sched_setaffinity(0, {args.cpu})
    src = Path(__file__).resolve().parent
    flags = ["-O3", "-DNDEBUG", "-std=c++20", "-fPIC"]
    root = [args.stock.resolve(), args.candidate.resolve()]
    result = {"compiler":subprocess.check_output([args.cxx, "--version"], text=True).splitlines()[0],
              "compile_flags":flags, "affinity":sorted(os.sched_getaffinity(0)),
              "dsp_header_sha256":[digest((r/"src/core/Dsp.h").read_bytes()) for r in root]}
    with tempfile.TemporaryDirectory(prefix="cs80-delay-wrap-") as name:
        work=Path(name)
        for i, tree in enumerate(root):
            for target in ("effect","traps"):
                subprocess.run([args.cxx, *flags, "-I"+str(tree/"src"), str(src/(target+".cpp")),
                                "-o", str(work/f"{target}{i}")], check=True)
        faults=[subprocess.run([str(work/f"traps{i}")],capture_output=True,check=True) for i in range(2)]
        if faults[0].stdout != faults[1].stdout or faults[0].stderr != faults[1].stderr:
            raise SystemExit("trap records differ")
        result["traps"]={"per_arm":faults[0].stderr.decode().strip(),"record_bytes":len(faults[0].stdout),
                         "sha256":digest(faults[0].stdout)}
        rows=[]
        for effect in ("plate", "chorus"):
            for rate in (16000,88200,96000,384000,1536000):
                for mxcsr in ("1f80","9fc0","3f80","5f80","7f80"):
                    raw=[subprocess.check_output([str(work/f"effect{i}"),effect,"131072",str(rate),"raw",mxcsr])
                         for i in range(2)]
                    if raw[0] != raw[1]:
                        at=next((j for j,(a,b) in enumerate(zip(*raw)) if a!=b),min(map(len,raw)))
                        raise SystemExit(f"{effect} {rate} {mxcsr}: first different byte {at}")
                    rows.append([effect,rate,mxcsr,len(raw[0]),digest(raw[0])])
        result["capture_columns"]=["effect","rate_hz","mxcsr_control","bytes","sha256"]
        result["exact_histories"]=rows
        result["total_effect_bytes_per_arm"]=sum(row[3] for row in rows)
        if args.timing:
            if args.cpu is None:
                raise SystemExit("--timing requires an explicit --cpu from the available affinity")
            rng=random.Random(0x80119)
            measurements=[]
            for effect,n in (("plate",2097152),("chorus",4194304)):
                for rate in (88200,96000,384000):
                    pairs=[]; hashes=set()
                    # Four AB and four BA blocks in repeatable, shuffled order.
                    orders=[(0,1),(1,0)]*4; rng.shuffle(orders)
                    for order in orders:
                        times=[None,None]
                        for idx in order:
                            out=subprocess.check_output([str(work/f"effect{idx}"), effect, str(n), str(rate),
                                                         "time", "9fc0"],text=True).split()
                            times[idx]=int(out[0]); hashes.add(out[1])
                        pairs.append(times)
                    if len(hashes)!=1: raise SystemExit(f"{effect} {rate}: timed output differs")
                    ratios=[b/a for a,b in pairs]
                    measurements.append({"effect":effect,"rate_hz":rate,"samples_per_trial":n,
                                         "order":orders,"paired_thread_cpu_ns_stock_candidate":pairs,
                                         "median_paired_change_percent":round((statistics.median(ratios)-1)*100,3),
                                         "range_paired_change_percent":[round((x-1)*100,3) for x in (min(ratios),max(ratios))],
                                         "output_fnv1a_word_hash":hashes.pop()})
            result["isolated_timings"]=measurements
    print(json.dumps(result,indent=2))

if __name__ == "__main__":
    main()
