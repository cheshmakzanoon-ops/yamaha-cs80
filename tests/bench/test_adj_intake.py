#!/usr/bin/env python3
"""Schema-only tests: fixtures are NOT Yamaha hardware measurements."""

from copy import deepcopy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import adj_intake as intake


SCRIPT = Path(__file__).with_name("adj_intake.py")


def placeholder():
    """No observations whatsoever: a structural placeholder, not evidence."""
    return {
        "schema_version": 1,
        "record_kind": "cs80_adj_technician_observation",
        "technician": {
            "identifier": "SCHEMA_TEST_ONLY_NO_TECHNICIAN",
            "report_reference": "SCHEMA_TEST_ONLY_NO_REPORT",
            "authorization_reference": "SCHEMA_TEST_ONLY_NO_AUTHORIZATION",
        },
        "instrument": {
            "serial": "SCHEMA_TEST_ONLY_NO_INSTRUMENT",
            "m_board": "SCHEMA_TEST_ONLY_NO_BOARD",
            "board_revision": "SCHEMA_TEST_ONLY_UNKNOWN",
            "channel": "I",
        },
        "snapshots": [{
            "checkpoint": "hp_as_found",
            "panel_setup": "as_found",
            "captured_at_utc": "2026-11-02T00:00:00Z",
            "held_note": "C1",
            "observations": [
                {"node": "CP2", "status": "not_acquired",
                 "reason": "SCHEMA TEST: no physical observations"},
                {"node": "CP3", "status": "not_acquired",
                 "reason": "SCHEMA TEST: no physical observations"},
            ],
        }],
    }


def synthetic_structure_only():
    """Synthetic ephemeral numeric exercise; never stored as hardware evidence."""
    doc = placeholder()
    snapshot = doc["snapshots"][0]
    snapshot["checkpoint"] = "hp_after_vr8_cp3_setting"
    snapshot["panel_setup"] = "yamaha_hp_p11"
    snapshot["scope"] = {
        "instrument_name": "SCHEMA_TEST_SCOPE_NOT_REAL",
        "bandwidth_hz": 1,
        "sample_rate_sps": 1,
        "acquisition_window_s": 1,
        "coupling": "DC",
        "ground_reference": "SCHEMA_TEST_REFERENCE_NOT_REAL",
    }
    snapshot["observations"] = [{
        "node": "CP3",
        "status": "observed",
        "quantity": "vpp",
        "value_v": 0.0,
        "probe": {
            "input_resistance_ohm": 1.0,
            "input_capacitance_pf": 0.0,
            "attenuation": 1.0,
        },
        "evidence": {
            "artifact_reference": "SCHEMA_TEST_ONLY_NO_ARTIFACT",
            "sha256": "0" * 64,
        },
    }]
    return doc


