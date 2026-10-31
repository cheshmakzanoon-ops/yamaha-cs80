#!/usr/bin/env python3
"""Summarize complete serial benchmark rounds; retain every measured row.

The minimum of at least three callback medians is selected per fixture, as in
the existing completion evidence. Its p99 and deadline misses come from that
same round. This reporting tool neither changes nor enforces DSP tolerances.
"""

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


KEYS = ("rate", "block", "mode", "scenario", "initial_selector", "phase")


def summarize(checkpoint: Path) -> dict:
    rounds = sorted(checkpoint.glob("round-*/results.csv"))
    if len(rounds) < 3:
        raise ValueError(f"{checkpoint}: at least three complete rounds are required")
    groups = {}
    expected = None
    hashes = {}
    for path in rounds:
        complete = json.loads((path.parent / "completed.json").read_text())
        if not complete.get("completed"):
            raise ValueError(f"{path.parent}: incomplete benchmark")
        with path.open(newline="") as file:
            rows = list(csv.DictReader(file))
        if len(rows) != complete["rows"] or complete["cases"] != 27 or len(rows) != 51:
            raise ValueError(f"{path.parent}: expected the unchanged 27-case, 51-row sweep")
        keys = {tuple(row[key] for key in KEYS) for row in rows}
        if len(keys) != len(rows) or (expected is not None and keys != expected):
            raise ValueError(f"{path.parent}: fixture mismatch between rounds")
        expected = keys
        for row in rows:
            for metric in ("median_rtf", "p99_rtf", "peak", "rms"):
                if not math.isfinite(float(row[metric])):
                    raise ValueError(f"{path}: nonfinite {metric}")
            groups.setdefault(tuple(row[key] for key in KEYS), []).append((path.parent.name, row))
        for file in sorted(path.parent.iterdir()):
            if file.is_file():
                hashes[str(file.relative_to(checkpoint))] = hashlib.sha256(file.read_bytes()).hexdigest()

    result = []
    for key, entries in sorted(groups.items()):
        selected, row = min(entries, key=lambda item: float(item[1]["median_rtf"]))
        result.append({
            **dict(zip(KEYS, key)),
            "rank_i_name": row["rank_i_name"], "rank_ii_name": row["rank_ii_name"],
            "selected_round": selected,
            "all_round_medians_rtf": {name: float(value["median_rtf"]) for name, value in entries},
            "median_rtf": float(row["median_rtf"]), "p99_rtf": float(row["p99_rtf"]),
            "budget_exceedances": int(row["budget_exceedances"]), "callbacks": int(row["callbacks"]),
            "nonfinite_count_all_rounds": sum(int(value["nonfinite_count"]) for _, value in entries),
        })
    held = [r for r in result if r["mode"] == "isolation" and r["scenario"] == "paired_tones"
            and r["phase"] == "warmed_held"]
    idle = [r for r in result if r["mode"] == "isolation" and r["scenario"] == "idle_quiet"]
    moving = [r for r in result if r["mode"] == "isolation" and r["scenario"] == "moving_tabs_pressure_wah"
              and r["phase"] == "warmed_held"]
    if len(held) != 11 or len(idle) != 1 or len(moving) != 1:
        raise ValueError(f"{checkpoint}: missing held-tone, moving-control or idle rows")
    return {
        "checkpoint": checkpoint.name, "rounds": len(rounds),
        "method": "Minimum callback median per fixture; p99 and misses from the same selected round; shared host, not DAW qualification",
        "held_median_rtf_range": [min(r["median_rtf"] for r in held), max(r["median_rtf"] for r in held)],
        "held_p99_rtf_range": [min(r["p99_rtf"] for r in held), max(r["p99_rtf"] for r in held)],
        "held_selected_round_budget_exceedances": sum(r["budget_exceedances"] for r in held),
        "idle_median_rtf": idle[0]["median_rtf"],
        "moving_controls": moving,
        "rows": result, "sha256": hashes,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint", type=Path)
    args = parser.parse_args()
    result = summarize(args.checkpoint)
    (args.checkpoint / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({key: value for key, value in result.items() if key not in ("rows", "sha256")}, indent=2))


if __name__ == "__main__":
    main()
