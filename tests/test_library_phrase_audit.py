#!/usr/bin/env python3
"""Fast positive/negative checker controls; synthetic rows are not audio proof.

Optional --capture-tool verifies the compiled producer's real inventory and
diagnostic CLI without running the expensive complete sound sweep.
"""
from __future__ import annotations

import argparse
import copy
import csv
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("library_audit", ROOT / "scripts/check_library_phrase_audit.py")
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)
CONTRACT = json.loads(audit.CONTRACT.read_text())
CAPTURE_TOOL = None


def fixture():
    inventory = copy.deepcopy(CONTRACT["fixtures"])
    rows = []
    for rate in CONTRACT["rates_hz"]:
        for item in inventory:
            row = {key: "0" for key in audit.HEADER}
            row.update(item, policy="source", rate=str(rate), peak="0.5", peak_db="-6.020599913279624",
                       click_retrigger="0.5", click_steal="0.5")
            rows.append(row)
    capture = {"schema_version": 1, "scope": CONTRACT["scope"], "complete": True,
               "diagnostic_subset": False, "rows_written": len(rows), "rates_hz": CONTRACT["rates_hz"],
               "policy": "source", "hardware_measured": False, "release_qualified": False}
    return inventory, capture, rows


class CheckerTests(unittest.TestCase):
    def setUp(self):
        self.inventory, self.capture, self.rows = fixture()

    def result(self):
        return audit.verify(CONTRACT, self.inventory, self.capture, self.rows)

    def test_complete_synthetic_positive(self):
        result = self.result()
        self.assertEqual(result["rows"], 1602)
        self.assertTrue(result["numerical_targets_met"])
        self.assertFalse(result["release_qualified"])

    def test_exact_thresholds_are_accepted(self):
        for metric, maximum in CONTRACT["maximums"].items():
            self.rows[0][metric] = str(maximum)
        self.assertTrue(self.result()["numerical_targets_met"])

    def test_each_numerical_target_rejects_excess(self):
        for metric, maximum in CONTRACT["maximums"].items():
            with self.subTest(metric=metric):
                self.setUp()
                self.rows[0][metric] = str(maximum + (1 if metric in audit.INTEGER else 1e-7))
                result = self.result()
                self.assertFalse(result["numerical_targets_met"])
                self.assertEqual(result["breaches"][0]["metric"], metric)

    def test_every_metric_rejects_missing_nonfinite_or_nonnumeric_values(self):
        for metric in set(audit.HEADER) - audit.TEXT:
            for value in ("", "NaN", "inf", "-inf", "1e999", "not-a-number"):
                with self.subTest(metric=metric, value=value):
                    self.setUp()
                    self.rows[0][metric] = value
                    with self.assertRaises(audit.Rejected):
                        self.result()

    def test_missing_curated_program_rejected(self):
        self.rows.pop(222)
        with self.assertRaises(audit.Rejected):
            self.result()

    def test_stride_sixteen_capture_rejected(self):
        self.rows = [row for row in self.rows if row["rate"] == "44100"
                     or row["library"] == "tone" or int(row["fixture_id"].split(":")[1]) % 16 == 0]
        with self.assertRaises(audit.Rejected):
            self.result()

    def test_duplicate_easy_row_cannot_replace_hard_row(self):
        self.rows[223] = self.rows[222].copy()
        with self.assertRaises(audit.Rejected):
            self.result()

    def test_fixture_substitutions_rejected(self):
        for field, value in (("name", "Easy program"), ("group", "program:Other"),
                             ("library", "legacy"), ("fixture_id", "program:9999"),
                             ("policy", "legacy"), ("rate", "88200")):
            with self.subTest(field=field):
                self.setUp()
                self.rows[222][field] = value
                with self.assertRaises(audit.Rejected):
                    self.result()

    def test_incomplete_claims_rejected(self):
        for key, value in (("complete", False), ("complete", 1), ("diagnostic_subset", True), ("schema_version", True),
                           ("scope", "original-full885"), ("rows_written", 534),
                           ("rates_hz", [44100]), ("release_qualified", True)):
            with self.subTest(key=key):
                self.setUp()
                self.capture[key] = value
                with self.assertRaises(audit.Rejected):
                    self.result()

    def test_schema_substitution_rejected(self):
        for kind in ("missing", "extra", "reordered"):
            self.setUp()
            row = self.rows[0]
            if kind == "missing": del row["click_retrigger"]
            if kind == "extra": row["alternate_score"] = "0"
            if kind == "reordered": self.rows[0] = dict(reversed(list(row.items())))
            with self.subTest(kind=kind), self.assertRaises(audit.Rejected):
                self.result()

    def test_invalid_integer_and_negative_metrics_rejected(self):
        for metric in audit.NONNEGATIVE:
            for value in ("-1", "-1e-999"):
                self.setUp()
                self.rows[0][metric] = value
                with self.subTest(metric=metric, value=value), self.assertRaises(audit.Rejected):
                    self.result()
        self.setUp()
        self.rows[0]["samples_over_1"] = "0.5"
        with self.assertRaises(audit.Rejected):
            self.result()

    def test_reporter_domain_boundaries_are_valid(self):
        for fraction in ("0", "0.5", "1"):
            for voices in ("0", "8"):
                with self.subTest(fraction=fraction, voices=voices):
                    self.setUp()
                    self.rows[0]["softclip_frac_chord"] = fraction
                    self.rows[0]["voices_at_end"] = voices
                    self.assertTrue(self.result()["numerical_targets_met"])

    def test_diagnostic_ratios_over_one_are_not_new_acceptance_limits(self):
        for field in ("click_off", "click_chord_off", "click_fresh_chord"):
            self.rows[0][field] = "1.5"
        for field in audit.DECIBELS:
            self.rows[0][field] = "-12"
        self.assertTrue(self.result()["numerical_targets_met"])

    def test_fraction_and_voice_domain_violations_rejected(self):
        for field, value in (("softclip_frac_chord", "2"),
                             ("softclip_frac_chord", "1.00000000000000000001"),
                             ("voices_at_end", "9"),
                             ("voices_at_end", "99999999999999999999999999999"),
                             ("voices_at_end", "8.0000000000000001"),
                             ("rate", "44100.0000000000000001")):
            with self.subTest(field=field, value=value):
                self.setUp()
                self.rows[0][field] = value
                with self.assertRaises(audit.Rejected):
                    self.result()

    def test_frame_counters_cannot_exceed_capture_length(self):
        for field in ("samples_over_1", "nonfinite"):
            self.setUp()
            frames = int(self.rows[0]["rate"]) * audit.PHRASE_SECONDS
            self.rows[0][field] = str(frames)
            # Valid schema at the boundary, but the original zero target fails.
            self.assertFalse(self.result()["numerical_targets_met"])
            self.rows[0][field] = str(frames + 1)
            with self.subTest(field=field), self.assertRaises(audit.Rejected):
                self.result()

    def test_domains_match_the_reused_reporter(self):
        engine = (ROOT / "src/core/Engine.h").read_text()
        reporter = (ROOT / "src/tools/engine_audit.cpp").read_text()
        capacity = re.search(r"static constexpr int kNumVoices\s*=\s*(\d+)\s*;", engine)
        duration = re.search(r"constexpr double length\s*=\s*([\d.]+)\s*;", reporter)
        self.assertIsNotNone(capacity)
        self.assertIsNotNone(duration)
        self.assertEqual(int(capacity.group(1)), audit.VOICE_CAPACITY)
        self.assertEqual(float(duration.group(1)), audit.PHRASE_SECONDS)

    def test_catalog_cannot_be_replaced(self):
        self.inventory[222]["name"] = "Another program"
        with self.assertRaises(audit.Rejected):
            self.result()

    def test_original_click_limits_unchanged(self):
        original = json.loads((ROOT / "tests/parity/tolerances-v2/ws1-voice.json").read_text())
        limits = original["stages"]["ig00151_vca"]
        for name in ("click_steal", "click_retrigger"):
            self.assertEqual(CONTRACT["maximums"][name], limits[name + "_max"])

    def test_checker_cli_exit_codes(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path / "inventory.json").write_text(json.dumps(self.inventory))
            (path / "capture.json").write_text(json.dumps(self.capture))
            for expected, bad_value in ((0, "0.5"), (1, "1.0000001"), (2, "nan")):
                self.rows[0]["click_retrigger"] = bad_value
                with (path / "library-phrase.csv").open("w", newline="") as stream:
                    writer = csv.DictWriter(stream, fieldnames=audit.HEADER)
                    writer.writeheader()
                    writer.writerows(self.rows)
                completed = subprocess.run([sys.executable, str(ROOT / "scripts/check_library_phrase_audit.py"), tmp],
                                           capture_output=True, text=True)
                self.assertEqual(completed.returncode, expected, completed.stdout + completed.stderr)

    def test_malformed_top_level_and_oversized_csv_exit_two(self):
        with self.assertRaises(audit.Rejected):
            audit.verify(CONTRACT, self.inventory, [], self.rows)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path / "inventory.json").write_text(json.dumps(self.inventory))
            with (path / "library-phrase.csv").open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=audit.HEADER)
                writer.writeheader()
                writer.writerows(self.rows)
            (path / "capture.json").write_text("[]")
            command = [sys.executable, str(ROOT / "scripts/check_library_phrase_audit.py"), tmp]
            invalid_receipt = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(invalid_receipt.returncode, 2, invalid_receipt.stderr)
            self.assertNotIn("Traceback", invalid_receipt.stderr)
            (path / "capture.json").write_text(json.dumps(self.capture))
            (path / "library-phrase.csv").write_text(",".join(audit.HEADER) + "\n" + "x" * 131073 + "\n")
            oversized_csv = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(oversized_csv.returncode, 2, oversized_csv.stderr)
            self.assertNotIn("Traceback", oversized_csv.stderr)

    def test_unrepresentable_decimal_text_is_invalid_not_a_numeric_miss(self):
        cases = (("click_off", "1e-999999999999999999999999999"),
                 ("voices_at_end", "0e999999999999999999999999999"))
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path / "inventory.json").write_text(json.dumps(self.inventory))
            (path / "capture.json").write_text(json.dumps(self.capture))
            for field, value in cases:
                self.setUp()
                self.rows[0][field] = value
                with self.subTest(field=field), self.assertRaises(audit.Rejected):
                    self.result()
                with (path / "library-phrase.csv").open("w", newline="") as stream:
                    writer = csv.DictWriter(stream, fieldnames=audit.HEADER)
                    writer.writeheader()
                    writer.writerows(self.rows)
                completed = subprocess.run([sys.executable, str(ROOT / "scripts/check_library_phrase_audit.py"), tmp],
                                           capture_output=True, text=True)
                self.assertEqual(completed.returncode, 2, completed.stderr)
                self.assertNotIn("Traceback", completed.stderr)

    def test_compiled_catalog_and_diagnostic_boundaries(self):
        if CAPTURE_TOOL is None:
            self.skipTest("supply --capture-tool for compiled producer checks")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "catalog"
            completed = subprocess.run([str(CAPTURE_TOOL), "--out", str(path), "--catalog-only"],
                                       capture_output=True, text=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(json.loads((path / "inventory.json").read_text()), self.inventory)
            receipt = json.loads((path / "capture.json").read_text())
            self.assertFalse(receipt["complete"])
            self.assertTrue(receipt["diagnostic_subset"])
            self.assertFalse((path / "library-phrase.csv").exists())
            for arguments in (["--out", str(path), "--catalog-only"],
                              ["--out", str(path / "bad"), "--fixture", "program:9999"],
                              ["--out", str(path / "bad"), "--rate", "48000trailing"],
                              ["--out", str(path / "bad"), "--quick"],
                              ["--out", str(path / "bad"), "--catalog-only", "--rate", "44100"]):
                self.assertEqual(subprocess.run([str(CAPTURE_TOOL), *arguments], capture_output=True).returncode, 2)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-tool", type=Path)
    options, remaining = parser.parse_known_args()
    CAPTURE_TOOL = options.capture_tool.resolve() if options.capture_tool else None
    unittest.main(argv=[sys.argv[0], *remaining])
