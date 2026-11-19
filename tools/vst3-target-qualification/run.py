#!/usr/bin/env python3
"""Stock-only controls: interleave matched VST3 timelines, never load a candidate."""
import argparse
import csv
import hashlib
import itertools
import json
import os
import platform
import re
import resource
import subprocess
import sys
import time
import uuid
from pathlib import Path

SCHEMA = "cs80-vst3-stock-interleave-v1"
ROLES = ("S0", "S1", "S+")
TRIPLETS, STRIPES, WIDTH, WARM = 12, 32, 17, 1024
PERMS = tuple("".join(p) for p in itertools.permutations("012"))
FILES = ("pcm.bin", "callbacks.bin", "initial.state", "postwarm.state", "postwindow.state",
         "continuation.state", "initial-metadata.csv", "final-metadata.csv")
KEYS = ("stripe", "position", "role", "lane")
RAW_FIELDS = KEYS + ("index", "global_block", "wall_start_ns", "cpu_start_ns", "cpu_end_ns", "wall_end_ns")
OS_FIELDS = KEYS + ("wall_start_ns", "wall_end_ns", "voluntary_switches", "involuntary_switches",
                    "minor_faults", "major_faults")

def need(ok, message):
    if not ok:
        raise ValueError(message)

def digest(p):
    h = hashlib.sha256()
    with p.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

def write(p, value):
    with p.open("x", encoding="utf-8") as f:
        json.dump(value, f, indent=2, sort_keys=True, allow_nan=False)
        f.write("\n")

def roster(group, key):
    # Deterministic total order with an ordinary lexical collision tie-break.
    return sorted(PERMS, key=lambda p: (hashlib.sha256(
        f"cs80-stock-interleave-v1/{group}/{key}/{p}".encode("ascii")).digest(), p))

def schedule(index):
    need(type(index) is int and 0 <= index < TRIPLETS, "triplet index")
    group, row = divmod(index, 6)
    mapping = roster(group, "physical")[row]
    order = [roster(group, f"stripe-{s}")[row] for s in range(STRIPES)]
    return mapping, order

def snapshot(cpu):
    """Read-only diagnostics. No governor, priority, isolation, or turbo changes."""
    def read(file):
        try:
            return Path(file).read_text(encoding="ascii", errors="replace").strip()
        except (FileNotFoundError, PermissionError, OSError):
            return None
    def cpulist(s):
        values = {cpu}
        for token in (s or "").split(","):
            if re.fullmatch(r"[0-9]+(?:-[0-9]+)?", token):
                lo, *hi = map(int, token.split("-"))
                values.update(range(lo, (hi[0] if hi else lo) + 1))
        return values
    root = Path(f"/sys/devices/system/cpu/cpu{cpu}")
    siblings = read(root / "topology/thread_siblings_list")
    sibling_ids = cpulist(siblings)
    fields = ("scaling_driver", "scaling_governor", "scaling_min_freq", "scaling_max_freq",
              "scaling_cur_freq", "energy_performance_preference")
    cpuinfo = read("/proc/cpuinfo") or ""
    blocks = [dict((k.strip(), v.strip()) for k, v in (line.split(":", 1) for line in b.splitlines()
              if ":" in line)) for b in cpuinfo.split("\n\n")]
    target = next((b for b in blocks if b.get("processor") == str(cpu)), {})
    dmi = {k: read("/sys/class/dmi/id/" + k) for k in ("sys_vendor", "product_name")}
    evidence = []
    if "hypervisor" in target.get("flags", "").split():
        evidence.append("cpu hypervisor flag")
    if re.search(r"qemu|kvm|vmware|virtualbox|virtual machine|hvm domu|google compute engine|bochs|openstack",
                 " ".join(v or "" for v in dmi.values()), flags=re.I):
        evidence.append("virtual machine DMI")
    if Path("/.dockerenv").exists() or Path("/run/.containerenv").exists():
        evidence.append("container marker")
    stat = {}
    for line in (read("/proc/stat") or "").splitlines():
        m = re.match(r"^cpu([0-9]+)\s+(.*)$", line)
        if m and int(m[1]) in sibling_ids:
            stat[m[1]] = [int(x) for x in m[2].split()]
    return {"cpu": cpu, "architecture": platform.machine(), "kernel": platform.release(),
            "model": target.get("model name"), "microcode": target.get("microcode"),
            "allowed_cpu_list": sorted(os.sched_getaffinity(0)), "thread_siblings_list": siblings,
            "logical_cpu_jiffies": stat, "cpufreq": {k: read(root / "cpufreq" / k) for k in fields},
            "intel_no_turbo": read("/sys/devices/system/cpu/intel_pstate/no_turbo"),
            "cpufreq_boost": read("/sys/devices/system/cpu/cpufreq/boost"),
            "thermal_throttle_counts": {k: read(root / "thermal_throttle" / k)
                for k in ("core_throttle_count", "package_throttle_count")},
            "dmi": dmi, "virtualization_evidence": evidence}

