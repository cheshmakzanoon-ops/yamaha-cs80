#include "core/SourceFactoryTones.h"
#include "candidate-function.h"
#include <bit>
#include <cfenv>
#include <cerrno>
#include <cstdio>
#include <limits>
#include <vector>
#include <immintrin.h>
namespace s=cs80::source_factory;
void add(std::vector<uint64_t>& v,double x){v.push_back(std::bit_cast<uint64_t>(x));}
template<class T,size_t N> void add(std::vector<uint64_t>& v,const std::array<T,N>& a){for(auto& x:a)add(v,x);}
std::vector<uint64_t> bits(const s::SharedReadout& r){std::vector<uint64_t> v;add(v,r.qVolts);add(v,r.q);add(v,r.levelPinVolts);add(v,r.levelGain);add(v,r.qBusVolts);add(v,r.levelBusVolts);add(v,r.masterResVolts);v.push_back(r.saturatedVoices);v.push_back(r.masterResSaturated);return v;}
__attribute__((noinline)) bool base(const std::array<s::RankBoundary,2>& b,double m,const s::BothRankDrives& d,s::SharedReadout& r){return s::evaluateShared(b,m,d,r);}
__attribute__((noinline)) bool cand(const std::array<s::RankBoundary,2>& b,double m,const s::BothRankDrives& d,s::SharedReadout& r){return s::evaluateSharedCandidate(b,m,d,r);}
uint64_t rng=0x650128ab12345;double rand01(){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return double(rng>>11)*0x1p-53;}
unsigned short status(){unsigned short x;asm volatile("fnstsw %0":"=am"(x));return x;}
int main(){const unsigned original=_mm_getcsr();unsigned count=0,invalid=0;for(int rounding:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO})for(unsigned denorm:{0u,0x40u,0x8000u,0x8040u}){
std::fesetround(rounding);_mm_setcsr((_mm_getcsr()&~0x8040u)|denorm);
for(int n=0;n<8192;++n){std::array<s::RankBoundary,2>b;for(auto& x:b){x.qHp={10*rand01(),10000*rand01()};x.qLp={10*rand01(),10000*rand01()};x.level={10*rand01(),10000*rand01()};x.initialLevelVolts=10*rand01();x.afterLevelVolts=10*rand01();}s::BothRankDrives d;for(auto& rank:d)for(auto& x:rank)x={8*rand01()-4,2*rand01()-.5,2*rand01()-.5};double m=2*rand01()-.5;
const double nan=std::numeric_limits<double>::quiet_NaN();
if(n%32==0)d[(n/32)%2][(n/64)%8].pressure=nan;if(n%32==1)m=nan;if(n%32==2)b[1].qHp.ohms=-1;if(n%32==3)b[0].qLp.volts=11;if(n%32==4)b[1].afterLevelVolts=nan;if(n%32==5)m=-0.;if(n%32==6)m=1.;if(n%32==7)b[0].qHp={0.,0.};if(n%32==8)b[1].qLp={10.,10000.};if(n%32==9)d[0][0].filterEnvelopeZ=std::numeric_limits<double>::denorm_min();
if(n%32==10)d[(n/32)%2][(n/64)%8].velocity=std::bit_cast<double>(uint64_t(0x7ff0000000000001));if(n%32==11)d[(n/32)%2][(n/64)%8].filterEnvelopeZ=std::numeric_limits<double>::infinity();
s::SharedReadout a{},c{};a.masterResVolts=c.masterResVolts=123.;a.saturatedVoices=c.saturatedVoices=123;
const auto initial=bits(a);std::feclearexcept(FE_ALL_EXCEPT);std::feraiseexcept((n%3==0)?FE_INVALID:((n%3==1)?FE_INEXACT|FE_UNDERFLOW:0));errno=17;bool ar=base(b,m,d,a);unsigned am=_mm_getcsr();auto af=status();int ae=errno;
std::feclearexcept(FE_ALL_EXCEPT);std::feraiseexcept((n%3==0)?FE_INVALID:((n%3==1)?FE_INEXACT|FE_UNDERFLOW:0));errno=17;bool cr=cand(b,m,d,c);unsigned cm=_mm_getcsr();auto cf=status();int ce=errno;
if(ar!=cr||bits(a)!=bits(c)||am!=cm||af!=cf||ae!=ce){std::printf("FAIL n=%d round=%d denorm=%u flags=%u/%u\n",n,rounding,denorm,am,cm);return 1;}++count;if(!ar){++invalid;if(bits(a)!=initial||bits(c)!=initial)return 2;}
}}
std::fesetround(FE_TONEAREST);_mm_setcsr(original);std::printf("PASS %u paired readouts, %u transactional invalid cases; full bits, MXCSR, x87 status, errno equal\n",count,invalid);}
