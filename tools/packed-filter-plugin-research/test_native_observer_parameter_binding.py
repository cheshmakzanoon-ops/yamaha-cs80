"""Source-aware owned A-D guards with inert SDK objects, never actual plugin."""
import os,pathlib,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
class ParameterBindingTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  sdk=os.environ.get('CS80_OBSERVER_SDK')
  if not sdk:raise unittest.SkipTest('Explicit pinned SDK required')
  cls.temp=tempfile.TemporaryDirectory();root=pathlib.Path(cls.temp.name);cls.exe=root/'binding'
  s=(ROOT/'native_observer_history_fixture.cpp').read_text()
  s=s.replace('static std::vector<ParamID> fullIds;', '''static std::vector<ParamID> fullIds;
static std::vector<ConnectedParameterSnapshot> sourceRows;
static std::map<ParamID,double> sourceValues;
static int readbackCalls=0,mutateStage=-1;static bool mutatePanel=false;static char mutationKind=0;
''')
  s=s.replace("p={};p.id=fullIds.at(static_cast<std::size_t>(index));p.title[0]='P';p.defaultNormalizedValue=defaultNormalized;",'p=sourceRows.at(static_cast<std::size_t>(index)).info;')
  s=s.replace('ParamValue PLUGIN_API getParamNormalized(ParamID) override {return normalized;}', '''ParamValue PLUGIN_API getParamNormalized(ParamID id) override {
require(std::this_thread::get_id()==uiThread && !processActive,"readback on idle UI");
const int call=readbackCalls++;double value=sourceValues.at(id);
if(call>=2378){const int stage=(call-2378)/2188;const auto target=mutatePanel?packed_filter_workload::parameter_id("hardware_source_1"):ParamID(0x62797073);
if(stage==mutateStage && id==target)value=mutationKind=='z'?-0.0:mutationKind=='n'?std::numeric_limits<double>::quiet_NaN():value==0?0.25:0.0;}
return value;}
''')
  s=s.replace('for(const auto& q:w::controls(c))fullIds.push_back(q.id);', '''for(const auto& e:sourceParameterExpectations())sourceRows.push_back({e.info,e.info.defaultNormalizedValue});for(const auto& row:sourceRows){fullIds.push_back(row.info.id);sourceValues[row.info.id]=row.current;}
if(test!="history" && test!="invalid-policy" && test!="unbound"){require(test.size()==2,"guard case");mutateStage=test[1]-'0';mutatePanel=test[0]=='p';mutationKind=test[0];}
''')
  s=s.replace('int renderStage=0,flushes=0;', '''
if(test=="invalid-policy"){require(failed([&]{ProcessingInstanceOwner o(module,handler,ControllerQueryPolicy::StrictSdk,AudioInputTopology::ZeroAudioInput,static_cast<ParameterBindingPolicy>(0));}),"invalid policy rejected");require(count("component-create")==0,"invalid policy before native create");module.close();return 0;}
if(test=="unbound"){ProcessingInstanceOwner o(module,handler,ControllerQueryPolicy::StrictSdk,AudioInputTopology::ZeroAudioInput,ParameterBindingPolicy::SourceAwarePanel);o.initializeConnected();o.captureConnected();auto plan=workload_detail::planFromRoster(c,o.connectedRoster());o.prepareAndStart(plan.setup,true);require(failed([&]{o.flush(plan.initial,{worker::Clock::now()+std::chrono::seconds(1),1000,std::chrono::milliseconds(1)},[]{});}),"public unbound source flush rejected");require(processCalls==0,"unbound source never processes");module.close();return 0;}
int renderStage=0,flushes=0;''')
  s=s.replace('actualFlushes.push_back(std::move(points));','for(const auto& point:points){sourceValues[point.id]=point.value;}actualFlushes.push_back(std::move(points));')
  s=s.replace('HistoryOwner history(module,handler,requested,ControllerStatePolicy::Supported);','HistoryOwner history(module,handler,requested,ControllerStatePolicy::Supported,ControllerQueryPolicy::StrictSdk,AudioInputTopology::ZeroAudioInput,ParameterBindingPolicy::SourceAwarePanel);')
  s=s.replace('}else {require(history.stage()', '}else {if(mutateStage>=0){require(!e.parameterReadbacks[mutateStage].data.empty(),"failed observed readback retained");try{std::rethrow_exception(history.primaryError());}catch(const std::exception& error){require(std::string(error.what()).find("observed_bits=")!=std::string::npos && std::string(error.what()).find("stage="+std::to_string(mutateStage))!=std::string::npos,"exact failed readback diagnostic");}}require(history.stage()')
  s=s.replace('const int limit=test==','const int limit=mutateStage>=0?mutateStage:test==')
  s=s.replace('if(good){','if(good){require(readbackCalls==2378+4*2188,"all four bounded full readbacks observed");for(const auto& b:e.parameterReadbacks)require(!b.data.empty(),"readback evidence retained");')
  path=root/'binding.cpp';path.write_text('#include <map>\n'+s)
  p=subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-ffp-contract=off','-I',sdk,'-I',str(ROOT),'-I',str(ROOT.parents[1]/'src'),str(path),'-o',str(cls.exe)],capture_output=True,text=True,timeout=90)
  if p.returncode:raise RuntimeError(p.stderr)
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def run_case(self,case):
  p=subprocess.run([str(self.exe),case],capture_output=True,text=True,timeout=10);self.assertEqual(p.returncode,0,p.stderr)
 def test_complete_history(self):self.run_case('history')
 def test_invalid_policy(self):self.run_case('invalid-policy')
 def test_public_unbound_source_flush(self):self.run_case('unbound')
for stage in range(4):
 for kind in ('r','p','z','n'):
  setattr(ParameterBindingTests,f'test_{kind}_before_{stage}',lambda self,k=kind,s=stage:self.run_case(k+str(s)))
if __name__=='__main__':unittest.main()
