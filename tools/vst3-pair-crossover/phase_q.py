#!/usr/bin/env python3
"""One sealed same-physical-stock placebo stage. No candidate interface or launch capability."""
import argparse
import csv
import gzip
import hashlib
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

HERE=Path(__file__).resolve().parent
BOUND_WALL=4500; BOUND_CPU=2400; BOUND_SLOT=100; BOUND_DISK=1<<30
NS=1_000_000_000
FROZEN_FILES=("PROTOCOL.md","crossover.py","preflight.py","host.cpp","q_clocks.h","build_q_host.py","phase_q.py")
SCHEMA="cs80-stock-pair-Q-seal-v1"
def need(ok,msg):
    if not ok:raise ValueError(msg)
def read(p):return json.loads(p.read_bytes())
def filesz(p):return sum(i.stat().st_size for i in p.rglob("*") if i.is_file())
def hashes():return {f:dry.sha(HERE/f) for f in FROZEN_FILES}
def ns():return time.monotonic_ns()
def journal(p,data):
    with p.open("a",encoding="utf-8") as f:
        f.write(json.dumps(data,sort_keys=True,allow_nan=False)+"\n");f.flush();os.fsync(f.fileno())
def atom(p,raw):
    with p.open("xb") as f:f.write(raw);f.flush();os.fsync(f.fileno())
def gzdata(data):return gzip.compress(json.dumps(data,sort_keys=True,separators=(",",":"),allow_nan=False).encode(),mtime=0)
def stock_preflight(host,stock,gold):
    base=read(gold/"preflight.json");orig=read(HERE/"zero-preflight/receipt.json")
    need(base["scope"]=="full" and base["complete"] is True and base["callback_clock_reads"]==0 and
         base["timed_candidate_runs"]==base["timed_placebo_runs"]==0 and base["stock_sha256"]==d.SHIPPING_CUTOFF and
         base["host_sha256"]==dry.sha(host) and base["stock_sha256"]==dry.sha(stock),"full dry required on exact Q and stock binaries")
    need(base["public_capture_per_worker"]==orig["public_capture_per_worker"] and base["controls_sha256"]==orig["controls_sha256"],
         "fresh Q-host dry differs from archived cutoff golden")
    need(base["controls_sha256"]==dry.sha(gold/"isolated/capture/controls.csv"),"dry controls changed")
    for scene,v in base["public_capture_per_worker"].items():
        for f,info in v["artifacts"].items():
            p=gold/"isolated/capture"/scene/f
            need(info["bytes"]==p.stat().st_size and info["sha256"]==dry.sha(p),"dry reference bytes changed")
    return base
def seal(host,stock,gold,out):
    host=host.resolve(strict=True);stock=stock.resolve(strict=True);gold=gold.resolve(strict=True);out=out.resolve()
    base=stock_preflight(host,stock,gold);cpus,sup=d.cpu_ids(os.sched_getaffinity(0))
    need(len(cpus)==4 and len(set(cpus))==4 and sup not in cpus,"four test CPUs and separate supervisor required")
    # One non-resumable stage under the prospective sealed directory.
    out.mkdir();manifest={"schema":SCHEMA,"scope":"same-physical-cutoff-stock-only","role":"Q",
        "sources":hashes(),"host_path":str(host),"stock_path":str(stock),"gold_directory":str(gold),
        "host_identity":dry.identity(host),"stock_identity":dry.identity(stock),"host_build_id":base["host_build_id"],
        "stock_build_id":base["stock_build_id"],"dry_receipt_sha256":dry.sha(gold/"preflight.json"),
        "allowed_cpus":sorted(os.sched_getaffinity(0)),"four_cpus":cpus,"supervisor_cpu":sup,
        "stage_budget":{"wall_seconds":BOUND_WALL,"pooled_worker_cpu_seconds":BOUND_CPU,
                        "slot_seconds":BOUND_SLOT,"retained_bytes":BOUND_DISK,"min_epoch_spacing_seconds":120,"post_epoch_seconds":10},
        "schedule":d.generate(),"lane_schedule_sha256":{
            f"{e}/{c}/{s}":hashlib.sha256(d.canonical(e,c,s).encode()).hexdigest()
            for e in range(12) for c in range(4) for s in d.SCENES}}
    dry.save(out/"seal.json",manifest)
    digest=dry.sha(out/"seal.json");print("Sealed for explicit launch (no clock calls):",digest);return digest

