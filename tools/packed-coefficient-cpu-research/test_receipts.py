import csv,json,struct,tempfile,unittest
from pathlib import Path
from analysis import validate_receipt,expected_events,validate_events
from pcm_store import canonicalize,read_pcm

class Receipts(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.run=self.root/'run';self.run.mkdir();self.item={'arm':'stock','row':0}
  self.result={'phase':'timed','arm':'stock','row':0,'selector':0,'factor':2,'render_policy':1,'latency':67,'rate':48000,'block':256,'batch_kernel':True,'callbacks':11250,'frames':2880000,'warmup_callbacks':375,'warmup_frames':96000,'measured_noteons':0,'measured_noteoffs':0,'pressure_events':0,'cpu_ns':11250,'mxcsr_initial':0x9fc0,'mxcsr_final':0x9fe0,'finite':True}
  self.save();self.events(0)
  (self.run/'parameters.csv').write_bytes((Path(__file__).with_name('parameter-oracle')/'row-0.csv').read_bytes())
  (self.run/'callbacks.csv').write_text('callback,cpu_ns,output_fnv64,active_voices\n'+''.join(f'{k},1,0,8\n' for k in range(11250)))
  with (self.run/'audio.f32').open('wb') as f:f.truncate(23040000)
 def tearDown(self):self.tmp.cleanup()
 def save(self):(self.run/'result.json').write_text(json.dumps(self.result))
 def events(self,row):
  with (self.run/'events.csv').open('w',newline='') as f:
   rows=expected_events(row);w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 def test_valid_and_dedup(self):
  _,digest=validate_receipt(self.run,self.item);receipt=canonicalize(self.run,self.root);self.assertEqual(digest,receipt['sha256']);self.assertFalse((self.run/'audio.f32').exists());self.assertEqual(validate_receipt(self.run,self.item,root=self.root)[1],digest)
 def test_dry_receipt_zero_clock_required(self):
  self.result['phase']='dry';self.result['cpu_ns']=0;self.save()
  (self.run/'callbacks.csv').write_text('callback,cpu_ns,output_fnv64,active_voices\n'+''.join(f'{k},0,0,8\n' for k in range(11250)))
  validate_receipt(self.run,self.item,phase='dry')
  with self.assertRaises(ValueError):validate_receipt(self.run,self.item)
  self.result['cpu_ns']=1;self.save()
  with self.assertRaises(ValueError):validate_receipt(self.run,self.item,phase='dry')
 def test_metadata_mutations(self):
  for field,bad in [('render_policy',0),('selector',4),('latency',8),('rate',44100),('block',128),('factor',1),('warmup_frames',95999),('callbacks',11249),('mxcsr_initial',0x1f80),('mxcsr_final',0xbfc0),('measured_noteons',1),('cpu_ns',11249),('batch_kernel','true'),('finite',False)]:
   original=self.result[field];self.result[field]=bad;self.save()
   with self.assertRaises(ValueError,msg=field):validate_receipt(self.run,self.item)
   self.result[field]=original
  self.save()
 def test_legacy_metadata_and_parameter_oracle(self):
  self.item={'arm':'candidate','row':6};self.result.update(arm='candidate',row=6,selector=4,latency=63,render_policy=0);self.save();self.events(6)
  (self.run/'parameters.csv').write_bytes((Path(__file__).with_name('parameter-oracle')/'row-6.csv').read_bytes())
  validate_receipt(self.run,self.item)
  for key,bad in [('latency',67),('render_policy',1),('factor',1)]:
   original=self.result[key];self.result[key]=bad;self.save()
   with self.assertRaises(ValueError):validate_receipt(self.run,self.item)
   self.result[key]=original;self.save()
  (self.run/'parameters.csv').write_bytes((Path(__file__).with_name('parameter-oracle')/'row-1.csv').read_bytes())
  with self.assertRaises(ValueError):validate_receipt(self.run,self.item)
 def test_parameter_bit_or_id_mutation(self):
  p=self.run/'parameters.csv';original=p.read_text()
  for changed in (original.replace('1039516303','1039516304'),original.replace('master_tune','wrong_id'),original+original.splitlines()[1]+'\n'):
   p.write_text(changed)
   with self.assertRaises(ValueError):validate_receipt(self.run,self.item)
  p.write_text(original)
 def test_wrong_event_frames_and_extra_rows(self):
  p=self.run/'events.csv';text=p.read_text();p.write_text(text.replace('1,256,','1,257,',1))
  with self.assertRaises(ValueError):validate_events(p,0)
  p.write_text(text+text.splitlines()[1]+'\n')
  with self.assertRaises(ValueError):validate_events(p,0)
 def test_pressure_oracle(self):
  self.events(4);p=self.run/'events.csv';validate_events(p,4);rows=expected_events(4);self.assertEqual(rows[375]['pressure_bits'],str(0x3f800000));self.assertEqual(rows[750]['pressure_bits'],'0');text=p.read_text();p.write_text(text.replace(rows[1]['pressure_bits'],str(int(rows[1]['pressure_bits'])+1),1))
  with self.assertRaises(ValueError):validate_events(p,4)
 def test_release_oracle(self):
  rows=expected_events(5);self.assertEqual([int(x['callback']) for x in rows if x['noteons']=='8'],list(range(0,11250,1125)));self.assertEqual([int(x['callback']) for x in rows if x['noteoffs']=='8'],list(range(375,11250,1125)))
 def test_nonfinite_pcm(self):
  with (self.run/'audio.f32').open('r+b') as f:f.write(struct.pack('<f',float('nan')))
  with self.assertRaises(ValueError):validate_receipt(self.run,self.item)
 def test_callback_inventory(self):
  p=self.run/'callbacks.csv';p.write_text(p.read_text().replace('1,1,0,8','0,1,0,8',1))
  with self.assertRaises(ValueError):validate_receipt(self.run,self.item)
 def test_missing_pcm(self):
  (self.run/'audio.f32').unlink()
  with self.assertRaises(FileNotFoundError):validate_receipt(self.run,self.item)

class Store(unittest.TestCase):
 def test_duplicate_retains_one_verified_copy(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t)
   for name in ('a','b'):
    r=root/name;r.mkdir();(r/'audio.f32').write_bytes(b'12345678');canonicalize(r,root,expected_bytes=8);self.assertEqual(read_pcm(r,root),b'12345678')
   self.assertEqual(len(list((root/'canonical-pcm').glob('*'))),1)
 def test_corruption_preserves_raw(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);a=root/'a';a.mkdir();(a/'audio.f32').write_bytes(b'12345678');ref=canonicalize(a,root,expected_bytes=8);(root/ref['relative_path']).write_bytes(b'87654321');b=root/'b';b.mkdir();(b/'audio.f32').write_bytes(b'12345678')
   with self.assertRaises(ValueError):canonicalize(b,root,expected_bytes=8)
   self.assertTrue((b/'audio.f32').exists())
   with self.assertRaises(ValueError):read_pcm(a,root)
 def test_unique_limit_keeps_failed_unique(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t)
   for k in range(8):
    r=root/str(k);r.mkdir();(r/'audio.f32').write_bytes(bytes([k])*8)
    if k<7:canonicalize(r,root,expected_bytes=8)
    else:
     with self.assertRaises(ValueError):canonicalize(r,root,expected_bytes=8)
     self.assertTrue((r/'audio.f32').exists())
 def test_reference_path_and_missing_canonical(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);a=root/'a';a.mkdir();(a/'audio.f32').write_bytes(b'12345678');ref=canonicalize(a,root,expected_bytes=8);(root/ref['relative_path']).unlink()
   with self.assertRaises(FileNotFoundError):read_pcm(a,root)
   ref['relative_path']='../escape';(a/'audio-reference.json').write_text(json.dumps(ref))
   with self.assertRaises(ValueError):read_pcm(a,root)
 def test_symlink_rejected_without_copy(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);(root/'external').write_bytes(b'12345678');r=root/'run';r.mkdir();(r/'audio.f32').symlink_to(root/'external')
   with self.assertRaises(ValueError):canonicalize(r,root,expected_bytes=8)
   self.assertEqual((root/'external').read_bytes(),b'12345678')
if __name__=='__main__':unittest.main()
