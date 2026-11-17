import copy,gzip,hashlib,json,pathlib,shutil,tempfile,unittest
from inspect_paired_plugin_timing import ROOT,load,validate
BASE=ROOT/'evidence'/'paired-plugin-timing'
class TimingExportTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.original,cls.manifest=load(BASE)
 def test_all_arrays_blocks_and_negative_result(self):
  report=validate(self.original);self.assertEqual(report['run_count'],96);self.assertEqual(report['callback_cpu_samples'],909312);self.assertEqual(report['empty_clock_samples'],73728);self.assertFalse(report['credible_gain_demonstrated']);self.assertFalse(report['candidate_promoted']);self.assertFalse(report['native_execution_independently_reproduced'])
 def changed(self,edit):
  data=copy.deepcopy(self.original);edit(data)
  with self.assertRaises((ValueError,KeyError)):validate(data)
 def first(self,data):return data['records'][data['runs'][0]['decoded_record']]['data']
 def test_callback_sample_corruption(self):self.changed(lambda d:self.first(d)['timing_segments'][5]['callback_cpu_ns'].__setitem__(0,1))
 def test_dropped_callback(self):self.changed(lambda d:self.first(d)['timing_segments'][5]['callback_cpu_ns'].pop())
 def test_omitted_clock_pair(self):self.changed(lambda d:self.first(d)['timing_segments'][0]['clock_pair_ns'].pop())
 def test_schedule_arm_changed(self):self.changed(lambda d:self.first(d).__setitem__('arm','candidate'))
 def test_pcm_hash_mismatch(self):self.changed(lambda d:self.first(d)['timing_segments'][5].__setitem__('pcm_sha256','0'*64))
 def test_unsupported_gain_claim(self):self.changed(lambda d:d['records']['campaign-v2/summary.json']['data']['0'].__setitem__('gain_claim_supported',True))
 def test_v1_cause_not_retroactively_proven(self):self.changed(lambda d:d['records']['attempt-history.json']['data']['attempts'][0].__setitem__('actual_runloop_failure_enum','Receipt13'))
 def test_duplicate_block_rejected(self):self.changed(lambda d:d['blocks'].__setitem__(-1,copy.deepcopy(d['blocks'][-4])))
 def test_private_path_rejected_even_if_storage_rehashed(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=pathlib.Path(tmp)/'evidence';shutil.copytree(BASE,root);data=copy.deepcopy(self.original);data['private_path']='/workspace/private';raw=json.dumps(data,separators=(',',':')).encode();packed=gzip.compress(raw,mtime=0);(root/'campaign.json.gz').write_bytes(packed);m=json.loads((root/'manifest.json').read_text());m.update(compressed_bytes=len(packed),compressed_sha256=hashlib.sha256(packed).hexdigest(),uncompressed_bytes=len(raw),uncompressed_sha256=hashlib.sha256(raw).hexdigest());(root/'manifest.json').write_text(json.dumps(m))
   with self.assertRaisesRegex(ValueError,'private path'):load(root)
 def test_export_storage_corruption(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=pathlib.Path(tmp)/'evidence';shutil.copytree(BASE,root);p=root/'campaign.json.gz';p.write_bytes(p.read_bytes()+b'x')
   with self.assertRaisesRegex(ValueError,'compressed integrity'):load(root)
 def test_compact_export_preserves_every_original_record_hash(self):
  for record in self.original['records'].values():self.assertEqual(len(record['original_sha256']),64);self.assertGreaterEqual(record['original_bytes'],0);self.assertEqual(record['export_kind'],'sanitized-portable-export')
  self.assertFalse(self.original['scope']['raw_receipts_exported']);self.assertLess(self.manifest['compressed_bytes'],8*1024*1024)
if __name__=='__main__':unittest.main()
