// New bounded adapter only. Original normal stock/V2 plugin images stay unchanged.
#define main factory8_original_main_not_called
#include "factory8_base.cpp"
#undef main
#include <time.h>
#include <sched.h>
#include <sys/stat.h>
#include <unistd.h>
namespace measure8 {
using namespace factory8;
struct ClockPair{std::uint64_t wall0{},cpu0{},cpu1{},wall1{};};
struct Record{FP before{},after{},next{};double probe{};tresult result{};std::uint64_t silence{};unsigned eventReads{},eventCountReads{};ClockPair clock{};};
using PCM=std::array<float,128>;
std::uint64_t toNs(const timespec& t){return std::uint64_t(t.tv_sec)*1000000000ull+std::uint64_t(t.tv_nsec);}
std::uint64_t hashBytes(const void* ptr,std::size_t size){std::uint64_t h=1469598103934665603ull;const auto*p=static_cast<const unsigned char*>(ptr);for(std::size_t n=0;n<size;++n){h^=p[n];h*=1099511628211ull;}return h;}
std::string hex(std::uint64_t x){std::ostringstream s;s<<std::hex<<std::setw(16)<<std::setfill('0')<<x;return s.str();}
std::string affinity(){cpu_set_t c;CPU_ZERO(&c);require(sched_getaffinity(0,sizeof(c),&c)==0,"get affinity");std::ostringstream s;bool first=true;for(int n=0;n<CPU_SETSIZE;++n)if(CPU_ISSET(n,&c)){if(!first)s<<',';first=false;s<<n;}return s.str();}
void saveStatePreserved(Instance&i,const std::filesystem::path&p){std::fenv_t f;std::fegetenv(&f);const int e=errno;save(p,state(i));std::fesetenv(&f);errno=e;}
int run(bool timed,const char* moduleArg,const std::filesystem::path&out,unsigned count){
 const char* authorization=timed?std::getenv("CS80_NATIVE_AVERAGE_AUTH"):"";require(!timed||(authorization&&*authorization),"measure requires frozen authorization environment");
 require(count==512||count==544,"exact512 or544 ordinary callbacks");require(!std::filesystem::exists(out),"fresh output");std::filesystem::create_directories(out);
 const auto modulePath=std::filesystem::canonical(moduleArg);struct stat identity{};require(stat(modulePath.c_str(),&identity)==0,"module stat");
 const int firstCpu=sched_getcpu();const auto firstAffinity=affinity();const unsigned total=1024+count+1;
 // All PCM/record/input allocations precede any process call or measured span.
 std::vector<PCM>pcm(total);std::vector<Record>records(total);auto p=controls();writeControls(out/"controls.csv",p);
 Module module(modulePath.c_str());Dl_info image{};require(dladdr(dlsym(module.handle,"GetPluginFactory"),&image)!=0&&image.dli_fname,"actual loaded image");const auto loadedPath=std::filesystem::canonical(image.dli_fname);require(loadedPath==modulePath,"loaded image path matches");
 Instance instance(module,p,0,0);require(instance.audio->canProcessSampleSize(kSample32)==kResultTrue,"sample32 supported");require(instance.component->getBusCount(kAudio,kInput)==1&&instance.component->getBusCount(kAudio,kOutput)==1&&instance.component->getBusCount(kEvent,kInput)==1,"native bus count");BusInfo inInfo{},outInfo{};require(instance.component->getBusInfo(kAudio,kInput,0,inInfo)==kResultOk&&inInfo.channelCount==1,"mono input");require(instance.component->getBusInfo(kAudio,kOutput,0,outInfo)==kResultOk&&outInfo.channelCount==2,"stereo output");require(instance.component->activateBus(kAudio,kInput,0,false)==kResultOk,"inactive input");
 observeMetadata(instance,out,"initial");save(out/"initial.state",state(instance));
 HeldEvents events;events.count=8;std::ofstream inputs(out/"inputs.jsonl");
 for(int v=0;v<8;++v){auto&e=events.values[std::size_t(v)];e.type=Event::kNoteOnEvent;e.busIndex=0;e.sampleOffset=0;e.noteOn.channel=0;e.noteOn.pitch=int16(notes[v]);e.noteOn.velocity=102.0f/127.0f;e.noteOn.noteId=v;inputs<<"{\"event\":\"note-on\",\"block\":0,\"offset\":0,\"sdk_channel\":0,\"note_id\":"<<v<<",\"pitch\":"<<notes[v]<<",\"velocity_f32_bits\":"<<std::bit_cast<std::uint32_t>(e.noteOn.velocity)<<",\"decoded_midi7\":102}\n";}
 // Planar output is staged per callback, filled beforehand, then copied after clocks.
 std::array<float,64>left{},right{};float*outputs[]={left.data(),right.data()};float*inputPointers[]={nullptr};AudioBusBuffers output{};output.numChannels=2;output.channelBuffers32=outputs;AudioBusBuffers input{};input.numChannels=1;input.channelBuffers32=inputPointers;input.silenceFlags=1;
 ProcessContext context{};context.sampleRate=48000;context.state=0;ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=64;data.numInputs=1;data.inputs=&input;data.numOutputs=1;data.outputs=&output;data.inputEvents=&events;data.processContext=&context;
 std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x9fc0);errno=41;unsigned clockReads=0,processCalls=0;
 for(unsigned b=0;b<total;++b){
  // These fills/descriptors/observations are outside the clock brackets.
  left.fill(0);right.fill(0);output.silenceFlags=0;context.projectTimeSamples=int64(b)*64;events.count=b==0?8:0;events.reads=events.countReads=0;auto&r=records[b];r.before=fp();
  const bool clockThis=timed&&b>=1024&&b<1024+count;
  timespec wall0{},cpu0{},cpu1{},wall1{};int clockStatus0=0,clockStatus1=0,clockStatus2=0,clockStatus3=0;
  if(clockThis){clockStatus0=clock_gettime(CLOCK_MONOTONIC,&wall0);clockStatus1=clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpu0);}
  r.result=instance.audio->process(data);
  if(clockThis){clockStatus2=clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpu1);clockStatus3=clock_gettime(CLOCK_MONOTONIC,&wall1);}
  // Conversion and status validation occur after all four endpoint reads.
  if(clockThis){clockReads+=4;require(clockStatus0==0&&clockStatus1==0&&clockStatus2==0&&clockStatus3==0,"clock endpoints");r.clock={toNs(wall0),toNs(cpu0),toNs(cpu1),toNs(wall1)};}
  r.after=fp();++processCalls;r.silence=output.silenceFlags;r.eventReads=events.reads;r.eventCountReads=events.countReads;
  std::fenv_t saved;std::fegetenv(&saved);const int oldErrno=errno;volatile double one=1.,delta=0x1p-54;volatile double probe=one+delta;r.next=fp();r.probe=probe;
  for(unsigned n=0;n<64;++n){pcm[b][2*n]=left[n];pcm[b][2*n+1]=right[n];}
  std::fesetenv(&saved);errno=oldErrno;
  if(b==1023)saveStatePreserved(instance,out/"postwarm.state");
  if(b==1535)saveStatePreserved(instance,out/"common512.state");
  if(b==1023+count)saveStatePreserved(instance,out/"final-window.state");
  if(b==total-1)saveStatePreserved(instance,out/"continuation.state");
 }
 const auto endpoint=fp();const int finalCpu=sched_getcpu();const auto finalAffinity=affinity();
 require(processCalls==total&&clockReads==(timed?4*count:0),"actual process and clock count");
 std::ofstream pcmFile(out/"pcm.bin",std::ios::binary),trace(out/"callbacks.bin",std::ios::binary),clocks(out/"clocks.csv");clocks<<"window_index,global_block,monotonic_start_ns,thread_cpu_start_ns,thread_cpu_end_ns,monotonic_end_ns\n";
 std::uint64_t cpuNs=0,wallNs=0;unsigned nonzero=0,eventReads=0;
 for(unsigned b=0;b<total;++b){const auto&r=records[b];const unsigned mode=0;const double rate=48000;require(r.result==kResultOk,"all genuine callbacks returned success");require(r.eventReads==(b==0?8u:0u),"exact event consumption");eventReads+=r.eventReads;
  put(trace,b);put(trace,mode);put(trace,rate);put(trace,r.result);put(trace,r.silence);putFP(trace,r.before);putFP(trace,r.after);putFP(trace,r.next);put(trace,r.probe);
  inputs<<"{\"event\":\"callback\",\"block\":"<<b<<",\"event_reads\":"<<r.eventReads<<",\"event_count_reads\":"<<r.eventCountReads<<",\"parameter_points\":0,\"project_samples\":"<<b*64<<"}\n";
  for(unsigned n=0;n<64;++n){const float l=pcm[b][2*n],rr=pcm[b][2*n+1];require(std::isfinite(l)&&std::isfinite(rr),"finite full PCM");nonzero+=l!=0||rr!=0;put(pcmFile,l);put(pcmFile,rr);}
  if(b>=1024&&b<1024+count){const auto&c=r.clock;if(timed){require(c.cpu1>=c.cpu0&&c.wall1>=c.wall0,"clock order");cpuNs+=c.cpu1-c.cpu0;wallNs+=c.wall1-c.wall0;}else require(c.cpu0==0&&c.cpu1==0&&c.wall0==0&&c.wall1==0,"untimed has zero clock fields");clocks<<b-1024<<','<<b<<','<<c.wall0<<','<<c.cpu0<<','<<c.cpu1<<','<<c.wall1<<'\n';}
 }
 require(nonzero==total*64&&eventReads==8,"nonvacuous complete history");observeMetadata(instance,out,"final");
 const auto pcmDigest=[&](unsigned begin,unsigned n){return hex(hashBytes(pcm.data()+begin,std::size_t(n)*sizeof(PCM)));};
 std::string commonFP;commonFP.reserve(512*44);const auto append=[&](const void*p,std::size_t n){commonFP.append(static_cast<const char*>(p),n);};
 for(unsigned b=1024;b<1536;++b){const auto&r=records[b];for(auto f:{r.before,r.after,r.next}){append(&f.csr,4);append(&f.cw,2);append(&f.sw,2);append(&f.error,4);}append(&r.probe,8);}
 std::ostringstream cid;cid<<std::hex<<std::setfill('0');for(unsigned char c:module.componentId)cid<<std::setw(2)<<unsigned(c);
 const auto readFile=[](const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};const auto hashFile=[&](const char*name){const auto s=readFile(out/name);return hex(hashBytes(s.data(),s.size()));};
 std::ostringstream result;result<<"{\"schema\":\"native-factory8-paired-clock-v1\",\"mode\":\""<<(timed?"measure":"untimed")<<"\",\"authorization\":"<<std::quoted(authorization?authorization:"")<<",\"module_path\":"<<std::quoted(modulePath.string())<<",\"loaded_module_path\":"<<std::quoted(loadedPath.string())<<",\"module_device\":"<<identity.st_dev<<",\"module_inode\":"<<identity.st_ino<<",\"module_size\":"<<identity.st_size<<",\"component_cid\":\""<<cid.str()<<"\",\"pid\":"<<getpid()<<",\"cpu_before\":"<<firstCpu<<",\"cpu_after\":"<<finalCpu<<",\"affinity_before\":\""<<firstAffinity<<"\",\"affinity_after\":\""<<finalAffinity<<"\",\"sample_rate\":48000,\"block_size\":64,\"max_block_size\":64,\"warm_calls\":1024,\"window_calls\":"<<count<<",\"continuation_calls\":1,\"process_calls\":"<<processCalls<<",\"window_frames\":"<<count*64<<",\"clock_reads\":"<<clockReads<<",\"cpu_ns_sum\":"<<cpuNs<<",\"wall_ns_sum\":"<<wallNs<<",\"window_wall_start_ns\":"<<records[1024].clock.wall0<<",\"window_wall_end_ns\":"<<records[1023+count].clock.wall1<<",\"enclosing_wall_ns\":"<<(timed?records[1023+count].clock.wall1-records[1024].clock.wall0:0)<<",\"note_event_reads\":"<<eventReads<<",\"finite_pcm\":true,\"nonzero_stereo_frames\":"<<nonzero<<",\"postwarm_pcm_fnv64\":\""<<pcmDigest(0,1024)<<"\",\"common512_pcm_fnv64\":\""<<pcmDigest(1024,512)<<"\",\"common512_fp_fnv64\":\""<<hex(hashBytes(commonFP.data(),commonFP.size()))<<"\",\"window_pcm_fnv64\":\""<<pcmDigest(1024,count)<<"\",\"continuation_pcm_fnv64\":\""<<pcmDigest(total-1,1)<<"\",\"postwarm_state_fnv64\":\""<<hashFile("postwarm.state")<<"\",\"common512_state_fnv64\":\""<<hashFile("common512.state")<<"\",\"final_window_state_fnv64\":\""<<hashFile("final-window.state")<<"\",\"continuation_state_fnv64\":\""<<hashFile("continuation.state")<<"\",\"endpoint_fp\":["<<endpoint.csr<<','<<endpoint.cw<<','<<endpoint.sw<<','<<endpoint.error<<"],\"private_voice_counts_observed\":false,\"private_backend_counts_observed\":false}";
 save(out/"outcome.json",result.str()+"\n");std::cout<<result.str()<<'\n';return 0;
}
}
int main(int argc,char**argv){try{require(argc==5,"untimed|measure MODULE NEW_OUT 512|544");const std::string mode=argv[1];require(mode=="untimed"||mode=="measure","explicit mode");const std::string n=argv[4];require(n=="512"||n=="544","explicit callback count");return measure8::run(mode=="measure",argv[2],argv[3],n=="512"?512:544);}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