def inputs(folder,sealdigest):
    path=folder/"seal.json"
    need(dry.sha(path)==sealdigest,"explicit SHA does not match seal")
    m=read(path);need(m["schema"]==SCHEMA and m["scope"]=="same-physical-cutoff-stock-only"
        and m["role"]=="Q" and m["sources"]==hashes() and m["schedule"]==d.generate(),"seal/source/schedule changed")
    host=Path(m["host_path"]);stock=Path(m["stock_path"]);gold=Path(m["gold_directory"])
    need(tuple(m["host_identity"])==dry.identity(host) and tuple(m["stock_identity"])==dry.identity(stock)
         and m["stock_identity"][3]==d.SHIPPING_CUTOFF,"sealed native identity changed")
    base=stock_preflight(host,stock,gold)
    need(m["dry_receipt_sha256"]==dry.sha(gold/"preflight.json") and
         m["host_build_id"]==base["host_build_id"] and m["stock_build_id"]==base["stock_build_id"],"dry image mismatch")
    allowed=os.sched_getaffinity(0);cpus,sup=d.cpu_ids(allowed)
    need(sorted(allowed)==m["allowed_cpus"] and cpus==m["four_cpus"] and sup==m["supervisor_cpu"],"CPU set changed")
    return m,host,stock,gold,base,allowed
def env_snapshot(cpu):
    def get(path):
        try:return Path(path).read_text().strip()
        except OSError:return None
    root=Path(f"/sys/devices/system/cpu/cpu{cpu}")
    stat=get("/proc/stat") or ""
    return {"cpu":cpu,"allowed_supervisor":sorted(os.sched_getaffinity(0)),
        "frequency_governor":get(root/"cpufreq/scaling_governor"),"frequency_khz_if_visible":get(root/"cpufreq/scaling_cur_freq"),
        "siblings":get(root/"topology/thread_siblings_list"),"cpu_jiffies":next((l.split()[1:] for l in stat.splitlines() if l.startswith(f"cpu{cpu} ")),None)}
def live_cpu(procs):
    hz=os.sysconf("SC_CLK_TCK");ticks=0
    for p in procs:
        if p.returncode is not None:continue
        try:
            fields=Path(f"/proc/{p.pid}/stat").read_text().rsplit(")",1)[1].split()
            ticks+=int(fields[11])+int(fields[12])
        except FileNotFoundError:pass
    return (ticks*NS)//hz

def measure_pair(host,stock,folder,scope_dir,plan,epoch,cell,sealhash,elapsed_origin,stage_child0,fixed_disk):
    cpu=plan["four_cpus"][cell];physical=plan["stock_identity"]
    sockets=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET);procs=[];streams=[]
    start=ns();ru=dry.charge();disk_check=0
    try:
        first=d.orientation(epoch,cell)["launch_first"]
        for lane in (first,1-first):
            path=folder/f"peer-{lane}";path.mkdir();home=path/"home";home.mkdir()
            env={"HOME":str(home),"XDG_CONFIG_HOME":str(home/"config"),"XDG_CACHE_HOME":str(home/"cache"),
                 "LANG":"C","LC_ALL":"C","PATH":"/usr/bin:/bin","CS80_CROSSOVER_Q_SEAL":sealhash,
                 "CS80_CROSSOVER_Q_STOCK":":".join(map(str,physical))}
            out=(path/"stdout").open("xb");err=(path/"stderr").open("xb");streams.extend((out,err));fd=sockets[lane].fileno()
            procs.append(subprocess.Popen([str(host),str(stock),str(path/"capture"),str(cpu),str(lane),str(fd),str(scope_dir),"full"],
                env=env,cwd=path,pass_fds=(fd,),close_fds=True,start_new_session=True,stdout=out,stderr=err))
        for s in sockets:s.close()
        while True:
            now=ns();statuses=[p.poll() for p in procs]
            need(all(x in (0,None) for x in statuses),f"worker failed, retained statuses {statuses}")
            need(now-start<=BOUND_SLOT*NS and now-elapsed_origin<=BOUND_WALL*NS,"worker/total wall cap")
            need(dry.charge()-stage_child0+live_cpu(procs)<=BOUND_CPU*NS,"pooled stage worker CPU cap")
            if now>=disk_check:
                need(fixed_disk+filesz(folder)<=BOUND_DISK,"retained bytes cap");disk_check=now+NS
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
    return {"start":start,"end":ns(),"pooled_worker_cpu_ns":dry.charge()-ru,"launch_first":first}
