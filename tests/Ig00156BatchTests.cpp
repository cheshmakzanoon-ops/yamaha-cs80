#include "core/Ig00156Batch.h"
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#define BATCH_TEST_MXCSR 1
#else
#define BATCH_TEST_MXCSR 0
#endif
namespace {
using Stage=cs80::Ig00156Stage;
using Batch=cs80::Ig00156Batch;
int checks=0,failures=0;
std::uint64_t accelerated=0;
void check(bool value,const char* message) {
 ++checks;if(!value){if(failures<20)std::fprintf(stderr,"FAIL %s\n",message);++failures;}
}
template<class T> bool same(T a,T b) {
 if constexpr(sizeof(T)==4)return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);
 else return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);
}
void stateEqual(const Stage& a,const Stage& b) {
 const auto x=a.getMemory(),y=b.getMemory();
 const double lhs[]{x.z.input,x.z.feedback,x.z.bp,x.z.lp,x.v.input,x.v.feedback,x.v.bp,x.v.lp,x.slope1,x.slope2,x.halfIntegrators,x.halfCouplings};
 const double rhs[]{y.z.input,y.z.feedback,y.z.bp,y.z.lp,y.v.input,y.v.feedback,y.v.bp,y.v.lp,y.slope1,y.slope2,y.halfIntegrators,y.halfCouplings};
 for(int i=0;i<12;++i)check(same(lhs[i],rhs[i]),"all Memory members bit-identical");
 const auto h=a.halfSteps(),k=b.halfSteps();
 check(same(h.integrators,k.integrators)&&same(h.couplings,k.couplings),"half-step API unchanged");
 check(same(a.qSiemens(),b.qSiemens()),"Q coefficient remains scalar-authoritative");
}
std::array<Stage,4> makeStages() {
 return {Stage(cs80::kIg00156HighPassNetwork),Stage(cs80::kIg00156LowPassNetwork),
         Stage(cs80::kIg00156HighPassNetwork),Stage(cs80::kIg00156LowPassNetwork)};
}
Batch::Stages pointers(std::array<Stage,4>& a) {return {&a[0],&a[1],&a[2],&a[3]};}
bool compare(std::array<Stage,4>& actual,std::array<Stage,4>& scalar,
             const Batch::Inputs& inputs,Batch::Kernel kernel,bool mustFallback=false) {
 Batch::Outputs expected,got;
 for(int i=0;i<4;++i)expected[i]=scalar[i].process(inputs[i]);
 const bool used=cs80::processIg00156Four(pointers(actual),inputs,got,kernel);
 accelerated+=used;
 if(mustFallback)check(!used,"unsupported case takes scalar fallback");
#if !CS80_IG00156_EXACT_BATCH_BUILD
 check(!used,"feature-disabled caller never executes SIMD");
#endif
 for(int i=0;i<4;++i){
  check(same(got[i].lp,expected[i].lp),"LP output bits");
  check(same(got[i].hp,expected[i].hp),"HP output bits");
  check(same(got[i].bp,expected[i].bp),"BP output bits");
  stateEqual(actual[i],scalar[i]);
 }
 return used;
}
// Keep fixture coefficient setup in one runtime context. Clang can otherwise
// fold the identical literal setup differently on opposite sides of a cache
// invalidation, so this test would compare different initial coefficients,
// not the scalar and packed processing of the same prepared stage. This is a
// test-only barrier; production arithmetic and compiler flags are unchanged.
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void prepare(std::array<Stage,4>& a,float rate=192000.f,bool unwarped=false) {
 for(int i=0;i<4;++i)a[i].set(500.f+i*3300.f,.5f+i*2.f,rate,unwarped);
}
void movement(Batch::Kernel kernel) {
 for(float initialRate:{8000.f,44100.f,88200.f,96000.f,192000.f,384000.f,768000.f,1536000.f}){
  auto a=makeStages(),b=makeStages();
  std::array<Stage::Memory,4> saveA{},saveB{};
  for(int n=0;n<2048;++n){
   for(int i=0;i<4;++i){
    if(n==711){saveA[i]=a[i].getMemory();saveB[i]=b[i].getMemory();}
    if(n==803){a[i].restoreMemory(saveA[i]);b[i].restoreMemory(saveB[i]);}
    if(n==1101){a[i].reset();b[i].reset();}
    const auto ha=a[i].halfSteps(),hb=b[i].halfSteps();
    const float rate=n>=1400?96000.f:initialRate;
    const bool unwarped=n>=1700&&n<1800;
    const float fc=n%97<40?4000.f:-100.f+float((n*31+i*973)%60000);
    const float q=-.5f+float((n/7+i*139)%1150)/100.f;
    a[i].set(fc,q,rate,unwarped);b[i].set(fc,q,rate,unwarped);
    if(n==1400||n==1700||n==1800){a[i].reReference(ha);b[i].reReference(hb);}
    if(n%13==0){a[i].set(fc,q,rate,unwarped);b[i].set(fc,q,rate,unwarped);}
   }
   Batch::Inputs x{};
   for(int i=0;i<4;++i)x[i]=float(std::sin(n*(.013+i*.119))*(i==0?.001:i==1?.25:i==2?2.5:8.));
   compare(a,b,x,kernel,n>=1700&&n<1800);
  }
 }
}
void fallbackCases(Batch::Kernel kernel) {
 check(Batch::selectKernel(false)==nullptr,"explicit scalar selection");
 for(float v:{0.f,-0.f,65.f,-65.f,std::numeric_limits<float>::max(),
              std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::quiet_NaN(),std::bit_cast<float>(0x7f800001U)}){
  auto a=makeStages(),b=makeStages();prepare(a);prepare(b);
  Batch::Inputs x{.1f,.2f,v,-.3f};compare(a,b,x,kernel,!std::isfinite(v)||std::abs(v)>64);
 }
 for(double unsafe:{1025.,-1025.,std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
  auto a=makeStages(),b=makeStages();prepare(a);prepare(b);
  auto m=a[1].getMemory();m.z.bp=unsafe;a[1].restoreMemory(m);b[1].restoreMemory(m);
  prepare(a);prepare(b);compare(a,b,{.1f,.2f,.3f,.4f},kernel,true);
 }
 for(double slope:{-.001,-1.,1.000001,2.,std::numeric_limits<double>::quiet_NaN()}){
  for(int which=0;which<2;++which){
   auto a=makeStages(),b=makeStages();prepare(a);prepare(b);
   auto m=a[2].getMemory();if(which==0)m.slope1=slope;else m.slope2=slope;
   a[2].restoreMemory(m);b[2].restoreMemory(m);prepare(a);prepare(b);
   compare(a,b,{.1f,.2f,.3f,.4f},kernel,true);
  }
 }
 {
  // Malformed network creates negative feedforward coefficient; do not run
  // the SIMD Newton assumptions on it, but preserve original scalar behavior.
  auto net=cs80::kIg00156LowPassNetwork;net.feedforwardOhms=-4.7e6;
  auto a=makeStages(),b=makeStages();a[1]=Stage(net);b[1]=Stage(net);
  prepare(a);prepare(b);compare(a,b,{.1f,.2f,.3f,.4f},kernel,true);
 }
 for(float rate:{7999.f,1536001.f,0.f,-0.f,-1.f,std::numeric_limits<float>::infinity()}){
  auto a=makeStages(),b=makeStages();prepare(a,rate);prepare(b,rate);
  compare(a,b,{.1f,.2f,.3f,.4f},kernel,true);
 }
 {auto a=makeStages(),b=makeStages();compare(a,b,{0.f,.1f,.2f,.3f},kernel,true);}
 {auto a=makeStages(),b=makeStages();prepare(a);prepare(b);compare(a,b,{.1f,.2f,.3f,.4f},nullptr,true);}
 // Repeated pointers require sequential processing, including later reads of updated state.
 for(auto indices:{std::array<int,4>{0,0,1,2},{0,1,0,1},{0,0,0,0}}){
  auto a=makeStages(),b=makeStages();prepare(a);prepare(b);
  Batch::Stages refs{&a[indices[0]],&a[indices[1]],&a[indices[2]],&a[indices[3]]};
  Batch::Inputs input{.3f,-.7f,2.5f,-8.f};Batch::Outputs want,got;
  for(int i=0;i<4;++i)want[i]=b[indices[i]].process(input[i]);
  check(!cs80::processIg00156Four(refs,input,got,kernel),"duplicate stages fall back sequentially");
  for(int i=0;i<4;++i){check(same(want[i].lp,got[i].lp)&&same(want[i].hp,got[i].hp)&&same(want[i].bp,got[i].bp),"duplicate-lane output bits");stateEqual(a[i],b[i]);}
 }
}
void environments(Batch::Kernel kernel) {
 const int original=std::fegetround();
 for(int mode:{FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){
  if(std::fesetround(mode)!=0)continue;
  auto a=makeStages(),b=makeStages();prepare(a);prepare(b);
  compare(a,b,{.1f,-.2f,2.5f,-8.f},kernel,true);
  check(std::fegetround()==mode,"rounding mode preserved");
 }
 (void)std::fesetround(original);
#if BATCH_TEST_MXCSR
 const unsigned saved=_mm_getcsr();
 {
  // Unmask divide-by-zero only, with a fresh all-zero signal that cannot
  // trigger it. The backend must decline even this benign group.
  const unsigned csr=((saved|0x1f80U)&~(0x6000U|0x3fU|0x200U));
  _mm_setcsr(csr);
  auto a=makeStages(),b=makeStages();prepare(a);prepare(b);
  compare(a,b,{0.f,0.f,0.f,0.f},kernel,true);
  check((_mm_getcsr()&~0x3fU)==(csr&~0x3fU),"unmasked-trap control preserved");
  _mm_setcsr(saved);
 }
 for(unsigned denorm:{0U,0x40U,0x8000U,0x8040U}){
  const unsigned csr=(saved&~0xe040U)|denorm|0x1f80U;
  _mm_setcsr(csr);
  auto a=makeStages(),b=makeStages();prepare(a);prepare(b);
  for(int i=0;i<4;++i){
   auto m=a[i].getMemory();m.z.input=std::numeric_limits<double>::denorm_min();m.z.lp=-std::numeric_limits<double>::min();
   a[i].restoreMemory(m);b[i].restoreMemory(m);
  }
  prepare(a);prepare(b);
  for(int n=0;n<32;++n)compare(a,b,{0.f,-0.f,std::numeric_limits<float>::denorm_min(),-.25f},kernel);
  check((_mm_getcsr()&~0x3fU)==(csr&~0x3fU),"MXCSR control bits including FTZ/DAZ unchanged");
 }
 _mm_setcsr(saved);
#endif
}
}
int main() {
 const auto kernel=Batch::selectKernel(true);
 movement(kernel);fallbackCases(kernel);environments(kernel);
#if CS80_IG00156_EXACT_BATCH_BUILD
 if(kernel)check(accelerated>0,"available SIMD backend genuinely exercised");
#endif
 std::printf("Ig00156Batch: %d checks, %d failures, %llu accelerated groups\n",checks,failures,
             static_cast<unsigned long long>(accelerated));
 return failures?1:0;
}
