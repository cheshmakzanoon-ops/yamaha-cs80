#!/usr/bin/env python3
"""Single separately sealed exact FIR-only C stage with a fresh live stock sentinel in every cell."""
import argparse
import csv
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import candidate_pair as cp
import crossover as d
import phase_q as q
import preflight as dry

HERE=Path(__file__).resolve().parent
FROZEN=("build_c_host.py","c_identity.h","candidate_pair.py","phase_c.py")
SCHEMA="cs80-FIR-only-C-stage-seal-v1"
need=q.need
def source_hashes():
    return {**q.hashes(),**{f:dry.sha(HERE/f) for f in FROZEN}}
def verify_dry(host,images,drydir,prior,qw):
    m=q.read(drydir/"dry.json");ids=cp.bind(*images)
    need(m["schema"]=="cs80-C-FIR-only-full-dry-v1" and m["complete"] is True and m["callback_clock_reads"]==m["empty_clock_reads"]==0
        and m["host_sha256"]==dry.sha(host) and m["stock_sha256"]==ids[0][3] and m["fir_sha256"]==ids[1][3]
        and m["Q_gate"]==qw and m["build_ids"]==[dry.build_id(p) for p in (host,*images)],"exact complete C dry required")
    need(set(m["whole_resources"])=={"isolated_0","isolated_1","dry_S","dry_C"} and m["visits_per_worker"]==1840,
        "missing sentinel, candidate, or isolated native rehearsals")
    ref=drydir/"base/isolated/capture";old=q.read(HERE/"zero-preflight/receipt.json");order,plans=dry.schedule("full",0,0)
    need(m["reference_sha256s"]==old["public_capture_per_worker"] and m["controls_sha256"]==old["controls_sha256"]
        and dry.sha(ref/"controls.csv")==m["controls_sha256"],"C reference differs from Q golden")
    # Bind the *actual* zero-clock public artifacts and all lane/image identities, not just a summary boolean.
    for tag,i in (("base",0),("fir",1)):
        v,n=dry.verify(drydir/tag,"isolated",-1,tuple(ids[i]),m["cpu"],"full",order,plans)
        need(v==m["reference_sha256s"] and n==1840,"isolated C parity receipt mismatch")
        cp.all_equal(drydir/tag/"isolated/capture",ref,tag,order)
    for k in ("S","C"):
        for lane,im in enumerate(cp.mapping(0,0,k)):
            v,n=dry.verify(drydir/k,f"peer-{lane}",lane,tuple(ids[im]),m["cpu"],"full",order,plans)
            need(v==m["reference_sha256s"] and n==1840,"paired C dry parity mismatch")
            cp.all_equal(drydir/k/f"peer-{lane}"/"capture",ref,k+str(lane),order)
    all_receipts=[m];files=[drydir]
    for folder in prior:
        v=q.read(folder/"dry.json")
        need(v["schema"]==m["schema"] and v["complete"] is False and v["callback_clock_reads"]==v["empty_clock_reads"]==0
             and v["stock_sha256"]==m["stock_sha256"] and v["fir_sha256"]==m["fir_sha256"] and v["host_sha256"]==m["host_sha256"],
             "invalid earlier dry-attempt charge")
        all_receipts.append(v);files.append(folder)
    need(len(set(files))==len(files),"duplicate dry charge")
    return m,{"dry_cpu_ns":sum(x["pooled_worker_cpu_ns"] for v in all_receipts for x in v["whole_resources"].values()),
        "first_dry_start":min(v["started_at_monotonic_ns"] for v in all_receipts),
        "dry_directories_bytes":sum(q.filesz(p) for p in files),"dry_receipt_hashes":[dry.sha(p/"dry.json") for p in files],
        "earlier_rehearsals":len(prior)}
