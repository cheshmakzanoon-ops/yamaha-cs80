#!/usr/bin/env python3
"""No clocks, native workers or third-party packages; adversarial checks against the published aggregates."""
import json
from pathlib import Path
import tempfile
import unittest

import export
import replay

class EvidenceTests(unittest.TestCase):
    def test_all_six_failures(self):
        r=replay.replay(replay.load())
        self.assertEqual((r['gates_passed'],r['gates'],set(r['failed']),r['candidate_runs'],r['candidate_authorized']),(29,35,replay.F,0,False))
        self.assertEqual(r['numeric_values_checked'],195)
        raw=r['scenes']['idle']['worst_empty_fraction']
        self.assertEqual((raw['slot'],raw['epoch'],raw['cpu']),(20,5,2))
        self.assertAlmostEqual(100*raw['value'],3.532873515090584)
        self.assertAlmostEqual(r['scenes']['release_varied']['cpu_0_2_4_6_pct'][1],-9.922878537692458)
    def reject(self, mutate):
        data=replay.load();mutate(data)
        with self.assertRaises(ValueError):replay.replay(data)
    def test_cutoff_is_not_original(self):
        self.reject(lambda v:v['original'].update(elf_sha256='0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2'))
    def test_same_bytes_not_sufficient_for_physical_twin(self):
        self.reject(lambda v:v['slots'][0]['actual_object_tags'].__setitem__(1,'0'*64))
    def test_anticherry_pick(self):
        self.reject(lambda v:v['cells'].pop(10))
        self.reject(lambda v:v['cells'].reverse())
        self.reject(lambda v:v['slots'][4].update(epoch_start_ns=v['slots'][0]['epoch_start_ns']))
    def test_cannot_switch_d_on_or_gate_green(self):
        self.reject(lambda v:v['terminal'].update(candidate_stage_authorized=True))
        self.reject(lambda v:v['original_decision']['gates'].update({'release_varied:cpu-time-strata':True}))
    def test_cannot_suppress_cost_spike_or_weaken_criterion(self):
        self.reject(lambda v:[r.update(empty_ns_sum=0) for r in v['cells'] if r['scene']=='idle'])
        self.reject(lambda v:v['sealed_study_endpoints'].update(empty_fraction_below=.04))
    def test_all_arms_and_old_estimates_verified(self):
        self.reject(lambda v:v['cells'][0]['thread_ns'].update(B_1_0=123456))
        self.reject(lambda v:v['original_decision']['scenes']['release_varied']['extra_work'].update(point_pct=11.))
    def test_raw_crossing_and_extra_rows_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'piece.csv';header='index,cpu0,cpu1,wall0,wall1\n'
            p.write_text(header+'0,5,8,10,12\n1,9,10,13,15\n')
            self.assertEqual(export.endpoints(p,2,8,True),[[0,5,8,2,4],[1,9,10,5,7]])
            for rows in ('0,5,8,10,12\n1,9,10,11,15\n','0,5,8,10,12\n0,9,10,13,15\n','0,5,8,10,12\n'):
                p.write_text(header+rows)
                with self.assertRaises(ValueError):export.endpoints(p,2,8,True)
    def test_portable_bytes_and_no_machine_paths(self):
        data=replay.load();self.assertEqual(export.compact(data),replay.FILE.read_bytes())
        raw=json.dumps(data);self.assertFalse(any(x in raw for x in ('/workspace/','/home/','/tmp/','file://','\\Users\\')))

if __name__=='__main__':unittest.main()
