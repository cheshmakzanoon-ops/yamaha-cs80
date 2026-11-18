"""Replay a complete retained core screen from raw CPU totals without running the synth."""
from pathlib import Path
import csv
import hashlib
import json
import math
import random
import re

BASE=Path(__file__).resolve().parent
def require(b,why):
    if not b: raise ValueError(why)
def sha(b):return hashlib.sha256(b).hexdigest()
def perc(x,p):
    k=p*(len(x)-1);j=int(k);return x[j]+(x[min(j+1,len(x)-1)]-x[j])*(k-j)

def replay(root=BASE):
    plan=json.loads((root/"admission.json").read_bytes()); observed=json.loads((root/"result.json").read_bytes())
    qual=json.loads((root/"qualification.json").read_bytes())
    raw=(root/"raw.csv").read_bytes()
    require(sha(raw)==observed["raw_sha256"],"changed observation bytes")
    require(sha((root/"candidate.patch").read_bytes())==qual["patch_sha256"],"changed candidate")
    require(sha((root/"cost.cpp").read_bytes())==qual["driver_sha256"],"changed cost driver")
    cases=plan["case_order"];require(cases==["factory-held","factory-moving","panel-moving","pure-held"],"wrong cases")
    rows=list(csv.DictReader(raw.decode("ascii").splitlines()));schedule=[]
    for fam,n in [("aa",2),("comparison",6)]:
        for block in range(n):
            off=(block+(2 if fam=="comparison" else 0))%4
            for c in cases[off:]+cases[:off]:
                for s,l in enumerate("ABBA"):
                    arm="stock" if fam=="aa" or l=="A" else "candidate"
                    schedule.append((fam,str(block),c,str(s),l,arm))
    require(len(rows)==len(schedule)==128,"incomplete screen")
    data={}; worst_clock={c:0.0 for c in cases}
    for i,(r,slot) in enumerate(zip(rows,schedule)):
        c=r["case"]; observed_fields=[r[k] for k in ["pcm_fnv64","nonzero_audio","summed_active","mxcsr"]]
        require(tuple(r[k] for k in ["family","block","case","slot","label","arm"])==slot,"wrong slot or arm")
        require(r["global_slot"]==str(i) and r["cpu_start"]==r["cpu_end"]==str(qual["cpu"]),"affinity/position mismatch")
        require(observed_fields==qual["cases"][c]["output"] and r["diagnostic_hits"]=="0" and int(r["nonzero_audio"])>0 and int(r["summed_active"])==4096,"signal/control mismatch")
        ns,clock=int(r["render_cpu_ns"]),int(r["empty_cpu_ns"]);require(ns>clock>0,"invalid clock totals")
        data.setdefault((slot[0],int(slot[1]),c),[]).append(ns/(512*64))
        worst_clock[c]=max(worst_clock[c],clock/ns)
    rng=random.Random(plan["bootstrap_seed"]);resamples=[rng.choices(range(6),k=6) for _ in range(10000)]
    def blocks(c,f,n):
        out=[]
        for b in range(n):
            a,bb,bc,d=data[f,b,c]
            out.append((math.log(a)+math.log(d)-math.log(bb)-math.log(bc))/2)
        return out
    g=lambda x:-math.expm1(-x); lower={};stats={}
    for c in cases:
        v=blocks(c,"comparison",6); aa=blocks(c,"aa",2)
        dist=sorted(sum(v[i] for i in ix)/6 for ix in resamples)
        lo,hi=g(perc(dist,.025)),g(perc(dist,.975)); gain=g(sum(v)/6);lower[c]=lo
        stats[c]={"gain":gain,"ci95":[lo,hi],"aa_calibration_max_abs":max(abs(g(x)) for x in aa),"max_clock_fraction":worst_clock[c]}
        for k,value in stats[c].items():
            actual=observed["cases"][c][k];a=value if isinstance(value,list) else [value];b=actual if isinstance(actual,list) else [actual]
            require(len(a)==len(b) and all(math.isclose(x,y,rel_tol=1e-12,abs_tol=1e-13) for x,y in zip(a,b)),"mismatched statistic "+c+"/"+k)
    primary=plan["primary"];floor=max(.02,3*worst_clock[primary],stats[primary]["aa_calibration_max_abs"])
    admitted=lower[primary]>floor and all(lower[c]>=-.03 for c in cases if c!=primary)
    require(math.isclose(floor,observed["primary_floor"],abs_tol=1e-13) and admitted==observed["admit_for_native_matched_plugin_study"],"decision mismatch")
    return {"cases":stats,"completed":128,"floor":floor,"native_study_admitted":admitted}

if __name__=="__main__":
    print(json.dumps(replay(),indent=2))
