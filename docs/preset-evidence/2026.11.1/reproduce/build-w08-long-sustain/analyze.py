#!/usr/bin/env python3
"""Summarize the bounded long-sustain probe without retaining sample files."""
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(__file__).resolve().parent
EXPECTED_INVENTORY = "fe3ddc6ecdb9fa25e77170f1cd9f81e6f2fe0ef5aabd105506a0fa487eb286fa"
EXPECTED_LIBRARY = "6a4118e3e89472004861e08f81d8c1e182011a39fa006572a59b6faa02aa81b5"
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

with (OUT / "results.csv").open() as handle:
    raw = list(csv.DictReader(handle))
assert len(raw) == 12, "Probe did not complete all twelve programs"
assert len({row["name"] for row in raw}) == 12, "Duplicate program in completed matrix"
integer_keys = {"nonfinite", "over_full_scale", "max_active_voices", "held_min_active_voices",
                "final_active_voices", "checks", "failures"}
rows = [{key: value if key == "name" else int(value) if key in integer_keys else float(value)
         for key, value in row.items()} for row in raw]
assert all(math.isfinite(value) for row in rows for value in row.values() if isinstance(value, (int, float)))
with (OUT / "windows.csv").open() as handle:
    windows = list(csv.DictReader(handle))
assert len(windows) == 12 * 72, "Missing quarter-second windows"
inventory_path = ROOT / "build-audit/inventory-final.json"
library_path = ROOT / "build-audit/libcs80_core.a"
assert digest(inventory_path) == EXPECTED_INVENTORY
assert digest(library_path) == EXPECTED_LIBRARY
inventory = json.loads(inventory_path.read_text())
programs = {p["name"]: p for p in inventory["programs"]}
for row in rows:
    p = programs[row["name"]]
    values = dict(zip(inventory["parameter_ids"], p["values"]))
    row["factory_metadata"] = {"host_index": p["host_index"], "category": p["category"],
        "amp_attacks_seconds": [values["ch1_v_a"], values["ch2_v_a"]],
        "filter_attacks_seconds": [values["ch1_f_a"], values["ch2_f_a"]],
        "amp_releases_seconds": [values["ch1_v_r"], values["ch2_v_r"]],
        "volume": values["volume"]}
    local_windows = [w for w in windows if w["name"] == row["name"]]
    assert len(local_windows) == 72
    assert all(float(w["begin_seconds"]) == i * 0.25 and float(w["end_seconds"]) == (i + 1) * 0.25
               for i, w in enumerate(local_windows)), "Non-contiguous measurement windows"
    row["loudest_window"] = max(local_windows, key=lambda w: float(w["rms_dbfs"]))

input_names = ["build-audit/libcs80_core.a", "build-audit/inventory-final.json",
               "src/core/FactoryPresets.h", "src/core/FactoryPresets.cpp",
               "src/core/CuratedLibrary.inc", "src/core/CuratedLibraryLevels.inc",
               "src/core/curated/Pads.inc", "src/core/curated/Brass.inc", "src/core/curated/Fx.inc"]
summary = {
    "assignment": 39,
    "status": "PASS" if all(row["failures"] == 0 for row in rows) else "FAIL",
    "library_revision": inventory["library_revision"],
    "probe_compiler": subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
    "probe_compile_flags": ["-std=c++20", "-O2", "-I", "src"],
    "repository_head_at_summary": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
    "source_qualification": "The exact local static library and complete inventory hashes identify the rendered final calibrated bank. Repository HEAD may include subsequent packaging/documentation changes; this is not a CI-produced binary.",
    "selection": "Twelve representatives with amp attacks of at least 1.15 seconds in one rank: the longest attack cases plus broad family coverage, the late-filter Sweep Pad, and the explicitly requested Slow Pad, Cathedral Pad and Ether Pad. The five newly widened ensembles already have a separate chord audit and are excluded. This is not every slow factory program.",
    "conditions": {
        "sample_rate": 48000,
        "note_on_seconds": 0,
        "notes": [36, 43, 48, 52, 55, 60, 64, 67],
        "velocity": 0.9,
        "channel": 1,
        "note_off_seconds": 10,
        "total_seconds": 18,
        "pressure": "0 through one second; linear ramp to 1 from one to three seconds, delivered every 10 ms; remains at 1 through the held segment and entire release.",
        "factory_parameter_overrides": [],
        "render_block_maximum_frames": 128,
        "windows_seconds": {"early": [0, 2], "late_held": [7, 10], "tail": [17, 18]},
        "measurement": "Sample peak and stereo mean-power RMS. Quarter-second nonoverlapping windows. Active voices sampled after each render block. Voice-retirement timing therefore has at most 128-frame granularity.",
    },
    "captures": len(rows),
    "generated_seconds": len(rows) * 18,
    "checks": sum(row["checks"] for row in rows),
    "failures": sum(row["failures"] for row in rows),
    "bounds": {
        "maximum_peak_dbfs": max(row["peak_dbfs"] for row in rows),
        "nonfinite_samples": sum(row["nonfinite"] for row in rows),
        "over_full_scale_samples": sum(row["over_full_scale"] for row in rows),
        "minimum_tail_decline_db": min(-row["tail_minus_late_db"] for row in rows),
        "highest_tail_rms_dbfs": max(row["tail_rms_dbfs"] for row in rows),
        "latest_voice_retirement_seconds": max(row["retired_seconds"] for row in rows),
        "all_reached_eight_and_held_eight": all(row["max_active_voices"] == row["held_min_active_voices"] == 8 for row in rows),
        "all_voices_retired": all(row["final_active_voices"] == 0 for row in rows),
        "programs_with_higher_peak_after_two_seconds": sum(row["later_held_peak_dbfs"] > row["early_peak_dbfs"] for row in rows),
    },
    "results": rows,
    "input_sha256": {name: digest(ROOT / name) for name in input_names},
    "output_sha256": {name: digest(OUT / name) for name in ["probe.cpp", "probe", "results.csv", "windows.csv", "run.log", "analyze.py"]},
    "limitations": [
        "This is one deterministic eight-note chord at one velocity, sample rate and pressure history. It does not prove universal no-clipping behavior or DAW compatibility.",
        "The early and late windows differ in both envelope stage and pressure. Their RMS difference is a combined performance observation, not isolated envelope gain.",
        "Zero active voices is not a claim of digital silence; the output floor/effects can remain.",
        "The logarithmic reporting floor is -300 dBFS for RMS at or below 1e-15; it is not an acoustic noise measurement.",
        "No raw audio files are persisted; the probe measures every generated sample and reproduces the matrix from the identified bank.",
        "The measurements do not establish acoustic-instrument identity, calibrated CS-80 equivalence or a listening verdict."
    ],
}
(OUT / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps({key: summary[key] for key in ["status", "captures", "generated_seconds", "checks", "failures", "bounds"]}, indent=2))
