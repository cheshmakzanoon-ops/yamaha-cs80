import pathlib,shutil,tempfile,unittest
from inspect_failed_parameter_roster import inspect,ROOT
class StoredEvidenceTests(unittest.TestCase):
 def test_lossless_historical_inventory(self):
  result=inspect(ROOT/'evidence'/'failed-parameter-roster');self.assertFalse(result['admitted']);m=result['runtime_observation'];self.assertEqual(m['observed_exit'],24);self.assertEqual(m['connected']['parameter_count'],2378);c=result['source_pattern_classification'];self.assertEqual(c['role_counts'],{'generated_workload_control':192,'source_memory_control':104,'pinned_juce_wrapper_bypass_pattern':1,'pinned_juce_wrapper_program_pattern':1,'pinned_juce_midi_emulation_pattern':2080});self.assertEqual(c['missing_generated'],[]);self.assertEqual(c['missing_source_memory'],[])
 def corrupt(self,name):
  with tempfile.TemporaryDirectory() as tmp:
   dest=pathlib.Path(tmp)/'evidence';shutil.copytree(ROOT/'evidence'/'failed-parameter-roster',dest);p=dest/name;p.write_bytes(p.read_bytes()+b'x')
   with self.assertRaises(ValueError):inspect(dest)
 def test_receipt_change(self):self.corrupt('receipt.bin.gz')
 def test_execution_change(self):self.corrupt('execution.json')
 def test_source_map_change(self):self.corrupt('source-roster.json')
 def test_report_change(self):self.corrupt('report.json')
if __name__=='__main__':unittest.main()
