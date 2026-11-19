#!/usr/bin/env python3
"""Exact-cutoff/FIR crossover utilities. 'dry' below cannot set the C timing environment."""
import argparse
import csv
import gzip
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import time

import crossover as d
import preflight as dry
import phase_q as q

HERE=Path(__file__).resolve().parent
Q_SEAL="48e3499addacc08e363f43c75e5888ceee98b52c76978f450191e7f4994dd8ae"
FIR="74f2507db4a7042a23a96b9a8c2249cc2f76d1f7f417ebb4838498d7f0578895"
WALL=9000;CPU=4800; NS=q.NS
need=q.need
def q_gate(directory):
    """Reproduce every original raw slot and gate; a bare truth flag cannot authorize the new study."""
    directory=directory.resolve(strict=True);seal=directory/"seal.json";root=directory/"Q-run"
    m=q.read(seal);term=q.read(root/"terminal.json")
    need(dry.sha(seal)==Q_SEAL and m["role"]=="Q" and m["sources"]==q.hashes(),"wrong/changed Q seal or source")
    need(term.get("role")=="Q" and term.get("seal_sha256")==Q_SEAL and term.get("complete") is True and
         term.get("numerical_gates_pass") is True and term.get("completed_slots")==48 and
         term.get("candidate_runs")==0 and term.get("retained_rows")==276480,"Q did not fully qualify")
    with (root/"journal.jsonl").open() as f:log=[json.loads(l) for l in f]
    need(len(log)==97 and log[0]["event"]=="start" and log[0]["seal"]==Q_SEAL,"invalid Q journal")
    start=log[0]["t"];cells=[]
    for i,slot in enumerate(m["schedule"]["slots"]):
        s=root/f"slot-{i:02d}";verification=q.read(s/"verified.json");ee=verification["external"]
        a,b=log[1+i*2:3+i*2]
        need((a["event"],b["event"],a["slot"],b["slot"],a["epoch"],a["cell"])==
            ("attempt","verified",i,i,slot["epoch"],slot["cpu_cell"]) and
            b["normalized_sha256"]==dry.sha(s/"normalized.json.gz"),"Q slot transaction/hash mismatch")
        need(all([v["dev"],v["ino"],v["size"]]==m["stock_identity"][:3] for v in verification["actual_worker_identities"])
             and len(verification["actual_worker_identities"])==2,"Q nonidentical physical validation")
        part=[q.scene_clocks(s,k,slot["epoch"],slot["cpu_cell"],start,ee) for k in slot["scene_order"]]
        q.verify_scene_order(part)
        need(part==json.loads(gzip.decompress((s/"normalized.json.gz").read_bytes())),"Q raw replay differs")
        cells.extend(part)
    full={"role":"Q","a_sha256":d.SHIPPING_CUTOFF,"b_sha256":d.SHIPPING_CUTOFF,"cells":cells}
    need(full==json.loads(gzip.decompress((root/"complete-normalized.json.gz").read_bytes())),"Q normalized mismatch")
    gate=d.decide(full);need(gate==q.read(root/"numerical-gates.json") and gate["numerical_gates_pass"] is True,
        "the stock-only Q gates must all pass on raw data")
    return {"seal_sha256":Q_SEAL,"terminal_sha256":dry.sha(root/"terminal.json"),"raw_replay_cells":len(cells),
        "full_normalized_sha256":dry.sha(root/"complete-normalized.json.gz"),"decision_sha256":dry.sha(root/"numerical-gates.json"),
        "gates_passed":sum(gate["gates"].values()),"gate_count":len(gate["gates"])}
def bind(stock,fir):
    ids=[list(dry.identity(stock)),list(dry.identity(fir))]
    need(ids[0][3]==d.SHIPPING_CUTOFF and ids[1][3]==FIR and ids[0][:2]!=ids[1][:2],"only pinned distinct cutoff and FIR ELFs")
    return ids
def mapping(e,c,pair):
    need(pair in ("S","C"),"unknown live pair")
    a=d.orientation(e,c)["a_lane"]
    return [1 if pair=="C" and lane!=a else 0 for lane in (0,1)]
def kind_order(e,c):
    return ("S","C") if d.orientation(e,c)["sentinel_first"] else ("C","S")
def make_plan(out,e,c):
    out.mkdir();order,plans=dry.schedule("full",e,c);(out/"scenes.txt").write_text("\n".join(order)+"\n")
    for s in order:(out/(s+".csv")).write_text(d.canonical(e,c,s))
    return order,plans
