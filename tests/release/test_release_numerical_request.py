#!/usr/bin/env python3
"""Preregister source-lock negatives for the local numerical CI producer."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "scripts/prepare_release_numerical_request.py"
SPEC = importlib.util.spec_from_file_location("release_numerical_request", MODULE)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("source-locked request preparation unavailable")
request = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(request)

CONTRACT = json.loads((ROOT / ".github/release-numerical-contract.json").read_text())
AUDIT = json.loads((ROOT / ".github/full-engine-audit-request.json").read_text())
M = "b" * 40
SRC = AUDIT["expected_src_tree"]
REG = "00da4498792523cf8d97fd40c17e12f215e190b8"


def env():
    return {"GITHUB_ACTIONS": "true", "GITHUB_EVENT_NAME": "push",
            "GITHUB_REPOSITORY": "cheshmakzanoon-ops/yamaha-cs80",
            "GITHUB_REF": "refs/heads/main", "GITHUB_SHA": M,
            "GITHUB_RUN_ID": "101", "GITHUB_RUN_ATTEMPT": "1",
            "GITHUB_JOB": "trusted-source-and-same-host-evidence"}


class ReleaseNumericalRequestTests(unittest.TestCase):
    def test_main_event_preflight_rejects_untrusted_context(self):
        request.validate_ci_context(env(), M, M)
        mutations = {"GITHUB_ACTIONS": "false", "GITHUB_EVENT_NAME": "workflow_dispatch",
                     "GITHUB_REPOSITORY": "attacker/yamaha-cs80", "GITHUB_REF": "refs/tags/v1.0.0",
                     "GITHUB_SHA": "c" * 40, "GITHUB_RUN_ID": "", "GITHUB_JOB": ""}
        for key, value in mutations.items():
            with self.subTest(field=key):
                candidate = env()
                candidate[key] = value
                with self.assertRaises(ValueError):
                    request.validate_ci_context(candidate, M, M)
        with self.assertRaisesRegex(ValueError, "current main"):
            request.validate_ci_context(env(), M, "c" * 40)

    def test_producer_request_pins_frozen_before_benchmark_and_workload(self):
        src = request.cpu_request(CONTRACT, M, SRC)
        self.assertEqual(src["before_sha"], "7c864f266a939aa2a42422440dd4c1c0cabe3814")
        self.assertEqual(src["before_benchmark_blob"], "92ea7e0b1801bf98fccaed21c13e676890039c94")
        self.assertEqual(src["after_benchmark_blob"], src["before_benchmark_blob"])
        self.assertEqual((src["rate"], src["block"], src["callbacks"], src["rounds_per_source"]),
                         (48000, 256, 1024, 3))
        self.assertEqual(src["reviewed_sha"], M)
        self.assertEqual(src["reviewed_source_tree_sha"], SRC)

    def test_identity_uses_original_registered_audit_not_invented_source(self):
        identity = request.source_identity(CONTRACT, AUDIT, M, SRC, REG)
        self.assertEqual(identity["reviewed_sha"], M)
        self.assertEqual(identity["source_tree_sha"], SRC)
        self.assertEqual(identity["audio_sha"], AUDIT["production_audio_commit"])
        self.assertEqual(identity["registration_sha"], REG)
        self.assertEqual(identity["before_sha"], CONTRACT["cpu"]["registered_before_source_sha"])
        for mutation in ("0" * 40, "a" * 40):
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    request.source_identity(CONTRACT, {**AUDIT, "expected_src_tree": mutation}, M, SRC, REG)


if __name__ == "__main__":
    unittest.main()