def check_target(info, collect, attested, cloud=False):
    need(info["architecture"].lower() in ("x86_64", "amd64") and sys.platform.startswith("linux"),
         "Linux x86-64 required")
    need(info["cpu"] in info["allowed_cpu_list"], "CPU not in current affinity")
    need(not (attested and cloud), "cloud diagnostic cannot be a dedicated target")
    need(collect or not (cloud or attested), "target classification is only for collection")
    if collect and not cloud:
        need(attested, "collect requires --dedicated-target (see README)")
        need(not info["virtualization_evidence"], "known virtualized/container host: smoke only")

def numbered(v):
    need(isinstance(v, str) and bool(re.fullmatch(r"0|[1-9][0-9]*", v)), "noncanonical unsigned field")
    return int(v)

def rows(path, fields):
    with path.open(encoding="ascii", newline="") as f:
        reader = csv.DictReader(f)
        need(tuple(reader.fieldnames or ()) == fields, f"{path.name}: header")
        out = list(reader)
    need(all(set(row) == set(fields) and None not in row.values() for row in out), f"{path.name}: columns")
    return out

def validate_raw(raw, os_rows, mode, index):
    mapping, order = schedule(index)
    nlanes = 1 if mode == "reference" else 3
    need(mode in ("reference", "untimed", "measure"), "unknown mode")
    need(len(raw) == STRIPES*nlanes*WIDTH and len(os_rows)==STRIPES*nlanes, "incomplete callback or OS rows")
    totals = dict.fromkeys(ROLES, 0); counts = totals.copy()
    prev_owner_wall = prev_callback_wall = prev_cpu = common = extra = 0
    for i, env in enumerate(os_rows):
        stripe, pos = divmod(i, nlanes)
        n = 0 if nlanes==1 else int(order[stripe][pos])
        role, lane = ROLES[n], (0 if nlanes==1 else int(mapping[n]))
        keys = [str(stripe), str(pos), role, str(lane)]
        need([env[k] for k in KEYS] == keys, "OS row schedule or physical lane mismatch")
        start, end, *_ = [numbered(env[k]) for k in OS_FIELDS[4:]]
        need(0 < start < end and start >= prev_owner_wall, "overlapping or invalid OS slot bounds")
        prev_owner_wall = end
        for j in range(WIDTH):
            r = raw[i*WIDTH+j]
            need([r[k] for k in KEYS] == keys and numbered(r["index"]) == j and
                 numbered(r["global_block"]) == WARM+stripe*WIDTH+j, "out-of-order or mismatched callback")
            w0,c0,c1,w1 = [numbered(r[k]) for k in RAW_FIELDS[6:]]
            timed = mode == "measure" and (n==2 or j<WIDTH-1)
            if not timed:
                need((w0,c0,c1,w1) == (0,0,0,0), "untimed callback has endpoints")
                continue
            need(start <= w0 < w1 <= end and w0 >= prev_callback_wall and
                 0 < c0 < c1 and c0 >= prev_cpu, "invalid nested clock chronology")
            prev_callback_wall, prev_cpu = w1,c1
            d = c1-c0; counts[role]+=1; totals[role]+=d
            if n==2:
                if j==WIDTH-1:
                    extra+=d
                else:
                    common+=d
    want = {"S0":512, "S1":512, "S+":544} if mode=="measure" else dict.fromkeys(ROLES, 0)
    need(counts==want and common+extra==totals["S+"], "wrong matched control counts")
    return {"index":index, "cpu_ns":totals, "measured_calls":counts,
            "positive_common_cpu_ns":common, "positive_extra_cpu_ns":extra}

