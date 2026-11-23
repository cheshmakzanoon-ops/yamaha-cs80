#!/usr/bin/env python3
"""Compare complete, matched 2026.11.3/4 capture metrics, without a realism score.

The input CSVs are producer measurements, not raw audio. This script validates
their identities, conditions, finite/sample flags, inventory digests and source
manifests, and separates level from spectral, envelope and stereo changes. It
never interprets a numerical difference as a perceptual improvement. Output is
atomic; invalid or incomplete input leaves any existing report untouched.

Run --self-test for synthetic negative/admission and gain-only comparison tests.
All paths may be overridden; pass --before-pressure and --after-pressure together
to include the two programs that are intentionally silent without aftertouch.
"""
from __future__ import annotations

import argparse
from collections import Counter
from copy import deepcopy
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile


COUNT = 523
BEFORE_COMMIT = "fa57967c7317828cf769cc7ca97f5857e566520f"
BEFORE_TREE = "715212d1f2cb45b1aad3d7fd23416276b8b938d7"
BEFORE_CSV_SHA = "72e51e3433d9695c63b7b91d46f6b325d68ac5cae502e0b7c51fead363d215ab"
BEFORE_INVENTORY_SHA = "6f070ed15a04259fd5a9d5f0eb06a6e9554f821871c3313e9a793a3b28a2b521"
INIT = {"Init Patch", "Init Circuit"}
PRESSURE = {"Electric Leads 136", "Pressure Lead"}
LEVEL = ("peak_dbfs", "rms_dbfs", "early_rms_dbfs", "body_rms_dbfs", "tail_rms_dbfs")
SPECTRAL = ("centroid_hz", "low_power", "mid_power", "high_power", "tilt")
STEREO = ("stereo_correlation",)
AUDIO = LEVEL + ("peak_window_seconds",) + SPECTRAL + STEREO
INTEGERS = ("index", "note", "sample_rate", "rank", "nonfinite", "samples_over_full_scale")
CONDITIONS = ("note", "sample_rate", "velocity", "pressure", "hold_seconds", "tail_seconds", "rank")
HEADER = ("index", "name", "category") + CONDITIONS[:1] + CONDITIONS[1:6] + ("rank", "parameter_digest") + AUDIO + ("nonfinite", "samples_over_full_scale")
REQUIRED_SOURCES = {
    "src/core/" + f for f in (
        "Engine.cpp", "Engine.h", "InstrumentBody.h", "InstrumentBodyFilter.h", "Params.h",
        "FactoryPresets.cpp", "FactoryPresets.h", "CuratedLibrary.inc", "OriginalCircuitLibrary.inc",
        "CuratedLibraryLevels.inc", "OriginalCircuitLevels.inc")
} | {"src/core/curated/" + f + ".inc" for f in (
    "Bass", "Bells", "Brass", "Fx", "Keys", "Leads", "Modern", "Organ", "Pads", "Plucked",
    "Polysynth", "Strings", "Winds")}


