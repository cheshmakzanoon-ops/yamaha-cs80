#!/usr/bin/env python3
"""A34 timing census and five supplemental complete-program captures.

Only evidence in this directory and the explicit scratch directory is written.
No parameter changes, calibration, threshold changes, or all-bank rerender.
"""
import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import subprocess
import tarfile
import xml.etree.ElementTree as ET

import numpy as np

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
SOURCE = "c5549f7d095a5c5f49a5ad22ec0ccc819245d201"
BASELINE = "18711f93a39aa49f3d6f9a41784be456224facbe"


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def db(value):
    return 20 * math.log10(max(float(value), 1e-15))


def segment(audio, start, end, rate=48000):
    return audio[round(start * rate):round(end * rate)]


def rms(audio):
    return float(np.sqrt(np.mean(np.square(audio.astype(np.float64)))))


def spectral_centroid(audio, rate=48000):
    size = 8192
    hann = np.hanning(size)[:, None]
    power = np.zeros(size // 2 + 1)
    for at in range(0, len(audio) - size + 1, size // 2):
        f = np.fft.rfft(audio[at:at+size].astype(np.float64) * hann, axis=0)
        power += np.sum(np.abs(f) ** 2, axis=1)
    frequencies = np.fft.rfftfreq(size, 1 / rate)
    return float((frequencies[1:] * power[1:]).sum() / max(power[1:].sum(), 1e-30))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tmp", type=Path, default=Path("/tmp/cs80-a34-1cee5a"))
    parser.add_argument("--build", type=Path, default=Path("/tmp/cs80-build-1cee5a"))
    parser.add_argument("--render", action="store_true", help="Render the five selected cases; otherwise reanalyze retained temporary PCM")
    args = parser.parse_args()
    args.tmp.mkdir(parents=True, exist_ok=True)
    archive = git("archive", SOURCE, "src")
    with tarfile.open(fileobj=io.BytesIO(archive)) as bundle:
        bundle.extractall(args.tmp / "source", filter="data")
    core = args.build / "libcs80_core.a"
    core_hash = sha(core.read_bytes())
    commands = []
    # Timing census is cheap and uses the exact archived header implementations.
    for name in ("timing_probe", "long_probe"):
        command = ["c++", "-O2", "-std=c++20", "-I" + str(args.tmp / "source/src"),
                   str(OUT / (name + ".cpp")), str(core), "-o", str(args.tmp / name)]
        subprocess.run(command, check=True)
        commands.append(command)
    with (OUT / "all-rank-timing.csv").open("w") as output:
        subprocess.run([str(args.tmp / "timing_probe")], stdout=output, check=True)
    if args.render:
        with (OUT / "renders.csv").open("w") as output:
            subprocess.run([str(args.tmp / "long_probe"), str(args.tmp / "pcm")], stdout=output, check=True)
    assert sha(core.read_bytes()) == core_hash
    write("compile-provenance.json", {"source": SOURCE, "source_archive_sha256": sha(archive),
          "core_path": str(core), "core_sha256_before_and_after": core_hash, "commands": commands,
          "executables": {name: sha((args.tmp / name).read_bytes()) for name in ("timing_probe", "long_probe")}})

    inventory_path = ROOT / "docs/preset-evidence/2026.11.7/verification/a12/inventory.json"
    current = json.loads(inventory_path.read_bytes())
    previous = json.loads(git("show", BASELINE + ":docs/preset-evidence/2026.11.6/verification/a15/inventory.json"))
    assert current["programs"] == previous["programs"] and current["parameter_ids"] == previous["parameter_ids"]
    assert len(current["programs"]) == 523
    ids = current["parameter_ids"]
    for program in current["programs"]:
        values = dict(zip(ids, program["values"]))
        assert all(values[f"macro{x}"] == .5 for x in (1,2,3,4))
        assert values["velocity_curve"] == values["mod_velocity_curve"] == 0
    ranks = list(csv.DictReader((OUT / "all-rank-timing.csv").open()))
    assert len(ranks) == 1046
    by_program = {}
    for row in ranks:
        # Program names in this bank do not contain commas/quotes; C++ CSV names are quoted.
        for key, value in list(row.items()):
            if key not in ("name", "category"):
                row[key] = float(value) if key not in ("index", "rank", "capture_active", "sounding_path", "filter_span", "body_model") else int(value)
        by_program.setdefault(row["name"], []).append(row)
    selected = []
    for name, rows in by_program.items():
        active = [r for r in rows if r["sounding_path"]]
        authored = any(r["authored_v_a"] > 3 or (r["filter_span"] and r["authored_f_a"] > 3) for r in active)
        mapped = any(r["amp_max_v08"] > 3 or (r["filter_span"] and r["filter_max"] > 3) for r in active)
        cold = any(r["amp_card0_v08"] > 3 or (r["filter_span"] and r["filter_card0"] > 3) for r in active)
        if mapped:
            program = current["programs"][rows[0]["index"]]
            values = dict(zip(ids, program["values"]))
            assert values["arp_on"] == values["kas_scan_timing"] == values["voice_mode"] == 0
            selected.append({"index": rows[0]["index"], "name": name, "category": rows[0]["category"],
                             "authored_attack_over_3s": authored, "cold_card0_attack_over_3s_at_velocity_08": cold,
                             "cli_default_hold": rows[0]["cli_default_hold"], "description": program["description"], "ranks": active})
    assert len(selected) == 14 and sum(p["authored_attack_over_3s"] for p in selected) == 12
    default_long = [name for name, rows in by_program.items() if rows[0]["cli_default_hold"] > 3]
    assert len(default_long) == 50
    worst_margin = min(r["cli_default_hold"] - max(r["amp_max_v0"], r["filter_max"] if r["filter_span"] else 0)
                       for r in ranks if r["sounding_path"])
    assert worst_margin > 0
    write("timing-census.json", {"source": SOURCE, "programs": 523, "rank_rows": 1046,
          "complete_records_equal_to_dot6": True, "velocity_for_main_mapping": .8,
          "mapped_voice_cards": 8, "neutral_macros_all_programs": True,
          "authored_over_3s_program_count": 12, "any_card_mapped_over_3s_program_count": 14,
          "cold_card0_over_3s_program_count": sum(p["cold_card0_attack_over_3s_at_velocity_08"] for p in selected),
          "default_cli_longer_than_3s_count": 50, "default_cli_longer_than_3s_names": default_long,
          "minimum_cli_hold_minus_maximum_mapped_attack_over_all_cards_and_velocity_range_s": worst_margin,
          "mapping": "Amplitude=max(.001,savedA*bodyArticulation.attack*2^(-1.1*(2*macro2-1))*card.ampEnvTime); filter=max(.001,savedA*macroScale*card.filterEnvTime). With neutral macros all macroScale=1. Card factors use drawCard(voice*2+rank), scale=voiceDispersion/.35. Filter IL=AL=0 has no entrance span. Cold note selects card0. Mapped times are from accepted note-on, with sample quantization and output latency separate.",
          "interpretation": "An attack stage can exceed three seconds while the complete patch is already audible. This census is about completing entrances, not a silence gate or a quality score.",
          "programs_beyond_3s": selected})

    suite_path = ROOT / "docs/preset-evidence/2026.11.7/development/cs80-focused.xml"
    suite = ET.parse(suite_path).getroot()
    names = {"cs80_library_audit_tests", "cs80_pad_poly_refinement_tests", "cs80_preset_voicing_tests",
             "cs80_fx_refinement_tests", "cs80_modern_preset_refinement_tests"}
    reused = [{"name": t.get("name"), "status": t.get("status"), "seconds": float(t.get("time")),
               "failed": t.find("failure") is not None} for t in suite if t.get("name") in names]
    assert len(reused) == len(names) and all(t["status"] == "run" and not t["failed"] for t in reused)
    write("reused-tests.json", {"source": SOURCE, "xml_sha256": sha(suite_path.read_bytes()), "suite_passed": 31,
          "tests_reused_without_rerun": reused,
          "library_audit_scope": "44100 Hz: category-dependent phrase at velocity .9, pressure ramp .3-.6 s, release at first 256-frame boundary >=1.2 s, total ~1.9 s; separate eight-note 36/43/48/55/60/64/67/72 velocity1 pressure1 stress through first boundary >=1 s.",
          "pad_poly_long_headroom_scope": "Two complete presets Cathedral Pad and Golden Pads 66, 48000 Hz, twelve-second eight-note 60/62/64/65/67/69/71/72 chord, velocity1 pressure1, unchanged -1.5 dBFS ceiling; current suite passes. This is a different voicing from A34's supplemental low chord.",
          "voicing_long_swells_scope": "Slow Pad3s, Slow Strings2.6s, Bowed Swell Strings3.2s, Brass Pad Swell3.2s, Sweep Pad4.8s; onset/bloom/held windows, no pressure manufacture.",
          "modern_long_motion_scope": "LFO Sweep Pad and selected other modern programs: ten-second moving-versus-frozen modulation controls.",
          "stdout_limit": "CTest JUnit truncates some individual outputs; passed execution plus frozen source establishes the stated coverage, not invented per-test measured numbers.",
          "other_current_supplements": {"a21": "Sweep Pad6s single+8note; Choir Pad/Dark Choir full-pressure stacks and tails",
              "a22": "Seven Polysynth programs6s full-pressure eight-note chords+4s tails; four pressure0 controls",
              "a25": "Space Sweep, Sweep Riser, Distant FX139 and four other FX10s hold+16s tail; body/touch variants"}})

    measurements = []
    manifest = []
    windows = []
    for scene in csv.DictReader((OUT / "renders.csv").open()):
        path = args.tmp / "pcm" / scene["file"]
        raw = path.read_bytes()
        audio = np.frombuffer(raw, dtype="<f4").reshape(-1, 2)
        held, tail = int(scene["hold"]), int(scene["tail"])
        assert len(audio) == int(scene["frames"])
        nonfinite = int(np.count_nonzero(~np.isfinite(audio)))
        fullscale = int(np.count_nonzero(np.abs(audio) >= 1))
        assert nonfinite == fullscale == 0
        blocks = segment(audio, 0, held).reshape(-1, 2400, 2).astype(np.float64)
        levels = np.sqrt(np.mean(blocks * blocks, axis=(1, 2)))
        peak_window = int(np.argmax(levels))
        peak_frame = int(np.argmax(np.max(np.abs(audio), axis=1)))
        onset = rms(segment(audio,.025,.125))
        measured = {"name": scene["name"], "scenario": scene["scenario"], "hold_seconds": held, "tail_seconds": tail,
            "peak_dbfs": db(np.max(np.abs(audio))), "peak_time_seconds": peak_frame / 48000,
            "first_3s_held_peak_dbfs": db(np.max(np.abs(segment(audio,0,3)))),
            "after_3s_held_peak_dbfs": db(np.max(np.abs(segment(audio,3,held)))),
            "onset_025_to_125ms_rms_dbfs": db(onset), "loudest_50ms_rms_dbfs": db(levels[peak_window]),
            "loudest_50ms_window_start_seconds": peak_window * .05,
            "loudest_50ms_vs_onset_db": db(levels[peak_window] / max(onset, 1e-15)),
            "held_2_5_to_3s_rms_dbfs": db(rms(segment(audio,2.5,3))),
            "held_final_halfsecond_rms_dbfs": db(rms(segment(audio,held-.5,held))),
            "tail_final_halfsecond_rms_dbfs": db(rms(segment(audio,held+tail-.5,held+tail))),
            "centroid_1_to_2s_hz": spectral_centroid(segment(audio,1,2)),
            "centroid_4_to_5s_hz": spectral_centroid(segment(audio,4,5)),
            "active_voices_at_end": int(scene["active_voices_at_end"]),
            "nonfinite_samples": nonfinite, "samples_at_or_above_full_scale": fullscale}
        measurements.append(measured)
        manifest.append({"name": scene["name"], "file": str(path), "bytes": len(raw), "sha256": sha(raw)})
        for second in range(held + tail):
            a = segment(audio,second,second+1)
            windows.append({"name": scene["name"], "start_seconds": second, "held": second < held,
                            "rms_dbfs": db(rms(a)), "peak_dbfs": db(np.max(np.abs(a)))})
    assert len(measurements) == 5
    matched_path = ROOT / "docs/preset-evidence/2026.11.7/development/current-bank-metrics.csv"
    matched = list(csv.DictReader(matched_path.open()))
    assert len(matched) == 523
    category_notes = {}
    for row in matched:
        expected = 48 if row["category"] == "Bass" else 67 if row["category"] in ("Winds","Leads") else 72 if row["category"] == "Bells" else 60
        assert int(row["note"]) == expected and int(row["sample_rate"]) == 48000
        assert abs(float(row["velocity"]) - .8) < 1e-6 and float(row["pressure"]) == 0
        assert float(row["hold_seconds"]) == 3 and float(row["tail_seconds"]) == 1
        assert int(row["nonfinite"]) == int(row["samples_over_full_scale"]) == 0
        category_notes[row["category"]] = expected
    matched_by_name = {row["name"]: row for row in matched}
    controls = []
    for row in measurements:
        if row["scenario"] == "single-midi60":
            old = matched_by_name[row["name"]]
            peak_delta = row["first_3s_held_peak_dbfs"] - float(old["peak_dbfs"])
            body_delta = row["held_2_5_to_3s_rms_dbfs"] - float(old["body_rms_dbfs"])
            assert abs(peak_delta) < 1e-7 and abs(body_delta) < 1e-7
            controls.append({"name":row["name"],"first3s_peak_delta_from_root_rounded_csv_db":peak_delta,
                             "2_5_to_3s_rms_delta_from_root_rounded_csv_db":body_delta})
    write("matched-bank-scope.json", {"source":SOURCE,"rows":523,"rate":48000,"velocity":.8,"pressure":0,
          "hold_seconds":3,"tail_seconds":1,"category_notes":category_notes,
          "nonfinite_samples":0,"samples_over_full_scale":0,"metrics_sha256":sha(matched_path.read_bytes()),
          "three_single_note_prefix_controls":controls,
          "meaning":"For these three cases, first3s peak and2.5-3s RMS agree with the rounded root CSV within1e-7dB. Root whole-capture peak includes release; this agreement does not claim retained PCM identity or longer-hold coverage."})
    for name, rows in (("metrics.csv", measurements), ("one-second-windows.csv", windows)):
        with (OUT / name).open("w") as file:
            writer = csv.DictWriter(file, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    write("pcm-manifest.json", manifest)
    write("summary.json", {"source": SOURCE, "scope": "Five current-source complete unmodified programs; no normalization, ranks/effects/levels unchanged; numerical evidence only",
        "rate": 48000, "quality": "authored Standard,2x internal rate,nonRealtime=false", "block_frames": 96,
        "single_notes": {"programs": ["Slow Brass 10","Evolving Pad","Golden FX 156"], "midi_note": 60,"velocity":.8,"pressure":0},
        "stacks": {"programs": ["Cathedral Pad","Golden Pads 66"], "notes":[36,43,48,55,60,64,67,72],"velocity":1,"pressure":1},
        "captured_seconds": sum(m["hold_seconds"]+m["tail_seconds"] for m in measurements),
        "nonfinite_samples": sum(m["nonfinite_samples"] for m in measurements),
        "samples_at_or_above_full_scale": sum(m["samples_at_or_above_full_scale"] for m in measurements),
        "maximum_peak_dbfs": max(m["peak_dbfs"] for m in measurements),
        "measurements": measurements,
        "metric_scope": "RMS averages both channels' squared values, not a mono sum; spectra Welch/Hann8192,50% overlap, summed channel power. 50ms windows nonoverlap. Peaks use sample absolute maximum. No new level gate introduced; finite/below-full-scale retains existing scope.",
        "limits": "Five scenes are not a new whole-bank long-hold qualification. Reused suites plus timing census cover separate questions. Tail endpoints can retain expected long-envelope voice state; final residual is reported, not assumed silence."})

    source_files = ["src/core/Engine.cpp","src/core/Engine.h","src/core/Dsp.h","src/core/BodyArticulation.h",
        "src/core/ToleranceModel.h","src/core/AdsrRcReconstruction.inc","src/core/FilterEnvRcReconstruction.inc",
        "src/core/FactoryPresets.cpp","src/core/FactoryPresets.h","src/core/CuratedLibrary.inc","src/core/OriginalCircuitLibrary.inc",
        "src/core/CuratedLibraryLevels.inc","src/core/OriginalCircuitLevels.inc","src/core/curated/Pads.inc",
        "src/core/curated/Fx.inc","src/core/curated/Brass.inc","src/tools/preset_capture.cpp",
        "tests/LibraryAuditTests.cpp","tests/PadPolyRefinementTests.cpp","tests/PresetVoicingTests.cpp"]
    receipts = {path: {"frozen_sha256":sha(git("show",SOURCE+":"+path)),
                      "checkout_matches_frozen": (ROOT/path).read_bytes()==git("show",SOURCE+":"+path)} for path in source_files}
    assert all(row["checkout_matches_frozen"] for path,row in receipts.items() if path.startswith("src/"))
    input_paths = [inventory_path, suite_path, ROOT/"docs/preset-evidence/2026.11.7/development/current-bank-metrics.csv",
        ROOT/"docs/preset-evidence/2026.11.7/development/matched-published-current-bank.json",
        ROOT/"docs/preset-evidence/2026.11.7/verification/a21/rendered-measurements.json",
        ROOT/"docs/preset-evidence/2026.11.7/verification/a22/summary.json",
        ROOT/"docs/preset-evidence/2026.11.7/verification/a25/review.json"]
    write("binding.json", {"audio_source":SOURCE,"source_files":receipts,
        "input_files_sha256":{str(p.relative_to(ROOT)):sha(p.read_bytes()) for p in input_paths},
        "evidence_files_sha256":{p.name:sha(p.read_bytes()) for p in sorted(OUT.iterdir()) if p.is_file() and p.name!="binding.json"},
        "library_sha256_after":sha(core.read_bytes())})
    print(json.dumps({"programs_over_3s_authored":12,"programs_over_3s_mapped_any_card":14,
        "programs_over_3s_cold_card0":sum(p["cold_card0_attack_over_3s_at_velocity_08"] for p in selected),
        "cli_default_extends":50,"captures":len(measurements),"seconds":sum(m["hold_seconds"]+m["tail_seconds"] for m in measurements),
        "worst_peak_dbfs":max(m["peak_dbfs"] for m in measurements)},indent=2))


if __name__ == "__main__":
    main()
