#!/usr/bin/env python3
"""Compare all six untrimmed 51-row drift-experiment CPU rounds.

All fixture keys, checksum/peak/RMS, ranks, events and other non-time fields
must match across *all* before and after rounds. No minimum-of-three timing
selection and no outlier removal; report per-round and paired-preset medians,
Legacy anchors, missed budgets and each worst p99 with exact raw provenance.
Print JSON only; do not modify the completed measurement folders.
"""

import argparse
import csv
import hashlib
import json
import math
import pathlib
import statistics

KEYS = ("rate", "block", "isolation", "scenario", "initial_selector",
        "phase", "mode", "source_performance")
TIMING = set("min_us median_us p95_us p99_us max_us mean_rtf median_rtf "
             "p99_rtf max_rtf budget_exceedances process_cpu_seconds_including_checks "
             "thread_cpu_seconds_including_checks event_median_us event_max_us".split())


def round_data(prefix, i):
    directory = pathlib.Path(f"{prefix}{i}")
    meta_file = directory / "metadata.json"
    result_file = directory / "results.csv"
    complete_file = directory / "completed.json"
    metadata = json.loads(meta_file.read_text())
    completed = json.loads(complete_file.read_text())
    with result_file.open(newline="") as f:
        rows = list(csv.DictReader(f))
    if not completed.get("completed") or completed.get("cases") != 27 or len(rows) != 51:
        raise ValueError(f"{directory}: incomplete or noncanonical 27-case/51-row fixture")
    if (metadata.get("fixture_id") != "source-core-timing-v2" or
            metadata.get("suite") != "baseline" or
            metadata.get("callbacks_per_held_phase") != 1024 or
            metadata.get("warmup_seconds") != 1):
        raise ValueError(f"{directory}: source factory fixture/warmup changed")
    if any(r["rate"] != "48000" or r["block"] != "256" for r in rows):
        raise ValueError(f"{directory}: rate or block changed")
    keys = [tuple(row[k] for k in KEYS) for row in rows]
    if len(set(keys)) != 51:
        raise ValueError(f"{directory}: duplicate fixture key")
    if any(not math.isfinite(float(row[t])) for row in rows
           for t in ("median_rtf", "p99_rtf", "checksum", "peak", "rms")):
        raise ValueError(f"{directory}: nonfinite timing or audio checksum")
    shas = {name: hashlib.sha256((directory / name).read_bytes()).hexdigest()
            for name in ("results.csv", "metadata.json", "completed.json")}
    return {"path": str(directory), "source_commit": metadata["source_commit_caller_verified"],
            "started_unix_seconds": metadata["started_unix_seconds"],
            "source_fingerprint": metadata["source_fingerprint"],
            "sha256": shas, "rows": dict(zip(keys, rows))}


def med(values):
    return statistics.median(values)


