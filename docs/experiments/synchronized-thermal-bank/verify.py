"""Verify the archived prefix and derive bounds without loading or timing a synth."""
import csv
import hashlib
import io
import json
import math
import random
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
CASES = ("factory-quiet", "panel-quiet", "factory-held", "factory-moving",
         "panel-held", "pure-held", "legacy-held")
HEADER = ("family", "block", "case", "slot", "label", "arm", "cpu_start",
          "cpu_end", "render_thread_ns", "clock_pair_ns", "pcm_fnv64",
          "nonzero_samples", "active_voice_sample_blocks")
PLAN_SHA = "da19880c27be31ce2f3b7bf15db1b07d443db994f6522c2ce1174078b4e6f0af"
ROW_SHA = "43c0ac984667e0d4737ae58a1399a09ee97e005e64c32c0acc0ec6db427828d4"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def read_rows(raw, cpu):
    stream = csv.DictReader(io.StringIO(raw.decode("ascii"), newline=""))
    require(tuple(stream.fieldnames or ()) == HEADER, "unexpected CSV columns")
    rows = list(stream)
    schedule = [(family, str(block), case, str(slot), label,
                 "candidate" if family == "comparison" and label == "B" else "stock")
                for family, count in (("aa", 2), ("comparison", 6))
                for block in range(count) for case in CASES
                for slot, label in enumerate("ABBA")]
    require(len(schedule) == 224 and len(rows) == 187, "not the archived 187/224 prefix")
    data, output = {}, {}
    primary_clock = 0.0
    for row, expected in zip(rows, schedule):
        require(None not in row and None not in row.values(), "malformed row")
        require(tuple(row[key] for key in HEADER[:6]) == expected, "slot not in frozen order")
        require(row["cpu_start"] == row["cpu_end"] == str(cpu), "changed CPU")
        ns, clk = int(row["render_thread_ns"]), int(row["clock_pair_ns"])
        require(ns > clk > 0, "invalid clock totals")
        case = row["case"]
        h = row["pcm_fnv64"]
        nz, occ = int(row["nonzero_samples"]), int(row["active_voice_sample_blocks"])
        require(re.fullmatch(r"[0-9a-f]{16}", h), "invalid PCM identity")
        quiet = case in CASES[:2]
        require(nz == 0 if quiet else 0 < nz <= 131072, "unexpected signal occupancy")
        require(occ == (0 if quiet else 8192), "unexpected voice occupancy")
        require(output.setdefault(case, (h, nz, occ)) == (h, nz, occ),
                "within-case PCM/occupancy changed")
        data.setdefault((row["family"], int(row["block"]), case), []).append(ns / 65536)
        if quiet:
            primary_clock = max(primary_clock, clk / ns)
    return data, output, primary_clock


def block_logs(data, family, cases):
    # Deliberately identical log arithmetic/order to the original timing runner.
    ans = []
    for i in range(2 if family == "aa" else 6):
        terms = []
        for c in cases:
            v = data.get((family, i, c), ())
            if len(v) != 4:
                break
            terms.append((math.log(v[0]) + math.log(v[3])
                          - math.log(v[1]) - math.log(v[2])) / 2)
        ans.append(sum(terms) / len(cases) if len(terms) == len(cases) else None)
    return ans


def gain(z):
    return -math.expm1(-z)


def optimistic_lower(logs, draws):
    # A missing log gain can be arbitrarily favorable. Draws omitting all missing
    # blocks are fixed; move every other draw above them. The sorted order
    # statistics, linear interpolation and gain transformation are monotone.
    fixed = sorted(sum(logs[i] for i in d) / 6 for d in draws
                   if all(logs[i] is not None for i in d))
    position = 0.025 * (len(draws) - 1)
    j = int(position)
    require(len(fixed) > j + 1, "insufficient fixed draws for this proof")
    return gain(fixed[j] + (fixed[j+1] - fixed[j]) * (position-j)), len(fixed)


def verify(root=HERE):
    raw, raw_plan = (root/"bench/raw.csv").read_bytes(), (root/"bench/plan.json").read_bytes()
    qualification = json.loads((root/"qualification.json").read_bytes())
    require(digest(raw) == ROW_SHA == qualification["observed_rows_sha256"], "changed raw CSV")
    require(digest(raw_plan) == PLAN_SHA == qualification["recorded_timing_plan_sha256"],
            "changed frozen plan")
    plan = json.loads(raw_plan)
    require(tuple(plan["cases"]) == CASES, "changed case order")
    for path, sha in plan["sha256"].items():
        if path not in ("bench/stock", "bench/candidate"):  # executables are not distributed
            require(digest((root/path).read_bytes()) == sha, "changed " + path)
    for filename, key in (("trace.cpp", "trace_driver_sha256"),
                          ("exact-bank.cpp", "drift_differential_driver_sha256")):
        require(digest((root/filename).read_bytes()) == qualification[key], "changed " + filename)
    data, output, clock = read_rows(raw, plan["cpu"])
    rng = random.Random(20261118)
    draws = [rng.choices(range(6), k=6) for _ in range(10000)]
    logs = block_logs(data, "comparison", CASES[:2])
    require([x is None for x in logs] == [False]*5+[True], "unexpected missing primary blocks")
    aa = block_logs(data, "aa", CASES[:2])
    require(None not in aa, "incomplete A/A")
    floor = max(0.02, max(abs(gain(x)) for x in aa), 3*clock)
    pooled, nfixed = optimistic_lower(logs, draws)
    factory, _ = optimistic_lower(block_logs(data, "comparison", CASES[:1]), draws)
    require(pooled < floor and factory < 0, "admission not ruled out")
    return dict(planned_slots=224, recovered_slots=187, missing_slots=37,
                unchanged_output_identity_cases=len(output), draws=10000,
                fixed_primary_draws=nfixed, minimum_primary_gate_floor=floor,
                best_possible_pooled_lower95=pooled,
                best_possible_factory_quiet_lower95=factory,
                measured_full_run_gain=None, measured_full_run_interval=None,
                native_study_admitted=False)


if __name__ == "__main__":
    print(json.dumps(verify(), indent=2))
