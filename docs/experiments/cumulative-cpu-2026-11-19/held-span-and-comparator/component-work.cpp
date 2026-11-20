#include "core/EngineWscHeld.h"
#include <valgrind/callgrind.h>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
using O=cs80::ReconstructedOscillator;using A=cs80::EngineWscHeldAccess;
int main(int argc,char** argv) {
 std::array<O,16> osc;
 std::array<double,16> hz;
 std::array<cs80::WscControls,16> ctl;
 for(int i=0;i<16;++i) {
  hz[i]=220.+7.25*i;
  ctl[i].pulseThresholdVolts=(i%3-1)*0.25;ctl[i].sawEnabled=i%3!=0;ctl[i].pulseEnabled=i%4!=0;
  osc[i].reset(double(i%11)/11.,hz[i]);
  for(int j=0;j<1024;++j) osc[i].tick(hz[i],ctl[i]);
 }
 std::array<double,3> sum{};
 unsigned borrowed=0; int n=argc>1?std::atoi(argv[1]):256;
 CALLGRIND_START_INSTRUMENTATION;CALLGRIND_ZERO_STATS;
 for(int j=0;j<n;++j)for(int i=0;i<16;++i) {
  auto& o=osc[i];auto f=hz[i]; auto c=ctl[i]; auto yes=A::canBorrow(o,f,c);
  auto v=yes?A::run(o,f):o.tick(f,c); borrowed+=yes;
  sum[0]+=v.sawPin5;sum[1]+=v.pulsePin7;sum[2]+=v.sinePin2;
 }
 CALLGRIND_DUMP_STATS;CALLGRIND_STOP_INSTRUMENTATION;
 std::printf("n=%d borrowed=%u checksum=%016lx/%016lx/%016lx\n",n,borrowed,std::bit_cast<unsigned long>(sum[0]),std::bit_cast<unsigned long>(sum[1]),std::bit_cast<unsigned long>(sum[2]));
}
