#!/usr/bin/env python3
"""Summarize bounded rapid-strike diagnostics, run from the repository root."""
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path.cwd()
OUT = ROOT / "build-w08-rapid-strikes"
assert sys.byteorder == "little", "Raw float hashes describe this little-endian execution"

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def records(name):
    with (OUT / name).open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    for row in rows:
        for key, value in list(row.items()):
            if key not in {"program", "variant", "raw_float32le_sha256"}:
                row[key] = float(value)
    return rows

captures = records("captures.csv")
boundaries = records("boundaries.csv")
envelopes = records("envelopes.csv")
onsets = records("onset-levels.csv")
assert len(captures) == 8 and len(boundaries) == 4 and len(envelopes) == 32
assert len(onsets) == 16
assert len({row["raw_float32le_sha256"] for row in captures}) == 8
assert all(row["nonfinite"] == row["over_full_scale"] == 0 for row in captures)
assert all(row["max_active_voices"] == 8 and row["final_active_voices"] == 0
           and row["final_held_voices"] == 0 and row["params_unchanged"] == 1 for row in captures)
assert all(row["burst_reuses"] > 0 for row in captures)
assert all(row["prefix_max_error"] == 0 and row["event_sample_max_error"] < 1e-7
           and row["prior_slot_active"] == 1 and row["prior_slot_held"] == 0 for row in boundaries)
log = (OUT / "run.log").read_text()
result = re.search(r"Rapid strike performance: (\d+)/(\d+) checks passed, 8 captures / 96 seconds", log)
assert result and result[1] == result[2]

inputs = ["build-audit/libcs80_core.a", "build-audit/inventory-final.json",
          "src/core/FactoryPresets.cpp", "src/core/FactoryPresets.h", "src/core/CuratedLibrary.inc",
          "src/core/CuratedLibraryLevels.inc", "src/core/curated/Plucked.inc", "src/core/curated/Bass.inc",
          "src/core/Engine.cpp", "src/core/Engine.h", "src/core/Params.h", "src/tools/TruthInputSha256.h"]
artifacts = ["probe.cpp", "probe", "summarize.py", "captures.csv", "envelopes.csv",
             "boundaries.csv", "events.csv", "onset-levels.csv", "run.log"]
summary = {
    "assignment": 40,
    "status": "PASS",
    "scope": "Repeated articulation and released-slot reuse for four strengthened factory attack layers",
    "recorded_git_head": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
    "library_revision": "2026.11.1",
    "source_state": "Final factory data from the locally compiled pre-publication core. Exact executable, library, inventory and relevant source hashes identify this execution; this is not a CI-produced plugin artifact.",
    "sample_rate": 48000,
    "program_notes": {"Harp": 60, "Kalimba": 60, "Sine Bass": 48, "Upright Pluck": 48},
    "conditions": {
        "factory_parameters_modified": False,
        "pressure": 0,
        "channel": 1,
        "velocity_alternation": [0.3, 0.95],
        "repeated_notes": "Four same-note strikes at 0, 0.45, 0.90, 1.35 seconds, each held 120 ms.",
        "arpeggio": "Twelve notes from 2.00 s at 90 ms spacing, 35 ms holds, offsets 0/4/7/12/7/4/0/7/12/16/12/7.",
        "allocation_burst": "Sixteen onsets from 3.60 s at 10 ms spacing and 6 ms holds, offsets 0/2/4/5/7/9/11/12/14/16/17/19/16/14/12/11. This is a deliberate allocation stress burst, not a normal musical tempo.",
        "last_individual_note_off_seconds": 3.756,
        "all_notes_off_release_seconds": 4.05,
        "duration_per_capture_seconds": 12,
        "render_blocks": "At most 64 frames, split exactly at each event and envelope checkpoint.",
        "matched_control": "Identical engine/history/events except omission of onset 25 and its note-off, the tenth burst note at 3.69 s. All earlier audio is bit-identical. Subsequent allocation diverges intentionally, so only the initial 6 ms residual is interpreted causally."
    },
    "results": {
        "checks_passed": int(result[1]),
        "checks_total": int(result[2]),
        "capture_count": len(captures),
        "generated_seconds": sum(row["seconds"] for row in captures),
        "factory_onsets_per_program": 32,
        "control_onsets_per_program": 31,
        "max_factory_peak_dbfs": max(row["peak_dbfs"] for row in captures if row["variant"] == "factory"),
        "max_all_capture_peak_dbfs": max(row["peak_dbfs"] for row in captures),
        "latest_retirement_seconds": max(row["retired_seconds"] for row in captures),
        "nonfinite_samples": 0,
        "over_full_scale_samples": 0,
        "max_boundary_residual": max(row["event_sample_max_error"] for row in boundaries),
        "max_first_ms_residual": max(row["first_ms_residual_peak"] for row in boundaries),
        "largest_first_ms_residual_derivative_dbfs": 20 * math.log10(max(row["residual_derivative_first_ms"] for row in boundaries)),
        "smallest_derivative_margin_below_control_db": min(20 * math.log10(row["control_derivative_first_ms"] / row["residual_derivative_first_ms"]) for row in boundaries)
    },
    "captures": captures,
    "matched_boundaries": boundaries,
    "repeated_onset_envelopes": envelopes,
    "running_phrase_onset_levels": onsets,
    "limits": [
        "Eight physical voices are the maximum. The burst exercises reuse of released, still-active slots; it does not represent sixteen simultaneous voices or held-note stealing.",
        "The immediate-output residual test is not a perceptual click-free guarantee. Intended pluck/click transients produce strong later derivatives and are not classified as defects.",
        "Running-phrase onset RMS includes earlier release tails and is not an isolated velocity-transfer estimate.",
        "Retirement is observed at render-block boundaries and is an upper bound within at most 64 host samples.",
        "The -300 dBFS output is the reporting floor, not a universal claim of mathematical silence.",
        "No raw audio files were written. Exact little-endian float32 capture hashes are recorded; the probe regenerates them under matching inputs.",
        "No acoustic-instrument identity, listening-panel result, external DAW behavior or arbitrary performance history is established."
    ],
    "input_sha256": {name: sha(ROOT / name) for name in inputs},
    "artifact_sha256": {name: sha(OUT / name) for name in artifacts},
    "reproduction": [
        "mkdir -p build-w08-rapid-strikes",
        "c++ -std=c++20 -O2 -Isrc build-w08-rapid-strikes/probe.cpp build-audit/libcs80_core.a -o build-w08-rapid-strikes/probe",
        "./build-w08-rapid-strikes/probe > build-w08-rapid-strikes/run.log 2>&1",
        "python build-w08-rapid-strikes/summarize.py"
    ]
}
(OUT / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
print(json.dumps(summary["results"], indent=2))
