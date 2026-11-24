#!/usr/bin/env python3
"""Reproduce A11 complete-preset development captures and gain-free shape metrics.

These are authored-model comparisons, not recordings or reference-instrument fits.
Raw WAVs are reproducible scratch artifacts; JSON keeps all metrics and hashes.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import numpy as np

NAMES = ("Harp", "Koto", "Banjo", "Kalimba")
RATE = 48000
NOTES = (48, 60, 72)
VELOCITIES = (.25, .55, .85)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_wav(path):
    data = path.read_bytes()
    offset = 12
    fmt = None
    audio = None
    while offset + 8 <= len(data):
        tag, size = struct.unpack_from("<4sI", data, offset)
        start = offset + 8
        if tag == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", data, start)
        if tag == b"data":
            audio = np.frombuffer(data[start:start + size], dtype="<f4").astype(np.float64)
        offset = start + size + (size & 1)
    if fmt is None or fmt[0] != 3 or fmt[1] != 2 or fmt[2] != RATE or fmt[5] != 32 or audio is None:
        raise RuntimeError(f"Expected stereo 48kHz float WAV: {path}")
    return audio.reshape(-1, 2)


def db(amplitude):
    return float(20 * np.log10(max(float(amplitude), 1e-15)))


def rms(audio, start, stop):
    return float(np.sqrt(np.mean(audio[round(start * RATE):round(stop * RATE)] ** 2)))


def spectrum(audio):
    segment = audio[round(.025 * RATE):round(.300 * RATE)].mean(axis=1)
    power = np.abs(np.fft.rfft(segment * np.hanning(len(segment)))) ** 2
    frequency = np.fft.rfftfreq(len(segment), 1 / RATE)
    boundaries = 40 * 2 ** (np.arange(56) / 6)
    bands = np.array([power[(frequency >= a) & (frequency < b)].sum()
                      for a, b in zip(boundaries[:-1], boundaries[1:])])
    bands /= bands.sum()
    centroid = float((power * frequency).sum() / power.sum())
    return bands, centroid


def metrics(audio):
    _, centroid = spectrum(audio)
    return dict(finite=bool(np.isfinite(audio).all()), peak_dbfs=db(np.abs(audio).max()),
                early_rms_dbfs=db(rms(audio, .015, .45)),
                held_tail_relative_db=db(rms(audio, 2, 2.4) / rms(audio, .015, .15)),
                post_release_rms_dbfs=db(rms(audio, 2.8, 3.2)),
                early_power_centroid_hz=centroid)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--wav-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    executables = {"baseline": args.baseline.resolve(), "candidate": args.candidate.resolve()}
    manifest = {"comparison": "Complete two-rank factory patches; no gain processing in WAVs",
                "baseline_commit": "c76ad288e02fbeca48f39652bf4fcdd99837f793",
                "candidate_provenance": "Development working tree, not a release commit",
                "settings": {"sample_rate": RATE, "hold_seconds": 2.5, "tail_seconds": 1.2,
                             "pressure": 0, "notes": NOTES, "velocities": VELOCITIES},
                "binary_sha256": {label: sha(path) for label, path in executables.items()},
                "candidate_core_source_sha256": {str(p.relative_to(args.repo)): sha(p)
                    for p in sorted((args.repo / "src/core").rglob("*"))
                    if p.is_file() and p.suffix in (".h", ".cpp", ".inc")}}
    jobs = [(label, note, velocity) for label in executables for note in NOTES for velocity in VELOCITIES]
    def capture(job):
        label, note, velocity = job
        target = args.wav_dir / label / f"n{note}-v{round(velocity * 100)}"
        command = [str(executables[label]), "--out", str(target), "--wav", "--library", "curated"]
        for name in NAMES:
            command += ["--preset", name]
        command += ["--rate", str(RATE), "--note", str(note), "--velocity", str(velocity),
                    "--pressure", "0", "--hold", "2.5", "--tail", "1.2"]
        result = subprocess.run(command, check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(label, note, velocity, "captured", flush=True)
        return {"label": label, "note": note, "velocity": velocity, "command": command,
                "stdout": result.stdout, "metrics_csv_sha256": sha(target / "metrics.csv")}
    with ThreadPoolExecutor(max_workers=2) as pool:
        manifest["captures"] = list(pool.map(capture, jobs))
    comparisons = []
    for name in NAMES:
        for note in NOTES:
            for velocity in VELOCITIES:
                row = dict(name=name, note=note, velocity=velocity)
                pair = {}
                for label in executables:
                    target = args.wav_dir / label / f"n{note}-v{round(velocity * 100)}"
                    paths = list(target.glob(f"*-{name}.wav"))
                    if len(paths) != 1:
                        raise RuntimeError(f"Expected one {name} in {target}")
                    audio = read_wav(paths[0])
                    pair[label] = audio
                    row[label] = dict(wav_sha256=sha(paths[0]), **metrics(audio))
                old, new = [spectrum(pair[label])[0] for label in ("baseline", "candidate")]
                row["normalized_spectral_power_l1_distance"] = float(np.abs(old - new).sum())
                row["early_rms_change_db"] = row["candidate"]["early_rms_dbfs"] - row["baseline"]["early_rms_dbfs"]
                comparisons.append(row)
    manifest["comparisons"] = comparisons
    manifest["summary"] = {}
    for name in NAMES:
        rows = [row for row in comparisons if row["name"] == name]
        manifest["summary"][name] = {
            "render_pairs": len(rows),
            "shape_distance_min_max": [min(r["normalized_spectral_power_l1_distance"] for r in rows), max(r["normalized_spectral_power_l1_distance"] for r in rows)],
            "early_rms_change_db_min_max": [min(r["early_rms_change_db"] for r in rows), max(r["early_rms_change_db"] for r in rows)],
            "candidate_peak_dbfs_max": max(r["candidate"]["peak_dbfs"] for r in rows),
            "candidate_all_finite": all(r["candidate"]["finite"] for r in rows),
        }
    manifest["interpretation_limits"] = [
        "No physical reference recording was fitted or perceptually matched.",
        "Uniform gain cancels from normalized spectral power distributions.",
        "Kalimba now plays the MIDI pitch in both ranks; c76 used octave and double-octave ranks, so its baseline comparison also reflects the corrected register.",
        "The separate PluckedMaterialPresetTests material-off control isolates the material at identical pitch, envelopes and gain.",
        "Perceptual superiority and exact instrument identity cannot be established from these metrics alone."]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest["summary"], indent=2), flush=True)


if __name__ == "__main__":
    main()
