"""Pure source-schema classifier controls. No native module is loaded."""
import csv,hashlib,io,json,os,pathlib,shutil,subprocess,tempfile,unittest
from inspect_failed_parameter_roster import inspect
ROOT=pathlib.Path(__file__).parent
class ParameterClassifierTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  sdk=os.environ.get('CS80_OBSERVER_SDK')
  if not sdk:raise unittest.SkipTest('explicit pinned SDK required')
  for name,wanted in json.loads((ROOT/'native_observer_sdk_provenance.json').read_text())['headers'].items():
   if hashlib.sha256((pathlib.Path(sdk)/name).read_bytes()).hexdigest()!=wanted:raise RuntimeError('SDK drift '+name)
  for name,wanted in json.loads((ROOT/'native_observer_parameter_classifier_provenance.json').read_text())['sources'].items():
   if hashlib.sha256((ROOT/name).read_bytes()).hexdigest()!=wanted:raise RuntimeError('source drift '+name)
  cls.tmp=tempfile.TemporaryDirectory();cls.exe=pathlib.Path(cls.tmp.name)/'classifier'
  cls.compiler=shutil.which('g++');cls.src=ROOT.parents[1]/'src'
  subprocess.run([cls.compiler,'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-I',sdk,'-I',str(cls.src),str(ROOT/'native_observer_parameter_classifier_fixture.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,check=True,timeout=40)
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def case(self,name):
  p=subprocess.run([str(self.exe),name],capture_output=True,text=True,timeout=5);self.assertEqual(p.returncode,0,p.stderr)
 def test_published_rejected_capture_matches_all_source_fields(self):
  # Regression evidence ONLY. Expected values originate in C++ source, never
  # copied from this rejected diagnostic; matching cannot admit any execution.
  observed=inspect(ROOT/'evidence'/'failed-parameter-roster')['runtime_observation']['connected']['parameters'];by_id={r['id']:r for r in observed}
  p=subprocess.run([str(self.exe),'export'],capture_output=True,text=True,check=True,timeout=5);rows=list(csv.reader(io.StringIO(p.stdout),delimiter='\t'))
  self.assertEqual(len(rows),2378);self.assertEqual(len(by_id),2378);counts=[0]*5
  for r in rows:
   q=by_id[int(r[0])];role=int(r[1]);counts[role]+=1
   actual=[q['title']['text'],q['short_title']['text'],q['units']['text'],q['step_count'],q['default_normalized'],q['unit_id'],q['flags_raw']]
   expected=r[2:5]+[int(r[5]),float(r[6]),int(r[7]),int(r[8])]
   self.assertEqual(actual,expected,str(q['id']))
   if role:self.assertEqual(q['current_normalized'],float(r[6]))
  self.assertEqual(counts,[192,104,1,1,2080])
 def test_program_count_from_source_compiled_probe(self):
  p=pathlib.Path(self.tmp.name)/'count.cpp';exe=p.with_suffix('')
  p.write_text('#include "core/FactoryPresets.h"\n#include <iostream>\nint main(){std::cout<<cs80::programLibrary().size();}\n')
  subprocess.run([self.compiler,'-std=c++20','-I',str(self.src),str(p),str(self.src/'core'/'FactoryPresets.cpp'),'-o',str(exe)],capture_output=True,text=True,check=True,timeout=45)
  self.assertEqual(subprocess.run([str(exe)],capture_output=True,text=True,check=True,timeout=10).stdout,'523')
for mode in ('valid','missing','extra','duplicate','unknown','id-vector','reorder','workload-current','nan-current','residual-current','negative-zero-default','negative-zero-residual'):
 setattr(ParameterClassifierTests,'test_'+mode.replace('-','_'),lambda self,m=mode:self.case(m))
# Every metadata field tested for each of the five source roles; select both
# a generated panel selector and a memory float with nontrivial normalization.
for index in (0,190,197,296,297,298,2377):
 for field in ('title','short','units','steps','default','unit','flags','unterminated'):
  setattr(ParameterClassifierTests,f'test_{field}_{index}',lambda self,m=f'{field}:{index}':self.case(m))
if __name__=='__main__':unittest.main()
