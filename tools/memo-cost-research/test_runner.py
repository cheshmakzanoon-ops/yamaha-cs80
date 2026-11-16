import unittest,tempfile,pathlib,json,csv
from unittest.mock import Mock,patch,call
import run
from metrics import inventory,ORDERS,calibration,analyze,loss,read
class Gates(unittest.TestCase):
 def runner(self):
  r=run.Runner.__new__(run.Runner);r.out=pathlib.Path('/unused');r.tooling_hashes={};r.qualify_binding=Mock();r.prepare=Mock();r.check_tooling=Mock();r.calibrate=Mock();r.triplets=Mock();r.save=Mock();return r
 def test_empty_allowlist_blocks_prepare(self):
  r=self.runner();r.qualify_binding=run.Runner.qualify_binding.__get__(r);r.repo=pathlib.Path('/unused')
  with patch.dict(run.os.environ,{},clear=True),patch('run.json.loads',return_value=[]),self.assertRaisesRegex(AssertionError,'EMPTY ALLOWLIST'):r.execute()
  r.prepare.assert_not_called();r.calibrate.assert_not_called()
 def test_interposition_stops(self):
  r=self.runner();r.qualify_binding=run.Runner.qualify_binding.__get__(r)
  with patch.dict(run.os.environ,{'LD_PRELOAD':'bad.so'},clear=True),self.assertRaisesRegex(AssertionError,'LD_PRELOAD'):r.execute()
  r.prepare.assert_not_called()
 def test_fingerprint_mismatch(self):
  x={'candidate':run.CANDIDATE,'reviewed':True,'review_receipt_sha256':'a'*64,'inspection_tool_hashes':{'a':'b'},'fingerprint':{'a':1}}
  self.assertEqual(run.approve([x],{'a':1},{'a':'b'}),x)
  for entries,fp,tools in [([],{'a':1},{'a':'b'}),([x],{'a':2},{'a':'b'}),([x],{'a':1},{'a':'changed'})]:
   with self.assertRaises(AssertionError):run.approve(entries,fp,tools)
 def test_no_prepare_before_approval(self):
  r=self.runner();r.binding_approved=False
  with self.assertRaisesRegex(AssertionError,'no memoON'):run.Runner.prepare(r)
 def test_active_calibration_stop(self):
  r=self.runner();r.calibrate.return_value={'usable':False};r.execute();self.assertEqual(r.calibrate.call_args_list,[call(False)]);r.triplets.assert_not_called()
 def test_low_calibration_stop(self):
  r=self.runner();r.calibrate.side_effect=[{'usable':True},{'usable':False}];r.execute();self.assertEqual(r.calibrate.call_args_list,[call(False),call(True)]);r.triplets.assert_not_called()
 def test_fixed_sequence_even_no_signal(self):
  r=self.runner();r.calibrate.side_effect=[{'usable':True,'tag':'active'},{'usable':True,'tag':'low'}];r.triplets.side_effect=[{'material_signal':False},{'hard_losses':[],'cautions':[]}];r.execute();self.assertEqual(r.triplets.call_args_list,[call(False,{'usable':True,'tag':'active'}),call(True,{'usable':True,'tag':'low'})])
 def test_all_triplet_orders(self):
  r=self.runner();r.run_one=Mock()
  with patch('run.analyze',return_value={}):
   run.Runner.triplets(r,False,{});run.Runner.triplets(r,True,{})
  expected=[call(suite,i,arm,arm,low) for suite,low in [('active',False),('lowload',True)] for i,order in enumerate(ORDERS,1) for arm in order]
  self.assertEqual(r.run_one.call_args_list,expected);self.assertEqual(len(expected),36)
 def test_all_calibration_orders(self):
  r=self.runner();r.run_one=Mock()
  with patch('run.calibration',return_value={}):
   run.Runner.calibrate(r,False);run.Runner.calibrate(r,True)
  expected=[call('calibration-'+s,i,a,'A',low) for s,low in [('active',False),('lowload',True)] for i in range(1,5) for a in (('left','right') if i%2 else ('right','left'))]
  self.assertEqual(r.run_one.call_args_list,expected);self.assertEqual(len(expected),16)
class Metrics(unittest.TestCase):
 def setUp(self):self.t=tempfile.TemporaryDirectory();self.p=pathlib.Path(self.t.name)
 def tearDown(self):self.t.cleanup()
 def write(self,name,low,fn):
  p=self.p/name;p.mkdir();rows=[]
  for k in sorted(inventory(low)):
   x=dict(zip(('mode','scenario','initial_selector','phase'),k));v=fn(k);x.update(rate='48000',block='128',checksum='7',callbacks=str(512 if low else 256),audio_seconds='1',callback_cpu_call_count=str(512 if low else 256),nonfinite_count='0',callback_cpu_status='available',callback_cpu_seconds=str(v),callback_cpu_rtf=str(v));rows.append(x)
  with (p/'results.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
  (p/'completed.json').write_text(json.dumps(dict(completed=True,cases=11 if low else 15,rows=len(rows))))
 def test_primary_target_and_default_off_loss(self):
  for low in (False,True):
   suite='lowload' if low else 'active'
   for i in range(1,5):
    for a in ('left','right'):self.write(f'calibration-{suite}-{i}-{a}',low,lambda k:1)
   cal=calibration(self.p,low);self.assertTrue(cal['usable'])
   for i in range(1,7):
    for arm in 'ABC':self.write(f'{suite}-{i}-{arm}',low,lambda k,arm=arm: .97 if arm=='C' and k[0] in ('source_performance','isolation') and k[1]=='paired_tones' and k[3]=='warmed_held' else 1)
   a=analyze(self.p,low,cal)
   if not low:self.assertTrue(a['material_signal'])
   self.assertFalse(a['hard_losses'])
  # Rewrite one B file to corrupt workload, prove reference check catches it.
  p=self.p/'active-1-B/results.csv';s=p.read_text();p.write_text(s.replace(',7,',',9,'))
  with self.assertRaises(AssertionError):analyze(self.p,False,calibration(self.p,False))
 def test_default_off_regression_blocks(self):
  for i in range(1,7):
   for arm in 'ABC':self.write(f'active-{i}-{arm}',False,lambda k,arm=arm:1.06 if arm=='B' else .97 if arm=='C' else 1)
  cal={'usable':True,'E':{'total':0,'source_performance_dry_held':0,'isolation_dry_held':0}}
  a=analyze(self.p,False,cal);self.assertFalse(a['material_signal']);self.assertIn(['B/A','total'],a['hard_losses'])
 def test_inconsistent_large_effect_caution(self):self.assertEqual(loss({'median':1.06,'ratios':[1.06]*4+[.5]*2}),'caution')
 def test_inventory_missing(self):
  self.write('x',False,lambda k:1);p=self.p/'x/results.csv';lines=p.read_text().splitlines();p.write_text('\n'.join(lines[:-1])+'\n')
  with self.assertRaises(AssertionError):read(self.p/'x',False)
if __name__=='__main__':unittest.main()
