#!/usr/bin/env python3
"""Preregistered raw nonfinite-render witness; existing thresholds unchanged."""
from __future__ import annotations

import unittest
from test_numerical_release_gate import TEST_CONTRACT, fixture, gate


class AliasNonfiniteWitnessTests(unittest.TestCase):
    def test_raw_alias_sentinel_cannot_hide_behind_false_green_summary(self):
        # engine_audit.cpp emits the finite numeric value 999 if any rendered
        # alias sample is nonfinite. The original frozen reporter recognizes
        # this sentinel, but the independent validator must verify raw rows too.
        for policy in ('source', 'legacy'):
            for rate in ('44100', '48000', '96000'):
                for ring in ('0', '1'):
                    if policy == 'source' and rate == '48000' and ring == '0':
                        continue  # Already caught by the original -70 dB gate.
                    for sentinel in ('999', '999.0', '9.99e2'):
                        with self.subTest(policy=policy, rate=rate, ring=ring, sentinel=sentinel):
                            candidate = fixture()
                            row = next(row for row in candidate['audit']['suites']['alias.csv']
                                       if row['policy'] == policy and row['rate'] == rate
                                       and row['ring'] == ring)
                            row['nonharmonic_db_vs_partials'] = sentinel
                            self.assertEqual(candidate['audit']['summary']['nonfinite']
                                             ['alias_nonfinite_render_rows'], [])
                            with self.assertRaisesRegex(gate.GateError, 'alias.*nonfinite.*sentinel'):
                                gate.validate_evidence(TEST_CONTRACT, candidate)


if __name__ == '__main__':
    unittest.main()