def observations(folder, lanes):
    total = WARM + STRIPES*WIDTH + 1
    out = []
    for i in range(lanes):
        base = folder/f"lane{i}"
        need((base/"pcm.bin").stat().st_size == total*128*4, "incomplete stereo PCM")
        # b/result/silence/event_reads/event_count_reads + 3 FP words + 64-bit probe = 68 bytes.
        need((base/"callbacks.bin").stat().st_size == total*68, "incomplete FP/MIDI trace")
        need(all((base/f).stat().st_size > 0 for f in FILES), "empty public artifact")
        out.append({f: digest(base/f) for f in FILES} | {"controls.csv":digest(folder/"controls.csv")})
    return out

def same_file(path, expected):
    st = path.stat()
    return (st.st_dev, st.st_ino, st.st_size, digest(path)) == expected

def identify(path):
    st = path.stat()
    return st.st_dev, st.st_ino, st.st_size, digest(path)

def child_cpu_ns():
    r = resource.getrusage(resource.RUSAGE_CHILDREN)
    return int((r.ru_utime + r.ru_stime)*1e9)

def capture(host, stock, ids, parent, mode, index, cpu, session, expected):
    name = mode if mode!="measure" else f"triplet-{index:02d}"
    base = parent/name; base.mkdir(); folder=base/"capture"
    mapping, order = schedule(index)
    order_file = base/"schedule.txt"
    order_file.write_text("\n".join([mapping,*order])+"\n",encoding="ascii")
    home = base/"home"; home.mkdir()
    env = {"PATH":"/usr/bin:/bin","LANG":"C","LC_ALL":"C","HOME":str(home)}
    if mode=="measure":
        env["CS80_STOCK_INTERLEAVE_SESSION"] = session
    need(same_file(host, ids[0]) and same_file(stock, ids[1]), "binary identity changed before launch")
    write(base/"before.json",snapshot(cpu))
    start=time.monotonic_ns(); process_cpu=child_cpu_ns()
    with (base/"stdout").open("xb") as out, (base/"stderr").open("xb") as err:
        result=subprocess.run([str(host),mode,str(stock),str(folder),str(cpu),str(order_file)],
                              env=env,cwd=base,stdout=out,stderr=err,timeout=180,check=False)
    elapsed_cpu=child_cpu_ns()-process_cpu; finish=time.monotonic_ns()
    write(base/"after.json",snapshot(cpu))
    write(base/"child.json",{"returncode":result.returncode,"wall_ns":finish-start,"cpu_ns":elapsed_cpu})
    need(result.returncode==0, f"{name}: native host failed; see {name}/stderr")
    need(same_file(host, ids[0]) and same_file(stock, ids[1]), "binary identity changed after launch")
    g = json.loads((folder/"identity.json").read_text())
    nlanes=1 if mode=="reference" else 3
    wanted={"schema":"stock-interleave-host-v1","mode":mode,"cpu":cpu,"lanes":nlanes,
            "sample_rate":48000,"block_size":64,"warm_calls_per_lane":1024,
            "window_calls_per_lane":544,"continuation_calls_per_lane":1,
            "module_dev":ids[1][0],"module_ino":ids[1][1],"module_size":ids[1][2]}
    need(all(type(g.get(k)) is type(v) and g[k]==v for k,v in wanted.items()) and
         all(type(g.get(k)) is int and g[k]>0 for k in ("thread_clock_resolution_ns","wall_clock_resolution_ns")),
         "native module/clock/workload identity mismatch")
    artifacts = observations(folder,nlanes)
    gold = artifacts[0] if expected is None else expected
    need(all(a==gold for a in artifacts), f"{name}: PCM, FP/MIDI, metadata, controls or public state differs from isolated reference")
    raw=rows(folder/"clocks.csv",RAW_FIELDS); os_rows=rows(folder/"os.csv",OS_FIELDS)
    numeric=validate_raw(raw,os_rows,mode,index)
    need(sum(numeric["cpu_ns"].values()) <= elapsed_cpu + 2000000, "callbacks exceed entire process CPU charge")
    need(start <= numbered(os_rows[0]["wall_start_ns"]) <= numbered(os_rows[-1]["wall_end_ns"]) <= finish,
         "slot clocks outside external process wall envelope")
    write(base/"integrity.json",{"mode":mode,"triplet_index":index,"actual_physical_mapping":mapping,
          "public_artifacts_by_lane":artifacts,"schedule_sha256":digest(order_file),
          "raw_clock_sha256":digest(folder/"clocks.csv"),"raw_os_sha256":digest(folder/"os.csv")})
    return numeric, gold

