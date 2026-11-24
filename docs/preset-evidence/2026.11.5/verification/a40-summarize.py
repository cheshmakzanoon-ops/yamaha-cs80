#!/usr/bin/env python3
"""Summarize A40's bounded local probe; never qualifies a native release."""
import csv
import hashlib
import json
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
EVIDENCE = ROOT / "docs/preset-evidence/2026.11.5"
VERIFY = EVIDENCE / "verification"
SOURCE = "6094bce18c8b19da804c2fbc9a00e31969a7f579"
NAMES = ["Choir Pad", "Dark Choir", "Slow Pad", "Evolving Pad", "Cathedral Pad", "Arctic Pad", "Pressure Pad"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def energy_db(rows):
    return 10 * math.log10(sum(10 ** (float(r["rms_dbfs"]) / 10) for r in rows) / len(rows))


rows = list(csv.DictReader((VERIFY / "a40-long-chord-windows.csv").open()))
assert len(rows) == 7 * 32
assert list(dict.fromkeys(r["program"] for r in rows)) == NAMES
summaries = []
for name in NAMES:
    selected = [r for r in rows if r["program"] == name]
    assert [int(r["second"]) for r in selected] == list(range(32))
    assert all(int(r["samples"]) == 96000 for r in selected)
    assert sum(int(r["nonfinite"]) + int(r["clipped"]) for r in selected) == 0
    assert int(selected[-1]["active_at_window_end"]) == 0
    silent_at = next((int(r["second"]) + 1 for r in selected[20:] if int(r["active_at_window_end"]) == 0), None)
    summaries.append({
        "name": name,
        "peak_dbfs": max(float(r["peak_dbfs"]) for r in selected),
        "max_one_second_rms_dbfs": max(float(r["rms_dbfs"]) for r in selected),
        "held_18_to_20_seconds_rms_dbfs": energy_db(selected[18:20]),
        "tail_30_to_32_seconds_rms_dbfs": energy_db(selected[30:32]),
        "tail_relative_to_held_db": energy_db(selected[30:32]) - energy_db(selected[18:20]),
        "zero_active_voices_observed_by_seconds_after_key_up": silent_at - 20 if silent_at else None,
        "all_finite": True,
        "clipped_samples": 0,
        "active_voices_at_end": 0,
    })

current = json.loads((EVIDENCE / "inventory.json").read_text())
previous = json.loads((EVIDENCE / "baseline/inventory.json").read_text())
owned_names = re.findall(r'add\("([^"]+)", "Pads"', (ROOT / "src/core/curated/Pads.inc").read_text())
current_pads = [p for p in current["programs"] if p["host_index"] >= 222 and p["name"] in owned_names]
previous_by_host = {p["host_index"]: p for p in previous["programs"]}
assert len(current_pads) == 30
assert [p["name"] for p in current_pads] == owned_names
identity = []
for p in current_pads:
    old = previous_by_host[p["host_index"]]
    assert (p["name"], p["category"], p["tags"]) == (old["name"], old["category"], old["tags"])
    identity.append({"host_index": p["host_index"], "name": p["name"], "category": p["category"], "tags": p["tags"]})

choirs = []
selected_ids = [
    "ch1_feet", "ch2_feet", "body1_model", "body2_model", "body1_variant", "body2_variant",
    "body1_mix", "body2_mix", "body1_size", "body2_size", "body1_brightness", "body2_brightness",
    "ch1_v_a", "ch2_v_a", "ch1_v_s", "ch2_v_s", "ch1_v_r", "ch2_v_r",
    "ch1_v_level", "ch2_v_level", "ch1_init_level", "ch2_init_level",
    "ch1_after_brill", "ch2_after_brill", "ch1_after_level", "ch2_after_level",
    "ch1_hpf_cutoff", "ch2_hpf_cutoff", "ch1_lpf_cutoff", "ch2_lpf_cutoff",
    "chorus_depth", "chorus_speed", "drift", "voice_dispersion",
]
for p in current_pads:
    if p["name"] not in ["Choir Pad", "Dark Choir"]:
        continue
    values = dict(zip(current["parameter_ids"], p["values"]))
    choirs.append({"name": p["name"], "host_index": p["host_index"], "parameters": {k: values[k] for k in selected_ids}})

inputs = [
    "src/core/InstrumentBody.h", "src/core/InstrumentBodyVariants.h", "src/core/Engine.cpp",
    "src/core/Engine.h", "src/core/Params.h", "src/core/FactoryPresets.h", "src/core/curated/Pads.inc",
    "src/plugin/PluginProcessor.cpp", "tests/InstrumentBodyHostTests.h", "tests/VocalBodyTests.cpp",
    "tests/PadPolyRefinementTests.cpp", "build-preset/libcs80_core.a",
    "docs/preset-evidence/2026.11.5/inventory.json", "docs/preset-evidence/2026.11.5/baseline/inventory.json",
    "docs/preset-evidence/2026.11.5/verification/a40-long-chord-probe.cpp",
    "docs/preset-evidence/2026.11.5/verification/a40-long-chord-probe.log",
    "docs/preset-evidence/2026.11.5/verification/a40-long-chord-windows.csv",
]
result = {
    "source_commit": SOURCE,
    "scope": "Bounded local Engine long-held chord safety and source-level choir/pad identity review",
    "native_release_qualified": False,
    "acoustic_equivalence_established": False,
    "sample_rate": 48000,
    "notes": [36, 43, 48, 55, 60, 64, 67, 72],
    "velocity": 1.0,
    "hold_seconds": 20,
    "tail_seconds": 12,
    "pressure_timeline": [{"start_seconds": 0, "value": 0}, {"start_seconds": 4, "value": 1}, {"start_seconds": 8, "value": 0}, {"start_seconds": 12, "value": 1}],
    "factory_controls_overridden": False,
    "audio_normalization": False,
    "all_notes_off_reset_or_mute": False,
    "stereo_frames": sum(int(r["samples"]) for r in rows) // 2,
    "checks": 686,
    "failures": 0,
    "long_chord_programs": summaries,
    "pad_identity_count": len(identity),
    "pad_identities_match_preceding_release": True,
    "pad_identities": identity,
    "compiled_choir_recipe_parameters": choirs,
    "input_sha256": {p: sha(ROOT / p) for p in inputs},
    "limits": [
        "Seven selected long-chord cases, not every pad or arbitrary performance event sequence.",
        "One local Linux core build at 48 kHz; native plugin/host qualification is separate.",
        "Long-tail numerical and voice-lifecycle evidence does not establish perceptual realism.",
        "Vowel centers are authored broad resonances; real singers, consonants and words are not modeled.",
        "Old-state restore findings are source/test-contract review, not an independent native APVTS execution.",
    ],
}
assert "686 checks, 0 failures" in (VERIFY / "a40-long-chord-probe.log").read_text()
target = VERIFY / "a40-review-results.json"
target.write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps({"checks": result["checks"], "failures": result["failures"], "stereo_frames": result["stereo_frames"], "programs": summaries}, indent=2))
