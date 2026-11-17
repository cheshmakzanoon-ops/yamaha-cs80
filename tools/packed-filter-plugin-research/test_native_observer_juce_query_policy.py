"""Narrow preselected optional-controller compatibility; no actual plugin load."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
class JuceQueryPolicyTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  sdk=os.environ.get('CS80_OBSERVER_SDK')
  if not sdk:raise unittest.SkipTest('explicit pinned SDK required')
  for name,wanted in json.loads((ROOT/'native_observer_sdk_provenance.json').read_text())['headers'].items():
   if hashlib.sha256((pathlib.Path(sdk)/name).read_bytes()).hexdigest()!=wanted:raise RuntimeError('SDK drift')
  original=(ROOT/'native_observer_initializer_fixture.cpp').read_text();needle='if(!combined)return kNoInterface;'
  if original.count(needle)!=1:raise RuntimeError('fixture seam drift')
  source='static int queryResult=1;static bool queryPointer=false;\n#define main original_initializer_main\n'+original.replace(needle,'if(!combined){if(queryPointer){*p=static_cast<IEditController*>(this);addRef();}return queryResult;}')+'\n#undef main\n'+r'''
int main(int argc,char** argv){try{
 require(argc==5,"args");const auto policy=static_cast<ControllerQueryPolicy>(std::stoi(argv[1]));queryResult=std::stoi(argv[2]);queryPointer=std::stoi(argv[3]);const bool accept=std::stoi(argv[4]);
 InitFake component("component",0),controller("controller",0);component.combined=false;
 InitFactory f(component,controller);f.separate=true;f.classes=2;factory=&f;
 Host host;FakeLoader loader;TUID cid{};std::memset(cid,5,16);InjectedModule module(loader);module.load("/sealed/fake.so",cid,host);
 Handler handler;InjectedInstance instance(module,handler,policy);bool rejected=false;
 try{instance.initializeConnected();}catch(...){rejected=true;}
 require(rejected!=accept,"exact expected admission");const auto q=instance.controllerQueryObservation();
 require(q.policy==policy && q.attempted && q.normal && q.result==queryResult && q.pointerPresent==queryPointer,"raw query evidence");
 if(accept){require(count("controller-class-id")==1 && count("controller-create")==1 && count("controller-initialize")==1 && count("component-connect")==1 && count("controller-connect")==1,"mandatory separate controller path");instance.close();}
 else require(count("controller-class-id")==0 && count("component-initialize")==0,"rejected before native initialization");
 require(component.refs==0 && controller.refs==0,"all fake references released");module.close();return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
  cls.tmp=tempfile.TemporaryDirectory();p=pathlib.Path(cls.tmp.name);cls.exe=p/'query';(p/'query.cpp').write_text(source)
  result=subprocess.run([shutil.which('g++'),'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',sdk,'-I',str(ROOT),str(p/'query.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
  if result.returncode:raise RuntimeError(result.stderr)
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def case(self,policy,result,pointer,accept):
  p=subprocess.run([str(self.exe),str(policy),str(result),str(int(pointer)),str(int(accept))],capture_output=True,text=True,timeout=5);self.assertEqual(p.returncode,0,p.stderr)
 def test_invalid_policy(self):
  p=subprocess.run([str(self.exe),'3','1','0','0'],capture_output=True,text=True,timeout=5);self.assertEqual(p.returncode,1);self.assertIn('unknown controller query policy',p.stderr)
 def test_strict_false_null_rejected(self):self.case(1,1,False,False)
 def test_pinned_juce_false_null_accepted(self):self.case(2,1,False,True)
 def test_strict_sdk_absent_accepted(self):self.case(1,-1,False,True)
 def test_juce_sdk_absent_accepted(self):self.case(2,-1,False,True)
for policy in (1,2):
 for result,pointer in ((1,True),(-1,True),(0,False),(3,False),(4,False)):
  setattr(JuceQueryPolicyTests,f'test_reject_{policy}_{result}_{pointer}',lambda self,p=policy,r=result,b=pointer:self.case(p,r,b,False))
if __name__=='__main__':unittest.main()
