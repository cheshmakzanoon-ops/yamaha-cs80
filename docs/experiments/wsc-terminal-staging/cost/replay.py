#!/usr/bin/env python3
"""Independent, standard-library-only OFFLINE cost replay. Never runs DSP.

Only the eight adjacent evidence files are read. No original collector/analyzer,
host paths, executable, compiler, profiler, native module or network is used.
"""
import argparse
import copy
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import random

SCENES = ["factory-held", "factory-moving", "panel-moving", "pure-held"]
ROLES = {"B": "BASE", "S": "SCALAR-STAGED", "V": "VECTOR-STAGED"}
PERMUTATIONS = ["BSV", "SVB", "VBS", "BVS", "SBV", "VSB"]
RAW_SHA256 = "7ed2ff80405b0d15f662ec26006fa3c62ffdd7e711a44e80e7b049b9417f2b46"
DRAW_SHA256 = "f42afd24a357904f93cdd405d800b345f1318f96e4233df259a48902611cf38a"
EMPTY_SHA256 = hashlib.sha256(b"").hexdigest()
FP_NAMES = ["mxcsr", "x87_cw", "x87_sw", "errno"]
PHASES = ["initial", "measured", "continuation"]
SLOT_FIELDS = ["slot", "family", "round", "scene", "position", "label", "actual_arm"]
FIELDS = SLOT_FIELDS + ["cpu_ns", "empty_cpu_ns", "cpu_start", "cpu_end", "slot_wall_s", "frame",
    "measured_last_pcm", "continuation_pcm"] + [p+"_"+f for p in PHASES for f in FP_NAMES] + ["stdout_sha256", "stderr_sha256"]
INTEGER_FIELDS = {"slot", "round", "position", "cpu_ns", "empty_cpu_ns", "cpu_start", "cpu_end"} | {p+"_"+f for p in PHASES for f in FP_NAMES}
FILES = {"README.md", "observations.csv", "protocol.json", "provenance.json", "replay.py", "result.json", "tests.json", "manifest.json"}


def require(ok, reason):
    if not ok: raise ValueError(reason)


def digest(data): return hashlib.sha256(data).hexdigest()


def compact(value): return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode()


def parse_json(text):
    def invalid(value): raise ValueError("invalid JSON constant: " + value)
    return json.loads(text, parse_constant=invalid)


def expected_schedule():
    rows = []
    for family, rounds in [("calibration", 3), ("comparison", 6)]:
        for number in range(rounds):
            permutation = PERMUTATIONS[number]
            offset = (number + (family == "comparison")) % 4
            for scene in SCENES[offset:] + SCENES[:offset]:
                for position, label in enumerate(permutation + permutation[::-1]):
                    rows.append(dict(slot=len(rows), family=family, round=number, scene=scene,
                                     position=position, label=label, actual_arm="BASE" if family=="calibration" else ROLES[label]))
    return rows


def parse_csv(data):
    reader = csv.DictReader(io.StringIO(data.decode("utf-8"), newline=""))
    require(reader.fieldnames == FIELDS, "CSV column schema/order differs")
    records = []
    for flat in reader:
        require(None not in flat and all(x is not None for x in flat.values()), "CSV row width differs")
        for field in INTEGER_FIELDS:
            value = int(flat[field]); require(str(value) == flat[field], "noncanonical integer")
            flat[field] = value
        flat["slot_wall_s"] = float(flat["slot_wall_s"])
        identity = {k: flat.pop(k) for k in ("frame", "measured_last_pcm", "continuation_pcm")}
        for phase in PHASES:
            identity[phase+"_fp"] = {name: flat.pop(phase+"_"+name) for name in FP_NAMES}
        flat["identity"] = identity
        records.append(flat)
    return records


def original_raw_bytes(rows):
    # The archived collector's exact fieldwise JSONL representation. All fields,
    # including full-precision wall values and fingerprints, survive normalization.
    return ("".join(json.dumps(row, sort_keys=True, allow_nan=False)+"\n" for row in rows)).encode()


