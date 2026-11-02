#!/usr/bin/env python3
"""Preregistered negative controls for the additive numerical release gate.

These are deliberately written before the gate implementation. No numerical
threshold is relaxed and no production source is touched.
"""
from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = json.loads((ROOT / ".github/release-numerical-contract.json").read_text())
MODULE = ROOT / "scripts/release_numerical_gate.py"
SPEC = importlib.util.spec_from_file_location("cs80_release_numerical_gate", MODULE)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("release numerical gate module unavailable")
gate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(gate)

BEFORE = "a" * 40
REVIEWED = "b" * 40
TREE = "c" * 40
AUDIO = "d" * 40
REGISTRATION = "e" * 40
RATES = [44100, 48000, 96000]


def raw_suites():
    phrase = []
    for policy in ("source", "legacy"):
        for rate, programs in ((44100, 222), (48000, 14), (96000, 14)):
            for i in range(programs + 11):
                phrase.append({"policy": policy, "rate": str(rate), "group": "tab" if i >= programs else "program:" + str(i),
                               "name": "Tone" if i >= programs else "Program",
                               "peak_db": "-1", "click_retrigger": "0.5", "click_steal": "0.5",
                               "click_off": "0.5", "click_chord_off": "0.5", "nonfinite": "0"})
    alias = []
    for policy in ("source", "legacy"):
        for rate in RATES:
            for ring in (0, 1):
                for wave in ("saw", "pulse"):
                    for _ in range(7):
                        alias.append({"policy": policy, "rate": str(rate), "ring": str(ring),
                                      "wave": wave, "nonharmonic_db_vs_partials": "-80"})
    fx = [{"policy": policy, "rate": str(rate), "nonfinite": "0", "peak_db": "-5"}
          for policy in ("source", "legacy") for rate in RATES for _ in range(17)]
    latency = [{"policy": ("source" if i % 2 else "legacy"), "rate": str(RATES[i % 3]),
                "delay_host_samples": "63", "reported_latency": "67"} for i in range(40)]
    causal = [{"coordinate": coordinate, "name": "Bass Stack" if coordinate != "tone_tabs" else "Funky 1 + Funky 2",
               "event": "single_note_off" if coordinate == "tone_tabs" else "note_on",
               "rate": str(rate), "baseline_score": "0.5"}
              for coordinate in ("original_physical_panel", "electrical_programmer", "tone_tabs")
              for rate in RATES]
    return {"phrase.csv": phrase, "alias.csv": alias, "fx.csv": fx,
            "latency.csv": latency, "click-causality.csv": causal}


def target_comparisons():
    values = []
    for target in CONTRACT["audit"]["existing_targets"]:
        maximum = target["maximum"]
        values.append({
            "scope": target["scope"], "metric": target["metric"], "maximum": maximum,
            "row_count": target["rows"], "exceeding_row_count": 0,
            "all_exported_values_within_target": True,
            "extrema": {"max": (-80 if maximum < 0 else -1 if maximum == 0 else 0.5)},
        })
    return values


def cpu_round(source, marker, held=0.18, idle=0.01):
    rows = []
    for i in range(11):
        rows.append({"rate": "48000", "block": "256", "mode": "isolation", "isolation": "1",
                     "scenario": "paired_tones", "phase": "warmed_held", "initial_selector": str(i),
                     "rank_i_name": "I" + str(i), "rank_ii_name": "II" + str(i),
                     "callbacks": "1024", "median_rtf": str(held), "p99_rtf": "0.19",
                     "budget_exceedances": "0", "nonfinite_count": "0", "checksum": "0.123",
                     "peak": "0.25", "rms": "0.1"})
    rows.append({"rate": "48000", "block": "256", "mode": "isolation", "isolation": "1",
                 "scenario": "idle_quiet", "phase": "warmed_held", "initial_selector": "0",
                 "rank_i_name": "", "rank_ii_name": "", "callbacks": "1024",
                 "median_rtf": str(idle), "p99_rtf": "0.013", "budget_exceedances": "0",
                 "nonfinite_count": "0", "checksum": "0", "peak": "0", "rms": "0"})
    for i in range(39):
        rows.append({"rate": "48000", "block": "256", "mode": "isolation", "isolation": "1",
                     "scenario": "other-" + str(i), "phase": "release_tail",
                     "callbacks": "128", "median_rtf": "0.01", "p99_rtf": "0.02",
                     "budget_exceedances": "0", "nonfinite_count": "0", "checksum": "0",
                     "peak": "0", "rms": "0"})
    return {
        "metadata": {"schema_version": 2, "fixture_id": "source-core-timing-v2",
                     "suite": "baseline", "source_commit_caller_verified": source,
                     "compiler": "13.3.0", "source_fingerprint": "fixed-driver-v2",
                     "quick_subset": False, "callbacks_per_held_phase": 1024},
        "completed": {"completed": True, "cases": 27, "rows": 51},
        "receipt": {"host_id": "same-runner", "runner_job_id": "single-job",
                    "compiler_id": "gcc-13.3", "flags_sha256": "f" * 64,
                    "benchmark_source_sha256": "9" * 64,
                    "runner_environment_sha256": "8" * 64,
                    "capture_id": marker},
        "rows": rows,
    }


