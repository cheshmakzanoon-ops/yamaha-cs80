"""Defect control: pinned JUCE optional-interface false/null vs strict SDK gate.
No native plugin is loaded and no compatibility relaxation is implemented.
"""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
class JuceQueryDefectTests(unittest.TestCase):
 def test_exact_false_null_reproduces_rejection(self):
  sdk=os.environ.get('CS80_OBSERVER_SDK')
  if not sdk:self.skipTest('explicit pinned SDK required')
  for name,wanted in json.loads((ROOT/'native_observer_sdk_provenance.json').read_text())['headers'].items():
   self.assertEqual(hashlib.sha256((pathlib.Path(sdk)/name).read_bytes()).hexdigest(),wanted)
  original=(ROOT/'native_observer_initializer_fixture.cpp').read_text();needle='if(!combined)return kNoInterface;';self.assertEqual(original.count(needle),1)
  # Reproduce precisely the pinned JUCE default QueryInterfaceResult, not a
  # general failure or a successful combined controller.
  source='#define main original_initializer_main\n'+original.replace(needle,'if(!combined)return kResultFalse;')+'\n#undef main\n'+r'''
int main(){try{
 InitFake component("component",0),controller("controller",0);component.combined=false;
 InitFactory f(component,controller);f.separate=true;f.classes=2;factory=&f;
 Host host;FakeLoader loader;TUID cid{};std::memset(cid,5,16);InjectedModule module(loader);module.load("/sealed/fake.so",cid,host);
 Handler handler;InjectedInstance instance(module,handler);bool rejected=false;
 try{instance.initializeConnected();}catch(const std::exception& e){rejected=std::string(e.what())=="controller query failed";}
 require(rejected && !instance.initializationSucceeded,"strict query gate must reject known mismatch");
 bool observed=false;for(std::size_t i=0;i<instance.calls.count;++i){const auto& call=instance.calls.rows[i];if(std::string(call.name)=="combined-query"){require(call.normal && call.returned==kResultFalse && !call.ok && !call.error,"exact normal false result recorded");observed=true;}}
 require(observed && count("controller-class-id")==0 && count("component-initialize")==0,"blocked before separate controller path");
 require(component.refs==0 && controller.refs==0,"failed fake ownership cleaned");module.close();return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d);(p/'defect.cpp').write_text(source)
   result=subprocess.run([shutil.which('g++'),'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',sdk,'-I',str(ROOT),str(p/'defect.cpp'),'-o',str(p/'defect')],capture_output=True,text=True,timeout=30);self.assertEqual(result.returncode,0,result.stderr)
   result=subprocess.run([str(p/'defect')],capture_output=True,text=True,timeout=5);self.assertEqual(result.returncode,0,result.stderr)
if __name__=='__main__':unittest.main()