def seal(host,stock,fir,qd,gd,prior,out):
    host=host.resolve(strict=True);images=[stock.resolve(strict=True),fir.resolve(strict=True)]
    qd=qd.resolve(strict=True);gd=gd.resolve(strict=True);prior=[p.resolve(strict=True) for p in prior];out=out.resolve()
    qw=cp.q_gate(qd);m,charges=verify_dry(host,images,gd,prior,qw)
    ids=cp.bind(*images);allowed=sorted(os.sched_getaffinity(0));cores,sup=d.cpu_ids(allowed)
    Q=q.read(qd/"seal.json");need(allowed==Q["allowed_cpus"] and cores==Q["four_cpus"] and sup==Q["supervisor_cpu"],"C requires the same four Q vCPUs")
    need(dry.sha(qd/"seal.json")==cp.Q_SEAL and m["cpu"]==cores[0] and m["supervisor_cpu"]==sup,"wrong dry/core binding")
    need(q.ns()-charges["first_dry_start"]<cp.WALL*q.NS and charges["dry_cpu_ns"]<cp.CPU*q.NS,"C admission budget already exhausted")
    out.mkdir()
    obj={"schema":SCHEMA,"scope":"cutoff baseline vs FIR-only on same cutoff; mandatory fresh live S sentinels",
        "Q":qw,"q_dir":str(qd),"dry_dir":str(gd),"prior_dry_dirs":[str(p) for p in prior],
        "sources":source_hashes(),"dry_charges":charges,"host_path":str(host),"host_identity":list(dry.identity(host)),
        "image_paths":[str(p) for p in images],"image_identities":ids,"build_ids":m["build_ids"],
        "allowed_cpus":allowed,"four_cpus":cores,"supervisor_cpu":sup,"schedule":d.generate(),
        "lane_schedule_sha256":Q["lane_schedule_sha256"],"count_by_role":{"S":48,"C":48},
        "stage_budget":{"wall_seconds":cp.WALL,"pooled_worker_CPU_seconds":cp.CPU,"slot_seconds_per_pair":q.BOUND_SLOT,
          "retained_bytes":q.BOUND_DISK,"epoch_start_spacing_seconds":120,"post_epoch_seconds":10,
          "all_new_dry_admissions_charged":True}}
    dry.save(out/"C-seal.json",obj);digest=dry.sha(out/"C-seal.json")
    print("Sealed C (no native timing started):",digest)
    return digest
def inputs(folder,sealhash):
    f=folder/"C-seal.json";need(dry.sha(f)==sealhash,"wrong C seal acknowledgement")
    m=q.read(f);need(m["schema"]==SCHEMA and m["Q"]["seal_sha256"]==cp.Q_SEAL and m["sources"]==source_hashes()
        and m["schedule"]==d.generate() and m["count_by_role"]=={"S":48,"C":48},"wrong stage/sources/schedule")
    host=Path(m["host_path"]);images=list(map(Path,m["image_paths"]));gd=Path(m["dry_dir"]);qd=Path(m["q_dir"])
    need(cp.bind(*images)==m["image_identities"] and list(dry.identity(host))==m["host_identity"],"stage-C identity drift")
    qw=cp.q_gate(qd);need(qw==m["Q"],"Q changes invalidate C")
    base,cost=verify_dry(host,images,gd,list(map(Path,m["prior_dry_dirs"])),qw)
    need(cost==m["dry_charges"] and base["build_ids"]==m["build_ids"],"dry evidence changed")
    allowed=os.sched_getaffinity(0);cores,sup=d.cpu_ids(allowed)
    need(sorted(allowed)==m["allowed_cpus"] and cores==m["four_cpus"] and sup==m["supervisor_cpu"],"C CPU set changed")
    return m,host,images,base,gd/"base/isolated/capture",allowed

def annotate(cell,k):
    need(k in ("S","C") and cell["a_sha256"]==d.SHIPPING_CUTOFF,"invalid cell role")
    cell["role"]=k;cell["b_sha256"]=d.SHIPPING_CUTOFF if k=="S" else cp.FIR
    return cell
def compact(folder,k,slot,m,base,ref,cpu,ext,origin):
    e,c=slot["epoch"],slot["cpu_cell"];order,plans=dry.schedule("full",e,c)
    imap=cp.mapping(e,c,k);ori=d.orientation(e,c)
    need(ext["image_indexes_by_lane"]==imap and ext["a_lane"]==ori["a_lane"] and ext["launch_first"]==ori["launch_first"] and
         ext["pair"]==k,"unexpected pair launch mapping")
    copies=[];native=[];counts={}
    for l,im in enumerate(imap):
        ids=m["image_identities"][im];cap=folder/f"peer-{l}"/"capture";v=q.read(cap/"identity.json")
        need(v["lane"]==l and v["cpu"]==cpu and v["callback_clock_reads"]==11520 and v["q_empty_clock_reads"]==2560 and
             [v["dev"],v["ino"],v["size"]]==ids[:3],"native C/S module/clock verification failed")
        with (cap/"baton.csv").open() as f:
            r=csv.reader(f);need(next(r)==["serial","scene","placement","phase","index","count","first","lane"] and
                                 list(map(tuple,r))==list(dry.visits("full",order,plans,l)),"incorrect C/S socket ledger")
        cp.all_equal(cap,ref,f"{k}/{l}",order)
        copies+=[cap/s/f for s in order for f in dry.FILES]
        native.append({**v,"verified_sha256":ids[3]})
        counts[str(l)]=sum((cap/s/f).stat().st_size for s in order for f in dry.FILES)
    cells=[annotate(q.scene_clocks(folder,s,e,c,origin,ext),k) for s in order];q.verify_scene_order(cells)
    need(sum(r[3]-r[2] for obj in cells for r in obj["clocks"])<=ext["pooled_worker_cpu_ns"]+2_000_000,"callbacks exceed C/S process envelope")
    dry.save(folder/"verified.json",{"epoch":e,"cpu_cell":c,"pair":k,"image_indexes_by_lane":imap,"a_lane":ori["a_lane"],
        "workers":native,"compared_public_bytes":counts,"controls_sha256":base["controls_sha256"],"external":ext})
    for p in copies:p.unlink()  # only verified exact copies; all original timing/failure rows retained
    return cells