def exact(a,b):
    if a.stat().st_size!=b.stat().st_size:return False
    with a.open("rb") as x,b.open("rb") as y:
        while True:
            s,t=x.read(1<<20),y.read(1<<20)
            if s!=t:return False
            if not s:return True

def endpoints(path,expected):
    with path.open(encoding="ascii") as f:
        r=csv.reader(f);need(next(r)==["index","cpu0","cpu1","wall0","wall1"],"wrong clock CSV header");out=[]
        for expected_i,row in enumerate(r):
            need(len(row)==5 and all(v.isascii() and v.isdecimal() for v in row),"invalid integer clock")
            i,c0,c1,w0,w1=map(int,row)
            need(i==expected_i and c0>0 and w0>0 and c1>=c0 and w1>w0,"backwards endpoint")
            if out:need(out[-1][2]<=c0 and out[-1][4]<=w0,"nonmonotonic lane clock")
            out.append([i,c0,c1,w0,w1])
    need(len(out)==expected,"incomplete clock rows");return out
def scene_clocks(folder,scene,epoch,cell,origin,limits):
    by={};empties=[];empty_end=[];resource=[]
    for lane in (0,1):
        p=folder/f"peer-{lane}"/"capture"/scene
        real=endpoints(p/"clocks.csv",576);empty=endpoints(p/"empty.csv",128)
        with (p/"resources.csv").open() as f:
            rows=list(csv.DictReader(f));need(len(rows)==1,"scene resources")
            res={k:int(v) for k,v in rows[0].items()}
        need(set(res)=={"wall0","wall1","voluntary_switches","involuntary_switches","minor_faults","major_faults"}
             and all(v>=0 for v in res.values()),"resource fields")
        need(limits["start"]<=res["wall0"]<=real[0][3] and real[-1][4]<=empty[0][3]<=empty[-1][4]<=res["wall1"]<=limits["end"]
             and real[-1][2]<=empty[0][1],"clock rows outside genuine/window/empty bounds")
        # Thread CPU is already relative to that worker's lifetime: retain its original origin
        # across every scene. One common stage-wide wall origin preserves both workers' order.
        empties.append([r[2]-r[1] for r in empty])
        empty_end.append([[i,c0,c1,w0-origin,w1-origin] for i,c0,c1,w0,w1 in empty])
        res.update({"wall0":res["wall0"]-origin,"wall1":res["wall1"]-origin});resource.append(res)
        for i,c0,c1,w0,w1 in real:
            need(c1>c0,"empty genuine callback")
            by[i,lane]=[i,lane,c0,c1,w0-origin,w1-origin]
    ordered=[by[i,l] for i,l in d.visits(d.plan(epoch,cell,scene))]
    need(len(ordered)==1152 and all(ordered[i-1][5]<=ordered[i][4] for i in range(1,len(ordered))),
         "chronology violates matched q1/q4 baton")
    return {"epoch":epoch,"cpu_cell":cell,"scene":scene,"role":"Q","a_sha256":d.SHIPPING_CUTOFF,
           "b_sha256":d.SHIPPING_CUTOFF,"clocks":ordered,"empty_cpu_ns":empties,
           "empty_endpoints":empty_end,"scene_rusage":resource}

