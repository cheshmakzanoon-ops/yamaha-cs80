#!/usr/bin/env python3
"""Summarize ABBA-block ratios; keep per-run data in raw.csv."""
import collections
import csv
from pathlib import Path
import statistics as stats

directory = Path(__file__).parent
rows = list(csv.DictReader((directory / "raw.csv").open()))
groups = collections.defaultdict(list)
for row in rows:
    groups[(row["case"], row["comparison"])].append(row)
fields = ["case", "comparison", "baseline_modes", "new_modes", "baseline_cpu_ns", "new_cpu_ns",
          "paired_cpu_ratio", "paired_ratio_min", "paired_ratio_max", "baseline_wall_ns", "new_wall_ns",
          "allocation_count", "pcm_equal"]
with (directory / "summary.csv").open("w") as stream:
    writer = csv.DictWriter(stream, fieldnames=fields)
    writer.writeheader()
    for (case, comparison), group in groups.items():
        assert len(group) == 20
        before = [r for r in group if r["version"] != "new"]
        after = [r for r in group if r["version"] == "new"]
        ratios = []
        for block in range(5):
            a = stats.mean(float(r["cpu_ns"]) for r in after if int(r["block"]) == block)
            b = stats.mean(float(r["cpu_ns"]) for r in before if int(r["block"]) == block)
            ratios.append(a / b)
        result = dict(case=case, comparison=comparison,
                      baseline_modes=before[0]["modes"], new_modes=after[0]["modes"],
                      baseline_cpu_ns=stats.median(float(r["cpu_ns"]) for r in before),
                      new_cpu_ns=stats.median(float(r["cpu_ns"]) for r in after),
                      paired_cpu_ratio=stats.median(ratios),
                      paired_ratio_min=min(ratios), paired_ratio_max=max(ratios),
                      baseline_wall_ns=stats.median(float(r["wall_ns"]) for r in before),
                      new_wall_ns=stats.median(float(r["wall_ns"]) for r in after),
                      allocation_count=sum(int(r["allocations"]) for r in group),
                      pcm_equal=before[0]["pcm_hash"] == after[0]["pcm_hash"])
        for key, value in result.items():
            if isinstance(value, float):
                result[key] = f"{value:.6f}"
        writer.writerow(result)
assert len(rows) == 960
assert not any(int(r["allocations"]) for r in rows)
hashes = collections.defaultdict(set)
for row in rows:
    hashes[row["case"], row["version"]].add(row["pcm_hash"])
assert all(len(h) == 1 for h in hashes.values())
print(f"{len(rows)} measured runs; 0 timed allocations; every case/version repeats its PCM hash")
wall_ratios = [float(r["wall_ns"]) / float(r["cpu_ns"]) for r in rows]
print(f"wall/CPU ratio median={stats.median(wall_ratios):.6f}, max={max(wall_ratios):.6f}")
