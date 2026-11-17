import unittest
from analysis import schedule,decision
class Tests(unittest.TestCase):
 def test_schedule(self):
  s=schedule();self.assertEqual(len(s),108);self.assertEqual(len({(x['phase'],x['round'],x['row'],x['role']) for x in s}),108)
  for i in range(0,108,2):self.assertEqual(s[i]['row'],s[i+1]['row']);self.assertEqual({s[i]['role'],s[i+1]['role']},{'A','B'})
 def test_gates(self):
  one=[[1.0]*6 for _ in range(3)];fast=[[.8]*6 for _ in range(3)]
  self.assertEqual(decision(one,one,fast)['decision'],'MEASURED_USEFUL_GAIN')
  bad=[r[:] for r in one];bad[0][0]=1.031
  self.assertEqual(decision(bad)['decision'],'STOP_NO_RETRY_AA')
  self.assertEqual(decision(one,bad)['decision'],'STOP_NO_RETRY_OFF_CONTROL')
  fast[0][4]=1.031;self.assertFalse(decision(one,one,fast)['broad_lower_cost_statement_allowed'])
  self.assertFalse(decision(one,one,[[.995]*6 for _ in range(3)])['material_10pct_target_met'])
if __name__=='__main__':unittest.main()