def verify_scene_order(cells):
    lastCPU=[0,0];lastWall=0
    for row in cells:
        for lane in (0,1):
            first=next(a for a in row["clocks"] if a[1]==lane)
            need(first[2]>=lastCPU[lane] and first[4]>=lastWall,"cross-scene thread or joint wall chronology")
            lastCPU[lane]=row["empty_endpoints"][lane][-1][2]
        lastWall=max(r[-1][4] for r in row["empty_endpoints"])

def verify_and_compact(folder,slot,stock_id,base,gold,cpu,external,origin):
    epoch,cell=slot["epoch"],slot["cpu_cell"];order,plans=dry.schedule("full",epoch,cell);copies=[]
    identities=[];sizes={}
    for lane in (0,1):
        cap=folder/f"peer-{lane}"/"capture";m=read(cap/"identity.json")
        need(m["lane"]==lane and m["cpu"]==cpu and m["callback_clock_reads"]==11520 and m["q_empty_clock_reads"]==2560
             and [m["dev"],m["ino"],m["size"]]==stock_id[:3],"actual native identity/CPU/endpoint count")
        need(exact(cap/"controls.csv",gold/"isolated/capture/controls.csv"),"stock panel controls changed")
        with (cap/"baton.csv").open() as f:
            r=csv.reader(f);need(next(r)==["serial","scene","placement","phase","index","count","first","lane"],"baton header")
            need(list(map(tuple,r))==list(dry.visits("full",order,plans,lane)),"actual baton ledger differs")
        for scene in order:
            for name in dry.FILES:
                p=cap/scene/name;ref=gold/"isolated/capture"/scene/name
                need(exact(p,ref),"PCM/FP/event/state differs: "+str(p.relative_to(folder)))
                copies.append(p)
        identities.append(m);sizes[str(lane)]=sum((cap/s/f).stat().st_size for s in order for f in dry.FILES)
    cells=[scene_clocks(folder,s,epoch,cell,origin,external) for s in order]
    verify_scene_order(cells)
    thread_sum=sum(r[3]-r[2] for c in cells for r in c["clocks"])
    need(thread_sum<=external["pooled_worker_cpu_ns"]+2_000_000,"callbacks exceed pooled worker usage")
    dry.save(folder/"verified.json",{"epoch":epoch,"cpu_cell":cell,"actual_worker_identities":identities,
        "public_bytes_compared_by_lane":sizes,"public_reference_sha256":base["controls_sha256"],
        "genuine_callbacks":576*5*2,"empty_brackets":128*5*2,"external":external})
    # Remove only generated duplicates proved byte-equal to the retained native golden;
    # keep every original timing/OS row, visit, diagnostic and unsuccessful slot.
    for p in copies:p.unlink()
    return cells

