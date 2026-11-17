import unittest
from protocol_math import schedule,decision
class Test(unittest.TestCase):
 def matrix(self,x=1):return [[x]*7 for _ in range(3)]
 def test_schedule(self):
  s=schedule();self.assertEqual(len(s),84)
  for i in range(0,84,2):
   a,b=s[i:i+2];self.assertEqual((a['phase'],a['round'],a['row']),(b['phase'],b['round'],b['row']));self.assertEqual({a['role'],b['role']},{'A','B'})
  self.assertEqual([x['row'] for x in s[14:28:2]],list(reversed(range(7))))
 def test_aa_terminal(self):
  m=self.matrix();m[0][6]=1.031;self.assertEqual(decision(m)['decision'],'STOP_NO_RETRY_AA')
 def test_useful(self):
  m=self.matrix(.98)
  for r in m:r[6]=1
  self.assertEqual(decision(self.matrix(),m)['decision'],'MEASURED_USEFUL_GAIN')
 def test_legacy_hard(self):
  m=self.matrix(.98);m[2][6]=.96;self.assertEqual(decision(self.matrix(),m)['decision'],'STOP_NO_RETRY_LEGACY_CONTROL')
 def test_secondary_blocks_broad(self):
  m=self.matrix(.98)
  for r in m:r[6]=1
  m[0][5]=1.04;r=decision(self.matrix(),m);self.assertEqual(r['decision'],'MEASURED_USEFUL_GAIN');self.assertFalse(r['broad_lower_cost_statement_allowed'])
 def test_invalid(self):
  for x in [0,float('nan'),float('inf'),-1]:
   with self.assertRaises(ValueError):decision(self.matrix(x))
 def test_small_gain(self):self.assertEqual(decision(self.matrix(),self.matrix(.998))['decision'],'NO_RESOLVED_USEFUL_GAIN')
 def test_primary_aa_fails_inside_row_bounds(self):
  self.assertEqual(decision(self.matrix(1.02))['decision'],'STOP_NO_RETRY_AA')
 def test_noise_scaled_minimum(self):
  aa=self.matrix(1.01);c=self.matrix(.985)
  for r in c:r[6]=1
  d=decision(aa,c);self.assertEqual(d['decision'],'NO_RESOLVED_USEFUL_GAIN');self.assertAlmostEqual(d['minimum_gain'],.02)
 def test_each_primary_round_must_improve(self):
  c=self.matrix(.98);c[1][:4]=[1]*4
  for r in c:r[6]=1
  self.assertEqual(decision(self.matrix(),c)['decision'],'NO_RESOLVED_USEFUL_GAIN')
 def test_inclusive_row_bounds(self):
  aa=self.matrix();aa[0][4]=.97;aa[1][5]=1.03
  self.assertEqual(decision(aa)['decision'],'AA_PASS_ONLY')
  c=self.matrix(.98)
  for r in c:r[6]=.97
  c[0][5]=1.03
  self.assertTrue(decision(aa,c)['broad_lower_cost_statement_allowed'])
 def test_gain_threshold_adjacent(self):
  for ratio,expected in [(.995000001,'NO_RESOLVED_USEFUL_GAIN'),(.994999999,'MEASURED_USEFUL_GAIN')]:
   c=self.matrix(ratio)
   for r in c:r[6]=1
   self.assertEqual(decision(self.matrix(),c)['decision'],expected)
if __name__=='__main__':unittest.main()
