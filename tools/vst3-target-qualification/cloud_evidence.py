#!/usr/bin/env python3
"""Export/replay the cloud diagnostic's complete clock rows; no native or clock calls."""
import argparse
import csv
import gzip
import hashlib
import io
import json
import sys
from pathlib import Path

import analyze
import run as q

SLOTS = ("reference", "untimed", *(f"triplet-{i:02d}" for i in range(q.TRIPLETS)))

def mode_index(slot):
    return ("measure", int(slot[-2:])) if slot.startswith("triplet-") else (slot, 0)

def load(p):
    return json.loads(p.read_text(encoding="utf-8"),object_pairs_hook=analyze.strict_object)

def encode(tables, fields):
    out=io.StringIO(newline="")
    writer=csv.DictWriter(out,("slot",*fields),lineterminator="\n")
    writer.writeheader()
    for slot in SLOTS:
        for row in tables[slot]:
            writer.writerow({"slot":slot,**row})
    return gzip.compress(out.getvalue().encode("ascii"),compresslevel=9,mtime=0)

def decode(p, fields):
    out={s:[] for s in SLOTS}; previous=0
    with gzip.open(p,"rt",encoding="ascii",newline="") as f:
        reader=csv.DictReader(f)
        q.need(tuple(reader.fieldnames or ())==("slot",*fields),"normalized header")
        for row in reader:
            q.need(set(row)=={"slot",*fields} and None not in row.values(),"normalized field count")
            slot=row.pop("slot")
            q.need(slot in out and SLOTS.index(slot)>=previous,"missing or reordered process")
            previous=SLOTS.index(slot); out[slot].append(row)
    return out

def export(src,dest):
    summary=load(src/"summary.json"); plan=load(src/"plan.json")
    q.need(summary.get("complete") is True and plan.get("acquisition_class")==summary.get("acquisition_class")=="cloud-diagnostic"
           and plan.get("dedicated_target_attestation") is False,"only completed cloud diagnostics")
    dest.mkdir()
    clocks={}; system={}; session={"schema":"cs80-stock-cloud-portable-v1",
        "session_id":summary["session_id"],"host_sha256":plan["host_sha256"],"stock_sha256":plan["stock_sha256"],
        "collector_sha256":plan["collector_sha256"],"target":plan["target"],"slots":{},
        "native_artifacts_included":False}
    first_wall=None
    for slot in SLOTS:
        mode,index=mode_index(slot); base=src/slot; capture=base/"capture"
        raw=q.rows(capture/"clocks.csv",q.RAW_FIELDS); os_rows=q.rows(capture/"os.csv",q.OS_FIELDS)
        q.validate_raw(raw,os_rows,mode,index)
        if first_wall is None:
            first_wall=int(os_rows[0]["wall_start_ns"])-1
        first_cpu=next((int(r["cpu_start_ns"])-1 for r in raw if int(r["cpu_start_ns"])),0)
        for r in raw:
            for field in ("wall_start_ns","wall_end_ns","cpu_start_ns","cpu_end_ns"):
                value=int(r[field])
                if value:
                    r[field]=str(value-(first_wall if field.startswith("wall_") else first_cpu))
        for r in os_rows:
            for field in ("wall_start_ns","wall_end_ns"):
                r[field]=str(int(r[field])-first_wall)
        clocks[slot]=raw; system[slot]=os_rows
        identity=load(capture/"identity.json")
        # Paths never enter the export; omit device and inode of the host filesystem.
        identity.pop("module_dev"); identity.pop("module_ino")
        original=load(base/"integrity.json")
        q.need(q.observations(capture,identity["lanes"])==original["public_artifacts_by_lane"],
               "actual lane capture differs from original receipt")
        q.need(original["raw_clock_sha256"]==q.digest(capture/"clocks.csv") and
               original["raw_os_sha256"]==q.digest(capture/"os.csv"),"original clock SHA mismatch")
        session["slots"][slot]={"identity":identity,"external":load(base/"child.json"),
            "integrity":original, "stdout_bytes":(base/"stdout").stat().st_size,
            "stderr_bytes":(base/"stderr").stat().st_size}
    for file,data in (("clocks.csv.gz",encode(clocks,q.RAW_FIELDS)),("os.csv.gz",encode(system,q.OS_FIELDS))):
        (dest/file).write_bytes(data)
    session["normalized_sha256"]={name:q.digest(dest/name) for name in ("clocks.csv.gz","os.csv.gz")}
    # Preserve the exact collector summary bytes.
    (dest/"summary.json").write_bytes((src/"summary.json").read_bytes())
    q.write(dest/"measurement.json",session)
    q.write(dest/"decision.json",analyze.analyze(summary))
    return replay(dest)