def release_summary(S,C):
    live=S["numerical_gates_pass"] is True; treatment=C["numerical_gates_pass"] is True
    return {"live_sentinel_gates_pass":live,"candidate_gates_pass":treatment,"large_idle_win_gates_pass":live and treatment,
        "both_primary_above_10pct_with_all_gates":live and treatment and C["both_primary_lower_bounds_above_10"],
        "broad_idle_win_with_all_gates":live and treatment and C["varied_release_lower_bound_above_5"],
        "scope":"tested cloud-guest callback-thread CPU only; no cross-machine, deadline, power or whole-host claim"}
def assemble(root,m,origin):
    data={k:{"role":k,"a_sha256":d.SHIPPING_CUTOFF,"b_sha256":d.SHIPPING_CUTOFF if k=="S" else cp.FIR,"cells":[]} for k in ("S","C")}
    last_end=0;last_epoch_start=0;last_epoch_end=0;seen=0;rawCPU=0
    log=[json.loads(x) for x in (root/"journal.jsonl").read_text().splitlines()]
    need(len(log)==1+2*96 and log[0]["event"]=="start" and log[0]["timed_stage_origin_ns"]==origin,"C journal")
    for i,slot in enumerate(m["schedule"]["slots"]):
        e,c=slot["epoch"],slot["cpu_cell"]
        for order,k in enumerate(cp.kind_order(e,c)):
            p=root/f"slot-{i:02d}"/f"{order}-{k}";v=q.read(p/"verified.json");ext=v["external"]
            a,b=log[1+seen*2:3+seen*2];seen+=1
            need((a["event"],b["event"],a["slot"],b["slot"],a["pair"],b["pair"])==("attempt","verified",i,i,k,k)
                and b["normalized_sha256"]==dry.sha(p/"normalized.json.gz"),"C/S journal/file hash")
            need(ext["start"]>=last_end and ext["end"]-ext["start"]<=q.BOUND_SLOT*q.NS,"overlap/slot cap")
            if i%4==0 and order==0:
                if i:need(ext["start"]>=last_epoch_start+120*q.NS and ext["start"]>=last_epoch_end+10*q.NS,"epoch spacing")
                # Back-to-back execution here normally takes longer than 120 seconds/epoch.
                # Record actual supervisor epoch origin separately to avoid substituting worker startup IO.
                last_epoch_start=a["epoch_start_ns"]
            last_end=ext["end"];last_epoch_end=last_end
            imap=cp.mapping(e,c,k)
            need(v["pair"]==k and v["image_indexes_by_lane"]==ext["image_indexes_by_lane"]==imap and
                v["a_lane"]==ext["a_lane"]==d.orientation(e,c)["a_lane"] and len(v["workers"])==2,"C/S physical role mapping")
            for l,im in enumerate(imap):
                x=v["workers"][l];ids=m["image_identities"][im]
                need(x["lane"]==l and x["verified_sha256"]==ids[3] and [x["dev"],x["ino"],x["size"]]==ids[:3],
                     "wrong actual native S/C image in replay")
            raw=[annotate(q.scene_clocks(p,s,e,c,origin,ext),k) for s in slot["scene_order"]];q.verify_scene_order(raw)
            need(raw==json.loads(gzip.decompress((p/"normalized.json.gz").read_bytes())),"final native C/S raw replay mismatch")
            data[k]["cells"].extend(raw);rawCPU+=ext["pooled_worker_cpu_ns"]
    return data, rawCPU
