#include "plugin/ModernLayerState.h"
#define main fixture_main_not_called
#include "../inputs/source_factory_benchmark.cpp"
#undef main
#include <bit>
#include <cfenv>
#include <cerrno>
#include <time.h>
#include <immintrin.h>
using namespace cs80;
namespace isolated {
constexpr unsigned iterations=131072;
std::uint64_t hash(std::uint64_t h,std::uint64_t x){return (h^x)*1099511628211ull;}
std::uint64_t configHash(const modern::ModernConfig& c){std::uint64_t h=1469598103934665603ull;for(const auto& s:c.msegs){for(const auto& p:s.points)for(float f:{p.time,p.level,p.curve})h=hash(h,std::bit_cast<std::uint32_t>(f));for(int v:{s.count,s.loopStart,s.loopEnd,int(s.loop),int(s.trigger),int(s.sync),int(s.bipolar)})h=hash(h,unsigned(v));}for(const auto& s:c.slots)for(int v:{int(s.source),int(s.via),int(s.destination),int(s.target)})h=hash(h,unsigned(v));return h;}
std::uint64_t transportHash(const modern::Transport& t){auto h=hash(1469598103934665603ull,std::bit_cast<std::uint64_t>(t.bpm));h=hash(h,std::bit_cast<std::uint64_t>(t.ppq));h=hash(h,t.hasPosition);return hash(h,t.playing);}
struct Fp {unsigned mxcsr;unsigned short cw,sw;int error;};
Fp fp(){Fp x{};asm volatile("stmxcsr %0;fnstcw %1;fnstsw %2":"=m"(x.mxcsr),"=m"(x.cw),"=m"(x.sw)::"memory");x.error=errno;return x;}
void dumpFp(Fp x){std::cout<<"{\"mxcsr\":"<<x.mxcsr<<",\"cw\":"<<x.cw<<",\"sw\":"<<x.sw<<",\"errno\":"<<x.error<<"}";}
modern::ModernConfig makeConfig(bool dense){modern::ModernConfig c;if(!dense)return c;for(int m=0;m<3;++m){auto& s=c.msegs[m];s.count=32;s.loopStart=4;s.loopEnd=27;s.loop=modern::MsegLoop::always;s.trigger=modern::MsegTrigger(m);s.sync=m!=0;s.bipolar=m==1;for(int i=0;i<32;++i)s.points[i]={float(i)/8.f,float((i+m)%5)/4.f,(i%3==0)?.25f:0.f};}for(int i=0;i<16;++i)c.slots[i]={modern::ModSource(int(modern::ModSource::mseg1)+i%3),modern::ModSource::none,i%2?modern::ModDestination::pan:modern::ModDestination::cutoff,modern::ModTarget(i%3)};return c;}
struct Pair {
 std::unique_ptr<Engine> first=std::make_unique<Engine>(),second=std::make_unique<Engine>();
 std::array<float,128> pcmA{},pcmB{};ControlFrame frame;
 explicit Pair(bool dense){Case c;c.rate=48000;c.block=64;c.sourcePerformance=true;c.selector=0;frame=makeFrame(c,0);frame.effectiveValues[P_drift]=.37f;frame.effectiveValues[P_voiceDispersion]=.18f;for(int i=0;i<16;++i)frame.effectiveValues[P_mod1Amount+i]=dense?.03125f:0.f;
  const auto config=makeConfig(dense);
  for(Engine* e:{first.get(),second.get()}){e->prepare(48000.);require(e->setControlFrame(frame),"full frame rejected");e->setModernConfig(config);e->reset();for(int i=0;i<8;++i)e->noteOn(notes[i],.8f,1);}
  for(unsigned b=0;b<16;++b){require(first->setControlFrame(frame)&&second->setControlFrame(frame),"warm frame rejected");first->setTransport({120.,double(b)*64.*2./48000.,true,true});ModernLayerState::mirrorToEngine(*first,*second);first->render(pcmA.data(),64);second->render(pcmB.data(),64);}
  first->setTransport({120.,16.*64.*2./48000.,true,true});ModernLayerState::mirrorToEngine(*first,*second);validate();
 }
 void validate(){require(first->activeVoices()==8&&second->activeVoices()==8,"not sixteen active voices");require(configHash(first->getModernConfig())==configHash(second->getModernConfig()),"config mismatch");require(transportHash(first->getModernTransport())==transportHash(second->getModernTransport()),"transport mismatch");for(Engine* e:{first.get(),second.get()})for(int i=0;i<8;++i){auto v=e->getVoiceInfo(i);require(v.active&&v.held&&v.note==notes[i]&&v.channel==1,"voice identity mismatch");}}
 std::uint64_t continuation(){first->render(pcmA.data(),64);second->render(pcmB.data(),64);validate();std::uint64_t h=1469598103934665603ull;bool nonzero=false;for(auto* p:{&pcmA,&pcmB})for(float f:*p){auto b=std::bit_cast<std::uint32_t>(f);require((b&0x7f800000u)!=0x7f800000u,"nonfinite PCM");nonzero|=(b&0x7fffffffu)!=0;h=hash(h,b);}require(nonzero,"silent continuation");return h;}
};
template<bool Extra> __attribute__((noinline)) void run(Pair& main,Pair& shadow,unsigned n){for(unsigned i=0;i<n;++i){ModernLayerState::mirrorToEngine(*main.first,*main.second);if constexpr(Extra)if((i&7u)==7u)ModernLayerState::mirrorToEngine(*shadow.first,*shadow.second);}}
unsigned clockReads=0;
std::uint64_t now(clockid_t id){++clockReads;timespec t{};require(clock_gettime(id,&t)==0,"clock failed");return std::uint64_t(t.tv_sec)*1000000000ull+t.tv_nsec;}
struct Timing{std::uint64_t cb,wb,we,ce;void begin(){cb=now(CLOCK_THREAD_CPUTIME_ID);wb=now(CLOCK_MONOTONIC);}void end(){we=now(CLOCK_MONOTONIC);ce=now(CLOCK_THREAD_CPUTIME_ID);}void dump(){std::cout<<"{\"cpu_begin_ns\":"<<cb<<",\"wall_begin_ns\":"<<wb<<",\"wall_end_ns\":"<<we<<",\"cpu_end_ns\":"<<ce<<"}";}};
}
int main(int argc,char** argv){using namespace isolated;if(argc<4||argc>5)return 2;const bool timed=std::string(argv[1])=="--time";if(!timed&&std::string(argv[1])!="--check")return 2;bool dense=std::string(argv[2])=="dense";if(!dense&&std::string(argv[2])!="default")return 2;const bool extra=std::string(argv[3])=="extra";if(!extra&&std::string(argv[3])!="plain")return 2;if(timed){const char* auth=std::getenv("MIRROR_COST_AUTH");if(argc!=5||!auth||std::strlen(auth)!=64||std::strcmp(auth,argv[4]))return 3;}else if(argc!=4)return 2;
 try{cpu_set_t mask;CPU_ZERO(&mask);CPU_SET(0,&mask);require(sched_setaffinity(0,sizeof mask,&mask)==0&&sched_getcpu()==0,"CPU0 affinity failed");fesetenv(FE_DFL_ENV);_mm_setcsr(0x9fc0);errno=0;Pair main(dense),shadow(dense);const unsigned count=timed?iterations:4096;
  std::array<Timing,3> empty{};if(timed)for(auto& t:empty){t.begin();t.end();}
  const auto before=fp();Timing t{};if(timed)t.begin();if(extra)run<true>(main,shadow,count);else run<false>(main,shadow,count);if(timed)t.end();const auto after=fp();require(sched_getcpu()==0,"CPU migrated");require((after.mxcsr&0xffc0)==0x9fc0&&after.cw==0x037f,"FP controls changed");main.validate();shadow.validate();const auto config=configHash(main.second->getModernConfig()),transport=transportHash(main.second->getModernTransport());const auto pcm=main.continuation(),shadowPcm=shadow.continuation();
  std::cout<<"{\"schema\":1,\"status\":\"PASS\",\"scene\":\""<<argv[2]<<"\",\"extra\":"<<(extra?"true":"false")<<",\"timed\":"<<(timed?"true":"false")<<",\"main_calls\":"<<count<<",\"shadow_calls\":"<<(extra?count/8:0)<<",\"engine_size\":"<<sizeof(Engine)<<",\"config_hash\":"<<config<<",\"transport_hash\":"<<transport<<",\"pcm_hash\":"<<pcm<<",\"shadow_pcm_hash\":"<<shadowPcm<<",\"before_fp\":";dumpFp(before);std::cout<<",\"after_fp\":";dumpFp(after);std::cout<<",\"cpu\":0,\"clock_reads\":"<<clockReads<<",\"timing\":";if(timed)t.dump();else std::cout<<"null";std::cout<<",\"empty_clocks\":[";if(timed)for(unsigned i=0;i<3;++i){if(i)std::cout<<',';empty[i].dump();}std::cout<<"]}\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