def replay(dest):
    session=load(dest/"measurement.json"); summary=load(dest/"summary.json")
    q.need(session["schema"]=="cs80-stock-cloud-portable-v1" and summary["acquisition_class"]=="cloud-diagnostic"
           and summary["session_id"]==session["session_id"],"wrong evidence scope")
    for file in ("clocks.csv.gz","os.csv.gz"):
        q.need(q.digest(dest/file)==session["normalized_sha256"][file],"normalized clock SHA mismatch")
    raw=decode(dest/"clocks.csv.gz",q.RAW_FIELDS); system=decode(dest/"os.csv.gz",q.OS_FIELDS)
    q.need(set(session["slots"])==set(SLOTS),"missing slot")
    reference=session["slots"]["reference"]["integrity"]["public_artifacts_by_lane"][0]
    q.need(set(reference)=={*q.FILES,"controls.csv"} and
           all(isinstance(h,str) and len(h)==64 and all(c in "0123456789abcdef" for c in h) for h in reference.values()),
           "invalid isolated reference receipt")
    observations=[]; measured=0; total_rows=0; end=0
    for slot in SLOTS:
        mode,index=mode_index(slot); entry=session["slots"][slot]; meta=entry["identity"]; receipt=entry["integrity"]
        num=1 if mode=="reference" else 3
        q.need(meta["mode"]==mode and meta["lanes"]==num and meta["cpu"]==session["target"]["cpu"]
               and receipt["mode"]==mode and receipt["triplet_index"]==index,"unexpected slot identity")
        q.need(meta["warm_calls_per_lane"]==1024 and meta["window_calls_per_lane"]==544
               and meta["continuation_calls_per_lane"]==1 and meta["sample_rate"]==48000 and meta["block_size"]==64,
               "unexpected workload")
        mapping,orders=q.schedule(index)
        expected_schedule=("\n".join([mapping,*orders])+"\n").encode("ascii")
        q.need(mapping==receipt["actual_physical_mapping"] and
               hashlib.sha256(expected_schedule).hexdigest()==receipt["schedule_sha256"],"unexpected schedule")
        q.need(receipt["public_artifacts_by_lane"]==[reference]*num,"public lane receipt mismatch")
        row=q.validate_raw(raw[slot],system[slot],mode,index)
        ext=entry["external"]
        q.need(ext["returncode"]==0 and sum(row["cpu_ns"].values())<=ext["cpu_ns"]+2000000,"external CPU envelope")
        start=int(system[slot][0]["wall_start_ns"]); stop=int(system[slot][-1]["wall_end_ns"])
        q.need(end < start <= stop and stop-start<=ext["wall_ns"],"cross-process wall envelope")
        end=stop
        if mode=="measure":
            observations.append(row)
            total_rows+=len(raw[slot]); measured+=sum(row["measured_calls"].values())
    q.need(summary["triplets"]==observations and summary["complete"] is True and len(observations)==12,
           "collector summary differs from complete normalized raw evidence")
    result=analyze.analyze(summary)
    q.need(result==load(dest/"decision.json"),"decision not reproducible from raw clocks")
    return {"schema":"cs80-stock-cloud-replay-v1","processes":len(SLOTS),"complete_triplets":len(observations),
            "measured_phase_rows":total_rows,"timed_callbacks":measured,"untimed_catchup_rows":total_rows-measured,
            "gates_pass":result["gates_pass"],"dedicated_target_qualified":result["qualified"],
            "native_artifacts_reexecuted":False}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    subs=ap.add_subparsers(dest="operation",required=True)
    p=subs.add_parser("replay"); p.add_argument("evidence",type=Path)
    p=subs.add_parser("export"); p.add_argument("session",type=Path); p.add_argument("new_evidence",type=Path)
    args=ap.parse_args()
    result=export(args.session,args.new_evidence) if args.operation=="export" else replay(args.evidence)
    print(json.dumps(result,indent=2))
if __name__=="__main__":
    try:
        main()
    except (ValueError,OSError,KeyError,TypeError,IndexError) as exc:
        sys.exit(f"invalid evidence: {exc}")
