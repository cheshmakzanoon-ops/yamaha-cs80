#!/usr/bin/env python3
"""Independent pins against losing the CPU contract during audit hardening.

The header and fixture hashes below were registered from the original 885-row
audit, not inferred from a candidate file being checked. Never relax original
audio/CPU acceptance limits to make this regression green.
"""
from __future__ import annotations

import copy
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = json.loads((ROOT / ".github/release-numerical-contract.json").read_text())
MODULE = importlib.util.spec_from_file_location(
    "cs80_release_cpu_producer", ROOT / "scripts/run_release_cpu_producer.py")
if MODULE is None or MODULE.loader is None:
    raise RuntimeError("CPU producer unavailable")
producer = importlib.util.module_from_spec(MODULE)
MODULE.loader.exec_module(producer)
GATE_SPEC = importlib.util.spec_from_file_location(
    "cs80_release_numerical_gate", ROOT / "scripts/release_numerical_gate.py")
if GATE_SPEC is None or GATE_SPEC.loader is None:
    raise RuntimeError("numerical gate unavailable")
gate = importlib.util.module_from_spec(GATE_SPEC)
GATE_SPEC.loader.exec_module(gate)

BEFORE = "7c864f266a939aa2a42422440dd4c1c0cabe3814"
BENCHMARK = "92ea7e0b1801bf98fccaed21c13e676890039c94"
HEADERS = {
    "latency.csv": "f50e395fd07bf65ac8062744f603f5c3749a4697bb36b1473e21d7268cb0a054",
    "alias.csv": "0af41ab4d818ae51ba177c74cb78aae310038416d9c877acb3dccf3a9e407017",
    "fx.csv": "caa626672a649b530e96a1e49fa354a5b074d2f6cbdbab377403892c54d3b368",
    "phrase.csv": "397b3b024212148eff97955ac8599e00cd8e60fc710f39f55078f9ee77ddd6d9",
    "click-causality.csv": "373bc5dc179d091ab8836a007c0cdff4fd7967fab7aa11e12112646a7907a422",
}
IDENTITIES = {
    "factory_programs_44100_sha256": "d169852773dd2e43ccf951531592eb870c7aff534212059b9f967eed5c2872c1",
    "tone_pairs_44100_sha256": "cbef2a1ffc829717bb0f996db1212f9e034148aef86451ec5cfb68219615c793",
}
M = "b" * 40
SRC = "c" * 40


def request():
    return {"schema_version": 1, "before_sha": BEFORE, "reviewed_sha": M,
            "reviewed_source_tree_sha": SRC, "before_benchmark_blob": BENCHMARK,
            "after_benchmark_blob": BENCHMARK, "rate": 48000, "block": 256,
            "callbacks": 1024, "rounds_per_source": 3, "suite": "baseline"}


class CombinedReleaseContract(unittest.TestCase):
    def test_cpu_registration_remains_exact(self):
        cpu = CONTRACT["cpu"]
        self.assertEqual(cpu["registered_before_source_sha"], BEFORE)
        self.assertEqual(cpu["registered_benchmark_blob_sha"], BENCHMARK)
        self.assertEqual(cpu["registered_benchmark_source_sha256"],
                         "3a747c959529a15ef0675784a8bf496a23f74669fc41ccd7899789dec7441d74")
        self.assertEqual(cpu["registered_case_identity_sha256"],
                         "8430527f7cad6540179c4024e1834e57f483faa045a6f28521354e7b35638f4b")
        self.assertEqual(len(cpu["registered_tone_pairs_by_selector"]), 11)
        self.assertEqual(gate.cpu_case_identity_sha256(
            gate.registered_cpu_case_identities(cpu).elements()),
            cpu["registered_case_identity_sha256"])
        self.assertEqual(cpu["upper_bounds"],
                         {"held_median_rtf": 0.20, "idle_median_rtf": 0.02})
        self.assertEqual((cpu["required_before_rounds"], cpu["required_after_rounds"]), (3, 3))
        producer.validate_registration(request(), CONTRACT, M, SRC, True)

    def test_original_audit_headers_and_fixture_identities_coexist(self):
        audit = CONTRACT["audit"]
        self.assertEqual(audit["frozen_csv_header_sha256"], HEADERS)
        self.assertEqual({name: audit["frozen_fixture_identities"][name]
                          for name in IDENTITIES}, IDENTITIES)
        self.assertEqual(audit["expected_rows"],
                         {"latency.csv": 40, "alias.csv": 168, "fx.csv": 102,
                          "phrase.csv": 566, "click-causality.csv": 9})
        self.assertEqual([t["maximum"] for t in audit["existing_targets"]],
                         [0, 1, 1, 1, 1, -70, 1, 1, 1])
        self.assertEqual(audit["frozen_reporter_sha256"],
                         "3f1a68208c8c848c645088dae8d1a398190f32c05c1344bbffd3f4c071aa2fd9")

    def test_earlier_pre_ring_885_row_archive_stays_authentic_and_red(self):
        # This committed pre-ring local predecessor is NOT the frozen 39ae6c
        # run 41336194253. Both share fixture pins but different click counts.
        root = ROOT / "docs/evidence/qualification-2026-11-01/pre-ring-local-integration"
        archive = root / "audit"
        rows = {}
        for name, count in CONTRACT["audit"]["expected_rows"].items():
            with (archive / name).open(newline="", encoding="utf-8") as source:
                reader = csv.DictReader(source)
                expected_header = hashlib.sha256(",".join(reader.fieldnames).encode()).hexdigest()
                self.assertEqual(expected_header, HEADERS[name])
                rows[name] = list(reader)
                self.assertEqual(len(rows[name]), count)
        self.assertEqual(sum(map(len, rows.values())), 885)
        for key, tab in (("factory_programs_44100_sha256", False),
                         ("tone_pairs_44100_sha256", True)):
            actual = [r for r in rows["phrase.csv"] if r["policy"] == "source"
                      and r["rate"] == "44100" and (r["group"] == "tab") == tab]
            self.assertEqual(gate.fixture_identity_sha256(actual), IDENTITIES[key])
        summary = json.loads((root / "audit-summary.json").read_text())
        self.assertEqual(summary["source_snapshot_sha"],
                         "1e5893854c3d0d9b0aa1ab1c725fd2e8084717f6")
        self.assertEqual([x["exceeding_row_count"] for x in summary["existing_target_comparisons"]],
                         [0, 2, 1, 23, 37, 0, 2, 0, 3])
        self.assertEqual(summary["summarizer_sha256"], CONTRACT["audit"]["frozen_reporter_sha256"])

    def test_missing_or_swapped_cpu_registration_fails_producer(self):
        for field in ("registered_before_source_sha", "registered_benchmark_blob_sha"):
            with self.subTest(field=field, mutation="missing"):
                contract = copy.deepcopy(CONTRACT)
                del contract["cpu"][field]
                with self.assertRaises((KeyError, ValueError)):
                    producer.validate_registration(request(), contract, M, SRC, True)
            with self.subTest(field=field, mutation="forged"):
                contract = copy.deepcopy(CONTRACT)
                contract["cpu"][field] = "f" * 40
                with self.assertRaises((KeyError, ValueError)):
                    producer.validate_registration(request(), contract, M, SRC, True)


if __name__ == "__main__":
    unittest.main()
