import gzip,hashlib,json,pathlib,shutil,tempfile,unittest
from inspect_stock_history_diagnostic import inspect,ROOT
BASE=ROOT/'evidence'/'stock-history-diagnostic'
class StockHistoryEvidenceTests(unittest.TestCase):
 def test_complete_stock_history(self):
  report=inspect(BASE);self.assertFalse(report['production_qualified']);self.assertFalse(report['candidate_compared']);self.assertFalse(report['CPU_timing_claim']);self.assertFalse(report['raw_receipt_retained']);proof=report['source_history_proof'];self.assertEqual(proof['complete_parameter_count'],2378);self.assertEqual(proof['initial_write_count'],192);self.assertEqual(proof['alter_write_count'],2);self.assertEqual(proof['residual_write_count'],0);self.assertEqual(len(proof['observe_only_readback_sha256']),4)
 def copied(self):
  temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);root=pathlib.Path(temp.name)/'evidence';shutil.copytree(BASE,root);return root
 def corrupt(self,name):
  root=self.copied();p=root/name;p.write_bytes(p.read_bytes()+b'x')
  with self.assertRaises(ValueError):inspect(root)
 def rehash(self,root,name):
  p=root/'manifest.json';m=json.loads(p.read_text());b=(root/name).read_bytes();m['files'][name]={'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()};p.write_text(json.dumps(m))
 def test_payload_storage_changed(self):self.corrupt('payload.bin.gz')
 def test_contract_changed(self):self.corrupt('payload-contract.json')
 def test_execution_changed(self):self.corrupt('execution.json')
 def test_report_changed(self):self.corrupt('report.json')
 def test_expectations_changed(self):self.corrupt('source-expectations.txt')
 def test_sanitization_label_required(self):
  root=self.copied();p=root/'execution.json';e=json.loads(p.read_text());e['export_redaction']['kind']='raw';p.write_text(json.dumps(e));self.rehash(root,p.name)
  with self.assertRaisesRegex(ValueError,'sanitization'):inspect(root)
 def test_report_cannot_assert_different_readback(self):
  root=self.copied();p=root/'report.json';r=json.loads(p.read_text());r['observe_only_readback_sha256'][0]='0'*64;p.write_text(json.dumps(r));self.rehash(root,p.name)
  with self.assertRaisesRegex(ValueError,'decoded payload'):inspect(root)
 def test_no_private_workspace_paths(self):
  for p in BASE.iterdir():
   if not p.is_file():continue
   data=gzip.decompress(p.read_bytes()) if p.name.endswith('.gz') else p.read_bytes()
   for prefix in (b'/workspace/',b'/home/agent'):self.assertNotIn(prefix,data,p.name)
if __name__=='__main__':unittest.main()