def launch(host,images,ids,folder,plandir,e,c,cpu,pair,authorization,stage_start,stage_ru0,charged_dry_cpu,fixed_disk):
    """A fresh execute + socket pair for each sentinel/treatment. No PATH inherited or shared HOME."""
    indexes=mapping(e,c,pair);ori=d.orientation(e,c)
    sockets=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);procs=[];streams=[];start=q.ns();ru=dry.charge();disk_check=0
    try:
        for lane in (ori["launch_first"],1-ori["launch_first"]):
            k=indexes[lane];path=folder/f"peer-{lane}";path.mkdir();home=path/"home";home.mkdir()
            env={"HOME":str(home),"XDG_CONFIG_HOME":str(home/"config"),"XDG_CACHE_HOME":str(home/"cache"),
                "LANG":"C","LC_ALL":"C","PATH":"/usr/bin:/bin"}
            if authorization is not None:
                need(len(authorization)==64 and all(ch in "0123456789abcdef" for ch in authorization),"bad C seal")
                env.update(CS80_DISTINCT_C_SEAL=authorization,CS80_DISTINCT_C_PAIR=pair,CS80_DISTINCT_C_BASE_LANE=str(ori["a_lane"]),
                           CS80_DISTINCT_C_IMAGE=":".join(map(str,ids[k])))
            stdout=(path/"stdout").open("xb");stderr=(path/"stderr").open("xb");streams.extend((stdout,stderr));fd=sockets[lane].fileno()
            procs.append(subprocess.Popen([str(host),str(images[k]),str(path/"capture"),str(cpu),str(lane),str(fd),str(plandir),"full"],
                env=env,cwd=path,pass_fds=(fd,),start_new_session=True,close_fds=True,stdout=stdout,stderr=stderr))
        for s in sockets:s.close()
        while True:
            statuses=[p.poll() for p in procs];now=q.ns()
            need(all(x in (0,None) for x in statuses),f"worker failed; retained statuses {statuses}")
            need(now-start<=q.BOUND_SLOT*NS and now-stage_start<=WALL*NS,"C pair/whole-stage wall bound exceeded")
            need(charged_dry_cpu+dry.charge()-stage_ru0+q.live_cpu(procs)<=CPU*NS,"C worker CPU bound exceeded")
            if now>=disk_check:
                need(fixed_disk+q.filesz(folder)<=q.BOUND_DISK,"C data cap");disk_check=now+NS
            if all(x==0 for x in statuses):break
            time.sleep(.04)
    finally:
        for s in sockets:s.close()
        for p in procs:
            if p.poll() is None:
                try:os.killpg(p.pid,signal.SIGKILL)
                except ProcessLookupError:pass
        for p in procs:p.wait()
        for f in streams:f.close()
    return {"start":start,"end":q.ns(),"pooled_worker_cpu_ns":dry.charge()-ru,"launch_first":ori["launch_first"],
            "image_indexes_by_lane":indexes,"a_lane":ori["a_lane"],"pair":pair}
def all_equal(folder,gold,scope,order):
    for s in order:
        for f in dry.FILES:need(q.exact(folder/s/f,gold/s/f),"actual byte mismatch: "+scope+"/"+s+"/"+f)
    need(q.exact(folder/"controls.csv",gold/"controls.csv"),"panel control mismatch: "+scope)
def do_dry(host,stock,fir,qdir,out):
    host=host.resolve(strict=True);images=[stock.resolve(strict=True),fir.resolve(strict=True)];out=out.resolve()
    witness=q_gate(qdir);ids=bind(*images);hostid=dry.identity(host);out.mkdir();start=q.ns();ru=dry.charge()
    allowed=os.sched_getaffinity(0);cores,sup=d.cpu_ids(allowed);e=c=0;cpu=cores[c]
    plan=out/"plan";order,plans=make_plan(plan,e,c);statuses={};original=q.read(HERE/"zero-preflight/receipt.json")
    base={"schema":"cs80-C-FIR-only-full-dry-v1","complete":False,"callback_clock_reads":0,"empty_clock_reads":0,
        "cpu":cpu,"supervisor_cpu":sup,"Q_gate":witness,"stock_sha256":ids[0][3],"fir_sha256":ids[1][3],
        "host_sha256":hostid[3],"build_ids":[dry.build_id(p) for p in (host,*images)],"started_at_monotonic_ns":start,
        "whole_resources":statuses,"placement_scope":"16 independent wake placements; full five scenes"}
    try:
        os.sched_setaffinity(0,{sup})
        for k,p in enumerate(images):
            dst=out/("base" if k==0 else "fir");dst.mkdir()
            statuses["isolated_"+str(k)]=dry.launch(False,host,p,dst,plan,"full",cpu,0)
            got,n=dry.verify(dst,"isolated",-1,tuple(ids[k]),cpu,"full",order,plans)
            need(got==original["public_capture_per_worker"] and n==original["visits_per_worker"],"isolated stock/FIR differs from historical gold")
        gold=out/"base/isolated/capture"
        all_equal(out/"fir/isolated/capture",gold,"isolated-FIR",order)
        for pair in kind_order(e,c):
            dest=out/pair;dest.mkdir()
            statuses["dry_"+pair]=launch(host,images,ids,dest,plan,e,c,cpu,pair,None,start,ru,0,q.filesz(out))
            for lane,k in enumerate(mapping(e,c,pair)):
                got,n=dry.verify(dest,f"peer-{lane}",lane,tuple(ids[k]),cpu,"full",order,plans)
                need(got==original["public_capture_per_worker"] and n==original["visits_per_worker"],"dry pair differed from gold")
                all_equal(dest/f"peer-{lane}"/"capture",gold,pair+str(lane),order)
        need(bind(*images)==ids and dry.identity(host)==hostid,"native file changed during C dry")
        base.update({"complete":True,"reference_sha256s":original["public_capture_per_worker"],"controls_sha256":dry.sha(gold/"controls.csv"),
            "pooled_dry_worker_cpu_ns":sum(v["pooled_worker_cpu_ns"] for v in statuses.values()),
            "elapsed_dry_ns":q.ns()-start,"bytes_retained":q.filesz(out),"visits_per_worker":1840})
        need(base["bytes_retained"]<=q.BOUND_DISK,"dry data cap")
    except BaseException as exc:
        base["error"]=f"{type(exc).__name__}: {exc}";raise
    finally:
        os.sched_setaffinity(0,allowed);dry.save(out/"dry.json",base)
    print("C DRY PASS: isolated baseline/FIR plus fresh S and C peers; all five scenes canonical, no callback clocks")
if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    for k in ("host","stock","fir","q-stage","out"):p.add_argument("--"+k,type=Path,required=True)
    a=p.parse_args()
    try:do_dry(a.host,a.stock,a.fir,a.q_stage,a.out)
    except (ValueError,KeyError,OSError,TypeError,subprocess.SubprocessError) as exc:sys.exit(str(exc))
