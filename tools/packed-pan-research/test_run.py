import copy,csv,importlib.util,json,pathlib,tempfile,unittest
spec=importlib.util.spec_from_file_location('study',pathlib.Path(__file__).with_name('run.py'))
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
def rows(scale=1):
 return {('48000','128',g,'pair','0','warmed_held'):{'mode':g,'phase':'warmed_held','callback_cpu_seconds':str(scale),'audio_seconds':'2','checksum':'7'} for g in ('source_performance','isolation','legacy')}
class Tests(unittest.TestCase):
 def test_calibration_pass(self):self.assertTrue(r.calibration([(rows(),rows())]*3)[0])
 def test_one_failed_pair_zero_candidates(self):
  calls=[]
  def run(name,arm):calls.append((name,arm));return rows(1.3 if name=='cal-1-b' else 1)
  d=r.cost_phase(run);self.assertEqual(d['decision'],'TERMINAL_UNUSABLE_CALIBRATION');self.assertEqual(d['candidate_cost_executions'],0);self.assertEqual(len(calls),6);self.assertTrue(all(a=='baseline' for _,a in calls))
 def test_candidate_order_and_pass(self):
  calls=[]
  def run(name,arm):calls.append((name,arm));return rows(.95 if arm=='candidate' else 1)
  d=r.cost_phase(run);self.assertEqual(d['decision'],'SCOPED_COST_PASS');self.assertEqual([a for _,a in calls[6:]],['baseline','candidate','candidate','baseline','baseline','candidate'])
 def test_no_material_gain(self):self.assertEqual(r.assess([(rows(),rows(.99))]*3)['decision'],'REJECT_MATERIAL_GAIN_OR_REGRESSION_GATE')
 def test_output_mismatch_stops(self):
  x=rows();next(iter(x.values()))['checksum']='8'
  with self.assertRaises(ValueError):r.exact(rows(),x)
 def test_individual_row_guard(self):
  x=rows();list(x.values())[2]['callback_cpu_seconds']='1.2'
  self.assertFalse(r.calibration([(rows(),x)]*3)[0])
 def test_regression_rejected(self):
  x=rows(.95);list(x.values())[1]['callback_cpu_seconds']='1.06'
  self.assertEqual(r.assess([(rows(),x)]*3)['decision'],'REJECT_MATERIAL_GAIN_OR_REGRESSION_GATE')
 def test_missing_rows_rejected(self):
  x=rows();x.pop(next(iter(x)))
  with self.assertRaises(ValueError):r.exact(rows(),x)
 def test_qualification_anchor_closed(self):
  with self.assertRaises(ValueError):r.qualification_source('not source')
 def fixture(self,p,field=None,value=None):
  (p/'completed.json').write_text(json.dumps({'completed':True,'cases':15,'rows':36}))
  rows=[{'rate':'48000','block':'128','mode':'source_performance','scenario':'pair','initial_selector':str(i),'phase':'warmed_held','nonfinite_count':'0','callback_cpu_status':'available','callback_cpu_call_count':'512','callbacks':'512','callback_cpu_seconds':'3','audio_seconds':'2','callback_cpu_rtf':'1.5'} for i in range(36)]
  if field:rows[0][field]=value
  with (p/'results.csv').open('w') as stream:
   writer=csv.DictWriter(stream,fieldnames=rows[0].keys());writer.writeheader();writer.writerows(rows)
 def test_valid_clock_fixture(self):
  with tempfile.TemporaryDirectory() as t:
   p=pathlib.Path(t);self.fixture(p);self.assertEqual(len(r.read_rows(p,36,True)),36)
 def test_invalid_clock_fields(self):
  cases=[('callback_cpu_seconds',v) for v in ('nan','inf','0','-1')]+[('audio_seconds',v) for v in ('nan','inf','0','-1')]+[('callback_cpu_rtf','nan'),('callback_cpu_rtf','2'),('callback_cpu_call_count','511'),('callback_cpu_status','unavailable'),('nonfinite_count','1'),('rate','44100'),('block','256')]
  for field,value in cases:
   with self.subTest(field=field,value=value),tempfile.TemporaryDirectory() as t:
    p=pathlib.Path(t);self.fixture(p,field,value)
    with self.assertRaises(ValueError):r.read_rows(p,36,True)
if __name__=='__main__':unittest.main()