def run(folder,sealhash):
    folder=folder.resolve();m,host,images,base,ref,allowed=inputs(folder,sealhash)
    root=folder/"C-run";root.mkdir() # one-shot permanent attempt; never resume or overwrite
    t={"schema":"cs80-FIR-only-C-terminal-v1","role":"C-with-live-S","seal_sha256":sealhash,"Q_seal_sha256":cp.Q_SEAL,
       "complete":False,"completed_slots":0,"completed_pairs":{"S":0,"C":0},"no_automatic_publication":True}
    ns=q.ns;start=ns();cpu0=dry.charge();cost=m["dry_charges"]
    stage0=cost["first_dry_start"];drycpu=cost["dry_cpu_ns"];drybytes=cost["dry_directories_bytes"]
    lastestart=lastend=None
    try:
        os.sched_setaffinity(0,{m["supervisor_cpu"]});log=root/"journal.jsonl"
        q.journal(log,{"event":"start","timed_stage_origin_ns":start,"first_dry_origin_ns":stage0,"seal":sealhash})
        for i,slot in enumerate(m["schedule"]["slots"]):
            e,c=slot["epoch"],slot["cpu_cell"];cpu=m["four_cpus"][c]
            if i%4==0:
                if i:
                    earliest=max(lastestart+120*q.NS,lastend+10*q.NS)
                    while ns()<earliest:
                        need(ns()-stage0<cp.WALL*q.NS,"total C elapsed budget (including dry)");time.sleep(min(.5,max(0,(earliest-ns())/q.NS)))
                lastestart=ns()
            sdir=root/f"slot-{i:02d}";sdir.mkdir();pland=sdir/"plan";order,_=cp.make_plan(pland,e,c)
            need(all(dry.sha(pland/(s+".csv"))==m["lane_schedule_sha256"][f"{e}/{c}/{s}"] for s in order),"C visit plan changed")
            for j,k in enumerate(cp.kind_order(e,c)):
                need(ns()-stage0<cp.WALL*q.NS and drycpu+dry.charge()-cpu0<cp.CPU*q.NS and drybytes+q.filesz(root)<q.BOUND_DISK,"whole C budget")
                p=sdir/f"{j}-{k}";p.mkdir();prior=drybytes+q.filesz(root)
                need(cp.bind(*images)==m["image_identities"] and list(dry.identity(host))==m["host_identity"],"C images changed")
                q.journal(log,{"event":"attempt","slot":i,"epoch":e,"cell":c,"pair":k,"t":ns(),"epoch_start_ns":lastestart})
                dry.save(p/"environment-before.json",q.env_snapshot(cpu))
                ext=cp.launch(host,images,m["image_identities"],p,pland,e,c,cpu,k,sealhash,stage0,cpu0,drycpu,prior)
                lastend=ext["end"];dry.save(p/"environment-after.json",q.env_snapshot(cpu))
                need(cp.bind(*images)==m["image_identities"] and list(dry.identity(host))==m["host_identity"],"C images changed after launch")
                cells=compact(p,k,slot,m,base,ref,cpu,ext,start);q.atom(p/"normalized.json.gz",q.gzdata(cells))
                t["completed_pairs"][k]+=1
                q.journal(log,{"event":"verified","slot":i,"pair":k,"t":ns(),"normalized_sha256":dry.sha(p/"normalized.json.gz")})
            t["completed_slots"]+=1
        need(t["completed_slots"]==48 and t["completed_pairs"]=={"S":48,"C":48},"incomplete C/S stage")
        datasets,cpuSum=assemble(root,m,start);decisions={}
        for k in ("S","C"):
            q.atom(root/f"complete-{k}.json.gz",q.gzdata(datasets[k]));decisions[k]=d.decide(datasets[k]);dry.save(root/f"decision-{k}.json",decisions[k])
        need(ns()-stage0<=cp.WALL*q.NS and drycpu+dry.charge()-cpu0<=cp.CPU*q.NS and drybytes+q.filesz(root)<q.BOUND_DISK,
             "terminal C budget")
        t.update({"complete":True,**release_summary(decisions["S"],decisions["C"]),
            "elapsed_ns_including_dry_and_waits":ns()-stage0,"pooled_worker_cpu_ns_including_dry":drycpu+dry.charge()-cpu0,
            "sum_of_measured_pair_external_worker_cpu_ns":cpuSum,"original_genuine_clock_rows":96*5*2*576,
            "original_empty_brackets":96*5*2*128,"native_dry_clock_reads":0,"retained_bytes_including_dry":drybytes+q.filesz(root)})
    except BaseException as exc:t["error"]=f"{type(exc).__name__}: {exc}";raise
    finally:os.sched_setaffinity(0,allowed);dry.save(root/"terminal.json",t)
    print(json.dumps(t,indent=2))
def main():
    p=argparse.ArgumentParser(description=__doc__);s=p.add_subparsers(dest="op",required=True)
    a=s.add_parser("seal")
    for k in ("host","stock","fir","q-stage","full-dry","out"):a.add_argument("--"+k,type=Path,required=True)
    a.add_argument("--prior-dry",action="append",default=[],type=Path,help="charge retained earlier untimed engineering preflights")
    b=s.add_parser("run");b.add_argument("--sealed",type=Path,required=True);b.add_argument("--accept-seal",required=True)
    x=p.parse_args()
    if x.op=="seal":seal(x.host,x.stock,x.fir,x.q_stage,x.full_dry,x.prior_dry,x.out)
    else:run(x.sealed,x.accept_seal)
if __name__=="__main__":
    try:main()
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError) as exc:sys.exit(str(exc))
