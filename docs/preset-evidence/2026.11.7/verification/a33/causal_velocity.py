#!/usr/bin/env python3
"""A bounded, two-preset causal diagnostic; only eight 1.5-second captures.

The current source/header tree must match frozen c5549f7d. This script never
compiles old callers against current libraries. It uses one current library
for both the authored patch and a two-control baseline-restoration diagnostic.
"""
import argparse
import csv
import hashlib
import io
import json
import struct
import subprocess
from pathlib import Path

OUT = Path(__file__).resolve().parent
EVIDENCE = OUT.parent.parent
ROOT = EVIDENCE.parents[2]
SOURCE = "c5549f7d095a5c5f49a5ad22ec0ccc819245d201"
OLD_SOURCE = "078c0981ffa1b5472342e0fb5ab8702bc3bd2889"
PUBLISHED = EVIDENCE / "development/published-4-inventory.json"
CURRENT = EVIDENCE / "verification/a12/inventory.json"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def binary32(value):
    return struct.pack("<f", float(value))


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, default=Path("/tmp/cs80-build-1cee5a"))
parser.add_argument("--scratch", type=Path, default=Path("/tmp/cs80-a33-1cee5a"))
args = parser.parse_args()
args.scratch.mkdir(parents=True, exist_ok=True)
before = json.loads(PUBLISHED.read_text())
current = json.loads(CURRENT.read_text())
assert sha(PUBLISHED) == "0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611"
assert sha(CURRENT) == "55c08568074a6560ef10c8c525733a40aec42d9b8c6d20644bf4984cdd0d3e0d"
source_hashes = {}
# Authenticate the whole tracked src tree, including transitive inline headers.
paths = subprocess.check_output(["git", "ls-tree", "-r", "--name-only", SOURCE, "--", "src"], cwd=ROOT, text=True).splitlines()
for path in paths:
    expected = subprocess.check_output(["git", "show", SOURCE + ":" + path], cwd=ROOT)
    actual = (ROOT / path).read_bytes()
    assert actual == expected, "Source changed: " + path
    source_hashes[path] = hashlib.sha256(actual).hexdigest()

library = args.build / "libcs80_core.a"
capture = args.build / "cs80_preset_capture"
capture_receipt = json.loads((EVIDENCE / "development/matched-published-current-bank.json").read_text())
assert sha(capture) == next(r["capture_sha256"] for r in capture_receipt["runs"] if r["label"] == "current")
# The executable and linked core are those already source-bound by A23.
a23_manifest = json.loads((EVIDENCE / "verification/a23/pcm-manifest.json").read_text())
core_record = next(r for r in a23_manifest["files"] if r["path"] == "/tmp/cs80-build-1cee5a/libcs80_core.a")
assert core_record["source"] == SOURCE and core_record["sha256"] == sha(library)
assert "-DCS80_ENABLE_BORROWED_HELD=1" in (args.build / "build.ninja").read_text()
commands = []


def run(command):
    command = list(map(str, command))
    commands.append(command)
    return subprocess.check_output(command, cwd=ROOT, text=True, stderr=subprocess.STDOUT)


probe = args.scratch / "causal_velocity_probe"
compiler = run(["c++", "--version"])
compile_command = ["c++", "-std=c++20", "-O2", "-DNDEBUG", "-DCS80_ENABLE_BORROWED_HELD=1", "-I", ROOT / "src",
                   OUT / "causal_velocity_probe.cpp", library, "-o", probe]
