import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import analyze


def sample():
    rows = []
    for i in range(12):
        s = 400_000_000 + i * 25_000_000
        rows.append({
            "index": i,
            "cpu_ns": {"S0": s, "S1": s, "S+": s * 17 // 16},
            "positive_common_cpu_ns": s,
            "positive_extra_cpu_ns": s // 16,
            "measured_calls": dict(analyze.COUNTS),
        })
    return {"schema": analyze.SCHEMA, "session_id": "synthetic", "complete": True,
            "acquisition_class": "dedicated-target",
            "triplets": rows}


class AnalyzeTests(unittest.TestCase):
    def test_perfect_pairing_cancels_run_scale_but_keeps_real_extra_cost(self):
        result = analyze.analyze(sample())
        self.assertTrue(result["qualified"])
        self.assertAlmostEqual(result["gates"]["same_stock_with_extra_calls"]["point_percent"], 6.25)
        self.assertAlmostEqual(result["gates"]["same_stock_twins"]["point_percent"], 0)

    def test_gate_rejects_spread_and_does_not_trim(self):
        data = sample()
        for i, row in enumerate(data["triplets"]):
            s = row["cpu_ns"]["S0"]
            row["cpu_ns"]["S1"] = s * (110 if i % 2 else 90) // 100
        result = analyze.analyze(data)
        gate = result["gates"]["same_stock_twins"]
        self.assertLess(abs(gate["point_percent"]), 2)
        self.assertFalse(gate["pass"])
        self.assertFalse(result["qualified"])

    def test_cloud_pass_is_not_dedicated_target_qualification(self):
        result = analyze.analyze(dict(sample(), acquisition_class="cloud-diagnostic"))
        self.assertTrue(result["gates_pass"])
        self.assertFalse(result["qualified"])
        self.assertIn("cloud-only", result["scope"])

    def test_wrong_common_cost_cannot_pass_via_valid_total(self):
        data = sample()
        for row in data["triplets"]:
            row["positive_common_cpu_ns"] = row["cpu_ns"]["S0"] * 103 // 100
            row["positive_extra_cpu_ns"] = row["cpu_ns"]["S+"] - row["positive_common_cpu_ns"]
        result = analyze.analyze(data)
        self.assertTrue(result["gates"]["same_stock_with_extra_calls"]["pass"])
        self.assertFalse(result["gates"]["same_stock_positive_common_calls"]["pass"])
        self.assertFalse(result["qualified"])

    def test_real_extra_work_gate_not_normalized_away(self):
        data = sample()
        for row in data["triplets"]:
            row["positive_extra_cpu_ns"] //= 8
            row["cpu_ns"]["S+"] = row["positive_common_cpu_ns"] + row["positive_extra_cpu_ns"]
        self.assertFalse(analyze.analyze(data)["gates"]["same_stock_with_extra_calls"]["pass"])

    def test_fixed_interval(self):
        values = [i / 1000 for i in range(12)]
        result = analyze.confidence(values, -2, 2)
        self.assertAlmostEqual(result["point_percent"], 100 * math.expm1(0.0055))
        self.assertAlmostEqual(result["interval_95_percent"][0], 0.3214296822391, places=8)
        self.assertTrue(result["pass"])

    def test_invalid_acquisitions_are_not_nonqualifications(self):
        cases = {
            "incomplete": lambda x: x.update(complete=False),
            "missing": lambda x: x["triplets"].pop(),
            "smoke": lambda x: x.update(acquisition_class="untimed-smoke"),
            "reordered": lambda x: x["triplets"].reverse(),
            "duplicate": lambda x: x["triplets"][1].update(index=0),
            "extra_arm": lambda x: x["triplets"][0]["cpu_ns"].update(C=5),
            "short_positive": lambda x: x["triplets"][0]["measured_calls"].update({"S+": 512}),
            "false_number": lambda x: x["triplets"][0]["cpu_ns"].update(S0=True),
            "disconnected_clocks": lambda x: x["triplets"][0].update(positive_extra_cpu_ns=2),
        }
        for name, mutate in cases.items():
            with self.subTest(name=name):
                data = sample()
                mutate(data)
                with self.assertRaises(ValueError):
                    analyze.analyze(data)

    def test_cli_refuses_duplicate_keys(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "summary.json"
            path.write_text(json.dumps(sample())[:-1] + ', "complete": true}')
            run = subprocess.run([sys.executable, analyze.__file__, str(path)],
                                 capture_output=True, check=False)
        self.assertEqual(run.returncode, 1)
        self.assertIn(b"duplicate JSON field", run.stderr)

    def test_cli_distinguishes_valid_nonqualification(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "summary.json"
            data = sample()
            for row in data["triplets"]:
                row["cpu_ns"]["S1"] *= 2
            path.write_text(json.dumps(data))
            run = subprocess.run([sys.executable, analyze.__file__, str(path)],
                                 capture_output=True, check=False)
        self.assertEqual(run.returncode, 2)
        self.assertFalse(json.loads(run.stdout)["qualified"])


if __name__ == "__main__":
    unittest.main()
