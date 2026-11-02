#!/usr/bin/env python3
"""Read-only verification of the rejected phase Engine SIMD CPU evidence.

No benchmark is rerun, no source is compiled and no tolerances are changed.
The selected-row convention matches scripts/summarize-cpu-benchmarks.py.
"""

import csv
import hashlib
import json
import math
from pathlib import Path
import statistics


HERE = Path(__file__).resolve().parent
ROUNDS = ("before-1", "before-2", "before-3", "after-1", "after-2", "after-3")
CSV_SHA256 = (
    "b5a21b58e9af9a085de26775853d6cb85793221d554356a92d7399f51dc657ed",
    "b554892842c7153206a8c433b971d875ddea2a88efa506bfa52fa5fa68b1ff2e",
    "81c3c277c8a20de3352b0051fb355d1dba40d4c1f6476d9e7c242fb657cafa82",
    "b654873af20d61b970919234e838db006a386b26bb681855cedf831c1dc79e8e",
    "9272db985bd0b5758f6df409400e79acad8a955f8dc899d8e6e82c3e20663215",
    "7c1dad9337cdf74ac1ecd224d0912574f1605bce8c7b41fe3df83a8bf7116ac4",
)
SOURCE_COMMITS = (
    "7c864f266a939aa2a42422440dd4c1c0cabe3814",
    "b1b722bdbd09f534cd0ad9a7d30d6636e19e8906",
)
KEYS = ("rate", "block", "mode", "scenario", "initial_selector", "phase")
SIGNAL_FIELDS = (
    "checksum", "peak", "rms", "nonfinite_count", "abs_at_or_above_one",
    "tail_voices_complete", "final_active_voices", "source_compilations",
    "last_block_peak", "selector_events", "controller_events",
    "mpe_member_state_checks", "pedal_held_callbacks",
)


def checked_round(name, expected_sha, expected_commit):
    path = HERE / "raw" / name
    csv_path = path / "results.csv"
    assert hashlib.sha256(csv_path.read_bytes()).hexdigest() == expected_sha, name
    metadata = json.loads((path / "metadata.json").read_text())
    complete = json.loads((path / "completed.json").read_text())
    assert complete == {"completed": True, "cases": 27, "rows": 51}, name
    assert metadata["source_commit_caller_verified"] == expected_commit, name
    assert metadata["source_fingerprint"] == "a95212770767733c", name
    assert metadata["quick_subset"] is False, name
    assert metadata["suite"] == "baseline", name
    assert metadata["callbacks_per_held_phase"] == 1024, name
    assert metadata["hardware_equivalence_or_realtime_qualification"] is False, name
    with csv_path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 51, name
    by_key = {tuple(row[field] for field in KEYS): row for row in rows}
    assert len(by_key) == len(rows), name
    assert all(row["rate"] == "48000" and row["block"] == "256"
               and row["nonfinite_count"] == "0" for row in rows), name
    assert all(math.isfinite(float(row[metric])) for row in rows
               for metric in ("median_rtf", "p99_rtf", "peak", "rms")), name
    return metadata["started_unix_seconds"], by_key


def selected(rows, key):
    # The p99 and missed callbacks deliberately come from the same row as
    # the minimum median, never independently selected.
    return min((r[key] for r in rows), key=lambda row: float(row["median_rtf"]))


def main():
    collected = [checked_round(name, sha, SOURCE_COMMITS[0 if i < 3 else 1])
                 for i, (name, sha) in enumerate(zip(ROUNDS, CSV_SHA256))]
    times = [start for start, _ in collected]
    assert times == sorted(times) and len(set(times)) == 6, "not serial before/after"
    maps = [rows for _, rows in collected]
    keys = set(maps[0])
    assert all(set(rows) == keys for rows in maps), "changed fixture list"
    for key in keys:
        for field in SIGNAL_FIELDS:
            assert len({rows[key][field] for rows in maps}) == 1, (key, field)
    stats = {}
    for name, first in (("before", maps[:3]), ("after", maps[3:])):
        chosen = {key: selected(first, key) for key in keys}
        held = [row for key, row in chosen.items()
                if key[2] == "isolation" and key[3] == "paired_tones" and key[5] == "warmed_held"]
        idle = [row for key, row in chosen.items()
                if key[2] == "isolation" and key[3] == "idle_quiet"]
        moving = [row for key, row in chosen.items()
                  if key[2] == "isolation" and key[3] == "moving_tabs_pressure_wah"
                  and key[5] == "warmed_held"]
        assert len(held) == 11 and len(idle) == 1 and len(moving) == 1, name
        medians = [float(row["median_rtf"]) for row in held]
        stats[name] = {
            "held_selected_median_of_eleven": statistics.median(medians),
            "held_selected_range": [min(medians), max(medians)],
            "held_selected_misses": sum(int(row["budget_exceedances"]) for row in held),
            "idle_selected_median": float(idle[0]["median_rtf"]),
            "idle_selected_p99": float(idle[0]["p99_rtf"]),
            "moving_selected_median": float(moving[0]["median_rtf"]),
            "moving_selected_p99": float(moving[0]["p99_rtf"]),
            "moving_selected_misses": int(moving[0]["budget_exceedances"]),
        }
    assert stats["before"]["held_selected_median_of_eleven"] == 0.726036
    assert stats["after"]["held_selected_median_of_eleven"] == 0.57352725
    assert stats["after"]["idle_selected_median"] == 0.061194375
    assert stats["after"]["moving_selected_p99"] == 1.9640908125
    assert stats["after"]["held_selected_range"][0] > 0.20
    assert stats["after"]["idle_selected_median"] > 0.02
    print(json.dumps({"rounds": 6, "cases_per_round": 27, "rows_per_round": 51,
                      "exact_non_timing_field_discrepancies": 0, "before": stats["before"],
                      "after": stats["after"], "cpu_gate": "REJECTED"}, indent=2))


if __name__ == "__main__":
    main()