def original_stdout(row):
    # Normal cost.cpp printf order, compact JSON, no diagnostic/observer fields.
    output = dict(schema="lane-core-cost-v1", scene=row["scene"], diagnostic=False, observers=False, timed=True,
                  cpu_start=row["cpu_start"], cpu_end=row["cpu_end"], cpu_ns=row["cpu_ns"], empty_cpu_ns=row["empty_cpu_ns"],
                  warm_blocks=1024, measured_blocks=512, block_frames=64, identity=row["identity"])
    return (json.dumps(output, separators=(",", ":"), allow_nan=False)+"\n").encode()


def validate_protocol(protocol):
    require(protocol["schema"] == "wsc-portable-cost-protocol-v1" and protocol["mode"] == "three-arm", "not the completed three-arm protocol")
    require(protocol["primary"] == SCENES[0] and protocol["scenes"] == SCENES, "post-hoc endpoint substitution")
    require(compact(protocol["schedule"]) == compact(expected_schedule()), "frozen complete schedule differs")
    require(protocol["thresholds"] == dict(minimum_gain=.02, historical_policy_floor=.117, secondary_noninferiority=-.03,
                calibration="maximum absolute individual round/pair effect across every scene", clock_multiplier=3,
                primary_comparison="B_to_V", primary_relation="lower95 strictly greater than global floor"), "threshold/rule changed")
    require(protocol["bootstrap"] == dict(round_clusters=6, draws=10000, seed=20261118017,
                unit="whole comparison round, all scenes and contrasts together", percentile=[.025,.975],
                quantiles="linear interpolation", shared_index_sha256=DRAW_SHA256), "bootstrap changed")
    require(protocol["limits"] == dict(slot_cpu_s=10, slot_wall_s=12, campaign_wall_s=600), "fixed limits changed")
    require(protocol["settings"]["rate"] == 48000 and protocol["settings"]["block_frames"] == 64
            and protocol["settings"]["warm_blocks"] == 1024 and protocol["settings"]["measured_blocks"] == 512
            and protocol["settings"]["continuation_blocks"] == 1
            and protocol["settings"]["normal_frame_before_new_midi_every_block"] is True, "actual fixture/window differs")
    for fixture in protocol["fixture_sources"].values():
        require(digest(fixture["text"].encode()) == fixture["sha256"], "embedded fixture source changed")


def validate_rows(rows, protocol):
    validate_protocol(protocol)
    require(len(rows) == 216, "incomplete/padded campaign: no prefix estimate")
    for row, slot in zip(rows, expected_schedule()):
        require(all(type(row.get(k)) is type(v) and row[k] == v for k,v in slot.items()), "slot/arm/order mismatch")
        require(type(row["cpu_ns"]) is int and type(row["empty_cpu_ns"]) is int
                and 0 < row["empty_cpu_ns"] < row["cpu_ns"] <= 10_000_000_000, "invalid raw CPU clocks")
        require(row["cpu_start"] == row["cpu_end"] == protocol["cpu"] == 0, "wrong CPU/migration")
        require(math.isfinite(row["slot_wall_s"]) and 0 <= row["slot_wall_s"] <= 12, "slot wall bound differs")
        require(row["identity"] == protocol["expected_identity"][row["scene"]], "final PCM/continuation/caller-FP identity differs")
        require(row["stderr_sha256"] == EMPTY_SHA256 and digest(original_stdout(row)) == row["stdout_sha256"], "original normal stdout/stderr digest mismatch")
    require(digest(original_raw_bytes(rows)) == RAW_SHA256, "normalized observations no longer reconstruct the exact original raw evidence")


def validate_provenance(provenance, protocol):
    require(provenance["original_records"]["raw_jsonl_sha256"] == RAW_SHA256, "original raw commitment differs")
    sources = provenance["sources"]
    base = sources["base_inventory"]
    for label in "BSV":
        files = dict(base)
        if label != "B":
            for path in sources["deltas"][label]["removed"]: del files[path]
            files.update(sources["deltas"][label]["changed_or_added"])
        require(len(files) == provenance["arms"][label]["source_file_count"]
                and digest(compact(files)) == provenance["arms"][label]["source_inventory_sha256"], "complete source inventory digest mismatch")
    require(provenance["independent_export_audit"]["original_attempts_checked"] == 216
            and provenance["independent_export_audit"]["original_stdout_stderr_pairs_checked"] == 216,
            "original-record audit scope incomplete")
    require(provenance["diagnostic_efficacy"] == protocol["diagnostic_efficacy"], "diagnostic efficacy provenance differs")


