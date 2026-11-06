#!/usr/bin/env python3
"""Advisory, checksum-checked paired core timings; no plugin or GUI dependency."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision", action="append", required=True, metavar="LABEL=CHECKOUT")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--trials", type=int, default=11)
    parser.add_argument("--scenario", action="append", type=int, choices=range(11))
    parser.add_argument("--cpu", type=int, required=True, help="Permitted Linux CPU for taskset")
    args = parser.parse_args()
    if args.trials < 1:
        parser.error("--trials must be positive")
    if not hasattr(os, "sched_getaffinity") or args.cpu not in os.sched_getaffinity(0):
        parser.error("Choose a CPU in this Linux process's permitted affinity set")
    revisions = {}
    for value in args.revision:
        label, separator, path = value.partition("=")
        if not separator or not label.isidentifier() or label in revisions:
            parser.error("Use unique identifier labels and LABEL=CHECKOUT")
        revisions[label] = Path(path).resolve()
    if len(revisions) < 2:
        parser.error("Supply a baseline first, followed by at least one candidate")
    scenarios = args.scenario or [0, 2, 4, 9, 10]
    out = args.output_dir.resolve()
    # Do not overwrite or append to previous evidence.
    out.mkdir(parents=True, exist_ok=False)
    probe = Path(__file__).resolve().parent.parent / "tests/FilterCachePerformanceProbe.cpp"
    flags = ["-O3", "-DNDEBUG", "-std=gnu++20", "-fPIC"]
    provenance = {
        "compiler": subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
        "flags": flags, "cpu": args.cpu, "trials": args.trials, "scenarios": scenarios,
        "platform": platform.platform(),
        "cpu_model": next((line.split(":", 1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                           if line.startswith("model name")), "unknown"),
        "probe_sha256": hashlib.sha256(probe.read_bytes()).hexdigest(), "revisions": {},
    }
    for label, checkout in revisions.items():
        metadata = {}
        for name, command in {
            "head": ["rev-parse", "HEAD"], "tree": ["rev-parse", "HEAD^{tree}"],
            "status": ["status", "--porcelain"],
        }.items():
            metadata[name] = subprocess.check_output(["git", "-C", str(checkout), *command], text=True).strip()
        header = checkout / "src/core/Ig00156Filter.h"
        metadata["filter_header_sha256"] = hashlib.sha256(header.read_bytes()).hexdigest()
        # Include dirty source bytes, not just the committed tree identity.
        metadata["source_sha256"] = {name: hashlib.sha256((checkout / name).read_bytes()).hexdigest()
                                     for name in ["src/core/Engine.cpp", "src/core/Engine.h",
                                                  "src/core/FactoryPresets.cpp", "src/core/Ig00156Filter.h"]}
        provenance["revisions"][label] = metadata
        command = ["g++", *flags, "-I" + str(checkout / "src"), str(probe),
                   str(checkout / "src/core/Engine.cpp"), str(checkout / "src/core/FactoryPresets.cpp"),
                   "-o", str(out / ("probe-" + label))]
        metadata["build_command"] = command
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out / ("build-" + label + ".log")).write_text(result.stdout)
        result.check_returncode()
    (out / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    rows = []
    hashes = {}
    labels = list(revisions)
    for trial in range(args.trials):
        order = scenarios.copy()
        random.Random(64000 + trial).shuffle(order)
        for scenario in order:
            permutation = labels.copy()
            random.Random(51000 + 11 * trial + scenario).shuffle(permutation)
            # Palindromic order gives each revision symmetric early/late positions.
            for position, label in enumerate(permutation + permutation[::-1]):
                result = subprocess.run(["taskset", "-c", str(args.cpu), str(out / ("probe-" + label)),
                                         "bench", str(scenario)], text=True, capture_output=True, check=True)
                row = json.loads(result.stdout)
                if row["cpu"] <= 0 or row["wall"] <= 0:
                    raise RuntimeError("Nonpositive measured duration")
                if not row["valid"] or row["hash"] != hashes.setdefault(scenario, row["hash"]):
                    raise RuntimeError("Nonfinite output or differing audio/state checksum")
                row.update(revision=label, trial=trial, scenario=scenario, position=position)
                rows.append(row)
                with (out / "observations.jsonl").open("a") as file:
                    file.write(json.dumps(row) + "\n")
        print(f"Completed round {trial + 1}/{args.trials}", flush=True)
    summary = []
    for scenario in scenarios:
        subset = [r for r in rows if r["scenario"] == scenario]
        means = [{label: statistics.mean(r["cpu"] for r in subset
                                       if r["trial"] == trial and r["revision"] == label)
                  for label in labels} for trial in range(args.trials)]
        for label in labels[1:]:
            savings = [100 * (1 - trial[label] / trial[labels[0]]) for trial in means]
            summary.append({"scenario": scenario, "name": subset[0]["name"], "revision": label,
                            "baseline_cpu_median": statistics.median(m[labels[0]] for m in means),
                            "candidate_cpu_median": statistics.median(m[label] for m in means),
                            "paired_saved_percent_median": statistics.median(savings),
                            "paired_saved_percent": savings, "hash": hashes[scenario],
                            "versus_other_revision_paired_saved_percent_median": {
                                other: statistics.median(100 * (1 - trial[label] / trial[other]) for trial in means)
                                for other in labels if other != label},
                            "max_wall_to_cpu_ratio": max(r["wall"] / r["cpu"] for r in subset)})
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
