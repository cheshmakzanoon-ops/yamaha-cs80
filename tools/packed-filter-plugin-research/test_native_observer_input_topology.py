"""Explicit inactive mono input lifecycle; inert SDK instances only."""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).parent
class InputTopologyTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  sdk=os.environ.get('CS80_OBSERVER_SDK')
  if not sdk:raise unittest.SkipTest('explicit pinned SDK required')
  for name,wanted in json.loads((ROOT/'native_observer_sdk_provenance.json').read_text())['headers'].items():
   if hashlib.sha256((pathlib.Path(sdk)/name).read_bytes()).hexdigest()!=wanted:raise RuntimeError('SDK drift')
  s=(ROOT/'native_observer_initializer_fixture.cpp').read_text()
  s=s.replace('return (media==kAudio && direction==kOutput) || (media==kEvent && direction==kInput)?1:0;', 'return media==kAudio?(topologyMutation=="count" && direction==kInput?2:1):(direction==kInput?1:0);')
  s=s.replace("b.channelCount=2;b.name[0]='O';",'''b.channelCount=2;b.name[0]='O';if(m==kAudio && d==kInput){b.channelCount=topologyMutation=="channels"?2:1;b.busType=topologyMutation=="type"?kAux:kMain;b.flags=topologyMutation=="active-flag"?BusInfo::kDefaultActive:0;const char16_t* n=topologyMutation=="name"?u"Other":u"External Modulation";for(std::size_t i=0;n[i];++i)b.name[i]=n[i];}''')
  s=s.replace('const int i=media==kAudio?0:1;', 'if(media==kAudio && direction==kInput){require(!active,"external input must never activate");++disableCalls;trace.push_back("external-input-disable");if(topologyMutation=="disable" || (topologyMutation=="second-disable" && disableCalls==2))return kResultFalse;return kResultOk;}const int i=media==kAudio?0:1;').replace('activateBus(MediaType media,BusDirection,int32,TBool active)', 'activateBus(MediaType media,BusDirection direction,int32,TBool active)')
  s=s.replace('setBusArrangements(SpeakerArrangement*,int32 inputs,', 'setBusArrangements(SpeakerArrangement* input,int32 inputs,').replace('require(inputs==0 && outputs==1 && output && *output==SpeakerArr::kStereo,"exact stereo arrangement");','require(inputs==1 && input && *input==SpeakerArr::kMono && outputs==1 && output && *output==SpeakerArr::kStereo,"exact mono/stereo arrangement");if(topologyMutation=="arrangement")return kResultFalse;')
  s=s.replace('getBusArrangement(BusDirection,int32,SpeakerArrangement& a) override {a=3;', 'getBusArrangement(BusDirection direction,int32,SpeakerArrangement& a) override {a=direction==kInput?(topologyMutation=="readback"?SpeakerArr::kStereo:SpeakerArr::kMono):SpeakerArr::kStereo;')
  s='#include <string>\nstatic std::string topologyMutation;static int disableCalls=0;\n#define main old_main\n'+s+'\n#undef main\n'+r'''
int main(int argc,char** argv){try{
 require(argc==2,"case");topologyMutation=argv[1];const bool good=topologyMutation=="valid" || topologyMutation=="separate";
 InitFake component("component",0),controller("controller",0);InitFactory f(component,controller);factory=&f;if(topologyMutation=="separate"){component.combined=false;f.separate=true;f.classes=2;}Host host;FakeLoader loader;TUID cid{};std::memset(cid,5,16);InjectedModule module(loader);module.load("/sealed/fake.so",cid,host);Handler handler;
 InjectedInstance instance(module,handler,ControllerQueryPolicy::StrictSdk,topologyMutation=="strict"?AudioInputTopology::ZeroAudioInput:AudioInputTopology::DisabledMonoExternalInput);
 bool rejected=false;try{instance.initializeConnected();instance.captureConnected();instance.prepareAndStart({},true);instance.saveState(ControllerStatePolicy::Supported);instance.suspend();instance.restoreSavedState();instance.resume();instance.suspend();instance.resume();instance.close();}catch(...){rejected=true;}
 require(rejected!=good,"expected frozen topology admission");
 if(good){require(disableCalls==3 && count("audio-bus-activate")==3 && count("audio-bus-deactivate")==3,"exact disabled input across three cycles");for(std::size_t i=0;i<instance.calls.count;++i)if(std::string(instance.calls.rows[i].name)=="external-input-disable"){require(i+1<instance.calls.count && std::string(instance.calls.rows[i+1].name)=="audio-bus-activate","disable before output enable");}}
 if(topologyMutation=="strict")require(disableCalls==0,"strict default rejects additional bus");
 require(instance.calls.count<=InjectedInstance::maximumScheduledCalls && InjectedInstance::maximumScheduledCalls==60,"complete frozen schedule bound");require(component.refs==0 && controller.refs==0,"owned fake references cleaned");module.close();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
  cls.tmp=tempfile.TemporaryDirectory();p=pathlib.Path(cls.tmp.name);cls.exe=p/'topology';(p/'topology.cpp').write_text(s)
  r=subprocess.run([shutil.which('g++'),'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',sdk,'-I',str(ROOT),str(p/'topology.cpp'),'-o',str(cls.exe)],capture_output=True,text=True,timeout=30)
  if r.returncode:raise RuntimeError(r.stderr)
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def run_case(self,name):
  p=subprocess.run([str(self.exe),name],capture_output=True,text=True,timeout=5);self.assertEqual(p.returncode,0,p.stderr)
for case in ('valid','separate','strict','count','channels','type','active-flag','name','arrangement','readback','disable','second-disable'):
 setattr(InputTopologyTests,'test_'+case.replace('-','_'),lambda self,c=case:self.run_case(c))
if __name__=='__main__':unittest.main()