def run(folder,sealhash):
    folder=folder.resolve();m,host,stock,gold,base,allowed=inputs(folder,sealhash)
    root=folder/"Q-run";root.mkdir(); # irrevocable one-shot latch; never replay a partial or failed Q by overwriting
    terminal={"schema":"cs80-Q-terminal-v1","seal_sha256":sealhash,"role":"Q","complete":False,
              "candidate_runs":0,"completed_slots":0}
    start=ns();initial_cpu=dry.charge();previous_epoch_start=previous_epoch_end=None
    try:
        os.sched_setaffinity(0,{m["supervisor_cpu"]})
        log=root/"journal.jsonl";journal(log,{"event":"start","t":start,"seal":sealhash})
        for n,slot in enumerate(m["schedule"]["slots"]):
            e,c=slot["epoch"],slot["cpu_cell"]
            if n%4==0:
                if n:
                    earliest=max(previous_epoch_start+120*NS,previous_epoch_end+10*NS)
                    while ns()<earliest:
                        need(ns()-start<BOUND_WALL*NS,"elapsed stage cap during spacing")
                        time.sleep(min(.5,max(0,(earliest-ns())/NS)))
                previous_epoch_start=ns()
            prior_bytes=filesz(root)
            need(ns()-start<BOUND_WALL*NS and dry.charge()-initial_cpu<BOUND_CPU*NS and prior_bytes<BOUND_DISK,"stage acquisition cap")
            sdir=root/f"slot-{n:02d}";sdir.mkdir();plan=sdir/"plan";plan.mkdir()
            order,_=dry.schedule("full",e,c);(plan/"scenes.txt").write_text("\n".join(order)+"\n")
            for s in order:
                p=plan/(s+".csv");p.write_text(d.canonical(e,c,s))
                need(dry.sha(p)==m["lane_schedule_sha256"][f"{e}/{c}/{s}"],"immutable visit schedule mismatch")
            need(list(dry.identity(host))==m["host_identity"] and list(dry.identity(stock))==m["stock_identity"],"image changed")
            journal(log,{"event":"attempt","slot":n,"epoch":e,"cell":c,"t":ns()})
            dry.save(sdir/"environment-before.json",env_snapshot(m["four_cpus"][c]))
            ext=measure_pair(host,stock,sdir,plan,m,e,c,sealhash,start,initial_cpu,prior_bytes)
            previous_epoch_end=ext["end"]
            dry.save(sdir/"environment-after.json",env_snapshot(m["four_cpus"][c]))
            need(list(dry.identity(host))==m["host_identity"] and list(dry.identity(stock))==m["stock_identity"],"image changed after slot")
            cells=verify_and_compact(sdir,slot,m["stock_identity"],base,gold,m["four_cpus"][c],ext,start)
            atom(sdir/"normalized.json.gz",gzdata(cells))
            terminal["completed_slots"]+=1
            journal(log,{"event":"verified","slot":n,"t":ns(),"normalized_sha256":dry.sha(sdir/"normalized.json.gz")})
        need(terminal["completed_slots"]==48,"incomplete 48-slot stage")
        cells=[]
        for n,slot in enumerate(m["schedule"]["slots"]):
            p=root/f"slot-{n:02d}";external=read(p/"verified.json")["external"]
            per=[scene_clocks(p,s,slot["epoch"],slot["cpu_cell"],start,external) for s in slot["scene_order"]]
            verify_scene_order(per)
            need(per==json.loads(gzip.decompress((p/"normalized.json.gz").read_bytes())),"final raw replay mismatch")
            cells.extend(per)
        output={"role":"Q","a_sha256":d.SHIPPING_CUTOFF,"b_sha256":d.SHIPPING_CUTOFF,"cells":cells}
        atom(root/"complete-normalized.json.gz",gzdata(output))
        decision=d.decide(output);dry.save(root/"numerical-gates.json",decision)
        need(ns()-start<=BOUND_WALL*NS and dry.charge()-initial_cpu<=BOUND_CPU*NS and filesz(root)<BOUND_DISK,"terminal stage cap")
        terminal.update({"complete":True,"numerical_gates_pass":decision["numerical_gates_pass"],
            "candidate_stage_authorized":False,"scenes":len(cells),"retained_rows":len(cells)*1152,
            "elapsed_ns":ns()-start,"pooled_worker_cpu_ns":dry.charge()-initial_cpu})
    except BaseException as exc:
        terminal["error"]=f"{type(exc).__name__}: {exc}";raise
    finally:
        os.sched_setaffinity(0,allowed);dry.save(root/"terminal.json",terminal)
    print(json.dumps(terminal,indent=2))
def main():
    p=argparse.ArgumentParser(description=__doc__);sp=p.add_subparsers(dest="op",required=True)
    s=sp.add_parser("seal")
    for arg in ("host","stock","full-dry","out"):s.add_argument("--"+arg,required=True,type=Path)
    r=sp.add_parser("run");r.add_argument("--sealed",required=True,type=Path);r.add_argument("--accept-seal",required=True)
    opt=p.parse_args()
    if opt.op=="seal":seal(opt.host,opt.stock,opt.full_dry,opt.out)
    else:run(opt.sealed,opt.accept_seal)
if __name__=="__main__":
    try:main()
    except (ValueError,RuntimeError,OSError,KeyError,TypeError,subprocess.SubprocessError) as exc:sys.exit(str(exc))
