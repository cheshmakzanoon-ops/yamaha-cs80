import csv
import itertools
import json
from collections import Counter
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import candidate_pair as cp
import crossover as d
import phase_c as c
import phase_q as q
import preflight as dry

class CStudy(unittest.TestCase):
    def test_counterbalanced_role_launch_and_live_sentinel(self):
        combos=Counter();pairs=Counter()
        for e,j in itertools.product(range(12),range(4)):
            ori=d.orientation(e,j);a=ori["a_lane"]
            self.assertEqual(cp.mapping(e,j,"S"),[0,0])
            m=cp.mapping(e,j,"C")
            self.assertEqual((m[a],m[1-a]),(0,1))
            combos[m.index(1),ori["launch_first"]]+=1;pairs[cp.kind_order(e,j)]+=1
        self.assertEqual(combos,Counter({(0,0):12,(0,1):12,(1,0):12,(1,1):12}))
        self.assertEqual(pairs,Counter({("S","C"):24,("C","S"):24}))
        with self.assertRaises(ValueError):cp.mapping(0,0,"Q")
    def test_live_controls_and_candidate_jointly_required(self):
        for live,treatment,double,broad in itertools.product((False,True),repeat=4):
            out=c.release_summary({"numerical_gates_pass":live},{"numerical_gates_pass":treatment,
                "both_primary_lower_bounds_above_10":double,"varied_release_lower_bound_above_5":broad})
            self.assertEqual(out["large_idle_win_gates_pass"],live and treatment)
            self.assertEqual(out["both_primary_above_10pct_with_all_gates"],live and treatment and double)
            self.assertEqual(out["broad_idle_win_with_all_gates"],live and treatment and broad)
    def test_FIR_identity_rejects_substitute_or_alias(self):
        A=[1,2,3,d.SHIPPING_CUTOFF];B=[1,8,3,cp.FIR]
        with mock.patch.object(dry,"identity",side_effect=[A,B]):
            self.assertEqual(cp.bind("stock","fir"),[A,B])
        for b in ([1,2,3,cp.FIR],[1,8,3,d.SHIPPING_CUTOFF],[1,8,3,"f"*64]):
            with mock.patch.object(dry,"identity",side_effect=[A,b]),self.assertRaises(ValueError):cp.bind("stock","fir")
    def test_output_observation_domains(self):
        x={"role":"Q","a_sha256":d.SHIPPING_CUTOFF,"b_sha256":d.SHIPPING_CUTOFF}
        self.assertEqual(c.annotate(x.copy(),"S")["b_sha256"],d.SHIPPING_CUTOFF)
        self.assertEqual(c.annotate(x.copy(),"C")["b_sha256"],cp.FIR)
        with self.assertRaises(ValueError):c.annotate(x.copy(),"Q")
        with self.assertRaises(ValueError):c.annotate({**x,"a_sha256":cp.FIR},"C")
    def test_Q_truth_flag_alone_not_admission(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/"Q-run").mkdir();(root/"seal.json").write_text('{"role":"Q"}')
            (root/"Q-run/terminal.json").write_text('{"complete":true,"numerical_gates_pass":true}')
            with self.assertRaisesRegex(ValueError,"Q seal"):cp.q_gate(root)
    def test_C_requires_different_action_time_seal(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/"C-seal.json").write_text("{}")
            with self.assertRaisesRegex(ValueError,"C seal"):c.inputs(root,cp.Q_SEAL)
    def test_missing_live_pair_or_forged_journal_invalidates_whole_stage(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/"journal.jsonl").write_text('{"event":"start","timed_stage_origin_ns":0}\n')
            with self.assertRaisesRegex(ValueError,"C journal"):c.assemble(root,{},0)
    def test_C_unexpected_physical_sentinel_fails_before_discard(self):
        e=j=0;slot={"epoch":e,"cpu_cell":j}
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);images=[[1,2,3,d.SHIPPING_CUTOFF],[1,9,3,cp.FIR]]
            cap=root/"peer-0/capture";cap.mkdir(parents=True)
            wrong={"lane":0,"cpu":0,"callback_clock_reads":11520,"q_empty_clock_reads":2560,"dev":1,"ino":9,"size":3}
            (cap/"identity.json").write_text(json.dumps(wrong));(cap/"proof").write_text("retain")
            ex={"a_lane":0,"launch_first":0,"pair":"S","image_indexes_by_lane":[0,0]}
            with self.assertRaisesRegex(ValueError,"module/clock"):
                c.compact(root,"S",slot,{"image_identities":images},{},root,0,ex,0)
            self.assertTrue((cap/"proof").exists())
    def test_C_wrong_launch_orientation_rejected(self):
        e,j=0,0
        with self.assertRaisesRegex(ValueError,"mapping"):
            c.compact(Path("/not-opened"),"C",{"epoch":e,"cpu_cell":j},{},{},None,0,
                {"image_indexes_by_lane":[1,0],"a_lane":0,"launch_first":0,"pair":"C"},0)

if __name__=="__main__":unittest.main()
