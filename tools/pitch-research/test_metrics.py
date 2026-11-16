import unittest,tempfile,pathlib,csv,json
from run import expected,BASE,CANDIDATE,PATCH,sha,HERE,Run
from unittest.mock import Mock,patch,call
from metrics import analyze,read,check
class Metrics(unittest.TestCase):
 def setUp(self):self.t=tempfile.TemporaryDirectory();self.p=pathlib.Path(self.t.name)
 def tearDown(self):self.t.cleanup()
 def fixture(self,stage,count,ratio=1.,change=None):
  low='lowload' in stage;keys=sorted(expected(low))
  for i in range(1,count+1):
   for role in ['baseline','candidate']:
    d=self.p/f'{stage}-{i}-{role}';d.mkdir();rows=[]
    for k in keys:
     r=dict(zip(['mode','scenario','initial_selector','phase'],k));value=ratio if role=='candidate' else 1.
     r.update(rate='48000',block='128',checksum='123',callbacks='256',audio_seconds='1',callback_cpu_call_count='256',nonfinite_count='0',callback_cpu_status='available',callback_cpu_seconds=str(value),callback_cpu_rtf=str(value))
     if change:change(i,role,k,r)
     rows.append(r)
    with (d/'results.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=rows[0].keys());w.writeheader();w.writerows(rows)
    (d/'completed.json').write_text(json.dumps(dict(completed=True,cases=11 if low else 15,rows=len(rows))))
 def test_patch_and_inventory(self):self.assertEqual(sha(HERE/'candidate.patch'),PATCH);self.assertEqual(len(expected(False)),36);self.assertEqual(len(expected(True)),19)
 def test_calibration_boundary_and_failure(self):
  self.fixture('calibration',4,1.02);self.assertTrue(analyze(self.p,'calibration',4)['usable'])
  self.fixture('calibration-lowload',4,1.021);self.assertFalse(analyze(self.p,'calibration-lowload',4)['usable'])
 def test_material_and_lowload(self):
  self.fixture('calibration',4);cal=analyze(self.p,'calibration',4)
  self.fixture('performance',6,.97);self.assertTrue(analyze(self.p,'performance',6,cal)['material_signal'])
  self.fixture('lowload',4,1.11);a=analyze(self.p,'lowload',4,cal);self.assertTrue(a['hard_losses']);self.assertEqual(a['subset_definition'],'four isolation nonidle held')
 def test_small_gain_not_material(self):
  self.fixture('performance',6,.99);self.assertFalse(analyze(self.p,'performance',6,{'usable':True,'empirical_aggregate_deviation':0})['material_signal'])
 def test_cross_run_corruption(self):
  self.fixture('calibration',4,change=lambda i,role,k,r:r.update(checksum='999') if i==2 else None)
  with self.assertRaises(AssertionError):analyze(self.p,'calibration',4)
 def test_nonfinite_clock(self):
  self.fixture('calibration',4,change=lambda i,role,k,r:r.update(callback_cpu_seconds='nan'))
  with self.assertRaises(AssertionError):analyze(self.p,'calibration',4)
class Flow(unittest.TestCase):
 def instance(self):
  r=Run.__new__(Run);r.out=pathlib.Path('/unused');r.prepare=Mock();r.suite=Mock();r.record=Mock();return r
 def test_active_calibration_stops(self):
  r=self.instance()
  with patch('run.analyze',return_value={'usable':False}):r.execute()
  self.assertEqual(r.suite.call_args_list,[call('calibration',4,cal=True)])
  self.assertFalse(r.record.call_args.args[1]['candidate_timed'])
 def test_lowload_calibration_stops(self):
  r=self.instance()
  with patch('run.analyze',side_effect=[{'usable':True},{'usable':False}]):r.execute()
  self.assertEqual(r.suite.call_args_list,[call('calibration',4,cal=True),call('calibration-lowload',4,low=True,cal=True)])
  self.assertFalse(r.record.call_args.args[1]['candidate_timed'])
 def test_fixed_candidate_sequence(self):
  r=self.instance()
  with patch('run.analyze',side_effect=[{'usable':True},{'usable':True},{'material_signal':False},{'hard_losses':[],'cautions':[]}]):r.execute()
  self.assertEqual(r.suite.call_args_list,[call('calibration',4,cal=True),call('calibration-lowload',4,low=True,cal=True),call('performance',6),call('lowload',4,low=True)])
  self.assertTrue(r.record.call_args.args[1]['candidate_timed'])
if __name__=='__main__':unittest.main()