def estimate(rows, protocol):
    """Independent transpose/group implementation; no imported study code."""
    # A single manually generated uniform matrix, shared for all endpoints.
    rng = random.Random(20261118017)
    draws = [[int(rng.random()*6) for _ in range(6)] for _ in range(10000)]
    require(digest(compact(draws)) == DRAW_SHA256, "PRNG sequence incompatible")
    groups = {(family, number, scene, label): [] for family,n in [("calibration",3),("comparison",6)]
              for number in range(n) for scene in SCENES for label in "BSV"}
    for row in rows: groups[row["family"],row["round"],row["scene"],row["label"]].append(math.log(row["cpu_ns"]))
    require(all(len(v)==2 for v in groups.values()), "two actual observations per arm/round required")
    means = {key: math.fsum(values)/2 for key,values in groups.items()}
    def summary(logs, tax=False):
        transform = math.expm1 if tax else lambda x: -math.expm1(x)
        sampled = sorted(transform(math.fsum(logs[i] for i in chosen)/6) for chosen in draws)
        bounds=[]
        for probability in [.025,.975]:
            position=9999*probability; lo=int(position); fraction=position-lo
            bounds.append(sampled[lo]+(sampled[min(lo+1,9999)]-sampled[lo])*fraction)
        return dict(estimate=transform(math.fsum(logs)/6), ci95=bounds,
                    round_log_contrast=logs, round_effect=[transform(x) for x in logs])
    cases={}; calibration=[]
    for scene in SCENES:
        round_means=[{label: means["comparison",r,scene,label] for label in "BSV"} for r in range(6)]
        effects={}
        for name,left,right,tax in [("staging_tax","S","B",True), ("vector_arithmetic_saving","V","S",False), ("net_vector_saving","V","B",False)]:
            effects[name]=summary([m[left]-m[right] for m in round_means],tax)
        cal=[]
        for r in range(3):
            for first,second in [("B","S"),("S","V"),("B","V")]:
                cal.append(dict(scene=scene, round=r, pair=first+"_to_"+second,
                    apparent_saving=-math.expm1(means["calibration",r,scene,second]-means["calibration",r,scene,first])))
        calibration.extend(cal)
        rtfs={}
        duration_ns=(512*64/48000)*1e9
        for label in "BSV":
            logs=[m[label] for m in round_means]
            geomean_ns=math.exp(math.fsum(logs)/6)
            rtfs[label]=dict(comparison_round_geomean_cpu_ns=[math.exp(x) for x in logs],
                comparison_round_rtf=[math.exp(x)/duration_ns for x in logs],
                comparison_six_round_geomean_cpu_ns=geomean_ns,
                comparison_six_round_geomean_rtf=geomean_ns/duration_ns,
                observations=12, rounds=6, target_0_15_met=geomean_ns/duration_ns <= .15)
        positions={}
        for family,n in [("calibration",3),("comparison",6)]:
            centered=[[] for _ in range(6)]; pairs=[]
            for number in range(n):
                group=sorted((row for row in rows if (row["family"],row["round"],row["scene"])==(family,number,scene)), key=lambda r:r["position"])
                center=math.fsum(math.log(row["cpu_ns"]) for row in group)/6
                for row in group:centered[row["position"]].append(math.log(row["cpu_ns"])-center)
                pair={}
                for label in "BSV":
                    both=[row for row in group if row["label"]==label]
                    pair[label]=math.log(both[1]["cpu_ns"])-math.log(both[0]["cpu_ns"])
                pairs.append(pair)
            positions[family]=dict(mean_centered_log_ns_by_position=[math.fsum(v)/n for v in centered], later_minus_earlier_by_arm_each_round=pairs)
        cases[scene]=dict(round_mean_log_cpu_ns=round_means, effects=effects, calibration_round_pair_effects=cal,
                         rtf=rtfs, positions=positions)
    aa=max(abs(c["apparent_saving"]) for c in calibration)
    clock=max(row["empty_cpu_ns"]/row["cpu_ns"] for row in rows)
    floor=max(.02,.117,aa,3*clock)
    primary=cases[SCENES[0]]["effects"]["net_vector_saving"]["ci95"][0]>floor
    controls={scene:cases[scene]["effects"]["net_vector_saving"]["ci95"][0]>=-.03 for scene in SCENES[1:]}
    return dict(schema="wsc-portable-cost-independent-result-v1", offline_replay=True, raw_sha256=RAW_SHA256,
        slots=216, calibration_slots=72, comparison_slots=144, comparison_round_clusters=6,
        per_scene=dict(calibration_baseline_processes=18, comparison_processes_per_arm=12),
        bootstrap=dict(seed=20261118017,draws=10000,shared_index_sha256=DRAW_SHA256),
        floor_components=dict(minimum=.02,historical_policy_only=.117,current_global_calibration_max=aa,
                              three_times_global_empty_clock_fraction=3*clock),
        primary_superiority_floor=floor, max_empty_clock_fraction=clock,
        calibration_max_records=[c for c in calibration if abs(c["apparent_saving"])==aa],
        primary="factory-held", primary_comparison="B_to_V", primary_requirement_passes=primary,
        secondary_requirements_pass=controls, threshold_gate_passes=primary and all(controls.values()),
        decision="inconclusive / no performance admission in this environment" if not(primary and all(controls.values())) else "core threshold passed only; native gate separate",
        scalar_is_not_a_substitute_primary=True, scalar_alternative_admitted=False,
        rtf_definition="exp(mean over six comparison rounds of each arm's two-observation mean log cpu_ns)) / (1e9 * 32768/48000); no calibration data pooled",
        rtf_absolute_engineering_target=.15, rtf_target_is_not_relative_superiority_floor=True,
        relative_effect_units="fractions; positive tax is slower, positive saving is faster",
        scope="whole core + authentic frame/MIDI work on this VM, not native plugin/DAW CPU or realtime capacity",
        scenes=cases, diagnostic_efficacy=protocol["diagnostic_efficacy"])


