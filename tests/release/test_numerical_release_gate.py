#!/usr/bin/env python3
"""Preregistered negative controls for the additive numerical release gate.

These are deliberately written before the gate implementation. No numerical
threshold is relaxed and no production source is touched.
"""
from __future__ import annotations

import csv
import hashlib
import importlib.util
import json
import os
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
        for rate in RATES:
            program_indices = range(222) if rate == 44100 else range(0, 222, 16)
            for index in program_indices:
                phrase.append({"policy": policy, "rate": str(rate), "group": "program:" + str(index),
                               "name": f"Program-{index:03d}",
                               "peak_db": "-1", "click_retrigger": "0.5", "click_steal": "0.5",
                               "click_off": "0.5", "click_chord_off": "0.5", "nonfinite": "0"})
            for index in range(11):
                phrase.append({"policy": policy, "rate": str(rate), "group": "tab",
                               "name": f"TonePair-{index:02d}",
                               "peak_db": "-1", "click_retrigger": "0.5", "click_steal": "0.5",
                               "click_off": "0.5", "click_chord_off": "0.5", "nonfinite": "0"})
    alias = []
    for policy in ("source", "legacy"):
        for rate in RATES:
            for ring in (0, 1):
                for wave in ("saw", "pulse"):
                    for note, feet in ((72, 1), (72, 4), (84, 1), (84, 4),
                                       (96, 1), (96, 4), (108, 1)):
                        alias.append({"policy": policy, "rate": str(rate), "ring": str(ring),
                                      "wave": wave, "note": str(note), "feet_ratio": str(feet),
                                      "nonharmonic_db_vs_partials": "-80"})
    fx_cases = [("000", "0")] + [(str(i) + "00", mix) for i in range(1, 6)
                                  for mix in ("0.5", "1")] + [
                                      ("333", "1"), ("444", "1"), ("222", "1"),
                                      ("555", "1"), ("123", "1"), ("435", "1")]
    fx = [{"policy": policy, "rate": str(rate), "name": "bright saw", "group": "fxbase",
           "fx_slots": slots, "fx_mix": mix, "nonfinite": "0", "peak_db": "-5"}
          for policy in ("source", "legacy") for rate in RATES for slots, mix in fx_cases]
    latency = [{"policy": policy, "rate": str(rate), "oversampling_factor": "2",
                "freq_hz": str(freq), "delay_host_samples": "63", "reported_latency": "67"}
               for policy in ("source", "legacy") for rate in RATES
               for freq in (250, 1000, 5000, 10000, 15000, 18000, 20000)
               if freq <= 0.45 * rate]
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
                     "quick_subset": False, "callbacks_per_held_phase": 1024,
                     "clock_is_steady": True, "order_seed": 803010},
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
               "production_change_sha": AUDIO,
               "summarizer_sha256": CONTRACT["audit"]["frozen_reporter_sha256"], "raw_files":
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
            "audit": {"summary": summary, "provenance": provenance, "suites": suites,
                      "observed_raw_files": raw_counts},
            "cpu": {"before": [cpu_round(BEFORE, "before-" + str(i), held=0.6, idle=0.07)
                                for i in range(3)],
                    "after": [cpu_round(REVIEWED, "after-" + str(i))
                               for i in range(3)]}}


def honestly_failed_original_scope(f):
    # This models the original complete 885-row capture: the summary honestly
    # reports a failed click target, so a green capture job must still be red.
    f["audit"]["suites"]["phrase.csv"][0]["click_retrigger"] = "1.23689"
    scope = f["audit"]["summary"]["existing_target_comparisons"][1]
    scope["exceeding_row_count"] = 1
    scope["all_exported_values_within_target"] = False
    scope["extrema"]["max"] = 1.23689