class MetadataOnly(unittest.TestCase):
    def test_missing_measurements_remain_explicitly_missing(self):
        doc = intake.validate_document(placeholder())
        report = intake.render_report(doc)
        self.assertIn("SCHEMA-VALID METADATA ONLY", report)
        self.assertIn("Submitted numeric node observations:** 0", report)
        self.assertIn("Explicit not-acquired nodes:** 2", report)
        self.assertIn("NOT ACQUIRED", report)
        self.assertIn("remain unverified", report)
        self.assertNotIn("SUBMITTED vpp", report)

    def test_synthetic_only_numeric_branch_has_unverified_warning(self):
        # These values, node claims and SHA are IN-MEMORY SOFTWARE SENTINELS,
        # not acquired, published, measured or incorporated into any gate.
        doc = intake.validate_document(synthetic_structure_only())
        report = intake.render_report(doc)
        self.assertIn("unverified", report)
        self.assertIn("No factory", report)
        self.assertIn("SUBMITTED vpp: 0 V", report)
        self.assertNotIn("PASS", report)

    def test_rejects_duplicate_keys_and_non_finite_json(self):
        with self.assertRaisesRegex(intake.IntakeError, "duplicate JSON key"):
            intake.decode_json(b'{"schema_version":1,"schema_version":1}')
        for malformed in (b'{"value":NaN}', b'{"value":Infinity}',
                          b'{"value":-Infinity}'):
            with self.subTest(malformed=malformed), self.assertRaises(intake.IntakeError):
                intake.decode_json(malformed)

    def test_rejects_schema_version_bool_extra_fields_missing_and_bad_types(self):
        changes = [
            lambda d: d.update(schema_version=True),
            lambda d: d.update(schema_version=2),
            lambda d: d.update(extra_field=9),
            lambda d: d.pop("technician"),
            lambda d: d["technician"].update(identifier=""),
            lambda d: d["instrument"].update(channel="III"),
            lambda d: d.update(snapshots=[]),
            lambda d: d["snapshots"][0].update(held_note="C2"),
            lambda d: d["snapshots"][0].update(captured_at_utc="2026-11-02"),
            lambda d: d["snapshots"][0].update(captured_at_utc="2026-13-99T00:00:00Z"),
            lambda d: d["snapshots"][0].update(panel_setup="yamaha_hp_p11"),
            lambda d: d["snapshots"][0].update(scope={"instrument_name": "no reading"}),
            lambda d: d["snapshots"][0].update(connected_m_board_count=0),
            lambda d: d["snapshots"][0]["observations"][0].update(value_v=1),
            lambda d: d["snapshots"][0]["observations"][0].update(node="CP4"),
            lambda d: d["snapshots"][0]["observations"][0].update(reason=""),
        ]
        for mutation in changes:
            candidate = placeholder()
            mutation(candidate)
            with self.subTest(candidate=candidate), self.assertRaises(intake.IntakeError):
                intake.validate_document(candidate)

    def test_rejects_duplicate_node_or_checkpoint(self):
        candidate = placeholder()
        candidate["snapshots"][0]["observations"].append(deepcopy(
            candidate["snapshots"][0]["observations"][0]))
        with self.assertRaisesRegex(intake.IntakeError, "duplicated node"):
            intake.validate_document(candidate)
        candidate = placeholder()
        candidate["snapshots"].append(deepcopy(candidate["snapshots"][0]))
        with self.assertRaisesRegex(intake.IntakeError, "repeated checkpoint"):
            intake.validate_document(candidate)

    def test_alignment_steps_remain_in_printed_order_when_both_given(self):
        candidate = placeholder()
        first = candidate["snapshots"][0]
        first["checkpoint"] = "hp_after_vr7_cp3_max"
        first["panel_setup"] = "yamaha_hp_p11"
        second = deepcopy(first)
        second["checkpoint"] = "hp_after_vr8_cp3_setting"
        candidate["snapshots"].append(second)
        intake.validate_document(candidate)
        candidate["snapshots"].reverse()
        with self.assertRaisesRegex(intake.IntakeError, "out of printed ADJ order"):
            intake.validate_document(candidate)

    def test_optional_context_is_labeled_not_invented(self):
        candidate = placeholder()
        snapshot = candidate["snapshots"][0]
        snapshot.update(connected_m_board_count=2, ambient_temperature_c=21,
                        warmup_minutes=10, control_reference="SCHEMA_TEST_ONLY")
        report = intake.render_report(intake.validate_document(candidate))
        self.assertIn("NI-connected M boards: 2", report)
        self.assertIn("ambient: 21 °C", report)
        self.assertIn("warmup: 10 min", report)
        self.assertIn("control reference:", report)
        self.assertNotIn("SUBMITTED vpp", report)

    def test_rejects_numeric_reading_without_provenance_or_sensing_metadata(self):
        changes = [
            lambda d: d["snapshots"][0].pop("scope"),
            lambda d: d["snapshots"][0]["observations"][0].pop("probe"),
            lambda d: d["snapshots"][0]["observations"][0].pop("evidence"),
            lambda d: d["snapshots"][0]["observations"][0].update(value_v=True),
            lambda d: d["snapshots"][0]["observations"][0].update(value_v=float("inf")),
            lambda d: d["snapshots"][0]["observations"][0].update(value_v=10**500),
            lambda d: d["snapshots"][0]["observations"][0].update(value_v=-1),
            lambda d: d["snapshots"][0]["observations"][0].update(quantity="vdc"),
            lambda d: d["snapshots"][0]["observations"][0]["probe"].update(
                input_resistance_ohm=0),
            lambda d: d["snapshots"][0]["observations"][0]["probe"].update(
                input_capacitance_pf=-1),
            lambda d: d["snapshots"][0]["observations"][0]["evidence"].update(
                sha256="UNVERIFIED_NOT_A_DIGEST"),
            lambda d: d["snapshots"][0]["scope"].update(bandwidth_hz=0),
            lambda d: d["snapshots"][0]["scope"].update(sample_rate_sps=False),
        ]
        for mutation in changes:
            candidate = synthetic_structure_only()
            mutation(candidate)
            with self.subTest(candidate=candidate), self.assertRaises(intake.IntakeError):
                intake.validate_document(candidate)

    def test_control_node_quantity_and_noise_metadata_boundary(self):
        candidate = synthetic_structure_only()
        candidate["snapshots"][0]["observations"][0]["node"] = "IC10_VFC4"
        candidate["snapshots"][0]["observations"][0]["quantity"] = "vdc"
        intake.validate_document(candidate)  # Not a claim that a physical voltage was seen.
        candidate["snapshots"][0].pop("scope")
        candidate["snapshots"][0]["meter"] = {
            "instrument_name": "SCHEMA_TEST_METER_NOT_REAL",
            "input_resistance_ohm": 1,
            "mode": "DC_V",
        }
        report = intake.render_report(intake.validate_document(candidate))
        self.assertIn("meter", report)
        candidate["snapshots"][0]["meter"]["mode"] = "AC_A"
        with self.assertRaises(intake.IntakeError):
            intake.validate_document(candidate)
        candidate["snapshots"][0]["meter"]["mode"] = "DC_V"
        candidate["snapshots"][0]["observations"][0]["quantity"] = "vpp"
        with self.assertRaises(intake.IntakeError):
            intake.validate_document(candidate)
        candidate = placeholder()
        candidate["snapshots"][0]["observations"][0]["node"] = "NO1"
        candidate["snapshots"][0]["observations"][1]["node"] = "NI"
        intake.validate_document(candidate)  # No implied source voltage or impedance.

    def test_untrusted_strings_cannot_form_markdown_table_columns(self):
        candidate = placeholder()
        candidate["instrument"]["serial"] = "UNTRUSTED|<script>* [label]"
        report = intake.render_report(intake.validate_document(candidate))
        self.assertNotIn("<script>", report)
        self.assertNotIn("UNTRUSTED|", report)
        self.assertIn("&#124;", report)
        self.assertIn("&lt;script&gt;", report)

    def test_cli_emits_only_missing_report_and_does_not_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "schema-placeholder.json"
            report = Path(directory) / "report.md"
            source.write_text(json.dumps(placeholder()), encoding="utf-8")
            cmd = [sys.executable, str(SCRIPT), "--input", str(source),
                   "--output", str(report)]
            first = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(first.returncode, 0, first.stderr)
            self.assertIn("Submitted numeric node observations:** 0", report.read_text())
            before = report.read_bytes()
            second = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(second.returncode, 2)
            self.assertEqual(report.read_bytes(), before)
            candidate = placeholder()
            candidate["schema_version"] = "1"
            source.write_text(json.dumps(candidate), encoding="utf-8")
            failing = subprocess.run([sys.executable, str(SCRIPT), "--input", str(source)],
                                     capture_output=True, text=True)
            self.assertEqual(failing.returncode, 2)
            self.assertIn("ADJ intake rejected:", failing.stderr)


if __name__ == "__main__":
    unittest.main()
