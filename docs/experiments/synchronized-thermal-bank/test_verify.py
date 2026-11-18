import math
import random
import unittest

import verify as v


class RecoveryTests(unittest.TestCase):
    def test_result(self):
        s = v.verify()
        self.assertEqual(s["fixed_primary_draws"], 3323)
        self.assertAlmostEqual(s["minimum_primary_gate_floor"], .05969593229355, 12)
        self.assertAlmostEqual(s["best_possible_pooled_lower95"], -.009572056665063, 12)
        self.assertAlmostEqual(s["best_possible_factory_quiet_lower95"], -.03706289437805, 12)
        self.assertIsNone(s["measured_full_run_interval"])
        self.assertFalse(s["native_study_admitted"])

    def test_reordered_or_extra_rows_are_not_the_historical_prefix(self):
        h, a, b, *rest = (v.HERE/"bench/raw.csv").read_bytes().splitlines(keepends=True)
        with self.assertRaisesRegex(ValueError, "frozen order"):
            v.read_rows(b"".join([h,b,a,*rest]), 0)
        with self.assertRaisesRegex(ValueError, "prefix"):
            v.read_rows(b"".join([h,a,b,*rest,a]), 0)

    def test_changed_pcm_rejected(self):
        raw = (v.HERE/"bench/raw.csv").read_bytes().replace(b"83f7c309b5b50383",
                                                             b"0000000000000000", 1)
        with self.assertRaisesRegex(ValueError, "PCM/occupancy"):
            v.read_rows(raw, 0)

    def test_ceiling_bounds_finite_completions(self):
        data, _, _ = v.read_rows((v.HERE/"bench/raw.csv").read_bytes(), 0)
        logs = v.block_logs(data, "comparison", v.CASES[:2])
        rng = random.Random(20261118)
        draws = [rng.choices(range(6), k=6) for _ in range(10000)]
        ceiling, _ = v.optimistic_lower(logs, draws)
        for hypothetical in (-4, 0, 1, 20):
            full = logs[:-1]+[hypothetical]
            values = sorted(sum(full[i] for i in d)/6 for d in draws)
            q = values[249]+(values[250]-values[249])*(.025*9999-249)
            self.assertLessEqual(1-math.exp(-q), ceiling+1e-14)


if __name__ == "__main__":
    unittest.main()
