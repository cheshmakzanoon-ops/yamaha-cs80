#!/usr/bin/env python3
"""Reconcile retained .4/.6/.7 bank metrics without rerendering the bank.

No numerical tolerance is used for reported CSV-vector inequality. Spectral
metrics and level windows are descriptive, not perceptual acceptance gates.
Run from any directory; outputs stay beside this script.
"""
import csv
import hashlib
import json
import math
import statistics
import subprocess
from collections import Counter
from pathlib import Path

OUT = Path(__file__).resolve().parent
EVIDENCE = OUT.parent.parent
ROOT = EVIDENCE.parents[2]
SOURCES = {
    "published_4": "078c0981ffa1b5472342e0fb5ab8702bc3bd2889",
    "baseline_6": "18711f93a39aa49f3d6f9a41784be456224facbe",
    "current_7": "c5549f7d095a5c5f49a5ad22ec0ccc819245d201",
}
INPUTS = {
    "published_4": EVIDENCE / "development/published-4-bank-metrics.csv",
    "baseline_6": EVIDENCE / "development/baseline-bank-metrics.csv",
    "current_7": EVIDENCE / "development/current-bank-metrics.csv",
    "matched_receipt": EVIDENCE / "development/matched-published-current-bank.json",
    "reconstruction": EVIDENCE / "development/published-4-reconstruction.json",
    "baseline_provenance": EVIDENCE / "development/baseline-provenance.json",
    "published_inventory": EVIDENCE / "development/published-4-inventory.json",
    "current_inventory": EVIDENCE / "verification/a12/inventory.json",
    "a12_classification": EVIDENCE / "verification/a12/current-value-classification.json",
    "a12_mechanisms": EVIDENCE / "verification/a12/mechanism-coverage.json",
    "a23_pressure": EVIDENCE / "verification/a23/published-pressure-comparison.json",
}
SIGNAL = [
    "peak_dbfs", "rms_dbfs", "early_rms_dbfs", "body_rms_dbfs", "tail_rms_dbfs",
    "peak_window_seconds", "centroid_hz", "low_power", "mid_power", "high_power",
    "tilt", "stereo_correlation",
]
SETTINGS = ["index", "name", "category", "note", "sample_rate", "velocity", "pressure",
            "hold_seconds", "tail_seconds", "rank"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(key):
    return json.loads(INPUTS[key].read_text())


def write_json(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


def quantile(values, fraction):
    ordered = sorted(values)
    at = fraction * (len(ordered) - 1)
    low = math.floor(at)
    high = math.ceil(at)
    return ordered[low] + (at - low) * (ordered[high] - ordered[low])


def distribution(values):
    return {"n": len(values), "min": min(values), "p10": quantile(values, .1),
            "median": statistics.median(values), "p90": quantile(values, .9), "max": max(values),
            "negative": sum(v < 0 for v in values), "zero": sum(v == 0 for v in values),
            "positive": sum(v > 0 for v in values)} if values else {"n": 0}


def changed(a, b):
    return any(float(a[k]) != float(b[k]) for k in SIGNAL)


def descriptor(row):
    return {"host_index": int(row["index"]), "name": row["name"], "category": row["category"]}


def deltas(a, b, column):
    return [float(y[column]) - float(x[column]) for x, y in zip(a, b)]


banks = {k: list(csv.DictReader(INPUTS[k].open())) for k in SOURCES}
for label, rows in banks.items():
    assert len(rows) == 523, label
    assert [int(r["index"]) for r in rows] == list(range(523)), label
    assert len({r["name"] for r in rows}) == 523, label
    for row in rows:
        assert all(math.isfinite(float(row[k])) for k in SIGNAL), (label, row["name"])
        assert row["sample_rate"] == "48000" and row["velocity"] == "0.8000000119"
        assert row["pressure"] == "0" and row["hold_seconds"] == "3" and row["tail_seconds"] == "1"
        assert row["rank"] == "0", "Both authored ranks, not an isolated rank"
for rows in banks.values():
    for a, b in zip(banks["current_7"], rows):
        assert all(a[k] == b[k] for k in SETTINGS), (a["index"], a["name"])

matched = read_json("matched_receipt")
reconstruction = read_json("reconstruction")
baseline_provenance = read_json("baseline_provenance")
for run, key in zip(matched["runs"], ["published_4", "current_7"]):
    assert run["source_commit"] == SOURCES[key]
    assert run["metrics_sha256"] == sha(INPUTS[key])
assert baseline_provenance["source_commit"] == SOURCES["baseline_6"]
assert baseline_provenance["files"]["baseline-bank-metrics.csv"]["sha256"] == sha(INPUTS["baseline_6"])
assert reconstruction["inventory_sha256"] == sha(INPUTS["published_inventory"])
assert reconstruction["inventory_exactly_matches_authenticated_a12_published_fixture"]

classification = read_json("a12_classification")
mechanisms = read_json("a12_mechanisms")
assert classification["before_sha256"] == sha(INPUTS["published_inventory"])
assert classification["current_sha256"] == sha(INPUTS["current_inventory"])
assert mechanisms["inventory_sha256"] == sha(INPUTS["current_inventory"])
classes = {p["host_index"]: p for p in classification["programs"]}
assert len(classes) == 523
direct = {i for i, p in classes.items() if p["beyond_conservative_levels_pan"]}
baseline_only = {i for i, p in classes.items() if p["category"] != "Init" and not p["beyond_conservative_levels_pan"]}
assert len(direct) == 493 and len(baseline_only) == 28
assert not direct & baseline_only
for i in baseline_only:
    p = classes[i]
    assert set(p["changed_published_controls"]) <= {"ch1_v_level", "ch2_v_level", "volume"}
    assert any(r["balance_gain"] > 0 and r["init_level"] > 0 and r["changed_additive_vca_baseline"]
               for r in p["ranks"])
assert classification["new_7_preset_parameter_edits"] == 0

musical = [i for i, r in enumerate(banks["current_7"]) if r["category"] != "Init"]
sounding = [i for i in musical if float(banks["published_4"][i]["peak_dbfs"]) > -240
            and float(banks["current_7"][i]["peak_dbfs"]) > -240]
assert len(musical) == 521 and len(sounding) == 519
assert set(musical) - set(sounding) == {179, 444}

comparisons = {}
for akey, bkey in [("published_4", "current_7"), ("published_4", "baseline_6"), ("baseline_6", "current_7")]:
    a, b = banks[akey], banks[bkey]
    changed_ids = [i for i in musical if changed(a[i], b[i])]
    aa, bb = [a[i] for i in sounding], [b[i] for i in sounding]
    result = {
        "before_source": SOURCES[akey], "after_source": SOURCES[bkey],
        "metric_vector_changed_musical": len(changed_ids),
        "changed_host_indices": changed_ids,
        "unchanged_all_programs": [descriptor(a[i]) for i in range(523) if not changed(a[i], b[i])],
        "changed_by_category": dict(sorted(Counter(a[i]["category"] for i in changed_ids).items())),
        "distributions_over_519_sounding_musical_programs": {
            k: distribution(deltas(aa, bb, k)) for k in SIGNAL},
        "centroid_relative_percent": distribution([
            100 * (float(y["centroid_hz"]) / float(x["centroid_hz"]) - 1) for x, y in zip(aa, bb)]),
    }
    comparisons[akey + "_to_" + bkey] = result
assert comparisons["published_4_to_current_7"]["metric_vector_changed_musical"] == 519
assert comparisons["baseline_6_to_current_7"]["metric_vector_changed_musical"] == 67

a, b = banks["published_4"], banks["current_7"]
category_rows = []
for category in dict.fromkeys(r["category"] for r in b):
    ids = [i for i, r in enumerate(b) if r["category"] == category]
    ids_sounding = [i for i in ids if i in sounding]
    category_rows.append({
        "category": category, "programs": len(ids), "musical_programs": sum(i in musical for i in ids),
        "beyond_conservative_level_pan": sum(i in direct for i in ids),
        "active_additive_vca_baseline_only": sum(i in baseline_only for i in ids),
        "changed_signal_vector_published4_to_current7": sum(changed(a[i], b[i]) for i in ids),
        "pressure_supplement_programs": sum(i in {179, 444} for i in ids),
        "changed_signal_vector_baseline6_to_current7": sum(changed(banks["baseline_6"][i], b[i]) for i in ids),
        "default_note": "/".join(sorted({r["note"] for r in b if r["category"] == category})),
        "sounding_musical_n": len(ids_sounding),
        "median_rms_delta_db": statistics.median(float(b[i]["rms_dbfs"]) - float(a[i]["rms_dbfs"]) for i in ids_sounding) if ids_sounding else None,
        "median_centroid_delta_hz": statistics.median(float(b[i]["centroid_hz"]) - float(a[i]["centroid_hz"]) for i in ids_sounding) if ids_sounding else None,
    })
with (OUT / "category-summary.csv").open("w", newline="") as f:
    w = csv.DictWriter(f, category_rows[0].keys()); w.writeheader(); w.writerows(category_rows)

program_rows = []
for i, row in enumerate(b):
    out = {**descriptor(row), "default_note": row["note"],
           "parameter_class": "beyond_level_pan" if i in direct else "additive_vca_baseline_only" if i in baseline_only else "Init",
           "signal_metrics_changed_4_to_7": changed(a[i], row),
           "signal_metrics_changed_6_to_7": changed(banks["baseline_6"][i], row),
           "pressure_supplement": i in {179, 444}}
    for k in SIGNAL:
        out["published4_" + k] = float(a[i][k])
        out["current7_" + k] = float(row[k])
        out["delta_" + k] = float(row[k]) - float(a[i][k])
    program_rows.append(out)
with (OUT / "program-deltas.csv").open("w", newline="") as f:
    w = csv.DictWriter(f, program_rows[0].keys()); w.writeheader(); w.writerows(program_rows)

window_sensitivity = {}
for column in ["body_rms_dbfs", "tail_rms_dbfs"]:
    window_sensitivity[column] = {
        "floor_dbfs": -240,
        "floor_counts": {key: sum(float(banks[key][i][column]) == -240 for i in musical) for key in SOURCES},
        "counts_strictly_above_level": {str(cut): {
            key: sum(float(banks[key][i][column]) > cut for i in musical) for key in SOURCES}
            for cut in [-100, -80, -60, -40]},
        "delta_distribution_both_windows_strictly_above_level": {str(cut): distribution([
            float(b[i][column]) - float(a[i][column]) for i in sounding
            if float(a[i][column]) > cut and float(b[i][column]) > cut])
            for cut in [-240, -120, -100, -80, -60]},
    }

extrema = {}
for column in ["peak_dbfs", "rms_dbfs", "body_rms_dbfs", "tail_rms_dbfs", "centroid_hz"]:
    ordered = sorted(sounding, key=lambda i: float(b[i][column]) - float(a[i][column]))
    extrema[column] = {label: [{**descriptor(a[i]), "published4": float(a[i][column]),
        "current7": float(b[i][column]), "delta": float(b[i][column]) - float(a[i][column])}
        for i in ids] for label, ids in [("lowest_three", ordered[:3]), ("highest_three", ordered[-3:])]}

pressure = read_json("a23_pressure")
assert pressure["captures"] == 8
assert {(r["host_index"], r["pressure"]) for r in pressure["comparisons"]} == {(i, p) for i in [179, 444] for p in [.3, 1.0]}
for r in pressure["comparisons"]:
    assert r["published_source"] == SOURCES["published_4"] and r["current_source"] == SOURCES["current_7"]
    assert r["max_abs_sample_difference"] > 0
    for field in ["published_metrics", "current_metrics"]:
        assert int(r[field]["nonfinite"]) == int(r[field]["samples_over_full_scale"]) == 0
        assert float(r[field]["peak_dbfs"]) < 0
pressure_summary = [{k: r[k] for k in ["preset", "host_index", "pressure", "current_minus_published_peak_db",
    "current_minus_published_body_rms_db", "waveform_difference_relative_to_published_rms_db"]} for r in pressure["comparisons"]]

changed7 = set(comparisons["baseline_6_to_current_7"]["changed_host_indices"])
mechanism_observation = {
    k: {"structural_programs": v["program_count"], "changed_in_bank_probe": len(changed7 & set(v["host_indices"]))}
    for k, v in mechanisms["summary"].items()
}
unique_primary = [set(mechanisms["summary"][k]["host_indices"]) & changed7
                  for k in ["generic_string_formants", "piano_partial_decay", "low_choir_harmonics"]]
assert len(set.union(*unique_primary)) == 67
assert sum(map(len, unique_primary)) == 67

safety = {}
for key, rows in banks.items():
    peak = max(rows, key=lambda r: float(r["peak_dbfs"]))
    safety[key] = {"programs": len(rows), "nonfinite": sum(int(r["nonfinite"]) for r in rows),
                   "samples_over_full_scale": sum(int(r["samples_over_full_scale"]) for r in rows),
                   "largest_peak_dbfs": float(peak["peak_dbfs"]), "peak_program": descriptor(peak)}
    assert safety[key]["nonfinite"] == safety[key]["samples_over_full_scale"] == 0

capture_sources = {}
for key, commit in SOURCES.items():
    data = subprocess.check_output(["git", "show", commit + ":src/tools/preset_capture.cpp"], cwd=ROOT)
    capture_sources[key] = hashlib.sha256(data).hexdigest()
assert len(set(capture_sources.values())) == 1

summary = {
    "scope": "Local deterministic core captures; numerical coverage only, no listening approval, native qualification, stress bound or full-envelope claim.",
    "sources": SOURCES,
    "capture_method": {"sample_rate": 48000, "velocity_float32": .8000000119, "pressure": 0,
        "hold_seconds": 3, "tail_seconds": 1, "both_ranks": True,
        "midi_note_counts_all_523": dict(sorted(Counter(r["note"] for r in b).items())),
        "signal_columns_compared": SIGNAL, "excluded_from_signal_equality": ["parameter_digest", "safety_counts"],
        "rms_dbfs_window_seconds": [0, 4], "early_rms_dbfs_window_seconds": [0, .2],
        "body_rms_dbfs_window_seconds": [2.5, 3], "tail_rms_dbfs_window_seconds": [3.5, 4],
        "centroid_definition": "Power-weighted stereo spectrum, 4096-point Hann window around each capture's loudest 50 ms held window; each capture selects its own window.",
        "decibel_floor": -240, "capture_source_sha256": capture_sources},
    "accounting": {"all_programs": 523, "musical": 521, "init": 2,
        "original_address_musical": 221, "curated_musical": 300,
        "parameter_changes_beyond_conservative_level_pan": len(direct),
        "active_additive_vca_baseline_only": len(baseline_only),
        "rank_baseline_only_by_category": dict(sorted(Counter(classes[i]["category"] for i in baseline_only).items())),
        "new_7_recipe_edits": 0, "changed_default_signal_vectors": 519,
        "additional_pressure_required_programs": 2, "union_musical_with_signal_change_evidence": 521,
        "published4_current7_whole_bank_captures": 1046, "whole_bank_audio_seconds": 4184,
        "a23_pressure_extra_captures": 8, "a23_pressure_extra_audio_seconds": 19.2,
        "no_new_whole_bank_render_by_a33": True},
    "comparisons": comparisons,
    "categories": category_rows,
    "window_sensitivity": window_sensitivity,
    "extrema": extrema,
    "pressure_supplement": pressure_summary,
    "mechanism_observation_6_to_7": mechanism_observation,
    "safety": safety,
}
write_json("metrics-summary.json", summary)
write_json("receipt.json", {
    "passed": True, "analysis_script_sha256": sha(Path(__file__)),
    "inputs": {k: {"path": str(v.relative_to(ROOT)), "sha256": sha(v), "bytes": v.stat().st_size} for k, v in INPUTS.items()},
    "outputs": {name: {"sha256": sha(OUT / name), "bytes": (OUT / name).stat().st_size}
                for name in ["metrics-summary.json", "category-summary.csv", "program-deltas.csv"]},
    "checks": "523 unique aligned records per bank; matched settings; all retained source/metric/inventory receipts authenticate; identical capture implementation; 493+28 parameter partition; 519+2 signal partition; 67 newly differing signal vectors; finite CSV values and zero recorded nonfinite/over-full-scale samples.",
})
print(json.dumps({"passed": True, "accounting": summary["accounting"], "notes": summary["capture_method"]["midi_note_counts_all_523"],
                  "rms_delta": comparisons["published_4_to_current_7"]["distributions_over_519_sounding_musical_programs"]["rms_dbfs"]}, indent=2))