class NumericalReleaseGateTests(unittest.TestCase):
    def test_same_row_count_forged_fixture_identity_must_be_rejected(self):
        # All these were false PASS cases before adding exact fixture coverage.
        # Each retains 885 rows, finite values and green numerical thresholds.
        def mutate_bank(f):
            phrase = f["audit"]["suites"]["phrase.csv"]
            phrase[1]["name"] = phrase[0]["name"]

        def mutate_secondary_bank(f):
            phrase = f["audit"]["suites"]["phrase.csv"]
            row = next(r for r in phrase if r["policy"] == "source" and r["rate"] == "48000"
                       and r["group"].startswith("program:"))
            row["name"] = "Unregistered Program"

        def mutate_alias(f):
            alias = f["audit"]["suites"]["alias.csv"]
            alias[1]["note"] = alias[0]["note"]
            alias[1]["feet_ratio"] = alias[0]["feet_ratio"]

        def mutate_fx(f):
            fx = f["audit"]["suites"]["fx.csv"]
            fx[1]["fx_slots"] = fx[0]["fx_slots"]
            fx[1]["fx_mix"] = fx[0]["fx_mix"]

        def mutate_latency(f):
            latency = f["audit"]["suites"]["latency.csv"]
            latency[1]["freq_hz"] = latency[0]["freq_hz"]

        edits = [
            ("duplicate bank program", mutate_bank),
            ("wrong secondary-rate bank sample", mutate_secondary_bank),
            ("wrong causal program", lambda f: f["audit"]["suites"]["click-causality.csv"][0].update(name="Wrong Bass")),
            ("wrong causal event", lambda f: f["audit"]["suites"]["click-causality.csv"][0].update(event="note_off")),
            ("duplicated alias note/feet", mutate_alias),
            ("duplicated FX configuration", mutate_fx),
            ("duplicated latency frequency", mutate_latency),
        ]
        for name, change in edits:
            with self.subTest(name=name):
                f = fixture()
                change(f)
                with self.assertRaises((gate.GateError, ValueError, KeyError)):
                    gate.validate_evidence(CONTRACT, f)

    def test_synthetic_positive(self):
        result = gate.validate_evidence(CONTRACT, fixture())
        self.assertTrue(result["pass"])
        self.assertEqual(len(result["selected_held"]), 11)
        self.assertLessEqual(result["selected_idle"]["median_rtf"], 0.02)

    def test_minimum_of_three_keeps_selected_round_p99_and_misses(self):
        f = fixture()
        for i, rd in enumerate(f["cpu"]["after"]):
            for row in rd["rows"]:
                if row.get("scenario") == "paired_tones":
                    row["median_rtf"] = str(0.19 - i * 0.01)
                    row["p99_rtf"] = str(0.25 + i * 0.01)
                    row["budget_exceedances"] = str(i + 2)
        result = gate.validate_evidence(CONTRACT, f, expected_main_sha=REVIEWED,
                                        expected_source_tree_sha=TREE)
        self.assertEqual({case["selected_round"] for case in result["selected_held"]}, {3})
        self.assertEqual({case["budget_exceedances"] for case in result["selected_held"]}, {4})
        self.assertEqual({case["p99_rtf"] for case in result["selected_held"]}, {0.27})

    def test_current_original_audit_artifact_is_hard_red_when_available(self):
        """Optional on-disk provenance cross-check; never writes or changes original rows."""
        root = os.environ.get("CS80_ORIGINAL_AUDIT_DIR")
        if not root:
            self.skipTest("set CS80_ORIGINAL_AUDIT_DIR to original frozen artifact root")
        root = Path(root)
        f = fixture()
        f["audit"]["summary"] = json.loads((root / "audit-summary.json").read_text())
        f["audit"]["provenance"] = json.loads((root / "audit-provenance.json").read_text())
        for name in CONTRACT["audit"]["expected_rows"]:
            data = (root / "audit" / name).read_bytes()
            with (root / "audit" / name).open(newline="", encoding="utf-8") as stream:
                f["audit"]["suites"][name] = list(csv.DictReader(stream))
            f["audit"]["observed_raw_files"][name] = {
                "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
                "rows": len(f["audit"]["suites"][name]),
            }
        old = f["audit"]["provenance"]
        f["source"].update(reviewed_sha=old["head_sha"], source_tree_sha=old["src_tree_sha"],
                           audio_sha=old["production_audio_commit"],
                           registration_sha=old["registration_commit"])
        for rd in f["cpu"]["after"]:
            rd["metadata"]["source_commit_caller_verified"] = old["head_sha"]
        with self.assertRaisesRegex(gate.GateError, "retrigger.*numerical failures"):
            gate.validate_evidence(CONTRACT, f)

    def test_original_gate_falsification_and_missing_evidence_rejected(self):
        edits = [
            ("missing original click target", lambda f: f["audit"]["summary"]["existing_target_comparisons"].pop(3)),
            ("weakened click ceiling", lambda f: f["audit"]["summary"]["existing_target_comparisons"][1].update(maximum=2)),
            ("different reporter", lambda f: f["audit"]["summary"].update(summarizer_sha256="0" * 64)),
            ("tampered raw SHA", lambda f: f["audit"]["observed_raw_files"]["phrase.csv"].update(sha256="0" * 64)),
            ("honest red audit still blocks release", honestly_failed_original_scope),
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
            ("workload order changed", lambda f: f["cpu"]["after"][1]["metadata"].update(order_seed=803011)),
            ("missing held fixture", lambda f: f["cpu"]["after"][0]["rows"].pop(0)),
            ("held RTF above target", lambda f: [r.update(median_rtf="0.21")
              for rd in f["cpu"]["after"] for r in rd["rows"] if r.get("scenario") == "paired_tones"]),
            ("one held preset above target", lambda f: [r.update(median_rtf="0.201")
              for rd in f["cpu"]["after"] for r in rd["rows"]
              if r.get("scenario") == "paired_tones" and r.get("initial_selector") == "0"]),
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

    def test_external_trusted_sha_checks(self):
        with self.assertRaisesRegex(gate.GateError, "differs from exact main"):
            gate.validate_evidence(CONTRACT, fixture(), expected_main_sha=BEFORE)
        with self.assertRaisesRegex(gate.GateError, "tree differs from exact main"):
            gate.validate_evidence(CONTRACT, fixture(), expected_source_tree_sha=BEFORE)


if __name__ == "__main__":
    unittest.main()
