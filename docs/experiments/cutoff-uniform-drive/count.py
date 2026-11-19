#!/usr/bin/env python3
"""Predeclared four-scene standalone render instruction/count screen."""
from concurrent.futures import ThreadPoolExecutor
import hashlib, json, os, pathlib, subprocess
BASE=pathlib.Path.cwd()
TOOL=pathlib.Path(os.environ["VALGRIND_USR"]).resolve(strict=True)
OUT=BASE/"cutoff-count-v2"
OUT.mkdir() # No reuse or omission
SCENES={0:"cold never played", 2:"settled uniform velocity release", 4:"held varied velocities",5:"settled varied velocity release"}
ARMS={"A":("stock",0), "A2":("stock",0), "positive":("stock",1), "suffix":("stock",2), "C":("candidate",0)}

def run(job):
    scene, role=job
    arm,block=ARMS[role]; p=OUT/f"scene-{scene}-{role}"
    pcm=p.with_suffix(".pcm"); count=p.with_suffix(".cg")
    argv=[str(TOOL/"bin/valgrind"),"--tool=callgrind","--vgdb=no",
          "--cache-sim=yes","--LL=8388608,16,64","--instr-atstart=no","--collect-atstart=no",
          f"--callgrind-out-file={count}",str(BASE/f"callgrind-{arm}-v2"),str(pcm),str(scene),str(block)]
    with p.with_suffix(".log").open("xb") as stream:
        r=subprocess.run(argv,env=os.environ|{"VALGRIND_LIB":str(TOOL/"libexec/valgrind")},
                         stdout=stream,stderr=stream,timeout=150,check=True)
    lines=pathlib.Path(str(count)+".1").read_text().splitlines()
    header=next(s.split()[1:] for s in lines if s.startswith("events:"))
    totals=next(tuple(map(int,s.split()[1:])) for s in lines if s.startswith("totals:"))
    assert header[:3]==["Ir","Dr","Dw"]
    assert min(totals[:3])>0
    return scene,role,{"counts":list(totals[:3]),"pcm_and_state_sha256":hashlib.sha256(pcm.read_bytes()).hexdigest()}

def main():
    validated_all={}
    with ThreadPoolExecutor(4) as pool:
        for scene,role,row in pool.map(run,[(s,a) for s in SCENES for a in ARMS]):
            validated_all.setdefault(scene,{})[role]=row
            print(scene,role,row["counts"],flush=True)
    result={}
    for scene,items in validated_all.items():
        number=lambda k:items[k]["counts"]
        placebo=number("A")==number("A2")
        positive=number("positive")==[a+s for a,s in zip(number("A"),number("suffix"))]
        exact=len({r["pcm_and_state_sha256"] for r in items.values()})==1
        delta=[100*(c/a-1) for a,c in zip(number("A"),number("C"))]
        result[SCENES[scene]]={"rows":items,"placebo_exact":placebo,
             "positive_exact":positive,"trace_exact":exact,"candidate_change_percent":delta}
    (OUT/"result.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result,indent=2))
    if not all(a["placebo_exact"] and a["positive_exact"] and a["trace_exact"] for a in result.values()):
        raise SystemExit("At least one count/trace control failed; no admitted comparison.")
if __name__=="__main__":
    main()
