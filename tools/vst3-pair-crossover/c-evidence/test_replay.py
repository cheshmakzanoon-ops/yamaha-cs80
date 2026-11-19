#!/usr/bin/env python3
"""Offline receipt checks, including failures that must not be rescued by a passing treatment."""
from copy import deepcopy
import itertools, unittest
import replay as r
class Evidence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.original=r.load()
    def test_exact_failed_decision(self):
        v=r.replay(self.original)
        self.assertEqual(v['compared_numbers'],320)
        self.assertLess(v['max_absolute_numerical_difference'],1e-12)
        self.assertEqual(v['decision']['S']['failures'],['idle:twins-equivalence'])
        self.assertEqual(v['decision']['C']['failures'],[])
        self.assertEqual((v['decision']['S']['passing'],v['decision']['C']['passing']),(34,19))
        self.assertFalse(v['outcome']['large_idle_win_gates_pass'])
    def test_complete_conjunction(self):
        for s,c,ten,broad in itertools.product((False,True),repeat=4):
            v=r.combined(s,c,ten,broad)
            self.assertEqual(v['large_idle_win_gates_pass'],s and c)
            self.assertEqual(v['broad_idle_win_with_all_gates'],s and c and broad)
            self.assertEqual(v['both_primary_above_10pct_with_all_gates'],s and c and ten)
    def rejected(self,change):
        d=deepcopy(self.original);change(d)
        with self.assertRaises(ValueError):r.replay(d)
    def test_cannot_rename_failure_success(self):
        self.rejected(lambda d:d['terminal'].__setitem__('large_idle_win_gates_pass',True))
    def test_no_round_away_boundary_failure(self):
        self.rejected(lambda d:d['original_decisions']['S']['gates'].__setitem__('idle:twins-equivalence',True))
    def test_cannot_claim_different_image(self):
        self.rejected(lambda d:d.__setitem__('fir_sha256',r.STOCK))
    def test_no_optional_cells(self):
        self.rejected(lambda d:d['cells']['S'].pop(0))
    def test_sentinel_uses_both_stock_lanes(self):
        self.rejected(lambda d:d['pairs'][0].__setitem__('indexes',[0,1]))
    def test_no_discarded_earlier_preflight(self):
        self.rejected(lambda d:d['admission'].__setitem__('earlier_rehearsals',0))
    def test_caps_not_adjustable(self):
        self.rejected(lambda d:d['stage_budgets'].__setitem__('slot_seconds_per_pair',101))
    def test_no_missing_extra_work(self):
        self.rejected(lambda d:d['cells']['C'][0]['real_thread_ns'].__setitem__('B_4_1',0))
    def test_keep_candidate_guard_units(self):
        self.rejected(lambda d:d['original_decisions']['C']['scenes']['wake_varied']['saving'].__setitem__('ci95_pct',[0,1]))
    def test_no_selective_role_order(self):
        self.rejected(lambda d:d['pairs'].reverse())
if __name__=='__main__':unittest.main()
