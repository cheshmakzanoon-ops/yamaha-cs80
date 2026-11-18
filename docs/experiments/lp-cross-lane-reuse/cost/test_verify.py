from pathlib import Path
import copy,sys,unittest
sys.dont_write_bytecode=True
import verify
class ReplayTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.data=verify.read(Path(__file__).resolve().parent)
 def trial(self,change):
  rows,res,receipt,_=copy.deepcopy(self.data)
  change(rows,res,receipt)
  with self.assertRaises(ValueError):verify.verify(rows,res,receipt)
 def test_true(self): verify.verify(*self.data)
 def test_prefix_rejected(self):self.trial(lambda rows,res,receipt:rows.pop())
 def test_swap_arms(self):self.trial(lambda rows,res,receipt:rows[-2].update(arm='wrong'))
 def test_identity(self):self.trial(lambda rows,res,receipt:rows[1]['identity'].update(continuation_pcm='bad'))
 def test_cpu_moved(self):self.trial(lambda rows,res,receipt:rows[1].update(cpu_start=9999))
 def test_endpoint_switch(self):self.trial(lambda rows,res,receipt:receipt.update(primary='factory-held'))
 def test_false_gain(self):self.trial(lambda rows,res,receipt:res['cases']['factory-moving'].update(gain=.5))
if __name__=='__main__':unittest.main()