compile_output = run(compile_command)
rows, patches, pcm_manifest = [], [], []
for name, note in [("Analog Bass", 48), ("Funky Pluck I", 60)]:
    old = next(p for p in before["programs"] if p["name"] == name)
    new = next(p for p in current["programs"] if p["name"] == name)
    old_values = dict(zip(before["parameter_ids"], old["values"]))
    new_values = dict(zip(current["parameter_ids"], new["values"]))
    changed = [k for k in old_values if binary32(old_values[k]) != binary32(new_values[k])]
    assert set(changed) == {"ch1_v_level", "ch2_v_level"}, (name, changed)
    patches.append({"name": name, "host_index": new["host_index"], "note": note,
                    "only_published_parameter_changes": changed,
                    "published_vca": [old_values[k] for k in changed],
                    "current_vca": [new_values[k] for k in changed]})
    raw = args.scratch / name.lower().replace(" ", "_")
    output = run([probe, raw, name, note, old_values["ch1_v_level"], old_values["ch2_v_level"]])
    subset = list(csv.DictReader(io.StringIO(output)))
    assert len(subset) == 4
    for row in subset:
        assert int(row["nonfinite"]) == int(row["samples_over_full_scale"]) == 0
        assert float(row["peak_dbfs"]) < 0
        source_values = old_values if row["mode"] == "restored_published_vca_baselines" else new_values
        assert binary32(row["vca1"]) == binary32(source_values["ch1_v_level"])
        assert binary32(row["vca2"]) == binary32(source_values["ch2_v_level"])
        pcm = raw / row["pcm_file"]
        assert pcm.stat().st_size == 48000 * 1.5 * 2 * 4
        pcm_manifest.append({"name": name, "mode": row["mode"], "velocity": float(row["velocity"]),
                             "path": str(pcm), "bytes": pcm.stat().st_size, "sha256": sha(pcm)})
    rows.extend(subset)
with (OUT / "causal-velocity-metrics.csv").open("w", newline="") as f:
    writer = csv.DictWriter(f, rows[0].keys()); writer.writeheader(); writer.writerows(rows)

results = []
for patch in patches:
    subset = [r for r in rows if r["name"] == patch["name"]]
    by = {(r["mode"], round(float(r["velocity"]), 2)): r for r in subset}
    measurements = {}
    for metric in ["early_rms_dbfs", "held_rms_dbfs", "late_held_rms_dbfs"]:
        dynamic_range = {mode: float(by[mode, .85][metric]) - float(by[mode, .25][metric])
                         for mode in ["current_7", "restored_published_vca_baselines"]}
        measurements[metric] = {
            "firm_minus_soft_db": dynamic_range,
            "current_minus_restored_at_soft_db": float(by["current_7", .25][metric]) - float(by["restored_published_vca_baselines", .25][metric]),
            "current_minus_restored_at_firm_db": float(by["current_7", .85][metric]) - float(by["restored_published_vca_baselines", .85][metric]),
            "change_in_firm_minus_soft_db": dynamic_range["current_7"] - dynamic_range["restored_published_vca_baselines"],
        }
    # Descriptive distinction from a post-output scalar, not a quality threshold.
    assert abs(measurements["early_rms_dbfs"]["change_in_firm_minus_soft_db"]) > .01
    results.append({**patch, "measurements": measurements})

write("causal-velocity-summary.json", {
    "scope": "Two representative patches only. Both arms use frozen .7 core; restored baselines are a controlled diagnostic, not full .4 renders. No subjective improvement claim.",
    "source": SOURCE, "published_parameter_source": OLD_SOURCE,
    "captures": 8, "audio_seconds": 12, "presets": 2,
    "sample_rate": 48000, "hold_seconds": 1, "tail_seconds": .5, "pressure": 0,
    "windows_seconds": {"early_rms_dbfs": [0, .2], "held_rms_dbfs": [0, 1],
                         "late_held_rms_dbfs": [.5, 1], "tail_rms_dbfs": [1, 1.5]},
    "results": results, "nonfinite": 0, "samples_over_full_scale": 0,
    "largest_peak_dbfs": max(float(r["peak_dbfs"]) for r in rows),
})
write("causal-velocity-receipt.json", {
    "passed": True, "source": SOURCE, "matched_header_tree": True,
    "source_tree_file_count": len(source_hashes),
    "source_tree_hashes": source_hashes,
    "compiler": compiler.splitlines()[0], "commands": commands,
    "core_sha256": sha(library), "capture_sha256": sha(capture), "probe_sha256": sha(probe),
    "probe_source_sha256": sha(OUT / "causal_velocity_probe.cpp"), "driver_sha256": sha(Path(__file__)),
    "published_inventory_sha256": sha(PUBLISHED), "current_inventory_sha256": sha(CURRENT),
    "metrics_sha256": sha(OUT / "causal-velocity-metrics.csv"),
    "summary_sha256": sha(OUT / "causal-velocity-summary.json"),
    "raw_pcm": pcm_manifest,
})
print((OUT / "causal-velocity-summary.json").read_text())