class InvalidEvidence(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise InvalidEvidence(message)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise InvalidEvidence(f"Nonfinite JSON constant: {value}")


def json_load(raw):
    return json.loads(raw, object_pairs_hook=unique_object, parse_constant=reject_constant)


def finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def f32(value):
    require(finite(value), f"Nonfinite/non-numeric inventory value: {value!r}")
    try:
        result = struct.pack("<f", value)
    except (OverflowError, struct.error) as error:
        raise InvalidEvidence(f"Not a finite binary32 value: {value!r}") from error
    require(math.isfinite(struct.unpack("<f", result)[0]), "Binary32 overflow")
    return result


def digest(values):
    h = 14695981039346656037
    for value in values:
        for byte in f32(value):
            h = ((h ^ byte) * 1099511628211) & 0xffffffffffffffff
    return format(h, "x")


def validate_inventory(inventory, revision, parameter_count):
    require(inventory.get("schema") == 1 and inventory.get("library_revision") == revision,
            f"Unexpected inventory schema/revision; expected {revision}")
    ids = inventory.get("parameter_ids", [])
    require(len(ids) == parameter_count and all(isinstance(x, str) and x for x in ids)
            and len(set(ids)) == len(ids), "Invalid/duplicate parameter IDs")
    require("volume" in ids, "Missing master volume parameter")
    programs = inventory.get("programs", [])
    require(len(programs) == COUNT, f"Inventory must contain exactly {COUNT} programs")
    names = set()
    for index, p in enumerate(programs):
        require(type(p.get("index")) is int and type(p.get("host_index")) is int
                and p["index"] == p["host_index"] == index, f"Inventory order mismatch at {index}")
        require(isinstance(p.get("name"), str) and p["name"] and p["name"] not in names,
                f"Missing/duplicate inventory name at {index}")
        names.add(p["name"])
        require(isinstance(p.get("category"), str) and p["category"], f"Missing category at {index}")
        require(isinstance(p.get("tags"), list) and all(isinstance(t, str) for t in p["tags"]),
                f"Invalid tags at {index}")
        require(len(p.get("values", [])) == len(ids), f"Parameter count mismatch at {index}")
        for value in p["values"]:
            f32(value)
    require({p["name"] for p in programs if p["category"] == "Init"} == INIT,
            "Unexpected Init identity set")


def validate_inventory_pair(before, after):
    validate_inventory(before, "2026.11.3", 198)
    validate_inventory(after, "2026.11.4", 200)
    require(after["parameter_ids"] == before["parameter_ids"] + ["body1_size", "body2_size"],
            "Existing parameter order changed or unexpected parameters appended")
    for a, b in zip(before["programs"], after["programs"]):
        require(all(a[key] == b[key] for key in ("index", "host_index", "name", "category", "tags")),
                f"Program identity changed: {a['name']}")
        if a["name"] in INIT:
            require([f32(v) for v in a["values"]] == [f32(v) for v in b["values"][:198]]
                    and b["values"][198:] == [1, 1], f"Init parameters changed: {a['name']}")


def default_note(category):
    return 48 if category == "Bass" else 67 if category in ("Winds", "Leads") else 72 if category == "Bells" else 60


def parse_metrics(raw):
    require(raw.endswith(b"\n"), "CSV is incomplete: final newline missing")
    reader = csv.DictReader(io.StringIO(raw.decode("utf-8")), strict=True)
    require(tuple(reader.fieldnames or ()) == HEADER, "Unexpected/missing/duplicate CSV header columns")
    result = []
    for number, raw_row in enumerate(reader, start=2):
        require(None not in raw_row and all(v not in (None, "") for v in raw_row.values()),
                f"Incomplete CSV row {number}")
        row = dict(raw_row)
        for key, value in raw_row.items():
            if key in ("name", "category", "parameter_digest"):
                continue
            if key in INTEGERS:
                require(re.fullmatch(r"0|[1-9][0-9]*", value), f"Invalid integer {key} at row {number}")
                row[key] = int(value)
            else:
                try:
                    row[key] = float(value)
                except ValueError as error:
                    raise InvalidEvidence(f"Invalid numeric {key} at row {number}") from error
                require(math.isfinite(row[key]), f"Nonfinite {key} at row {number}")
        require(re.fullmatch(r"[0-9a-f]{1,16}", row["parameter_digest"]), f"Invalid digest at row {number}")
        result.append(row)
    return result


def validate_rows(rows, inventory, pressure):
    selected = [p for p in inventory["programs"] if pressure == 0 or p["name"] in PRESSURE]
    require(len(rows) == len(selected), f"Incomplete/extra metric rows: {len(rows)}; expected {len(selected)}")
    require(len({r["name"] for r in rows}) == len(rows) and len({r["index"] for r in rows}) == len(rows),
            "Duplicate metric identity")
    for row, program in zip(rows, selected):
        name = program["name"]
        require(all(row[k] == program[k] for k in ("index", "name", "category")),
                f"CSV identity/order mismatch: expected {name}")
        expected = (default_note(program["category"]), 48000, float(struct.unpack("<f", f32(.8))[0]), pressure, 3, 1, 0)
        require(all(math.isclose(row[k], value, rel_tol=0, abs_tol=5e-10)
                    for k, value in zip(CONDITIONS, expected)), f"Unmatched render condition: {name}")
        require(row["parameter_digest"] == digest(program["values"]), f"Inventory/digest mismatch: {name}")
        require(row["nonfinite"] == row["samples_over_full_scale"] == 0,
                f"Nonfinite or over-full-scale samples: {name}")
        require(all(-240 <= row[k] <= 0 for k in LEVEL), f"Invalid dBFS range: {name}")
        require(all(row[k] <= row["peak_dbfs"] + 1e-7 for k in LEVEL[1:]), f"RMS exceeds peak: {name}")
        require(0 <= row["peak_window_seconds"] <= 2.95 + 1e-9, f"Invalid held peak window: {name}")
        require(abs(row["peak_window_seconds"] * 20 - round(row["peak_window_seconds"] * 20)) < 1e-7,
                f"Peak window not on 50ms grid: {name}")
        require(0 <= row["centroid_hz"] <= 24000 and 0 <= row["tilt"] <= 4 + 1e-8
                and -1 - 1e-9 <= row["stereo_correlation"] <= 1 + 1e-9,
                f"Invalid spectral/stereo range: {name}")
        bands = [row[k] for k in ("low_power", "mid_power", "high_power")]
        require(all(0 <= v <= 1 for v in bands) and (sum(bands) == 0 or abs(sum(bands) - 1) <= 1e-8),
                f"Invalid normalized band powers: {name}")
        silent = row["peak_dbfs"] == -240
        require(silent == (pressure == 0 and name in PRESSURE), f"Unexpected silence/pressure response: {name}")
        if silent:
            require(all(row[k] == -240 for k in LEVEL) and all(row[k] == 0 for k in AUDIO if k not in LEVEL),
                    f"Inconsistent silent measurements: {name}")


def envelope(row):
    # A floor-clamped window cannot supply a useful logarithmic ratio.
    def ratio(a, b):
        return row[a] - row[b] if min(row[a], row[b]) > -240 else None
    return {"body_minus_early_db": ratio("body_rms_dbfs", "early_rms_dbfs"),
            "tail_minus_body_db": ratio("tail_rms_dbfs", "body_rms_dbfs"),
            "crest_db": ratio("peak_dbfs", "rms_dbfs"),
            "peak_window_seconds": row["peak_window_seconds"] if row["peak_dbfs"] > -240 else None}


def numerical_change(a, b, key):
    # Tolerances bound CSV decimal rounding. They are not audibility thresholds.
    tolerance = 1e-6 if key.endswith("db") or key.endswith("dbfs") else 1e-5 if key == "centroid_hz" else 1e-9
    return a != b if a is None or b is None else abs(a - b) > tolerance


def compare_rows(before, after, before_inventory, after_inventory):
    require(len(before) == len(after), "Before/after row counts differ")
    volume_a = before_inventory["parameter_ids"].index("volume")
    volume_b = after_inventory["parameter_ids"].index("volume")
    records = []
    for a, b in zip(before, after):
        require(all(a[k] == b[k] for k in ("index", "name", "category") + CONDITIONS),
                f"Before/after identity/condition mismatch: {a['name']}")
        changed = [k for k in AUDIO if numerical_change(a[k], b[k], k)]
        exact_changed = [k for k in AUDIO if a[k] != b[k]]
        if a["name"] in INIT:
            require(not exact_changed, f"Init audio metrics changed: {a['name']}: {exact_changed}")
        ea, eb = envelope(a), envelope(b)
        envelope_changed = [k for k in ea if numerical_change(ea[k], eb[k], k)]
        pa = before_inventory["programs"][a["index"]]["values"]
        pb = after_inventory["programs"][b["index"]]["values"]
        groups = {"level": any(k in changed for k in LEVEL),
                  "spectral": any(k in changed for k in SPECTRAL),
                  "envelope": bool(envelope_changed),
                  "stereo": any(k in changed for k in STEREO)}
        records.append({
            "index": a["index"], "name": a["name"], "category": a["category"],
            "fixture": {k: a[k] for k in CONDITIONS},
            "parameter_digest": {"before": a["parameter_digest"], "after": b["parameter_digest"],
                                 "changed": a["parameter_digest"] != b["parameter_digest"]},
            "master_volume_parameter": {"before": pa[volume_a], "after": pb[volume_b],
                                        "changed": f32(pa[volume_a]) != f32(pb[volume_b])},
            "audio_metrics_changed": bool(changed), "metric_groups_changed": groups,
            "changed_fields": changed, "exactly_different_fields": exact_changed,
            "before": {k: a[k] for k in AUDIO}, "after": {k: b[k] for k in AUDIO},
            "delta_after_minus_before": {k: b[k] - a[k] for k in AUDIO},
            "relative_envelope": {"before": ea, "after": eb, "changed_fields": envelope_changed,
                                  "delta_after_minus_before": {k: eb[k] - ea[k] if ea[k] is not None and eb[k] is not None else None for k in ea}},
        })
    return records


def summarize(records):
    return {
        "programs": len(records),
        "parameter_digest_changed": sum(r["parameter_digest"]["changed"] for r in records),
        "master_volume_parameter_changed": sum(r["master_volume_parameter"]["changed"] for r in records),
        "audio_metrics_changed_above_rounding_tolerance": sum(r["audio_metrics_changed"] for r in records),
        "audio_metrics_exactly_different": sum(bool(r["exactly_different_fields"]) for r in records),
        "metric_group_changes": {k: sum(r["metric_groups_changed"][k] for r in records) for k in ("level", "spectral", "envelope", "stereo")},
        "only_level_metrics_changed": [r["name"] for r in records if r["metric_groups_changed"]["level"] and not any(r["metric_groups_changed"][k] for k in ("spectral", "envelope", "stereo"))],
        "unchanged_audio_metric_vectors": [r["name"] for r in records if not r["exactly_different_fields"]],
        "parameter_digest_changed_without_audio_metric_change": [r["name"] for r in records if r["parameter_digest"]["changed"] and not r["audio_metrics_changed"]],
        "categories": dict(sorted(Counter(r["category"] for r in records).items())),
    }


class Snapshot:
    def __init__(self, repo):
        self.repo = repo.resolve()
        self.files = {}

    def read(self, path, role):
        path = Path(path).resolve()
        raw = path.read_bytes()
        self.files[role] = (path, sha(raw))
        return raw

    def binding(self, role):
        path, checksum = self.files[role]
        return {"path": str(path.relative_to(self.repo)) if path.is_relative_to(self.repo) else path.name,
                "sha256": checksum}

    def recheck(self):
        for role, (path, checksum) in self.files.items():
            require(sha(path.read_bytes()) == checksum, f"Input changed during comparison: {role}")


def atomic_write(path, report):
    encoded = json.dumps(report, indent=2, sort_keys=True, allow_nan=False) + "\n"
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent,
                                         prefix=".audio-comparison-", suffix=".tmp", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(encoded)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary and temporary.exists():
            temporary.unlink()


def create_report(args):
    snapshot = Snapshot(args.repo)
    before = json_load(snapshot.read(args.before_inventory, "before_inventory"))
    after = json_load(snapshot.read(args.after_inventory, "after_inventory"))
    validate_inventory_pair(before, after)
    a = parse_metrics(snapshot.read(args.before, "before_metrics"))
    b = parse_metrics(snapshot.read(args.after, "after_metrics"))
    validate_rows(a, before, 0)
    validate_rows(b, after, 0)
    bp = json_load(snapshot.read(args.before_provenance, "before_provenance"))
    ap = json_load(snapshot.read(args.after_provenance, "after_provenance"))
    require(snapshot.binding("before_inventory")["sha256"] == BEFORE_INVENTORY_SHA
            and snapshot.binding("before_metrics")["sha256"] == BEFORE_CSV_SHA,
            "Baseline differs from the pinned 2026.11.3 evidence")
    require(bp.get("source") == BEFORE_COMMIT and bp.get("source_tree") == BEFORE_TREE,
            "Baseline source identity mismatch")
    require(bp.get("programs") == bp.get("metrics_rows") == COUNT and bp.get("parameters") == 198,
            "Baseline provenance counts mismatch")
    require(bp.get("fixture") == {"rate": 48000, "note": "category default", "velocity": .8, "pressure": 0, "hold": 3, "tail": 1},
            "Baseline provenance fixture mismatch")
    require(bp.get("sha256", {}).get("inventory.json") == BEFORE_INVENTORY_SHA
            and bp.get("sha256", {}).get("single-note-metrics.csv") == BEFORE_CSV_SHA,
            "Baseline provenance input hashes mismatch")
    snapshot.read(args.before_binary, "before_capture_binary")
    snapshot.read(args.after_binary, "after_capture_binary")
    require(snapshot.binding("before_capture_binary")["sha256"] == bp.get("development_binary_hashes_not_archived_here", {}).get("cs80_preset_capture"),
            "Baseline executable hash mismatch")
    require(snapshot.binding("after_capture_binary")["sha256"] == ap.get("capture_sha256"),
            "Candidate executable hash mismatch")
    require(snapshot.binding("after_inventory")["sha256"] == ap.get("inventory_sha256"),
            "Candidate inventory/provenance hash mismatch")
    source_hashes = ap.get("source_sha256", {})
    require(REQUIRED_SOURCES <= source_hashes.keys(), "Candidate source manifest is incomplete")
    for relative, expected in sorted(source_hashes.items()):
        path = (args.repo / relative).resolve()
        require(path.is_relative_to(args.repo.resolve()) and re.fullmatch(r"[0-9a-f]{64}", expected),
                f"Invalid source manifest entry: {relative}")
        require(sha(snapshot.read(path, "source:" + relative)) == expected,
                f"Candidate source hash mismatch: {relative}")
    producer = snapshot.read(args.repo / "src/tools/preset_capture.cpp", "capture_implementation")
    baseline_producer = subprocess.run(["git", "show", BEFORE_COMMIT + ":src/tools/preset_capture.cpp"],
                                      cwd=args.repo, check=True, capture_output=True).stdout
    require(producer == baseline_producer, "Capture measurement implementation changed from baseline")
    snapshot.read(Path(__file__), "comparison_implementation")
    records = compare_rows(a, b, before, after)
    require(bool(args.before_pressure) == bool(args.after_pressure), "Provide both pressure fixture CSVs")
    pressure_report = {"status": "not supplied; two pressure-dependent programs require separate pressure=1 evidence"}
    pressure_records = []
    if args.before_pressure:
        pa = parse_metrics(snapshot.read(args.before_pressure, "before_pressure_metrics"))
        pb = parse_metrics(snapshot.read(args.after_pressure, "after_pressure_metrics"))
        validate_rows(pa, before, 1)
        validate_rows(pb, after, 1)
        pressure_records = compare_rows(pa, pb, before, after)
        pressure_report = {"status": "validated; both programs produce non-silent finite audio at pressure=1",
                           "summary": summarize(pressure_records), "programs": pressure_records}
    audible = [r for r in records if r["name"] not in INIT | PRESSURE] + pressure_records
    report = {
        "schema": 1, "release": "2026.11.4", "status": "validated development metric comparison",
        "inputs": {role: snapshot.binding(role) for role in snapshot.files if not role.startswith("source:")},
        "source_provenance": {
            "before_commit": BEFORE_COMMIT, "before_tree": BEFORE_TREE,
            "after_state": ap.get("state"), "after_source_sha256_verified": source_hashes,
            "capture_implementation_equal_to_baseline": True,
            "qualification": "Input/executable hashes and current source manifest are verified; the manifest associates the binary with source, but this script does not independently prove its compilation or qualify native plugin builds.",
        },
        "validation": {
            "ordered_program_count": COUNT, "identities_and_conditions_match": True,
            "parameter_digests_match_float32_inventory_values": True,
            "all_recorded_numbers_finite": True, "nonfinite_sample_flags": 0,
            "over_full_scale_sample_flags": 0, "init_audio_metrics_exactly_unchanged": sorted(INIT),
            "intentional_zero_pressure_silence": sorted(PRESSURE),
            "expected_frames_per_render_derived_from_conditions": 192000,
            "expected_stereo_sample_values_per_render_derived_from_conditions": 384000,
            "sample_count_qualification": "CSV contains no observed frame/sample count; these counts are derived from 48000 Hz * (3 s held + 1 s tail), not independently measured from WAV files.",
        },
        "method": {
            "single_note_fixture": {"sample_rate": 48000, "velocity": struct.unpack("<f", f32(.8))[0], "pressure": 0, "hold_seconds": 3, "tail_seconds": 1,
                                    "category_notes": {"Bass": 48, "Winds": 67, "Leads": 67, "Bells": 72, "all_other_categories": 60}},
            "level_fields": list(LEVEL), "spectral_fields": list(SPECTRAL), "stereo_fields": list(STEREO),
            "relative_envelope": "Body minus early RMS, tail minus body RMS, crest factor and loudest held-window time; ratios touching the -240 dB floor are null.",
            "measurement_windows": {"early_seconds": [0, .2], "body_seconds": [2.5, 3], "tail_seconds": [3.5, 4],
                                    "peak_search_window_seconds": .05, "spectrum_fft_frames": 4096,
                                    "spectrum": "Hann-windowed summed left/right power around the loudest held window; DC excluded; low <500 Hz, mid 500..<2000 Hz, high >=2000 Hz."},
            "rounding_tolerances": {"db": 1e-6, "centroid_hz": 1e-5, "other_metrics": 1e-9},
            "limitations": [
                "No acoustic-reference recordings, perceptual score, or claim that every numerical change is an improvement.",
                "Parameter digests include appended fields: a changed digest alone does not establish an audio change, including for both unchanged Init programs.",
                "Normalized spectra, relative envelopes, crest and stereo correlation are invariant under ideal static linear gain above their numeric floors; output nonlinearity and all other changed controls can also affect them. These comparisons do not isolate causation.",
                "Spectral measurements can use different peak-window positions before and after; they do not describe every part of the sound.",
                "Three-second single-note holds do not cover full slow-pad development, performance range, voices, velocity/pressure curves, long effects tails, or all plugin/native builds.",
                "Exactly equal aggregate metric vectors are not proof of sample-identical waveforms; tolerance counts address CSV rounding only, not audibility.",
            ],
        },
        "summary": summarize(records),
        "musical_programs_using_pressure_where_required": summarize(audible),
        "pressure_fixtures": pressure_report, "programs": records,
    }
    snapshot.recheck()
    return report


def self_test():
    ids = ["volume"] + [f"p{i}" for i in range(1, 198)]
    names = [f"Program {i}" for i in range(COUNT)]
    names[43], names[179], names[444], names[522] = "Init Patch", "Electric Leads 136", "Pressure Lead", "Init Circuit"
    inventory = {"schema": 1, "library_revision": "2026.11.3", "parameter_ids": ids,
                 "programs": [{"index": i, "host_index": i, "name": name,
                               "category": "Init" if name in INIT else "Leads" if name in PRESSURE else "Keys",
                               "tags": [], "values": [.8] + [0.] * 197} for i, name in enumerate(names)]}
    candidate = deepcopy(inventory)
    candidate["library_revision"] = "2026.11.4"
    candidate["parameter_ids"] += ["body1_size", "body2_size"]
    for program in candidate["programs"]:
        program["values"] += [1., 1.]
    validate_inventory_pair(inventory, candidate)
    rows = []
    for p in inventory["programs"]:
        row = {"index": p["index"], "name": p["name"], "category": p["category"],
               "note": default_note(p["category"]), "sample_rate": 48000, "velocity": struct.unpack("<f", f32(.8))[0],
               "pressure": 0., "hold_seconds": 3., "tail_seconds": 1., "rank": 0,
               "parameter_digest": digest(p["values"]), "peak_dbfs": -10., "rms_dbfs": -20.,
               "early_rms_dbfs": -15., "body_rms_dbfs": -20., "tail_rms_dbfs": -60.,
               "peak_window_seconds": .1, "centroid_hz": 1000., "low_power": .2, "mid_power": .6,
               "high_power": .2, "tilt": .02, "stereo_correlation": .9, "nonfinite": 0, "samples_over_full_scale": 0}
        if p["name"] in PRESSURE:
            row.update({k: -240. if k in LEVEL else 0. for k in AUDIO})
        rows.append(row)
    validate_rows(rows, inventory, 0)
    count = 0

    def rejects(action):
        nonlocal count
        try:
            action()
        except (InvalidEvidence, ValueError):
            count += 1
        else:
            raise AssertionError("Invalid fixture was accepted")

    rejects(lambda: validate_rows(rows[:-1], inventory, 0))
    for key, value in (("name", rows[1]["name"]), ("velocity", .6), ("parameter_digest", "deadbeef"),
                       ("samples_over_full_scale", 1), ("nonfinite", 1), ("peak_dbfs", float("nan"))):
        bad = deepcopy(rows)
        bad[0][key] = value
        rejects(lambda: validate_rows(bad, inventory, 0))
    buffer = io.StringIO()
    writer = csv.DictWriter(buffer, fieldnames=HEADER)
    writer.writeheader()
    writer.writerows(rows)
    csv_bytes = buffer.getvalue().encode()
    require(parse_metrics(csv_bytes) == rows, "CSV round-trip failed")
    rejects(lambda: parse_metrics(csv_bytes.rstrip(b"\r\n")))
    rejects(lambda: parse_metrics(csv_bytes.replace(b"-10.0", b"nan", 1)))
    rejects(lambda: parse_metrics(csv_bytes.replace(b"peak_dbfs,", b"rms_dbfs,", 1)))
    rejects(lambda: json_load(b'{"x":1,"x":2}'))
    rejects(lambda: json_load(b'{"x":NaN}'))
    output_rows = deepcopy(rows)
    for row, program in zip(output_rows, candidate["programs"]):
        row["parameter_digest"] = digest(program["values"])
    unchanged = compare_rows(rows, output_rows, inventory, candidate)
    require(all(r["parameter_digest"]["changed"] and not r["audio_metrics_changed"] for r in unchanged),
            "Parameter digest incorrectly treated as an audio change")
    for key in LEVEL:
        output_rows[0][key] -= 6
    validate_rows(output_rows, candidate, 0)
    gain_only = compare_rows(rows, output_rows, inventory, candidate)[0]
    require(gain_only["metric_groups_changed"] == {"level": True, "spectral": False, "envelope": False, "stereo": False},
            "Uniform level shift incorrectly treated as timbral/envelope change")
    output_rows[43]["centroid_hz"] += .1
    rejects(lambda: compare_rows(rows, output_rows, inventory, candidate))
    with tempfile.TemporaryDirectory() as directory:
        destination = Path(directory) / "report.json"
        destination.write_bytes(b"existing report\n")
        rejects(lambda: atomic_write(destination, {"invalid": float("nan")}))
        require(destination.read_bytes() == b"existing report\n",
                "Invalid output replaced the previous report")
        atomic_write(destination, {"valid": True})
        require(json_load(destination.read_bytes()) == {"valid": True}
                and len(list(Path(directory).iterdir())) == 1,
                "Atomic output did not cleanly replace the report")
    print(f"PASS: {count} invalid fixtures rejected; digest-only and gain-only changes classified correctly")


def main():
    repo = Path(__file__).resolve().parents[1]
    evidence = repo / "docs/preset-evidence/2026.11.4"
    parser = argparse.ArgumentParser(description=__doc__)
    defaults = {
        "repo": repo, "before": evidence / "baseline/single-note-metrics.csv",
        "after": evidence / "development/final-bank/single-note-metrics.csv",
        "before-inventory": evidence / "baseline/inventory.json",
        "after-inventory": evidence / "development/final-bank/inventory.json",
        "before-provenance": evidence / "baseline/provenance.json",
        "after-provenance": evidence / "development/final-bank/provenance.json",
        "before-binary": repo.parent / "baseline/cs80_preset_capture",
        "after-binary": repo / "build-core/cs80_preset_capture",
        "out": evidence / "audio-comparison.json",
    }
    for option, default in defaults.items():
        parser.add_argument("--" + option, type=Path, default=default)
    parser.add_argument("--before-pressure", type=Path)
    parser.add_argument("--after-pressure", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            self_test()
            return 0
        report = create_report(args)
        atomic_write(args.out, report)
        print(json.dumps({"report": str(args.out), "summary": report["summary"],
                          "pressure_fixtures": report["pressure_fixtures"]["status"]}, indent=2))
        return 0
    except (InvalidEvidence, OSError, ValueError, KeyError, TypeError, csv.Error, subprocess.CalledProcessError) as error:
        print(f"Audio comparison rejected: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