def fixture():
    suites = raw_suites()
    raw_counts = {name: {"rows": len(rows), "bytes": 100, "sha256": "1" * 64}
                  for name, rows in suites.items()}
    summary = {"schema_version": 1, "source_snapshot_sha": REVIEWED,
               "production_change_sha": AUDIO, "raw_files":
               {name: {"row_count": info["rows"], "bytes": info["bytes"],
                       "sha256": info["sha256"]} for name, info in raw_counts.items()},
               "existing_target_comparisons": target_comparisons(),
               "nonfinite": {"phrase_stereo_frames": 0, "fx_stereo_frames": 0,
                             "alias_nonfinite_render_rows": [], "numeric_csv_cells": []},
               "scope": {"rates_hz": RATES, "cpu_suite_included": False,
                         "pure_rendered": False, "curated_library_rendered": False}}
    provenance = {"schema_version": 1, "complete_capture": True, "quick": False,
                  "head_sha": REVIEWED, "src_tree_sha": TREE,
                  "production_audio_commit": AUDIO, "registration_commit": REGISTRATION,
                  "registered_command": CONTRACT["audit"]["registered_command"],
                  "raw_files": raw_counts, "cpu_suite_included": False,
                  "nonfinite_numeric_cells_reported_not_hidden": 0}
    return {"source": {"reviewed_sha": REVIEWED, "source_tree_sha": TREE,
                       "audio_sha": AUDIO, "registration_sha": REGISTRATION,
                       "before_sha": BEFORE, "before_is_ancestor": True},
            "audit": {"summary": summary, "provenance": provenance, "suites": suites},
            "cpu": {"before": [cpu_round(BEFORE, "before-" + str(i), held=0.6, idle=0.07)
                                for i in range(3)],
                    "after": [cpu_round(REVIEWED, "after-" + str(i))
                               for i in range(3)]}}


class NumericalReleaseGateTests(unittest.TestCase):
    def test_synthetic_positive(self):
        result = gate.validate_evidence(CONTRACT, fixture())
        self.assertTrue(result["pass"])
        self.assertEqual(len(result["selected_held"]), 11)
        self.assertLessEqual(result["selected_idle"]["median_rtf"], 0.02)

    def test_original_gate_falsification_and_missing_evidence_rejected(self):
        edits = [
            ("missing original click target", lambda f: f["audit"]["summary"]["existing_target_comparisons"].pop(3)),
            ("weakened click ceiling", lambda f: f["audit"]["summary"]["existing_target_comparisons"][1].update(maximum=2)),
            ("false green reporter", lambda f: f["audit"]["suites"]["phrase.csv"][0].update(click_retrigger="1.2")),
            ("nonfinite phrase frame", lambda f: f["audit"]["suites"]["phrase.csv"][0].update(nonfinite="1")),
            ("nonfinite numeric field", lambda f: f["audit"]["suites"]["phrase.csv"][0].update(peak_db="nan")),
            ("missing audit row", lambda f: f["audit"]["suites"]["phrase.csv"].pop()),
            ("missing audit suite", lambda f: f["audit"]["suites"].pop("click-causality.csv")),
            ("stale audit source", lambda f: f["audit"]["provenance"].update(head_sha=BEFORE)),
            ("stale audit tree", lambda f: f["audit"]["provenance"].update(src_tree_sha=BEFORE)),
            ("not complete", lambda f: f["audit"]["provenance"].update(complete_capture=False)),
            ("quick capture", lambda f: f["audit"]["provenance"].update(quick=True)),
            ("missing before round", lambda f: f["cpu"]["before"].pop()),
            ("missing after round", lambda f: f["cpu"]["after"].pop()),
            ("wrong before identity", lambda f: f["cpu"]["before"][0]["metadata"].update(source_commit_caller_verified=REVIEWED)),
            ("wrong after identity", lambda f: f["cpu"]["after"][0]["metadata"].update(source_commit_caller_verified=BEFORE)),
            ("mixed hosts", lambda f: f["cpu"]["after"][1]["receipt"].update(host_id="other-runner")),
            ("compiler changed", lambda f: f["cpu"]["after"][2]["receipt"].update(compiler_id="clang")),
            ("not full workload", lambda f: f["cpu"]["after"][0]["metadata"].update(callbacks_per_held_phase=128)),
            ("missing held fixture", lambda f: f["cpu"]["after"][0]["rows"].pop(0)),
            ("held RTF above target", lambda f: [r.update(median_rtf="0.21")
              for rd in f["cpu"]["after"] for r in rd["rows"] if r.get("scenario") == "paired_tones"]),
            ("idle RTF above target", lambda f: [r.update(median_rtf="0.021")
              for rd in f["cpu"]["after"] for r in rd["rows"] if r.get("scenario") == "idle_quiet"]),
            ("CPU NaN", lambda f: f["cpu"]["after"][0]["rows"][0].update(p99_rtf="nan")),
            ("CPU nonfinite count", lambda f: f["cpu"]["after"][0]["rows"][0].update(nonfinite_count="1")),
            ("checksum differs across rounds", lambda f: f["cpu"]["after"][1]["rows"][0].update(checksum="0.987")),
            ("missing source ancestry", lambda f: f["source"].update(before_is_ancestor=False)),
        ]
        for name, edit in edits:
            with self.subTest(name=name):
                f = fixture()
                edit(f)
                with self.assertRaises((gate.GateError, ValueError, KeyError)):
                    gate.validate_evidence(CONTRACT, f)


if __name__ == "__main__":
    unittest.main()
