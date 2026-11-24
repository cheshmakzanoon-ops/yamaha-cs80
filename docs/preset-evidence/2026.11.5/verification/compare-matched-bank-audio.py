#!/usr/bin/env python3
"""Compare complete matched factory renders; this is not a perceptual score.

Requires Python 3 and NumPy. Does not render, modify WAVs, normalize deliverables,
or qualify a plugin release. Equal-RMS calculations apply only to analysis arrays.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import struct
from collections import defaultdict
from pathlib import Path

import numpy as np

BASELINE_SHA256 = "0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611"
SETTINGS = ("note", "sample_rate", "velocity", "pressure", "hold_seconds", "tail_seconds", "rank")
SILENT_NAMES = {"Electric Leads 136", "Pressure Lead"}
INIT_NAMES = {"Init Patch", "Init Circuit"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def db(value):
    return 20.0 * math.log10(max(float(value), 1e-12))


def rms(data):
    return float(np.sqrt(np.mean(np.square(data)))) if data.size else 0.0


def parameter_digest(values):
    result = 14695981039346656037
    for value in values:
        require(math.isfinite(value), "Nonfinite inventory value")
        for byte in struct.pack("<f", value):
            result = ((result ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return format(result, "x")


def filename(program):
    return str(program["index"]) + "-" + re.sub(r"[^A-Za-z0-9]", "-", program["name"]) + ".wav"


def read_metrics(path):
    with path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        require(reader.fieldnames is not None and len(set(reader.fieldnames)) == len(reader.fieldnames),
                "Missing or repeated CSV fields")
        rows = list(reader)
    indexed = {}
    for row in rows:
        index = int(row["index"])
        require(index not in indexed, "Duplicate metrics index")
        for key in SETTINGS:
            require(math.isfinite(float(row[key])), "Nonfinite capture setting")
        require(int(row["rank"]) == 0, "Expected complete-patch capture")
        indexed[index] = row
    return indexed


def read_wav(data, frames, rate):
    require(0 < frames <= 60 * rate, "Unsupported frame bound")
    require(len(data) == 58 + frames * 8, "Unexpected IEEE float WAV byte count")
    require(data[:4] == b"RIFF" and data[8:12] == b"WAVE", "Not RIFF/WAVE")
    require(struct.unpack_from("<I", data, 4)[0] == len(data) - 8, "RIFF size mismatch")
    offset = 12
    chunks = {}
    while offset < len(data):
        require(offset + 8 <= len(data), "Truncated chunk header")
        kind, size = struct.unpack_from("<4sI", data, offset)
        end = offset + 8 + size
        require(kind not in chunks and end <= len(data), "Duplicate or truncated chunk")
        chunks[kind] = data[offset + 8:end]
        offset = end + (size & 1)
    require(offset == len(data) and set(chunks) == {b"fmt ", b"fact", b"data"}, "Unexpected chunks")
    require(len(chunks[b"fmt "]) == 18 and struct.unpack("<HHIIHHH", chunks[b"fmt "]) ==
            (3, 2, rate, rate * 8, 8, 32, 0), "Expected stereo IEEE float32 at the stated rate")
    require(chunks[b"fact"] == struct.pack("<I", frames), "Incorrect fact frame count")
    pcm = chunks[b"data"]
    require(len(pcm) == frames * 8, "PCM length mismatch")
    samples = np.frombuffer(pcm, dtype="<f4").reshape(frames, 2)
    require(np.isfinite(samples).all(), "Nonfinite audio")
    require(float(np.max(np.abs(samples))) <= 1.0, "Audio exceeds full scale")
    return pcm, samples.astype(np.float64)


def averaged_spectrum(audio, held):
    """16 equal-spaced 4096-frame Hann windows; stereo powers, no mono cancellation."""
    size = 4096
    require(held >= size, "Held signal too short for spectral comparison")
    starts = np.unique(np.rint(np.linspace(0, held - size, 16)).astype(np.int64))
    windows = np.stack([audio[start:start + size] for start in starts])
    windows *= np.hanning(size)[None, :, None]
    bins = np.fft.rfft(windows, axis=1)
    power = np.mean(np.square(np.abs(bins)), axis=(0, 2))
    power[0] = 0.0
    power[1:-1] *= 2.0
    total = float(np.sum(power))
    return power / total if total > 0 else None


def audio_measures(audio, held, rate):
    early = rms(audio[:min(held, rate // 5)])
    late = rms(audio[max(0, held - rate // 2):held])
    tail = rms(audio[max(held, len(audio) - rate // 2):])
    return {
        "peak_dbfs": db(np.max(np.abs(audio))), "rms_dbfs": db(rms(audio)),
        "early_rms_dbfs": db(early), "body_rms_dbfs": db(late), "tail_rms_dbfs": db(tail),
        "late_held_relative_to_early_db": db(late / early) if early > 0 else None,
        "release_tail_relative_to_late_held_db": db(tail / late) if late > 0 else None,
    }


def compare_audio(before, after, held, rate):
    require(before.shape == after.shape, "Audio shapes differ")
    before_rms, after_rms = rms(before), rms(after)
    before_measure = audio_measures(before, held, rate)
    after_measure = audio_measures(after, held, rate)
    result = {
        "before": before_measure, "after": after_measure,
        "raw_difference_rms_dbfs": db(rms(after - before)),
        "both_silent": before_rms == 0.0 and after_rms == 0.0,
        "rms_change_db": db(after_rms / before_rms) if before_rms > 0 and after_rms > 0 else None,
        "equal_rms_residual": None, "waveform_correlation": None,
        "averaged_spectral_hellinger": None,
        "before_averaged_centroid_hz": None, "after_averaged_centroid_hz": None,
    }
    if before_rms > 0 and after_rms > 0:
        left, right = before / before_rms, after / after_rms
        result["equal_rms_residual"] = rms(right - left)
        result["waveform_correlation"] = min(1.0, max(-1.0, float(np.mean(left * right))))
        first, second = averaged_spectrum(before, held), averaged_spectrum(after, held)
        if first is not None and second is not None:
            result["averaged_spectral_hellinger"] = float(np.sqrt(0.5 * np.sum(np.square(np.sqrt(first) - np.sqrt(second)))))
            frequencies = np.arange(len(first)) * rate / 4096.0
            result["before_averaged_centroid_hz"] = float(np.sum(first * frequencies))
            result["after_averaged_centroid_hz"] = float(np.sum(second * frequencies))
    return result


def paths_by_name(directory):
    result = {}
    for path in directory.rglob("*.wav"):
        require(not path.is_symlink(), "Symlink audio is unsupported")
        require(path.name not in result, "Duplicate WAV filename")
        result[path.name] = path
    return result


def compare_set(before_dir, after_dir, before_inventory, after_inventory, expected_names=None):
    first = read_metrics(before_dir / "metrics.csv")
    second = read_metrics(after_dir / "metrics.csv")
    before_programs = {p["index"]: p for p in before_inventory["programs"]}
    after_programs = {p["index"]: p for p in after_inventory["programs"]}
    selected = set(before_programs) if expected_names is None else {
        i for i, p in before_programs.items() if p["name"] in expected_names}
    require(set(first) == set(second) == selected, "Missing or additional metric programs")
    before_paths, after_paths = paths_by_name(before_dir), paths_by_name(after_dir)
    expected_files = {filename(before_programs[i]) for i in selected}
    require(set(before_paths) == set(after_paths) == expected_files, "Missing or additional WAVs")
    results = []
    for index in sorted(selected):
        old, new = before_programs[index], after_programs[index]
        a, b = first[index], second[index]
        require((old["name"], old["category"], old["host_index"]) ==
                (new["name"], new["category"], new["host_index"]), "Program identity changed")
        require(a["name"] == b["name"] == old["name"] and a["category"] == b["category"] == old["category"],
                "CSV identity differs from inventory")
        for setting in SETTINGS:
            require(float(a[setting]) == float(b[setting]), "Unmatched capture condition")
        require(a["parameter_digest"].lower() == parameter_digest(old["values"]), "Old settings digest mismatch")
        require(b["parameter_digest"].lower() == parameter_digest(new["values"]), "New settings digest mismatch")
        rate = int(a["sample_rate"])
        require(rate == 48000 and 0 <= int(a["note"]) <= 127, "Unexpected note/rate")
        require(0 <= float(a["velocity"]) <= 1 and 0 <= float(a["pressure"]) <= 1, "Invalid gesture")
        held = math.floor(float(a["hold_seconds"]) * rate + 0.5)
        frames = held + math.floor(float(a["tail_seconds"]) * rate + 0.5)
        name = filename(old)
        before_data, after_data = before_paths[name].read_bytes(), after_paths[name].read_bytes()
        before_pcm, before_audio = read_wav(before_data, frames, rate)
        after_pcm, after_audio = read_wav(after_data, frames, rate)
        measured = compare_audio(before_audio, after_audio, held, rate)
        for metrics, actual in [(a, measured["before"]), (b, measured["after"])]:
            require(int(metrics["nonfinite"]) == int(metrics["samples_over_full_scale"]) == 0, "CSV integrity counters fail")
            for key in ("peak_dbfs", "rms_dbfs", "early_rms_dbfs", "body_rms_dbfs", "tail_rms_dbfs"):
                require(abs(float(metrics[key]) - actual[key]) < 1e-6, "CSV energy metric differs from actual PCM")
        results.append({
            "index": index, "name": old["name"], "category": old["category"],
            "settings": {key: a[key] for key in SETTINGS}, "frames": frames,
            "before_wav_sha256": sha(before_data), "after_wav_sha256": sha(after_data),
            "before_pcm_sha256": sha(before_pcm), "after_pcm_sha256": sha(after_pcm),
            "exact_pcm_equal": before_pcm == after_pcm,
            "changed_float32_bit_patterns": int(np.count_nonzero(
                np.frombuffer(before_pcm, dtype="<u4") != np.frombuffer(after_pcm, dtype="<u4"))),
            **measured,
        })
    return results


def summary(rows):
    audible = [r for r in rows if not r["both_silent"]]
    values = lambda key: [r[key] for r in audible if r[key] is not None]
    def span(key):
        data = values(key)
        return dict(min=min(data), median=float(np.median(data)), max=max(data)) if data else None
    return {
        "programs": len(rows), "stereo_frames_per_version": sum(r["frames"] for r in rows),
        "seconds_per_version": sum(r["frames"] for r in rows) / 48000,
        "all_samples_finite_and_within_full_scale": True,
        "exact_pcm_equal": [r["name"] for r in rows if r["exact_pcm_equal"]],
        "both_silent": [r["name"] for r in rows if r["both_silent"]],
        "changed_raw_pcm": sum(not r["exact_pcm_equal"] for r in rows),
        "audible_comparisons": len(audible),
        "equal_rms_residual_above_0_001": sum(r["equal_rms_residual"] is not None and r["equal_rms_residual"] > .001 for r in rows),
        "averaged_spectral_hellinger_above_0_01": sum(r["averaged_spectral_hellinger"] is not None and r["averaged_spectral_hellinger"] > .01 for r in rows),
        "rms_change_db": span("rms_change_db"), "equal_rms_residual": span("equal_rms_residual"),
        "averaged_spectral_hellinger": span("averaged_spectral_hellinger"),
        "highest_before_peak_dbfs": max(r["before"]["peak_dbfs"] for r in rows),
        "highest_after_peak_dbfs": max(r["after"]["peak_dbfs"] for r in rows),
    }


def self_test():
    rate, frames = 48000, 4800
    t = np.arange(frames) / rate
    a = np.repeat((.1 * np.sin(2 * np.pi * 500 * t))[:, None], 2, axis=1)
    p = compare_audio(a, a * 2, frames, rate)
    require(abs(p["rms_change_db"] - 6.020599913279624) < 1e-12 and p["equal_rms_residual"] < 1e-14,
            "Gain-only control was mislabeled as a waveform change")
    p = compare_audio(a, -a, frames, rate)
    require(abs(p["equal_rms_residual"] - 2) < 1e-12, "Polarity inversion was hidden")
    p = compare_audio(a * 0, a * 0, frames, rate)
    require(p["both_silent"] and p["equal_rms_residual"] is None, "Silent comparison invented a ratio")
    b = np.repeat((.1 * np.sin(2 * np.pi * 1500 * t))[:, None], 2, axis=1)
    require(compare_audio(a, b, frames, rate)["averaged_spectral_hellinger"] > .99, "Distinct spectra missed")
    pcm = a.astype("<f4").tobytes()
    data = b"RIFF" + struct.pack("<I", 50 + len(pcm)) + b"WAVEfmt " + struct.pack("<IHHIIHHH", 18, 3, 2, rate, rate * 8, 8, 32, 0)
    data += b"fact" + struct.pack("<II", 4, frames) + b"data" + struct.pack("<I", len(pcm)) + pcm
    require(read_wav(data, frames, rate)[0] == pcm, "Valid float WAV rejected")
    invalid = [data[:-1], data + b"\0", data[:20] + struct.pack("<H", 1) + data[22:],
               data[:46] + struct.pack("<I", frames + 1) + data[50:],
               data[:58] + struct.pack("<f", float("nan")) + data[62:],
               data[:58] + struct.pack("<f", 1.001) + data[62:]]
    for case in invalid:
        try:
            read_wav(case, frames, rate)
        except ValueError:
            continue
        raise AssertionError("Malformed WAV accepted")
    print("PASS: gain/polarity/silence/spectral controls, valid WAV and six malformed-WAV negatives")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path)
    parser.add_argument("--after", type=Path)
    parser.add_argument("--before-inventory", type=Path)
    parser.add_argument("--after-inventory", type=Path)
    parser.add_argument("--after-executable", type=Path)
    parser.add_argument("--pressure-before", type=Path)
    parser.add_argument("--pressure-after", type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    require(all(getattr(args, key) is not None for key in
                ("before", "after", "before_inventory", "after_inventory", "after_executable", "pressure_before", "pressure_after", "out")),
            "All comparison input paths and a fresh output are required")
    require(not args.out.exists(), "Refusing to overwrite existing evidence")
    before_bytes, after_bytes = args.before_inventory.read_bytes(), args.after_inventory.read_bytes()
    require(sha(before_bytes) == BASELINE_SHA256, "Frozen baseline inventory identity mismatch")
    before_inventory, after_inventory = json.loads(before_bytes), json.loads(after_bytes)
    require(before_inventory["library_revision"] == "2026.11.4" and after_inventory["library_revision"] == "2026.11.5", "Wrong revisions")
    require(len(before_inventory["programs"]) == len(after_inventory["programs"]) == 523, "Expected all 523 programs")
    require(after_inventory["parameter_ids"][:200] == before_inventory["parameter_ids"] and
            len(after_inventory["parameter_ids"]) == 202, "Parameter prefix/count mismatch")
    manifest_path = args.after / "capture-manifest.json"
    require(manifest_path.is_file() and (args.after / "metrics.csv").is_file(), "Final matched capture is incomplete")
    manifest_bytes = manifest_path.read_bytes()
    manifest = json.loads(manifest_bytes)
    require(manifest["program_count"] == 523 and sum(g["programs"] for g in manifest["groups"]) == 523,
            "Capture manifest is incomplete")
    require(manifest["baseline_metrics_sha256"] == sha((args.before / "metrics.csv").read_bytes()), "Baseline metrics changed")
    require(manifest["executable_sha256"] == sha(args.after_executable.read_bytes()), "Final capture executable identity mismatch")
    rows = compare_set(args.before, args.after, before_inventory, after_inventory)
    require({r["name"] for r in rows if r["both_silent"]} == SILENT_NAMES, "Unexpected zero-pressure silence")
    require(all(r["exact_pcm_equal"] for r in rows if r["name"] in INIT_NAMES), "Init audio differs")
    pressure = compare_set(args.pressure_before, args.pressure_after, before_inventory, after_inventory, SILENT_NAMES)
    require(all(float(r["settings"]["pressure"]) == 1 and not r["both_silent"] for r in pressure), "Missing audible pressure controls")
    categories = defaultdict(list)
    for row in rows:
        categories[row["category"]].append(row)
    result = {
        "schema": 1, "scope": "Matched local whole-bank PCM; separate from native release qualification and perceptual realism.",
        "release_qualified": False,
        "methods": {
            "waveform": "No time or phase alignment. Each complete stereo clip is divided by its own RMS for equal-RMS residual; delivered audio is never normalized.",
            "spectrum": "Mean stereo power of 16 equally spaced 4096-frame Hann windows within the held note; DC excluded, interior positive-frequency power doubled, total power normalized. Hellinger distance spans 0–1.",
            "time_windows": "Early: first 200 ms; late held: final 500 ms before key-up; release tail: last 500 ms after key-up.",
            "thresholds": "0.001 equal-RMS residual and 0.01 spectral distance are descriptive counts, not listening or quality acceptance thresholds.",
            "limitations": "One matched note and velocity gesture per address, plus pressure controls for two intentional silent-at-zero-pressure voices. Phase, articulation, tuning and effects all affect residuals. Differences do not prove realism, preference or exact instrument identity."
        },
        "inputs": {
            "analysis_script_sha256": sha(Path(__file__).read_bytes()), "numpy_version": np.__version__,
            "before_inventory_sha256": sha(before_bytes), "after_inventory_sha256": sha(after_bytes),
            "before_metrics_sha256": sha((args.before / "metrics.csv").read_bytes()),
            "after_metrics_sha256": sha((args.after / "metrics.csv").read_bytes()),
            "after_capture_manifest_sha256": sha(manifest_bytes),
            "after_executable_sha256": manifest["executable_sha256"],
            "pressure_before_metrics_sha256": sha((args.pressure_before / "metrics.csv").read_bytes()),
            "pressure_after_metrics_sha256": sha((args.pressure_after / "metrics.csv").read_bytes()),
        },
        "summary": summary(rows), "families": {key: summary(value) for key, value in sorted(categories.items())},
        "pressure_summary": summary(pressure), "programs": rows, "pressure_programs": pressure,
    }
    with args.out.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(json.dumps({"summary": result["summary"], "pressure_summary": result["pressure_summary"]}, indent=2))


if __name__ == "__main__":
    main()