def check_hash(data, expected): require(digest(data)==expected, "manifest file digest mismatch")


def verify_package(directory):
    manifest=parse_json((directory/"manifest.json").read_text())
    require(manifest["schema"]=="wsc-portable-cost-manifest-v1" and set(manifest["files"])==FILES-{"manifest.json"}, "manifest inventory differs")
    for name,record in manifest["files"].items():
        data=(directory/name).read_bytes();require(len(data)==record["bytes"], "manifest file length differs");check_hash(data,record["sha256"])
    protocol=parse_json((directory/"protocol.json").read_text())
    provenance=parse_json((directory/"provenance.json").read_text())
    rows=parse_csv((directory/"observations.csv").read_bytes())
    validate_rows(rows,protocol);validate_provenance(provenance,protocol)
    result=estimate(rows,protocol)
    require(result==parse_json((directory/"result.json").read_text()), "independent exact estimator does not match saved result")
    return rows,protocol,result


def self_test(rows,protocol):
    names=[]
    def reject(name,fn):
        try:fn()
        except (ValueError,KeyError,TypeError):names.append(name);return
        raise AssertionError("tamper accepted: "+name)
    reject("missing row",lambda:validate_rows(rows[:-1],protocol))
    reject("extra row",lambda:validate_rows(rows+[rows[-1]],protocol))
    for field,value in [("slot",999),("label","S"),("actual_arm","VECTOR-STAGED"),("position",3),
                        ("cpu_ns",0),("empty_cpu_ns",rows[0]["cpu_ns"]),("cpu_end",1),("slot_wall_s",13.)]:
        wrong=copy.deepcopy(rows);wrong[0][field]=value
        reject("changed "+field,lambda wrong=wrong:validate_rows(wrong,protocol))
    wrong=copy.deepcopy(rows);wrong[0],wrong[1]=wrong[1],wrong[0]
    reject("reordered observations",lambda:validate_rows(wrong,protocol))
    wrong=copy.deepcopy(rows);wrong[0]["cpu_ns"]+=1
    # Even recomputing the row's stdout hash cannot escape the raw commitment.
    wrong[0]["stdout_sha256"]=digest(original_stdout(wrong[0]))
    reject("coherently rehashed altered CPU sample",lambda:validate_rows(wrong,protocol))
    wrong=copy.deepcopy(rows);wrong[0]["identity"]["continuation_fp"]["mxcsr"]^=32
    reject("caller FP corruption",lambda:validate_rows(wrong,protocol))
    for key,value in [("primary","factory-moving"),("mode","scalar-only")]:
        changed=copy.deepcopy(protocol);changed[key]=value
        reject("post-hoc "+key,lambda changed=changed:validate_protocol(changed))
    changed=copy.deepcopy(protocol);changed["thresholds"]["historical_policy_floor"]=.02
    reject("lowered noise policy",lambda:validate_protocol(changed))
    changed=copy.deepcopy(protocol);changed["bootstrap"]["round_clusters"]=144
    reject("process-slot pseudoreplication",lambda:validate_protocol(changed))
    changed=copy.deepcopy(protocol);changed["bootstrap"]["seed"]+=1
    reject("changed bootstrap seed",lambda:validate_protocol(changed))
    changed=copy.deepcopy(protocol);next(iter(changed["fixture_sources"].values()))["text"]+="\n"
    reject("changed fixture source",lambda:validate_protocol(changed))
    reject("manifest byte corruption",lambda:check_hash(b"changed",digest(b"original")))
    # Truly fabricated estimator vectors: no renderer, file or elapsed-clock data.
    fabricated=[]
    for row in expected_schedule():
        ratio={"B":1.,"S":1.1,"V":.77}[row["label"]] if row["family"]=="comparison" else 1.
        fabricated.append({**row,"cpu_ns":round(1e9*ratio),"empty_cpu_ns":100})
    synthetic=estimate(fabricated,protocol)
    for scene in SCENES:
        e=synthetic["scenes"][scene]["effects"]
        require(abs(e["staging_tax"]["estimate"]-.10)<1e-12 and abs(e["vector_arithmetic_saving"]["estimate"]-.30)<1e-12
                and abs(e["net_vector_saving"]["estimate"]-.23)<1e-12,"synthetic direction/ratio attribution failed")
    names.append("fabricated 10% tax, 30% arithmetic saving, 23% net: ratios multiply")
    require(synthetic["threshold_gate_passes"],"fabricated positive gate failed")
    for row in fabricated:
        if row["family"]=="comparison" and row["scene"]=="pure-held" and row["label"]=="V":row["cpu_ns"]=1_100_000_000
    require(not estimate(fabricated,protocol)["threshold_gate_passes"],"negative secondary gate ignored")
    names.append("fabricated secondary regression veto")
    return dict(schema="wsc-portable-cost-tests-v1", passed=True, offline_only=True,
                renderer_invocations=0, compiler_invocations=0, network_requests=0,
                tests=names, count=len(names), actual_observation_count=216)


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test",action="store_true",help="verify package then exercise in-memory corruption and synthetic vectors")
    args=parser.parse_args();root=Path(__file__).resolve().parent
    rows,protocol,result=verify_package(root)
    if args.self_test:
        tests=self_test(rows,protocol)
        require(tests==parse_json((root/"tests.json").read_text()),"saved test receipt differs")
        print(json.dumps(tests,indent=2))
    else:
        primary=result["scenes"]["factory-held"]["effects"]["net_vector_saving"]
        print(json.dumps(dict(verified=True,observations=216,original_raw_sha256=RAW_SHA256,
            primary_net_saving=primary,superiority_floor=result["primary_superiority_floor"],
            secondary_requirements_pass=result["secondary_requirements_pass"],
            decision=result["decision"],renderer_invocations=0),indent=2))
