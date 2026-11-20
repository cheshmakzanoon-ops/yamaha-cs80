#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <time.h>
#include "pstock/core/EngineWscHeld.h"
#define cs80 cs80_candidate
#include "pcandidate/core/EngineWscHeld.h"
#undef cs80
template<class O,class Control,class A> __attribute__((noinline)) void trial(unsigned rounds,unsigned r,int tag,bool measured)
{
 std::array<O,16> osc;std::array<double,16> hz;
 std::array<Control,16> ctl;
 for(int i=0;i<16;++i) {
  hz[i]=220.+7.25*i;ctl[i].pulseThresholdVolts=(i%3-1)*.25;
  ctl[i].sawEnabled=i%3!=0;ctl[i].pulseEnabled=i%4!=0;
  osc[i].reset(double(i%11)/11.,hz[i]);for(int j=0;j<1024;++j)osc[i].tick(hz[i],ctl[i]);
 }
 std::array<double,3> sum{};unsigned borrowed=0;timespec t0{},t1{};
 if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t0)!=0)std::abort();
 for(unsigned j=0;j<rounds;++j)for(int i=0;i<16;++i) {
  auto& o=osc[i];const auto f=hz[i];const auto& c=ctl[i];const auto yes=A::canBorrow(o,f,c);
  const auto v=yes?A::run(o,f):o.tick(f,c);borrowed+=yes;
  sum[0]+=v.sawPin5;sum[1]+=v.pulsePin7;sum[2]+=v.sinePin2;
 }
 if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t1)!=0)std::abort();
 if(measured)std::printf("%u,%d,%u,%u,%lld,%016lx/%016lx/%016lx\n",r,tag,rounds,borrowed,(long long)(t1.tv_sec-t0.tv_sec)*1000000000ll+t1.tv_nsec-t0.tv_nsec,std::bit_cast<unsigned long>(sum[0]),std::bit_cast<unsigned long>(sum[1]),std::bit_cast<unsigned long>(sum[2]));
}
int main(){std::mt19937 rng(0x824b);
 for(unsigned i=0;i<33;++i){
  std::array<int,5> tags{0,1,2,3,4};std::shuffle(tags.begin(),tags.end(),rng);
  for(int tag:tags){
    const unsigned rounds=(i?32768:2048)*(tag==4?9:8)/8;
    if(tag==2)trial<cs80_candidate::ReconstructedOscillator,cs80_candidate::WscControls,cs80_candidate::EngineWscHeldAccess>(rounds,i,tag,i);
    else trial<cs80::ReconstructedOscillator,cs80::WscControls,cs80::EngineWscHeldAccess>(rounds,i,tag,i);
  }
  fflush(stdout);
 }
}
