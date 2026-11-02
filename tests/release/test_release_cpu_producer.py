#!/usr/bin/env python3
"""Preregister same-job producer semantics before the implementation exists."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = json.loads((ROOT / ".github/release-numerical-contract.json").read_text())
SPEC = importlib.util.spec_from_file_location(
    "cs80_release_cpu_producer", ROOT / "scripts/run_release_cpu_producer.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("release CPU producer unavailable")
producer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(producer)

B = "a" * 40
M = "b" * 40
SRC = "c" * 40
BENCH = "d" * 40


def registration():
    return {"schema_version": 1, "before_sha": B, "reviewed_sha": M,
            "reviewed_source_tree_sha": SRC, "before_benchmark_blob": BENCH,
            "after_benchmark_blob": BENCH, "rate": 48000, "block": 256,
            "callbacks": 1024, "rounds_per_source": 3, "suite": "baseline"}


def context():
    return {"GITHUB_ACTIONS": "true", "GITHUB_SHA": M,
            "GITHUB_REF": "refs/heads/main", "GITHUB_RUN_ID": "10001",
            "GITHUB_RUN_ATTEMPT": "1", "GITHUB_JOB": "cpu_same_host",
            "RUNNER_NAME": "host-epyc", "RUNNER_OS": "Linux", "RUNNER_ARCH": "X64"}


class TrustedSameHostProducer(unittest.TestCase):
    def test_positive_registration_and_timing_order(self):
        producer.validate_registration(registration(), CONTRACT, M, SRC, True)
        self.assertEqual(list(producer.serial_schedule()), [
            ("before", 1), ("after", 1),
            ("before", 2), ("after", 2),
            ("before", 3), ("after", 3)])
        ident = producer.require_ci_context(context(), M)
        self.assertEqual(ident["runner_job_id"], "10001/1/cpu_same_host")

    def test_negative_registration_mutations(self):
        edits = [
            ("before identical to M", lambda r: r.update(before_sha=M)),
            ("stale reviewed", lambda r: r.update(reviewed_sha=SRC)),
            ("wrong tree", lambda r: r.update(reviewed_source_tree_sha=B)),
            ("benchmark source changed", lambda r: r.update(after_benchmark_blob=B)),
            ("quick callback workload", lambda r: r.update(callbacks=128)),
            ("wrong sample rate", lambda r: r.update(rate=44100)),
            ("wrong block", lambda r: r.update(block=128)),
            ("missing round", lambda r: r.update(rounds_per_source=2)),
            ("legacy suite", lambda r: r.update(suite="quick")),
        ]
        for label, edit in edits:
            with self.subTest(label=label):
                r = registration()
                edit(r)
                with self.assertRaises((ValueError, KeyError)):
                    producer.validate_registration(r, CONTRACT, M, SRC, True)
        with self.assertRaises((ValueError, KeyError)):
            producer.validate_registration(registration(), CONTRACT, M, SRC, False)

    def test_negative_context_mutations(self):
        edits = [
            ("not CI", lambda c: c.update(GITHUB_ACTIONS="false")),
            ("not main", lambda c: c.update(GITHUB_REF="refs/heads/feature")),
            ("wrong main SHA", lambda c: c.update(GITHUB_SHA=B)),
            ("missing job", lambda c: c.pop("GITHUB_JOB")),
            ("missing runner", lambda c: c.pop("RUNNER_NAME")),
            ("no attempt", lambda c: c.update(GITHUB_RUN_ATTEMPT="")),
        ]
        for label, edit in edits:
            with self.subTest(label=label):
                c = context()
                edit(c)
                with self.assertRaises((ValueError, KeyError)):
                    producer.require_ci_context(c, M)


if __name__ == "__main__":
    unittest.main()
