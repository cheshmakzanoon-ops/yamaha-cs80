#define main original_benchmark_not_called
#include "tools/source_factory_benchmark.cpp"
#undef main
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <sched.h>
static std::uint64_t ns(){timespec t{};if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t))std::abort();return std::uint64_t(t.tv_sec)*1000000000ull+t.tv_nsec;}
int main(int argc,char**argv){
 if(argc!=2)return 2;int i=std::atoi(argv[1]);if(i<0||i>=7)return 2; const char*labels[]={"factory-quiet","panel-quiet","factory-held","factory-moving","panel-held","pure-held","legacy-held"};
 ScopedDenormals fp;Case c;c.rate=48000;c.block=64;c.selector=i==1||i==4?13:0;c.isolated=i==5;c.sourcePerformance=i!=5&&i!=6;c.performanceProfile=i==3?1:0;
 auto frame=makeFrame(c,c.selector);frame.effectiveValues[P_drift]=.37f;frame.effectiveValues[P_voiceDispersion]=.18f;frame.effectiveValues[P_driftModel]=0.f;Engine eng;eng.prepare(c.rate);
 if(!eng.setControlFrame(frame))return 4;eng.reset();
 if(i>=2)for(int v=0;v<8;++v)eng.noteOn(notes[v],.8f,i==3?v+2:1);
 const int start=sched_getcpu();std::array<float,128> out{};std::uint64_t total=0,base=0,hash=1469598103934665603ull;unsigned nz=0,active=0;
 for(int b=-128;b<1024;b++){
   if(i==3&&b%4==0){int phase=b+128;int v=(phase/4)%8;float x=float((phase*13)%97)/96.f;eng.channelPressure(v+2,x);eng.pitchBend(v+2,x*.1f);eng.controlChange(74,x,v+2);}
   if(b==128||b==384){if(!eng.setControlFrame(frame))return 4;}
   if(b<0){eng.render(out.data(),64);continue;}
   auto a=ns();eng.render(out.data(),64);auto z=ns();if(z<=a)return 5;total+=z-a;
   for(float x:out){if(!std::isfinite(x))return 6;nz+=x!=0;hash^=std::bit_cast<std::uint32_t>(x);hash*=1099511628211ull;}
   active+=unsigned(eng.activeVoices());
 }for(int j=0;j<1024;j++){auto a=ns();auto b=ns();if(b<=a)return 7;base+=b-a;}
 const int end=sched_getcpu();if(start!=end||start<0||!total)return 9;
 std::printf("%s,%d,%d,%llu,%llu,%016llx,%u,%u\n",labels[i],start,end,(unsigned long long)total,(unsigned long long)base,(unsigned long long)hash,nz,active);return 0;
}
