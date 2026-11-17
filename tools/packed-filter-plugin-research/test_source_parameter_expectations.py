"""Compare immutable Python expectations with pure C++ source exporter, no plugin."""
import os,pathlib,shutil,struct,subprocess,tempfile,unittest
from source_parameter_roles import GENERATED,MEMORY,expected_metadata
ROOT=pathlib.Path(__file__).parent
class RoleMapTests(unittest.TestCase):
 def test_exact_role_ids(self):
  rows=expected_metadata();self.assertEqual(len(rows),2378);self.assertEqual(len({r[0] for r in rows}),2378);self.assertEqual({i for _,i in GENERATED},{r[0] for r in rows if r[1]==0});self.assertEqual({i for _,i in MEMORY},{r[0] for r in rows if r[1]==1})
 def test_generated_name_hashes(self):
  for name,id in GENERATED+MEMORY:
   hash=0
   for c in name:hash=(hash*31+ord(c))&0xffffffff
   self.assertEqual(hash&0x7fffffff,id)
 @unittest.skipUnless(os.environ.get('CS80_OBSERVER_SDK'),'explicit pinned SDK required for source exporter')
 def test_cpp_source_export_exact(self):
  with tempfile.TemporaryDirectory() as tmp:
   exe=pathlib.Path(tmp)/'source-export';command=[shutil.which('g++'),'-std=c++20','-I',os.environ['CS80_OBSERVER_SDK'],'-I',str(ROOT.parents[1]/'src'),str(ROOT/'native_observer_parameter_classifier_fixture.cpp'),'-o',str(exe)]
   result=subprocess.run(command,capture_output=True,text=True,timeout=90);self.assertEqual(result.returncode,0,result.stderr)
   result=subprocess.run([str(exe),'export'],capture_output=True,text=True,timeout=10,check=True);self.assertLess(len(result.stdout),2*1024**2);rows=[]
   for line in result.stdout.splitlines():
    id,role,title,short,units,steps,default,unit,flags=line.split('\t');rows.append((int(id),int(role),title,short,units,int(steps),struct.pack('<d',float(default)).hex(),int(unit),int(flags)))
   self.assertEqual(tuple(rows),expected_metadata())
if __name__=='__main__':unittest.main()
