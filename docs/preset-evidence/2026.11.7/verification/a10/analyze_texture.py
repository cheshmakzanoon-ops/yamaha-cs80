#!/usr/bin/env python3
"""Analyze actual Engine captures, without private-state or source-model access."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import numpy as np

MODELS = {10: "flute", 9: "bowed", 11: "clarinet", 14: "brass"}
RATES = (44100, 48000, 96000)
FUNDAMENTAL = 440 * 2 ** ((60 - 69) / 12)


def measure(path, rate):
    raw = path.read_bytes()
    stereo = np.frombuffer(raw, dtype="<f4").reshape(-1, 2)
    assert len(stereo) == rate * 4 and np.isfinite(stereo).all(), str(path)
    mono = stereo.astype(np.float64).mean(axis=1)[rate:]
    window = np.hanning(len(mono))
    frequency = np.fft.rfftfreq(len(mono), 1 / rate)
    # One-sided mean-square contribution per FFT bin; frequency bins being
    # integrated here exclude DC and Nyquist, so all receive the factor of 2.
    power = 2 * abs(np.fft.rfft(mono * window)) ** 2 / (len(mono) * sum(window * window))
    harmonic_distance = abs(frequency - np.rint(frequency / FUNDAMENTAL) * FUNDAMENTAL)
    result = {
        "sha256": hashlib.sha256(raw).hexdigest(),
        "whole_rms": float(np.sqrt(np.mean(mono * mono))),
        "peak": float(np.max(abs(stereo))),
    }
    for width in (20, 40):
        away = harmonic_distance > width
        for low, high in ((1000, 12000), (1000, 4000), (4000, 12000)):
            band = (frequency >= low) & (frequency <= high) & away
            result[f"power_{low}_{high}_exclude{width}"] = float(power[band].sum())
    return result


def db_ratio(numerator, denominator):
    return float(10 * np.log10(numerator / denominator))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("captures", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    details = {}
    rows = []
    for model, name in MODELS.items():
        for rate in RATES:
            row = {"model": name, "rate_hz": rate, "rt_factor": 2, "hq_offline_factor": 4 if rate == 96000 else 8}
            for revision in ("before", "after"):
                metrics = {}
                paths = {}
                for mode in ("rt", "hq", "offline"):
                    path = args.captures / revision / "pcm" / f"{model}_{rate}_{mode}.f32"
                    paths[mode] = path
                    metrics[mode] = measure(path, rate)
                details[f"{revision}/{name}/{rate}"] = metrics
                row[f"{revision}_offline_exact_hq"] = paths["hq"].read_bytes() == paths["offline"].read_bytes()
                for width in (20, 40):
                    for low, high in ((1000, 12000), (1000, 4000), (4000, 12000)):
                        key = f"power_{low}_{high}_exclude{width}"
                        row[f"{revision}_hq_rt_db_{low}_{high}_exclude{width}"] = db_ratio(metrics["hq"][key], metrics["rt"][key])
                for mode in ("rt", "hq"):
                    row[f"{revision}_{mode}_whole_rms"] = metrics[mode]["whole_rms"]
            row["rt_before_after_exact"] = details[f"before/{name}/{rate}"]["rt"]["sha256"] == details[f"after/{name}/{rate}"]["rt"]["sha256"]
            rows.append(row)
            print(f"{name:8s} {rate:5d} Hz: before HQ/RT={row['before_hq_rt_db_1000_12000_exclude20']:+.4f} dB, after={row['after_hq_rt_db_1000_12000_exclude20']:+.4f} dB, offline=HQ {row['after_offline_exact_hq']}")
    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "texture_summary.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (args.output / "texture_measurements.json").write_text(json.dumps(details, indent=2) + "\n")
    assert all(row["after_offline_exact_hq"] and row["before_offline_exact_hq"] for row in rows)
    assert max(abs(row["after_hq_rt_db_1000_12000_exclude20"]) for row in rows) < .5
    assert max(abs(row["after_hq_rt_db_1000_12000_exclude40"]) for row in rows) < .5
    print("PASS: all offline/HQ streams exact; all corrected 1–12 kHz nonharmonic comparisons within 0.5 dB with both ±20 and ±40 Hz harmonic exclusions.")


if __name__ == "__main__":
    main()
