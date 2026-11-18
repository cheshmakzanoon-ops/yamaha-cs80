// Untimed genuine VST3 stock-only factory-eight held admission, not a core runner.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "sdk_scaffold.cpp"
#include <bit>
#include <cerrno>
#include <cfenv>
#include <fstream>
#include <iomanip>
#include <xmmintrin.h>

namespace factory8 {
constexpr unsigned warm=1024, window=16, blocks=warm+window+1, blockSize=64;
constexpr int notes[8]={36,43,48,55,60,64,67,72};
struct FP{unsigned csr;unsigned short cw,sw;int error;};
FP fp(){FP f{};asm volatile("stmxcsr %0; fnstcw %1; fnstsw %2":"=m"(f.csr),"=m"(f.cw),"=m"(f.sw)::"memory");f.error=errno;return f;}
template<class T>void put(std::ofstream& f,const T&v){f.write(reinterpret_cast<const char*>(&v),sizeof(v));require(bool(f),"complete binary write");}
void putFP(std::ofstream& f,FP x){put(f,x.csr);put(f,x.cw);put(f,x.sw);put(f,x.error);}
std::string state(Instance&i){StateCapture s;require(i.component->getState(&s)==kResultOk,"public component state");return s.bytes;}
void save(const std::filesystem::path&p,const std::string&s){std::ofstream f(p,std::ios::binary);f.write(s.data(),std::streamsize(s.size()));require(bool(f),"state output");}
cs80::ParamValues controls(){
 using namespace cs80;auto p=defaultValues();
 p[P_parityMode]=0;p[P_sourcePerformance]=1;p[P_voiceMode]=0;
 p[P_mix]=.5f;p[P_volume]=.12f;p[P_masterSustain]=p[P_keyRelease]=p[P_sustainMode]=0;
 p[P_chorusOn]=p[P_ringAmount]=0;p[P_subOriginalMode]=1;
 p[P_subVco]=p[P_subVcf]=p[P_subVca]=0;p[P_subAtVco]=p[P_subAtVcf]=p[P_subAtSpeed]=p[P_subExtLevel]=0;
 p[P_drift]=.37f;p[P_voiceDispersion]=.18f;p[P_driftModel]=0;p[P_analogNoise]=0;
 p[P_lfo1Depth]=p[P_lfo2Depth]=p[P_lfo3Depth]=0;
 p[P_fx1Type]=p[P_fx2Type]=p[P_fx3Type]=0;p[P_fx1Mix]=p[P_fx2Mix]=p[P_fx3Mix]=0;
 p[P_mpeOn]=p[P_arpOn]=p[P_footMode]=0;p[P_outputLevel]=p[P_outputMode]=1;p[P_outputJack]=0;
 p[P_oversampling]=1;
 // The real processor applies the selected factory's 26 programmer controls on
 // every callback. These four per-rank controls stay live and are explicit.
 for(int rank=0;rank<2;++rank){p[chParam(rank,CP_feet)]=1;p[chParam(rank,CP_pan)]=0;p[chParam(rank,CP_hpfOn)]=p[chParam(rank,CP_lpfOn)]=1;}
 return p;
}
void writeControls(const std::filesystem::path&p,const cs80::ParamValues&values){
 std::ofstream f(p);f<<"name,id,desired_physical_f32_bits,normalized_f64_bits\n";
 for(int n=0;n<cs80::P_numParams;++n){const auto name=cs80::paramId(n);const double normal=normalized(n,values[n]);f<<name<<','<<parameterId(name)<<','<<std::bit_cast<std::uint32_t>(values[n])<<','<<std::bit_cast<std::uint64_t>(normal)<<'\n';}
 for(int rank=0;rank<2;++rank){const std::string name="hardware_source_"+std::to_string(rank+1);f<<name<<','<<parameterId(name)<<",0,0\n";}require(bool(f),"complete controls");
}
void observeMetadata(Instance&i,const std::filesystem::path&out,const char*label){
 require(i.controller==nullptr,"fresh readback controller");TUID cid{};
 require(i.component->getControllerClassId(cid)==kResultOk,"controller class");
 require(i.factory->createInstance(cid,IEditController::iid,reinterpret_cast<void**>(&i.controller))==kResultOk,"readback controller create");
 require(i.controller->initialize(nullptr)==kResultOk,"readback controller initialize");
 require(i.component->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&i.componentConnection))==kResultOk,"component connection");
 require(i.controller->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&i.controllerConnection))==kResultOk,"controller connection");
 require(i.componentConnection->connect(i.controllerConnection)==kResultOk&&i.controllerConnection->connect(i.componentConnection)==kResultOk,"readback connect");
 StateCapture s;require(i.component->getState(&s)==kResultOk,"readback component state");require(s.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,"readback rewind");require(i.controller->setComponentState(&s)==kResultOk,"readback setComponentState");
 std::ofstream f(out/(std::string(label)+"-metadata.csv"));f<<"index,id,step_count,flags,default_f64_bits,current_f64_bits\n";std::vector<ParamID>ids;
 for(int32 n=0;n<i.controller->getParameterCount();++n){ParameterInfo x{};require(i.controller->getParameterInfo(n,x)==kResultOk,"actual metadata");ids.push_back(x.id);const double current=i.controller->getParamNormalized(x.id);require(std::isfinite(current),"finite readback");f<<n<<','<<x.id<<','<<x.stepCount<<','<<x.flags<<','<<std::bit_cast<std::uint64_t>(x.defaultNormalizedValue)<<','<<std::bit_cast<std::uint64_t>(current)<<'\n';}
 for(int n=0;n<cs80::P_numParams;++n)require(std::find(ids.begin(),ids.end(),parameterId(cs80::paramId(n)))!=ids.end(),"all ordinary control IDs present");
 for(const char*name:{"hardware_source_1","hardware_source_2"})require(std::find(ids.begin(),ids.end(),parameterId(name))!=ids.end(),"factory selector ID present");
 require(bool(f),"metadata output");i.releaseReadbackController();
}
struct HeldEvents final:IEventList{
 std::array<Event,8>values{};int count=0;unsigned reads=0,countReads=0;
 tresult PLUGIN_API queryInterface(const TUID t,void**o)override{if(!std::memcmp(t,IEventList::iid,sizeof(TUID))||!std::memcmp(t,FUnknown::iid,sizeof(TUID))){*o=this;return kResultOk;}*o=nullptr;return kNoInterface;}
 uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
 int32 PLUGIN_API getEventCount()override{++countReads;return count;}
 tresult PLUGIN_API getEvent(int32 index,Event&e)override{if(index<0||index>=count)return kInvalidArgument;++reads;e=values[std::size_t(index)];return kResultOk;}
 tresult PLUGIN_API addEvent(Event&)override{return kNotImplemented;}
};
int run(const char*modulePath,const std::filesystem::path&out){
 require(!std::filesystem::exists(out),"new capture directory");std::filesystem::create_directories(out);
 const auto p=controls();writeControls(out/"controls.csv",p);Module module(modulePath);Instance i(module,p,0,0);
 require(i.audio->canProcessSampleSize(kSample32)==kResultTrue,"sample32 supported");
 require(i.component->getBusCount(kAudio,kInput)==1&&i.component->getBusCount(kAudio,kOutput)==1&&i.component->getBusCount(kEvent,kInput)==1,"native bus count");
 BusInfo inInfo{},outInfo{};require(i.component->getBusInfo(kAudio,kInput,0,inInfo)==kResultOk&&inInfo.channelCount==1,"mono external input");require(i.component->getBusInfo(kAudio,kOutput,0,outInfo)==kResultOk&&outInfo.channelCount==2,"stereo native output");
 require(i.component->activateBus(kAudio,kInput,0,false)==kResultOk,"inactive external input");
 observeMetadata(i,out,"initial");save(out/"initial.state",state(i));
 std::ofstream pcm(out/"pcm.bin",std::ios::binary),trace(out/"callbacks.bin",std::ios::binary),inputs(out/"inputs.jsonl");require(bool(pcm)&&bool(trace)&&bool(inputs),"capture outputs");
 HeldEvents events;events.count=8;
 for(int v=0;v<8;++v){auto&e=events.values[std::size_t(v)];e.type=Event::kNoteOnEvent;e.busIndex=0;e.sampleOffset=0;e.noteOn.channel=0;e.noteOn.pitch=int16(notes[v]);e.noteOn.velocity=102.0f/127.0f;e.noteOn.noteId=v;inputs<<"{\"event\":\"note-on\",\"block\":0,\"offset\":0,\"sdk_channel\":0,\"note_id\":"<<v<<",\"pitch\":"<<notes[v]<<",\"velocity_f32_bits\":"<<std::bit_cast<std::uint32_t>(e.noteOn.velocity)<<",\"decoded_midi7\":102}\n";}
 std::array<float,64>left{},right{};float*outputs[]={left.data(),right.data()};float*inputPointers[]={nullptr};
 AudioBusBuffers output{};output.numChannels=2;output.channelBuffers32=outputs;AudioBusBuffers input{};input.numChannels=1;input.channelBuffers32=inputPointers;input.silenceFlags=1;
 ProcessContext context{};context.sampleRate=48000.;context.state=0;
 ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=64;data.numInputs=1;data.inputs=&input;data.numOutputs=1;data.outputs=&output;data.inputEvents=&events;data.processContext=&context;
 std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x9fc0);errno=41;unsigned nonzero=0,eventReads=0;
 for(unsigned b=0;b<blocks;++b){
  const unsigned mode=0;const double rate=48000;left.fill(0);right.fill(0);output.silenceFlags=0;context.projectTimeSamples=int64(b)*64;events.count=b==0?8:0;events.reads=events.countReads=0;
  const auto before=fp();const auto result=i.audio->process(data);const auto after=fp();require(result==kResultOk,"actual IAudioProcessor process");
  std::fenv_t saved;std::fegetenv(&saved);const int oldErrno=errno;volatile double one=1.,delta=0x1p-54;volatile double probe=one+delta;const auto next=fp();
  put(trace,b);put(trace,mode);put(trace,rate);put(trace,result);put(trace,output.silenceFlags);putFP(trace,before);putFP(trace,after);putFP(trace,next);const double value=probe;put(trace,value);
  require(events.reads==(b==0?8u:0u),"eight actual event reads, no later note events");eventReads+=events.reads;
  inputs<<"{\"event\":\"callback\",\"block\":"<<b<<",\"event_reads\":"<<events.reads<<",\"event_count_reads\":"<<events.countReads<<",\"parameter_points\":0,\"project_samples\":"<<context.projectTimeSamples<<"}\n";
  for(unsigned n=0;n<64;++n){require(std::isfinite(left[n])&&std::isfinite(right[n]),"finite PCM");nonzero+=left[n]!=0||right[n]!=0;put(pcm,left[n]);put(pcm,right[n]);}
  if(b==0||b==warm-1||b==warm+window-1||b==blocks-1)save(out/("block-"+std::to_string(b)+".state"),state(i));
  std::fesetenv(&saved);errno=oldErrno;
 }
 require(eventReads==8&&nonzero>0,"nonvacuous held history");observeMetadata(i,out,"final");
 std::cout<<"{\"schema\":\"native-factory8-held-stock-v1\",\"callbacks\":"<<blocks<<",\"warm\":1024,\"window\":16,\"continuation\":1,\"max_block_size\":64,\"sample_rate\":48000,\"frames\":"<<blocks*64<<",\"note_event_reads\":"<<eventReads<<",\"nonzero_stereo_frames\":"<<nonzero<<",\"controls\":192,\"private_voice_counts_observed\":false,\"private_route_or_backend_counts_observed\":false,\"timing\":false}\n";return 0;
}
}
int main(int argc,char**argv){try{require(argc==3,"MODULE NEW_CAPTURE_DIRECTORY");return factory8::run(argv[1],argv[2]);}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