def stats(rounds):
    result = []
    for item in rounds:
        rows = list(item["rows"].values())
        held = [row for row in rows if row["mode"] == "isolation" and
                row["scenario"] == "paired_tones" and row["phase"] == "warmed_held"]
        quiet = [row for row in rows if row["mode"] == "isolation" and
                 row["scenario"] == "idle_quiet"]
        moving = [row for row in rows if row["mode"] == "isolation" and
                  row["scenario"] == "moving_tabs_pressure_wah" and
                  row["phase"] == "warmed_held"]
        legacy = [row for row in rows if row["mode"] == "legacy" and
                  row["scenario"] == "paired_tones" and
                  row["initial_selector"] == "0" and row["phase"] == "warmed_held"]
        if tuple(map(len, (held, quiet, moving, legacy))) != (11,1,1,1):
            raise ValueError(f"missing source held/idle/moving or legacy anchor: {item['path']}")
        result.append({"path": item["path"],
                       "held_11_pair_fixture_median_rtf": med([float(r["median_rtf"]) for r in held]),
                       "held_worst_fixture_p99_rtf": max(float(r["p99_rtf"]) for r in held),
                       "held_deadline_misses_of_11264": sum(int(r["budget_exceedances"]) for r in held),
                       "quiet_idle_median_rtf": float(quiet[0]["median_rtf"]),
                       "quiet_idle_p99_rtf": float(quiet[0]["p99_rtf"]),
                       "moving_median_rtf": float(moving[0]["median_rtf"]),
                       "legacy_anchor_median_rtf": float(legacy[0]["median_rtf"]),
                       "legacy_anchor_p99_rtf": float(legacy[0]["p99_rtf"])})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before-prefix", default="artifacts/cpu-drift-sine-before-20261102-r")
    parser.add_argument("--after-prefix", default="artifacts/cpu-drift-sine-after-20261102-r")
    args = parser.parse_args()
    before = [round_data(args.before_prefix,i) for i in (1,2,3)]
    after = [round_data(args.after_prefix,i) for i in (1,2,3)]
    all_rounds = before+after
    first = all_rounds[0]["rows"]
    for round_ in all_rounds:
        if set(round_["rows"]) != set(first):
            raise ValueError(f"fixture keys changed: {round_['path']}")
        for key, row in round_["rows"].items():
            for field, original in first[key].items():
                if field not in TIMING and row[field] != original:
                    raise ValueError(f"non-timing difference {round_['path']} {key} {field}: "
                                     f"{row[field]!r} != {original!r}")
    preset_before, preset_after, preset_breakdown = [], [], []
    for key in first:
        mode = dict(zip(KEYS,key))
        if mode["mode"] == "isolation" and mode["scenario"] == "paired_tones" and mode["phase"] == "warmed_held":
            b = med([float(r["rows"][key]["median_rtf"]) for r in before])
            a = med([float(r["rows"][key]["median_rtf"]) for r in after])
            preset_before.append(b)
            preset_after.append(a)
            preset_breakdown.append({"selector": mode["initial_selector"],
                                     "rank_i_name": first[key]["rank_i_name"],
                                     "rank_ii_name": first[key]["rank_ii_name"],
                                     "before_median_rtf": b, "after_median_rtf": a,
                                     "improvement_pct": 100*(b-a)/b})
    paired_improvement = [100*(b-a)/b for b,a in zip(preset_before,preset_after)]
    bstats, astats = stats(before), stats(after)
    payload = {"exact_non_timing_fields_all_306_rows": "PASS", "cases_each": 27,
               "rows_each": 51, "source_before_commits": [r["source_commit"] for r in before],
               "source_after_commits": [r["source_commit"] for r in after],
               "source_fingerprints_before": [r["source_fingerprint"] for r in before],
               "source_fingerprints_after": [r["source_fingerprint"] for r in after],
               "before_rounds": bstats, "after_rounds": astats,
               "held_median_across_11_preset_three_round_medians_before": med(preset_before),
               "held_median_across_11_preset_three_round_medians_after": med(preset_after),
               "held_per_preset_paired_improvement_pct": paired_improvement,
               "held_per_preset_breakdown": sorted(preset_breakdown,
                                                    key=lambda x: int(x["selector"])),
               "held_paired_improvement_pct_median": med(paired_improvement),
               "held_preset_count_slower": sum(a>b for b,a in zip(preset_before,preset_after)),
               "quiet_idle_three_round_median_before": med([r["quiet_idle_median_rtf"] for r in bstats]),
               "quiet_idle_three_round_median_after": med([r["quiet_idle_median_rtf"] for r in astats]),
               "absolute_held_target_0_15_to_0_20": med(preset_after)<=.20,
               "absolute_idle_target_0_02": med([r["quiet_idle_median_rtf"] for r in astats])<=.02,
               "raw_round_files": [{"path": r["path"], "started_unix_seconds": r["started_unix_seconds"], "sha256": r["sha256"]} for r in all_rounds]}
    print(json.dumps(payload, indent=2))


if __name__ == "__main__":
    main()
