#!/usr/bin/env python3
"""A20 scoped Organ audit. Run from any directory; raw PCM stays in /tmp."""
import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import subprocess
import tarfile

import numpy as np

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
CURRENT = "c5549f7d095a5c5f49a5ad22ec0ccc819245d201"
BASELINE = "18711f93a39aa49f3d6f9a41784be456224facbe"
RATE = 48000


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def write_json(name, data):
    (OUT / name).write_text(json.dumps(data, indent=2, allow_nan=False) + "\n")


def db(x):
    return 20 * math.log10(max(float(x), 1e-15))


def rms(x, a, b):
    y = x[round(a * RATE):round(b * RATE)].astype(np.float64)
    return float(np.sqrt(np.mean(y * y)))


def tone(x, a, b, hz):
    """Hann-weighted narrowband magnitude; stereo energy avoids mono cancellation."""
    y = x[round(a * RATE):round(b * RATE)].astype(np.float64)
    w = np.hanning(len(y))
    phase = np.exp(-2j * np.pi * hz * np.arange(len(y)) / RATE)
    z = 2 * np.sum(y * (w * phase)[:, None], axis=0) / np.sum(w)
    return float(np.sqrt(np.mean(np.abs(z) ** 2)))


def run(args):
    subprocess.run(args, cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--current-build", type=Path, default=Path("/tmp/cs80-build-1cee5a"))
    parser.add_argument("--baseline-build", type=Path, default=Path("/tmp/cs80-reference-1cee5a"))
    parser.add_argument("--scratch", type=Path, default=Path("/tmp/cs80-a20"))
    opt = parser.parse_args()
    opt.scratch.mkdir(parents=True, exist_ok=True)
    inventory = json.loads((ROOT / "docs/preset-evidence/2026.11.7/verification/a12/inventory.json").read_text())
    old_inventory = json.loads(git("show", BASELINE + ":docs/preset-evidence/2026.11.6/verification/a15/inventory.json"))
    ids = inventory["parameter_ids"]
    organ = [p for p in inventory["programs"] if p["category"] == "Organ"]
    assert len(organ) == 15
    fields = ["mix", "volume", "oversampling", "detune", "drift", "voice_dispersion", "analog_noise",
              "sub_speed", "sub_vco", "sub_vca", "sub_at_speed", "sub_at_vco", "sub_at_vcf",
              "chorus_on", "chorus_mode", "chorus_speed", "chorus_depth",
              "lfo1_rate", "lfo1_depth", "lfo1_dest", "lfo1_wave"]
    for ch in (1, 2):
        fields += [f"ch{ch}_{k}" for k in ("feet", "saw", "pulse", "sine", "pw", "hpf_cutoff", "lpf_cutoff",
                    "v_a", "v_d", "v_s", "v_r", "v_level", "init_brill", "init_level", "after_brill", "after_level")]
        fields += [f"body{ch}_{k}" for k in ("model", "mix", "brightness", "decay", "size", "variant", "material")]
    audit = []
    for p in organ:
        old = old_inventory["programs"][p["index"]]
        assert p == old, p["name"]
        v = dict(zip(ids, p["values"]))
        assert all(v[f"ch{ch}_{key}"] == 0 for ch in (1, 2) for key in ("init_brill", "init_level"))
        audit.append({"index": p["index"], "host_index": p["host_index"], "name": p["name"],
                      "description": p["description"], "tags": p["tags"], "exactly_equal_to_dot6": True,
                      "all_204_values_sha256": sha(np.asarray(p["values"], dtype="<f4").tobytes()),
                      "controls": {k: v[k] for k in fields}})
    assert len({tuple(p["values"][i] for i in range(len(ids)) if ids[i] != "volume") for p in organ}) == 15
    write_json("inventory-audit.json", {"current_source": CURRENT, "baseline_source": BASELINE,
               "program_count": 15, "unchanged_program_records": 15, "programs": audit})

    builds = {"current": (CURRENT, opt.current_build), "baseline": (BASELINE, opt.baseline_build)}
    provenance = {"current_source": CURRENT, "baseline_source": BASELINE,
                  "probe_sha256": sha((OUT / "organ_probe.cpp").read_bytes()),
                  "reproduce_sha256": sha(Path(__file__).read_bytes()),
                  "sample_rate": RATE, "block_size": 96, "quality": "Standard, authored oversampling=1",
                  "chord8_notes": [48, 55, 60, 64, 67, 72, 76, 79],
                  "scope": "Local core actual-preset renders, no native package or listening claim", "builds": {}}
    for label, (commit, build) in builds.items():
        archive = git("archive", commit, "src")
        source = opt.scratch / (label + "-src")
        source.mkdir(exist_ok=True)
        # Both executables compile against their own complete archived src tree.
        # InstrumentBody's ABI differs between these commits.
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            tar.extractall(source, filter="data")
        lib = build / "libcs80_core.a"
        before = sha(lib.read_bytes())
        exe = opt.scratch / ("probe-" + label)
        command = ["c++", "-O2", "-std=c++20", "-I" + str(source / "src"),
                   str(OUT / "organ_probe.cpp"), str(lib), "-o", str(exe)]
        run(command)
        with (OUT / (label + "-renders.csv")).open("w") as stream:
            subprocess.run([str(exe), str(opt.scratch / label), label], stdout=stream, cwd=ROOT, check=True)
        assert before == sha(lib.read_bytes())
        provenance["builds"][label] = {"source_commit": commit, "src_archive_sha256": sha(archive),
               "library": str(lib), "library_sha256_before_and_after": before,
               "probe_executable_sha256": sha(exe.read_bytes()), "compile_command": command}

    metrics, audio, manifest = [], {}, []
    for label in builds:
        for entry in csv.DictReader((OUT / (label + "-renders.csv")).open()):
            name, scenario = entry["name"], entry["scenario"]
            stem = name.replace(" ", "_").replace("+", "_")
            file = opt.scratch / label / f"{stem}-{scenario}.f32"
            raw = file.read_bytes()
            x = np.frombuffer(raw, dtype="<f4").reshape(-1, 2)
            audio[label, name, scenario] = x
            off = int(entry["note_off_frame"]) / RATE
            peak = float(np.max(np.abs(x)))
            m = {"build": label, "name": name, "scenario": scenario, "frames": len(x),
                 "peak_dbfs": db(peak), "rms_dbfs": db(rms(x, 0, len(x) / RATE)),
                 "nonfinite": int(np.count_nonzero(~np.isfinite(x))),
                 "samples_over_full_scale": int(np.count_nonzero(np.abs(x) > 1)),
                 "first_20ms_rms_dbfs": db(rms(x, 0, .020)),
                 "body_rms_dbfs": db(rms(x, .5, 1)),
                 "late_body_rms_dbfs": db(rms(x, off - .5, off)),
                 "release_75_105ms_relative_db": db(rms(x, off + .075, off + .105) / rms(x, off - .3, off - .1))}
            assert m["nonfinite"] == 0 and m["samples_over_full_scale"] == 0 and peak > 1e-5
            metrics.append(m)
            manifest.append({"build": label, "name": name, "scenario": scenario,
                             "bytes": len(raw), "sha256": sha(raw)})
    with (OUT / "metrics.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(metrics[0]))
        writer.writeheader()
        writer.writerows(metrics)
    write_json("pcm-manifest.json", manifest)

    comparisons = []
    for (label, name, scenario), x in audio.items():
        if label != "baseline": continue
        y = audio["current", name, scenario]
        comparisons.append({"name": name, "scenario": scenario, "exact_pcm_equal": bool(np.array_equal(x, y)),
                            "maximum_sample_difference": float(np.max(np.abs(x - y)))})
    touch = []
    for name in [p["name"] for p in organ if ("current", p["name"], "single") in audio]:
        firm, soft = audio["current", name, "single"], audio["current", name, "soft"]
        row = {"name": name, "velocity_0_25_vs_0_85_exact_pcm_equal": bool(np.array_equal(firm, soft)),
               "velocity_held_contrast_db": db(rms(firm, .5, 2) / rms(soft, .5, 2))}
        if ("current", name, "pressure") in audio:
            pressed = audio["current", name, "pressure"]
            row.update({"pressure_0_vs_0_8_exact_pcm_equal": bool(np.array_equal(firm, pressed)),
                        "pressure_held_contrast_db": db(rms(pressed, .5, 2) / rms(firm, .5, 2)),
                        "pressure_523hz_contrast_db": db(tone(pressed, 1.2, 1.8, 523.2511306) / tone(firm, 1.2, 1.8, 523.2511306))})
        touch.append(row)

    percussion = audio["current", "Percussive Organ", "restrike"]
    strikes = []
    for start in (0, 1.8):
        root_early = tone(percussion, start + .025, start + .20, 261.6255653)
        root_late = tone(percussion, start + .95, start + 1.35, 261.6255653)
        octave_early = tone(percussion, start + .025, start + .20, 523.2511306)
        octave_late = tone(percussion, start + .95, start + 1.35, 523.2511306)
        strikes.append({"start_seconds": start, "early_octave_relative_root_db": db(octave_early / root_early),
                        "octave_late_relative_early_db": db(octave_late / octave_early),
                        "root_late_relative_early_db": db(root_late / root_early),
                        "early_octave_magnitude": octave_early})
    repeat_delta = db(strikes[1]["early_octave_magnitude"] / strikes[0]["early_octave_magnitude"])

    lfo = audio["current", "Tremolo LFO Organ", "single"]
    periods = [rms(lfo, t, t + .01) for t in np.arange(.5, 1.45, .01)]
    period_error = sum((a - rms(lfo, t + .25, t + .26)) ** 2 for t, a in zip(np.arange(.5, 1.45, .01), periods))
    half_error = sum((a - rms(lfo, t + .125, t + .135)) ** 2 for t, a in zip(np.arange(.5, 1.45, .01), periods))
    energy = sum(a * a for a in periods)
    result = {"capture_count": len(metrics), "current_count": sum(m["build"] == "current" for m in metrics),
              "baseline_count": sum(m["build"] == "baseline" for m in metrics),
              "all_finite_and_below_full_scale": True,
              "preservation_comparisons": comparisons, "touch_comparisons": touch,
              "percussion_restrike": {"strikes": strikes, "second_vs_first_early_octave_db": repeat_delta},
              "tremolo_lfo": {"quarter_second_normalized_error": period_error / energy,
                              "half_period_error_vs_quarter_second_error": half_error / period_error},
              "limitations": ["Five programs, one single-note pitch and one eight-note voicing; not a worst-case bank headroom guarantee.",
                  "Full-mix narrowband magnitudes describe spectral evidence, not isolated physical stops or perceived realism.",
                  "No listening assessment, physical contact click, rotary speaker, room, or native host qualification."]}
    write_json("summary.json", result)
    checks = {
        "15_identical_program_records_vs_dot6": len(audit) == 15,
        "10_baseline_current_pairs_exactly_identical": len(comparisons) == 10 and all(c["exact_pcm_equal"] for c in comparisons),
        "five_velocity_pairs_identical": len(touch) == 5 and all(c["velocity_0_25_vs_0_85_exact_pcm_equal"] for c in touch),
        "neutral_flute_pressure_identical": next(c for c in touch if c["name"] == "Flute Organ 8+4")["pressure_0_vs_0_8_exact_pcm_equal"],
        "intentional_lfo_pressure_changes_held_audio": next(c for c in touch if c["name"] == "Tremolo LFO Organ")["pressure_held_contrast_db"] > .1,
        "all_captures_finite_unclipped": all(m["nonfinite"] == 0 and m["samples_over_full_scale"] == 0 for m in metrics),
        "both_percussion_strikes_decay_while_root_holds": all(s["octave_late_relative_early_db"] < -12 and s["root_late_relative_early_db"] > -3 for s in strikes),
        "four_hz_tremolo_repeats": period_error < .015 * energy and half_error > 6 * period_error,
    }
    write_json("checks.json", checks)
    provenance["existing_evidence"] = {
        "all_family_previous_review": "docs/preset-evidence/2026.11.6/reviews/a24.md",
        "current_focused_receipt": "docs/preset-evidence/2026.11.7/development/focused-ctest-receipt.json",
        "current_focused_log_sha256": sha((ROOT / "docs/preset-evidence/2026.11.7/development/cs80-focused.log").read_bytes()),
        "current_inventory_sha256": sha((ROOT / "docs/preset-evidence/2026.11.7/verification/a12/inventory.json").read_bytes())}
    write_json("provenance.json", provenance)
    print(json.dumps({"checks": checks, "capture_count": len(metrics),
                      "largest_chord_peak_dbfs": max(m["peak_dbfs"] for m in metrics if m["build"] == "current" and m["scenario"] == "chord8")}, indent=2))
    if not all(checks.values()): raise SystemExit(1)


if __name__ == "__main__":
    main()
