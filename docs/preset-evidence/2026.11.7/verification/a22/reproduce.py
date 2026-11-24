#!/usr/bin/env python3
"""A22 Polysynth structural audit and bounded actual-preset chord probe."""
import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import re
import subprocess
import tarfile
import xml.etree.ElementTree as ET

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


def write_json(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


def db(value):
    return 20 * math.log10(max(float(value), 1e-15))


def rms(audio, begin, end):
    window = audio[round(begin * RATE):round(end * RATE)].astype(np.float64)
    return float(np.sqrt(np.mean(window * window)))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--current-build", type=Path, default=Path("/tmp/cs80-build-1cee5a"))
    parser.add_argument("--baseline-build", type=Path, default=Path("/tmp/cs80-reference-1cee5a"))
    parser.add_argument("--scratch", type=Path, default=Path("/tmp/cs80-a22-1cee5a"))
    args = parser.parse_args()
    args.scratch.mkdir(parents=True, exist_ok=True)

    inventory_path = ROOT / "docs/preset-evidence/2026.11.7/verification/a12/inventory.json"
    inventory = json.loads(inventory_path.read_bytes())
    previous_bytes = git("show", BASELINE + ":docs/preset-evidence/2026.11.6/verification/a15/inventory.json")
    previous = json.loads(previous_bytes)
    ids = inventory["parameter_ids"]
    assert ids == previous["parameter_ids"] and len(ids) == 204
    family = [p for p in inventory["programs"] if p["category"] == "Polysynth"]
    assert len(family) == 25
    source_members = {}
    for source in ("src/core/curated/Polysynth.inc", "src/core/curated/Modern.inc"):
        text = git("show", CURRENT + ":" + source).decode()
        for name in re.findall(r'add\("([^"]+)", "Polysynth"', text):
            source_members[name] = source
    assert set(source_members) == {p["name"] for p in family}
    controls = ["volume", "mix", "detune", "portamento", "chorus_on", "chorus_mode", "chorus_depth",
                "sub_speed", "sub_vco", "sub_vcf", "sub_vca", "sub_at_speed", "sub_at_vco", "sub_at_vcf",
                "ring_speed", "ring_depth", "ring_amount", "fx1_type", "fx1_mix", "fx2_type", "fx2_mix",
                "kbd_brilliance_low", "kbd_brilliance_high", "oversampling"]
    for rank in (1, 2):
        controls += [f"ch{rank}_{key}" for key in ("saw", "pulse", "sine", "feet", "pw", "pwm_depth", "pwm_speed",
                     "lpf_cutoff", "lpf_res", "hpf_cutoff", "f_a", "f_d", "v_a", "v_d", "v_s", "v_r",
                     "v_level", "init_brill", "init_level", "after_brill", "after_level", "pan")]
        controls += [f"body{rank}_{key}" for key in ("model", "mix", "variant", "material")]
    audit = []
    for p in family:
        assert p == previous["programs"][p["index"]], p["name"]
        v = dict(zip(ids, p["values"]))
        assert all(v[f"body{r}_{k}"] == 0 for r in (1, 2) for k in ("model", "mix", "variant", "material"))
        audit.append({"host_index": p["host_index"], "name": p["name"], "category": p["category"],
                      "source": source_members[p["name"]], "description": p["description"], "tags": p["tags"],
                      "complete_dot6_record_equal": True,
                      "all_204_float_values_sha256": sha(np.asarray(p["values"], dtype="<f4").tobytes()),
                      "controls": {k: v[k] for k in controls}})
    distinct = len({tuple(v for k, v in zip(ids, p["values"]) if k != "volume") for p in family})
    assert distinct == 25
    write_json("inventory-audit.json", {"current_source": CURRENT, "baseline_source": BASELINE,
               "category": "Polysynth", "count": len(audit), "distinct_non_volume_vectors": distinct,
               "all_body_models_mixes_variants_materials_zero": True,
               "inventory_sha256": sha(inventory_path.read_bytes()),
               "baseline_inventory_sha256": sha(previous_bytes), "programs": audit})

    focused_path = ROOT / "docs/preset-evidence/2026.11.7/development/cs80-focused.xml"
    suite = ET.parse(focused_path).getroot()
    names = {"cs80_pad_poly_refinement_tests", "cs80_modern_preset_refinement_tests",
             "cs80_preset_precision_tests", "cs80_library_audit_tests", "cs80_preset_voicing_tests"}
    reused = [{"name": t.get("name"), "status": t.get("status"), "seconds": float(t.get("time")),
               "failed": t.find("failure") is not None} for t in suite if t.get("name") in names]
    assert len(reused) == len(names) and all(not t["failed"] and t["status"] == "run" for t in reused)
    write_json("reused-tests.json", {"source": CURRENT, "suite_tests": int(suite.get("tests")),
               "suite_failures": int(suite.get("failures")), "xml_sha256": sha(focused_path.read_bytes()),
               "reused_without_rerun": reused,
               "scope_note": "JUnit logs truncate stdout. Test implementations establish assertions and coverage; metrics below come from A22 fresh renders."})

    provenance = {"current_source": CURRENT, "baseline_source": BASELINE,
                  "sample_rate": RATE, "block_frames": 96, "hold_seconds": 6, "tail_seconds": 4,
                  "notes": [48, 55, 60, 64, 67, 72, 76, 79], "velocity": 1,
                  "pressure": "1 at note onset; four current-only controls at 0",
                  "quality": "Untouched authored Standard (oversampling=1)",
                  "probe_sha256": sha((OUT / "poly_probe.cpp").read_bytes()),
                  "reproduce_sha256": sha(Path(__file__).read_bytes()), "builds": {}}
    for label, commit, build in (("current", CURRENT, args.current_build), ("baseline", BASELINE, args.baseline_build)):
        archive = git("archive", commit, "src")
        src = args.scratch / (label + "-src")
        src.mkdir(exist_ok=True)
        # Each caller uses every header from its own archived revision: Engine's ABI grew in .7.
        with tarfile.open(fileobj=io.BytesIO(archive)) as bundle:
            bundle.extractall(src, filter="data")
        library = build / "libcs80_core.a"
        library_hash = sha(library.read_bytes())
        executable = args.scratch / ("probe-" + label)
        command = ["c++", "-O2", "-std=c++20", "-I" + str(src / "src"), str(OUT / "poly_probe.cpp"),
                   str(library), "-o", str(executable)]
        subprocess.run(command, check=True, cwd=ROOT)
        with (OUT / (label + "-renders.csv")).open("w") as log:
            subprocess.run([str(executable), str(args.scratch / label), label], stdout=log, check=True, cwd=ROOT)
        assert library_hash == sha(library.read_bytes())
        provenance["builds"][label] = {"source": commit, "src_archive_sha256": sha(archive),
                  "library": str(library), "library_sha256_before_and_after": library_hash,
                  "probe_executable_sha256": sha(executable.read_bytes()), "compile_command": command}
        print(label, "captures complete", flush=True)
    write_json("provenance.json", provenance)

    metrics, manifest, audio = [], [], {}
    for label in ("current", "baseline"):
        for row in csv.DictReader((OUT / (label + "-renders.csv")).open()):
            name, scenario = row["name"], row["scenario"]
            path = args.scratch / label / (name.replace(" ", "_") + "-" + scenario + ".f32")
            raw = path.read_bytes()
            samples = np.frombuffer(raw, dtype="<f4").reshape(-1, 2)
            assert len(samples) == int(row["frames"])
            audio[label, name, scenario] = samples
            onset, late = rms(samples, .02, .2), rms(samples, 5.5, 6)
            m = {"build": label, "name": name, "scenario": scenario,
                 "peak_dbfs": db(np.max(np.abs(samples))),
                 "held_peak_dbfs": db(np.max(np.abs(samples[:6 * RATE]))),
                 "release_peak_dbfs": db(np.max(np.abs(samples[6 * RATE:]))),
                 "onset_rms_dbfs": db(onset), "held_5_5_to_6s_rms_dbfs": db(late),
                 "held_decay_1_8_to_2_3s_vs_onset_db": db(rms(samples, 1.8, 2.3) / max(onset, 1e-15)),
                 "release_100_to_250ms_rms_dbfs": db(rms(samples, 6.1, 6.25)),
                 "release_100_to_250ms_vs_late_hold_db": db(rms(samples, 6.1, 6.25) / max(late, 1e-15)),
                 "release_1_to_1_5s_rms_dbfs": db(rms(samples, 7, 7.5)),
                 "final_3_5_to_4s_rms_dbfs": db(rms(samples, 9.5, 10)),
                 "nonfinite_samples": int(np.count_nonzero(~np.isfinite(samples))),
                 "samples_at_or_above_full_scale": int(np.count_nonzero(np.abs(samples) >= 1))}
            metrics.append(m)
            manifest.append({"build": label, "name": name, "scenario": scenario, "bytes": len(raw), "sha256": sha(raw)})
    with (OUT / "metrics.csv").open("w") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(metrics[0]))
        writer.writeheader()
        writer.writerows(metrics)
    write_json("pcm-manifest.json", manifest)

    paired, pressure = [], []
    for (label, name, scenario), x in audio.items():
        if label == "baseline":
            y = audio["current", name, scenario]
            paired.append({"name": name, "scenario": scenario, "exact_pcm_equal": bool(np.array_equal(x, y)),
                           "max_absolute_sample_difference": float(np.max(np.abs(x - y)))})
        if label == "current" and scenario == "unpressed8":
            y = audio["current", name, "pressed8"]
            pressure.append({"name": name, "pressure0_to_1_exact_pcm_equal": bool(np.array_equal(x, y)),
                             "held_2_to_5s_pressure_gain_db": db(rms(y, 2, 5) / max(rms(x, 2, 5), 1e-15))})
    current = [m for m in metrics if m["build"] == "current"]
    checks = {
        "25_complete_records_equal_to_dot6": all(p["complete_dot6_record_equal"] for p in audit),
        "25_distinct_non_volume_vectors": distinct == 25,
        "all_18_captures_finite_and_below_full_scale": len(metrics) == 18 and all(
            m["nonfinite_samples"] == 0 and m["samples_at_or_above_full_scale"] == 0 for m in metrics),
        "seven_dot6_dot7_chords_exact_pcm_equal": len(paired) == 7 and all(p["exact_pcm_equal"] for p in paired),
        "all_final_tail_windows_below_minus70_dbfs": all(m["final_3_5_to_4s_rms_dbfs"] < -70 for m in metrics),
        "brass_and_pluck_continue_decaying_while_held": all(m["held_decay_1_8_to_2_3s_vs_onset_db"] < -30
            for m in current if m["name"] in ("Poly Brass Stab", "Poly Pluck")),
        "routed_pressure_increases_held_chord_energy": all(p["held_2_to_5s_pressure_gain_db"] > .1
            for p in pressure if p["name"] != "Poly Brass Stab"),
        "unrouted_brass_pressure_is_exact_noop": next(p for p in pressure if p["name"] == "Poly Brass Stab")["pressure0_to_1_exact_pcm_equal"],
    }
    write_json("checks.json", {"passed": all(checks.values()), "checks": checks})
    summary = {"passed": all(checks.values()), "all_family_structural_programs": 25,
               "fresh_rendered_programs": 7, "current_captures": 11, "baseline_captures": 7,
               "rendered_audio_seconds": 180, "largest_peak_dbfs": max(m["peak_dbfs"] for m in metrics),
               "loudest_final_tail_window_dbfs": max(m["final_3_5_to_4s_rms_dbfs"] for m in metrics),
               "paired_revisions": paired, "pressure_controls": pressure,
               "scope": "Local complete-engine numerical measurements; no listening, acoustic-replica, native-host or archive qualification."}
    write_json("summary.json", summary)
    print(json.dumps(summary, indent=2))
    if not summary["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
