// Diagnostic for the strict GCC 14.2 x86-64 uniform receiver transaction.
// The public scalar solver is the independent oracle. Failure must preserve
// the incoming result. No private path, PCM capture or plugin is required.
#include "core/Engine.h"
#include "core/UniformSourceCutoff.h"
#include <emmintrin.h>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cfenv>
#include <cerrno>
#include <vector>
namespace cs80 {
#include "core/SharedUniformReceivers.cpp.inc"
}
#if !defined(CS80_DETAIL_UNIFORM_RECEIVERS)
int main() { std::fputs("Build has no qualified uniform receiver entry.\n", stderr); return 77; }
#else
int main() {
 namespace sf=cs80::source_factory;
 using R=sf::SharedReadout;
 static_assert(std::is_trivially_copyable_v<R>);
 const double sNaN=std::bit_cast<double>(0x7ff0000000000011ULL);
 const double qNaN=std::bit_cast<double>(0x7ff8000000000011ULL);
 const double denorm=std::bit_cast<double>(1ULL);
 double values[]={0.,-0.,denorm,-denorm,.435678901234,-.31415926,1.,3.,-3.,7.,-15.,1e308,-1e308,sNaN,qNaN,INFINITY,-INFINITY};
 std::uint64_t n=0,mismatch=0,rawMismatch=0,errors=0,rejected=0;
 unsigned seed=11421;auto rng=[&](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
 auto val=[&](){return values[rng()%(sizeof(values)/sizeof(values[0]))];};
 auto finite=[&](){return double(rng()%9999)/9999.;};
 for(unsigned k=0;k<65536;++k) {
  std::array<sf::RankBoundary,2> bs;sf::BothRankDrives ds;
  for(int r=0;r<2;++r){
   auto& b=bs[r];b={{finite()*10,finite()*9999},{finite()*10,finite()*9999},{finite()*10,finite()*9999},finite()*10,finite()*10};
   auto& row=ds[r];sf::VoiceDrives dr={finite(),finite(),finite()};
   if(k%7!=0){const unsigned which=rng()%3;if(which==0)dr.filterEnvelopeZ=val();else if(which==1)dr.velocity=val();else dr.pressure=val();}
   if(k%13==0){switch(rng()%4){case 0:b.initialLevelVolts=val();break;case 1:b.qHp.volts=val();break;case 2:b.level.ohms=val();break;default:b.qLp.ohms=val();}}
   row.fill(dr);
  }
  double m=k%6==0?val():finite();
  // Public earlier output contains nonzero values; reserve distinctive tail bytes
  // to distinguish numerical/public state from compiler-dependent ABI padding.
  R x{},y{};std::memset(&x,0x3a,sizeof x);std::memset(&y,0x3a,sizeof y); x.masterResSaturated=y.masterResSaturated=(k&1);
  const unsigned csr=0x1f80u | ((k&1u)?0x8000u:0) | ((k&2u)?0x40u:0) | ((k>>2u)&0x3fu);
  std::fesetenv(FE_DFL_ENV);_mm_setcsr(csr);errno=21;bool ok=sf::evaluateShared(bs,m,ds,x);const auto mx1=_mm_getcsr();const auto e1=errno;unsigned short sw1;asm volatile("fnstsw %0":"=m"(sw1)::"memory");
  std::fesetenv(FE_DFL_ENV);_mm_setcsr(csr);errno=21;bool ok2=cs80::evaluateUniformReceivers(bs,m,ds,y);const auto mx2=_mm_getcsr();const auto e2=errno;unsigned short sw2;asm volatile("fnstsw %0":"=m"(sw2)::"memory");
  ++n;if(!ok)++rejected;if(ok!=ok2||mx1!=mx2||e1!=e2||sw1!=sw2){if(!errors)std::printf("flagsFirst %u %x %x %d %d %d %d\n",k,mx1,mx2,e1,e2,int(ok),int(ok2));++errors;}
  if(std::memcmp(&x,&y,offsetof(R,masterResSaturated)+1)){if(!mismatch)std::printf("first semantic mismatch %u\n",k);++mismatch;}
  if(std::memcmp(&x,&y,sizeof(R)))++rawMismatch;
 }
 std::printf("cases %llu rejected %llu flags-errors %llu value-mismatch %llu raw-object-difference %llu semantic bytes %zu object %zu\n",(unsigned long long)n,(unsigned long long)rejected,(unsigned long long)errors,(unsigned long long)mismatch,(unsigned long long)rawMismatch,offsetof(R,masterResSaturated)+1,sizeof(R));return errors || mismatch || rawMismatch || rejected != 27337;
}
#endif