def cli():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("mode",choices=("smoke","collect","inspect"))
    ap.add_argument("--cpu",type=int,required=True); ap.add_argument("--host",type=Path)
    ap.add_argument("--stock",type=Path); ap.add_argument("--stock-sha256")
    ap.add_argument("--out",type=Path); ap.add_argument("--dedicated-target",action="store_true")
    ap.add_argument("--cloud-diagnostic",action="store_true",
                    help="12-triplet diagnostic only; never qualifies representative hardware")
    opt=ap.parse_args()
    info=snapshot(opt.cpu)
    if opt.mode=="inspect":
        print(json.dumps(info,indent=2,sort_keys=True)); return
    if not (opt.host and opt.stock and opt.stock_sha256 and opt.out):
        ap.error("smoke/collect need --host --stock --stock-sha256 --out")
    check_target(info,opt.mode=="collect",opt.dedicated_target,opt.cloud_diagnostic)
    acquisition=("cloud-diagnostic" if opt.cloud_diagnostic else "dedicated-target" if opt.dedicated_target else "untimed-smoke")
    host=opt.host.resolve(strict=True); stock=opt.stock.resolve(strict=True)
    need(stock.is_file() and host.is_file() and os.access(host,os.X_OK),"native host and module required")
    need(bool(re.fullmatch("[a-f0-9]{64}",opt.stock_sha256)),"lowercase stock SHA-256 required")
    ids = identify(host), identify(stock)
    need(ids[1][-1]==opt.stock_sha256,"stock SHA-256 mismatch")
    out=opt.out.resolve(); out.mkdir() # never resume, reuse, or overwrite a session
    session=str(uuid.uuid4()); summary={"schema":SCHEMA,"session_id":session,"complete":False,"triplets":[],
                                       "acquisition_class":acquisition}
    try:
        write(out/"plan.json",{"schema":SCHEMA,"session_id":session,"mode":opt.mode,"stock_sha256":ids[1][-1],
              "host_sha256":ids[0][-1],"collector_sha256":digest(Path(__file__)),
              "dedicated_target_attestation":opt.dedicated_target,"acquisition_class":acquisition,
              "target":info,"triplets":TRIPLETS,
              "schedule":[[m,orders] for m,orders in map(schedule,range(TRIPLETS))]})
        _,gold = capture(host,stock,ids,out,"reference",0,opt.cpu,session,None)
        capture(host,stock,ids,out,"untimed",0,opt.cpu,session,gold)
        if opt.mode=="collect":
            for i in range(TRIPLETS):
                obs,_=capture(host,stock,ids,out,"measure",i,opt.cpu,session,gold)
                summary["triplets"].append(obs)
            summary["complete"]=True
        summary["mode"]=opt.mode
        write(out/"summary.json",summary)
    except BaseException as e:
        summary["error"]=f"{type(e).__name__}: {e}"
        if not (out/"summary.json").exists():
            write(out/"summary.json",summary)
        raise
    print(f"{opt.mode}: verified {len(summary['triplets'])} timed triplets; see {out/'summary.json'}")

if __name__ == "__main__":
    try:
        cli()
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        sys.exit(str(exc))
