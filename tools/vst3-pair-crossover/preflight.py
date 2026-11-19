#!/usr/bin/env python3
"""Cutoff-stock only, isolated and two-peer native zero-clock rehearsal; never starts Q or C."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import resource
import signal
import socket
import struct
import subprocess
import sys
import time

import crossover as design

HERE=Path(__file__).resolve().parent
FILES=("all.fp","all.pcm","states.bin","guards.csv","midi.csv")
TIMEOUT=100
CAP=1 << 30

def need(value,msg):
    if not value: raise ValueError(msg)

def sha(p):
    h=hashlib.sha256()
    with p.open("rb") as f:
        for b in iter(lambda:f.read(1<<20),b""): h.update(b)
    return h.hexdigest()

def save(path,data):
    with path.open("x",encoding="utf-8") as f:
        json.dump(data,f,indent=2,sort_keys=True,allow_nan=False);f.write("\n")

def schedule(scope,epoch,cell):
    order=design.scene_order(epoch,cell) if scope=="full" else ["wake_varied"]
    return order,{s:design.plan(epoch,cell,s) for s in order}

def visits(scope,order,plans,lane):
    serial=0
    for scene in order:
        scene_id=design.SCENES.index(scene)
        wake=scene=="wake_varied"
        places=(16 if scope=="full" else 2) if wake else 1
        warm=1024+(8192 if scene.startswith("release_") else 0)
        for place in range(places):
            def add(phase,index,count,first):
                nonlocal serial
                row=(str(serial),scene,str(place),str(phase),str(index),str(count),str(first),str(lane))
                serial+=1
                return row
            yield add(0,0,warm,(place+scene_id)%2)
            start=place*36 if wake else 0
            i=start; end=i+(36 if wake else 576)
            while i<end:
                k,q,first,_=plans[scene][i]
                need(k==i and i+q<=end,"out-of-bounds burst")
                yield add(1,i,q,first)
                i+=q
            yield add(2,0,33,(place+scene_id+1)%2)

def build_id(path):
    result=subprocess.run(["readelf","-n",str(path)],capture_output=True,text=True,timeout=5,check=True)
    ids=[line.split("Build ID:",1)[1].strip() for line in result.stdout.splitlines() if "Build ID:" in line]
    need(len(ids)==1 and len(ids[0])>=16,"one ELF build ID required");return ids[0]

def identity(p):
    st=p.stat();return st.st_dev,st.st_ino,st.st_size,sha(p)

def charge():
    r=resource.getrusage(resource.RUSAGE_CHILDREN);return int((r.ru_utime+r.ru_stime)*1e9)

def launch(pair,host,stock,out,plan_dir,scope,cpu,launch_first):
    lanes=(launch_first,1-launch_first) if pair else (-1,)
    sockets=socket.socketpair(socket.AF_UNIX,socket.SOCK_SEQPACKET) if pair else ()
    procs=[]; files=[]; start=time.monotonic_ns();initial_cpu=charge()
    try:
        for lane in lanes:
            path=out/("isolated" if lane==-1 else f"peer-{lane}");path.mkdir()
            home=path/"home";home.mkdir()
            env={"HOME":str(home),"XDG_CONFIG_HOME":str(home/"config"),"XDG_CACHE_HOME":str(home/"cache"),
                 "PATH":"/usr/bin:/bin","LANG":"C","LC_ALL":"C"}
            stdout=(path/"stdout").open("xb");files.append(stdout)
            stderr=(path/"stderr").open("xb");files.append(stderr)
            fd=sockets[lane].fileno() if pair else -1
            argv=[str(host),str(stock),str(path/"capture"),str(cpu),str(lane),str(fd),str(plan_dir),scope]
            p=subprocess.Popen(argv,stdout=stdout,stderr=stderr,env=env,cwd=path,close_fds=True,
                               pass_fds=(fd,) if pair else (),start_new_session=True)
            procs.append(p)
        for s in sockets:s.close()
        while True:
            statuses=[p.poll() for p in procs]
            need(all(x is None or x==0 for x in statuses), f"{'pair' if pair else 'isolated'} worker failed: {statuses}; see retained stderr")
            if all(x==0 for x in statuses):break
            need(time.monotonic_ns()-start < TIMEOUT*1_000_000_000,"worker slot exceeded fixed 100 sec wall budget")
            time.sleep(.01)
    finally:
        for s in sockets:s.close()
        for p in procs:
            if p.poll() is None:
                try:os.killpg(p.pid,signal.SIGKILL)
                except ProcessLookupError:pass
        for p in procs:p.wait()
        for f in files:f.close()
    return {"pooled_worker_cpu_ns":charge()-initial_cpu,"whole_stage_wall_ns":time.monotonic_ns()-start}

def check_scene(folder,scene,scope):
    wake=scene=="wake_varied";release=scene.startswith("release_")
    places=(16 if scope=="full" else 2) if wake else 1
    prep=1024+(8192 if release else 0);take=36 if wake else 576
    calls=places*(prep+take+33)
    need((folder/"all.pcm").stat().st_size==calls*2*64*4,"PCM length")
    rec=struct.Struct("<IHHiIHHiIHHid i Q II iiiii".replace(" ",""))
    raw=(folder/"all.fp").read_bytes();need(len(raw)==rec.size*calls,"FP/event trace length")
    # Format: before/after/next (MXCSR,x87 CW/SW,errno), next-operation double probe,
    # return/silence, note reads/count queries, placement/phase/index/type/velocity roster.
    note_ons=note_offs=0; expected_inputs=[]
    index=0
    for place in range(places):
        for phase,n in enumerate((prep,take,33)):
            for k in range(n):
                t=rec.unpack_from(raw,index);index+=rec.size
                absolute=k+place*36 if phase==1 and wake else k
                typ=0; is_varied=False
                if phase==0 and (release or scene=="held_varied"):
                    if k==0:typ=1;is_varied=(scene!="release_equal")
                    elif release and k==1024:typ=2
                if phase==1 and wake and k==0:typ=1;is_varied=True
                if phase==2 and k==0 and (release or scene=="idle"):typ=1;is_varied=True
                if phase==2 and k==32:typ=2
                want=(place,phase,absolute,typ,int(is_varied) if typ==1 else -1)
                need(t[-5:]==want and t[13]==0 and t[15]==(8 if typ else 0),"native callback result/event/timeline")
                note_ons+=t[15] if typ==1 else 0;note_offs+=t[15] if typ==2 else 0
                if typ:
                    for v,(pitch,velocity) in enumerate(zip((36,43,48,55,60,64,67,72),(41,53,65,77,89,101,113,125))):
                        value=(velocity if is_varied else 102)/127 if typ==1 else 0.0
                        bits=struct.unpack("<I",struct.pack("<f",value))[0]
                        expected_inputs.append((place,phase,absolute,v,typ,pitch,v,bits))
    with (folder/"midi.csv").open() as f:
        inp=csv.reader(f);need(next(inp)==["placement","phase","index","input","type","pitch","note_id","velocity_bits"],"MIDI header")
        need([tuple(map(int,r)) for r in inp]==expected_inputs,"wrong concrete VST3 event/velocity payload")
    data=(folder/"states.bin").read_bytes();n=0;pos=0
    while pos<len(data):
        need(pos+8<=len(data),"truncated state size");size=struct.unpack_from("<Q",data,pos)[0];pos+=8
        need(0<size<=1<<20 and pos+size<=len(data),"unbounded/truncated public state");pos+=size;n+=1
    need(n==3*places,"missing state phases")
    with (folder/"guards.csv").open() as f:guards=list(csv.DictReader(f))
    need(len(guards)==places,"guard placement count")
    for i,g in enumerate(guards):
        need(int(g["placement"])==i,"guard placement order")
        quiet=release or scene=="idle"
        if quiet or wake: need(0<=float(g["last64_peak"])<1e-6,"quiet prep guard")
        need(0<=float(g["window_peak"])<1e-6 if quiet else float(g["window_peak"])>1e-5,"window audio guard")
        need(float(g["continuation_peak"])>1e-5,"inaudible continuation")
    return {"callback_count":calls,"window_callbacks":places*take,"placements":places,"note_on_reads":note_ons,
            "note_off_reads":note_offs,"state_snapshots":n,"guards":guards,
            "artifacts":{f:{"bytes":(folder/f).stat().st_size,"sha256":sha(folder/f)} for f in FILES}}

def verify(base,slot,lane,stock_id,cpu,scope,order,plans):
    cap=base/slot/"capture";meta=json.loads((cap/"identity.json").read_text())
    need((meta["dev"],meta["ino"],meta["size"])==stock_id[:3] and
         meta["lane"]==lane and meta["cpu"]==cpu and meta["callback_clock_reads"]==0 and
         meta["schema"]=="cs80-pair-dry-worker-v1","wrong pinned worker/image identity")
    with (cap/"baton.csv").open() as f:
        r=csv.reader(f);need(next(r)==["serial","scene","placement","phase","index","count","first","lane"],"baton header")
        actual=list(map(tuple,r))
    want=list(visits(scope,order,plans,lane));need(actual==want and meta["visits"]==len(want),"lost/reordered baton visit")
    return {s:check_scene(cap/s,s,scope) for s in order},len(want)

def execute(host,stock,out,scope,epoch,cell):
    host=host.resolve(strict=True);stock=stock.resolve(strict=True);out=out.resolve()
    need(sys.platform.startswith("linux") and os.uname().machine=="x86_64","Linux x86-64 worker")
    ids=identity(host),identity(stock)
    need(ids[1][3]==design.SHIPPING_CUTOFF,"not the one admitted cutoff stock ELF")
    need(os.access(host,os.X_OK),"host must be executable")
    stock_build,host_build=build_id(stock),build_id(host)
    allowed=os.sched_getaffinity(0);test_cpus,supervisor=design.cpu_ids(allowed);cpu=test_cpus[cell]
    order,plans=schedule(scope,epoch,cell)
    out.mkdir();plan_dir=out/"plan";plan_dir.mkdir()
    (plan_dir/"scenes.txt").write_text("\n".join(order)+"\n")
    for s in order:(plan_dir/f"{s}.csv").write_text(design.canonical(epoch,cell,s))
    launch_first=design.orientation(epoch,cell)["launch_first"]
    receipt={"schema":"cs80-pair-zero-preflight-v1","complete":False,"scope":scope,"stage":"dry-only",
        "callback_clock_reads":0,"timed_candidate_runs":0,"timed_placebo_runs":0,
        "epoch":epoch,"cpu_cell":cell,"worker_cpu":cpu,"supervisor_cpu":supervisor,"permitted_cpus":sorted(allowed),
        "launch_first_lane":launch_first,"scene_order":order,"stock_sha256":ids[1][3],"host_sha256":ids[0][3],
        "stock_build_id":stock_build,"host_build_id":host_build,"schedules_sha256":{s:sha(plan_dir/f"{s}.csv") for s in order},
        "sources_sha256":{f:sha(HERE/f) for f in ("PROTOCOL.md","host.cpp","preflight.py","crossover.py","build-host.sh")},
        "max_wall_seconds_per_worker_slot":TIMEOUT,"max_retained_bytes":CAP}
    try:
        os.sched_setaffinity(0,{supervisor})
        isolated=launch(False,host,stock,out,plan_dir,scope,cpu,launch_first)
        ref,ref_visits=verify(out,"isolated",-1,ids[1],cpu,scope,order,plans)
        pair=launch(True,host,stock,out,plan_dir,scope,cpu,launch_first)
        need(identity(host)==ids[0] and identity(stock)==ids[1],"native identity changed")
        for lane in (0,1):
            need(sha(out/f"peer-{lane}"/"capture/controls.csv")==sha(out/"isolated/capture/controls.csv"),"control frame mismatch")
            got,n=verify(out,f"peer-{lane}",lane,ids[1],cpu,scope,order,plans)
            need(got==ref and n==ref_visits,"peer PCM/FP/events/state/guard differs from isolated reference")
            # Check actual bytes, not just matching digests, across every phase and continuation.
            import filecmp
            for s in order:
                for f in FILES:
                    need(filecmp.cmp(out/"isolated"/"capture"/s/f, out/f"peer-{lane}"/"capture"/s/f,
                                     shallow=False),"peer byte mismatch")
        retained=sum(p.stat().st_size for p in out.rglob("*") if p.is_file())
        need(retained<CAP,"retention cap exceeded")
        receipt.update({"complete":True,"visits_per_worker":ref_visits,"public_capture_per_worker":ref,
            "controls_sha256":sha(out/"isolated/capture/controls.csv"),
            "whole_resource_observations":{"isolated":isolated,"two_peer_slot":pair},"retained_bytes_before_receipt":retained})
        if scope=="full":
            w=pair["whole_stage_wall_ns"]/1e9
            receipt["Q_projection_from_one_dry_slot_only"]={"worker_CPU_seconds_48_slots":48*pair["pooled_worker_cpu_ns"]/1e9,
                "elapsed_seconds_with_epoch_spacing":48*w+11*max(10,120-4*w),
                "not_included":"additional callback clocks, post-window empty brackets, stage supervision/validation and machine drift",
                "hard_worker_CPU_budget_seconds":2400,"hard_elapsed_budget_seconds":4500}
        return receipt
    except BaseException as e:
        receipt["error"]=f"{type(e).__name__}: {e}";raise
    finally:
        os.sched_setaffinity(0,allowed);save(out/"preflight.json",receipt)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host",required=True,type=Path);ap.add_argument("--stock",required=True,type=Path)
    ap.add_argument("--out",required=True,type=Path);ap.add_argument("--scope",choices=("wake-two","full"),default="wake-two")
    ap.add_argument("--epoch",type=int,choices=range(12),default=0);ap.add_argument("--cpu-cell",type=int,choices=range(4),default=0)
    a=ap.parse_args();r=execute(a.host,a.stock,a.out,a.scope,a.epoch,a.cpu_cell)
    print(f"PASS {r['scope']}: exact isolated/peer0/peer1; {r['visits_per_worker']} ordered visits; zero callback clocks")

if __name__=="__main__":
    try:main()
    except (ValueError,OSError,subprocess.SubprocessError) as exc:sys.exit(str(exc))
