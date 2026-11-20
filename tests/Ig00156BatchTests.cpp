#include "core/Ig00156Batch.h"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
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
// A quartet can name unrelated stages in any order. Exercise every legal
// byte offset modulo 64 on this ABI, including rows crossing a cache line.
// Guard bytes flank each live Stage; no aligned-load or neighboring-object
// assumption is part of the batch API.
void independentStageLayouts(Batch::Kernel kernel) {
 constexpr std::size_t line = std::max<std::size_t>(64,alignof(Stage));
 struct alignas(line) Slot { unsigned char bytes[sizeof(Stage)+3*line]; };
 for(std::size_t shift=0; shift<line; shift+=alignof(Stage)) {
  std::array<Slot,4> slots;
  std::array<std::size_t,4> offsets;
  std::array<Stage*,4> actual;
  auto expected=makeStages();
  for(std::size_t i=0;i<4;++i) {
   auto& bytes=slots[i].bytes;
   std::fill(std::begin(bytes),std::end(bytes),0xa5);
   offsets[i]=line+(shift+i*alignof(Stage))%line;
   actual[i]=std::construct_at(reinterpret_cast<Stage*>(bytes+offsets[i]),expected[i]);
   const float cutoff=460.f+1234.f*static_cast<float>(i);
   const float q=.75f+static_cast<float>(i);
   actual[i]->set(cutoff,q,96000.f);
   expected[i].set(cutoff,q,96000.f);
  }
  std::array<int,4> order{0,1,2,3};
  constexpr Batch::Inputs input{.03125f,-.1875f,.4375f,-.5625f};
  do {
   Batch::Stages group;
   Batch::Outputs got,want;
   for(std::size_t j=0;j<4;++j) {
    group[j]=actual[order[j]];
    want[j]=expected[order[j]].process(input[j]);
   }
   const bool used=cs80::processIg00156Four(group,input,got,kernel);
   check(used==(kernel!=nullptr && Batch::environmentSupported()),"layout test executes selected path");
   accelerated+=used;
   for(std::size_t j=0;j<4;++j) {
    check(same(got[j].lp,want[j].lp) && same(got[j].hp,want[j].hp) && same(got[j].bp,want[j].bp),
          "permuted stage outputs retain lane identity");
    stateEqual(*actual[j],expected[j]);
   }
  } while(std::next_permutation(order.begin(),order.end()));
  for(std::size_t i=0;i<4;++i) {
   const auto& bytes=slots[i].bytes;
   auto intact=[](unsigned char value) {return value==0xa5;};
   check(std::all_of(std::begin(bytes),bytes+offsets[i],intact) &&
         std::all_of(bytes+offsets[i]+sizeof(Stage),std::end(bytes),intact),"stage layout guard bytes untouched");
   std::destroy_at(actual[i]);
  }
 }
}
// A quartet can contain two opposite saturated nodes beside polynomial
// lanes. The slope cache exposes the signed OTA endpoint independently of
// the public float output; a signless +1 clamp is not an exact substitute.
void mixedOtaEndpoints(Batch::Kernel kernel) {
 auto actual=makeStages(),scalar=makeStages();
 constexpr std::array<double,4> charge{48.0,-48.0,.03125,-.125};
 for (std::size_t lane=0;lane<charge.size();++lane) {
  auto m=actual[lane].getMemory();
  m.z.bp=charge[lane];
  actual[lane].restoreMemory(m); scalar[lane].restoreMemory(m);
  actual[lane].set(300.f,1.f,96000.f); scalar[lane].set(300.f,1.f,96000.f);
 }
 constexpr Batch::Inputs input{.125f,-.25f,.03125f,-.0625f};
 const bool used=compare(actual,scalar,input,kernel);
 check(used==(kernel!=nullptr && Batch::environmentSupported()),"mixed OTA fixture executes selected path");
 for (std::size_t lane=0;lane<charge.size();++lane) {
  const auto m=actual[lane].getMemory();
  const double x=m.v.bp*(1.0/cs80::kIg00156OtaLevel);
  if (lane<2) {
   check(lane==0 ? x>4.97 : x< -4.97,"opposite saturated OTA endpoints exercised");
   check(same(m.slope2,(lane==0 ? 1.0 : -1.0)/x),"cached slope uses correct endpoint sign");
  } else {
   check(std::abs(x)<4.97 && std::abs(x)>1.0e-9,"neighboring OTA lanes stay in the polynomial");
  }
 }
}
// The original nearest-rounding Newton loop retires lanes of this first
// quartet on iterations 0, 0, 1 and 2 respectively. In a frozen lane BP
// and the OTA/HP values computed from it must remain the *same bits*, not
// just close enough to the residual tolerance. Exercise every placement,
// both signs and the following sample's saved slope/history.
void staggeredNewtonConvergence(Batch::Kernel kernel) {
 const Batch::Inputs magnitudes{.01f,.05f,.35f,4.5f};
 std::array<int,4> order{0,1,2,3};
 do {
  for(float sign:{1.f,-1.f}) {
   std::array<Stage,4> actual;
   for(auto& stage:actual)stage.set(7000.f,.85f,96000.f);
   auto scalar=actual;
   Batch::Inputs x{};
   for(std::size_t j=0;j<4;++j)x[j]=sign*magnitudes[order[j]];
   const bool used=compare(actual,scalar,x,kernel);
   check(used==(kernel!=nullptr && Batch::environmentSupported()),"staggered convergence executes selected path");
   compare(actual,scalar,{-.37f,0.f,.19f,-.03125f},kernel);
  }
 } while(std::next_permutation(order.begin(),order.end()));
}
void dynamicAdmissionEdges(Batch::Kernel kernel) {
 if(kernel==nullptr) return;
 const double snan=std::bit_cast<double>(std::uint64_t{0x7ff0000000000001ULL});
 for(int lane=0;lane<4;++lane) for(int field=0;field<6;++field)
  for(double value:{0.0,-0.0,1.0,std::nextafter(1.0,0.0),std::nextafter(1.0,2.0),1024.0,-1024.0,std::nextafter(1024.0,0.0),
                   std::nextafter(1024.0,std::numeric_limits<double>::infinity()),
                   std::nextafter(-1024.0,-std::numeric_limits<double>::infinity()),
                   std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),
                   std::numeric_limits<double>::quiet_NaN(),snan,
                   std::numeric_limits<double>::denorm_min(),-std::numeric_limits<double>::denorm_min(),
                   std::numeric_limits<double>::min(),-std::numeric_limits<double>::min()}) {
   auto a=makeStages();prepare(a);
   auto m=a[lane].getMemory();
   double* fields[]{&m.z.input,&m.z.feedback,&m.z.bp,&m.z.lp,&m.slope1,&m.slope2};
   *fields[field]=value;a[lane].restoreMemory(m);prepare(a);
   const auto before=a;
   Batch::Outputs got;for(auto& out:got)out={123.f,456.f,789.f};
   const auto magnitude=std::bit_cast<std::uint64_t>(value)&0x7fffffffffffffffULL;
   const bool finiteBound=magnitude<=std::bit_cast<std::uint64_t>(1024.0);
   const bool allowed=finiteBound&&(field<4||(value>=0.0&&value<=1.0));
   std::feclearexcept(FE_ALL_EXCEPT);
   const bool used=kernel(pointers(a),{.1f,.2f,.3f,.4f},got);
   const int flags=std::fetestexcept(FE_INVALID);
   check(used==allowed,"dynamic admission boundary verdict");
   if(!allowed) {
    check(flags==0,"rejected dynamic state raises no invalid");
    for(int i=0;i<4;++i) {
     stateEqual(a[i],before[i]);
     check(got[i].lp==123.f&&got[i].hp==456.f&&got[i].bp==789.f,"rejected pack leaves outputs untouched");
    }
   }
  }
}
// At 1e6 the high word alone no longer distinguishes the inclusive
// coefficient limit from a slightly larger value. Exercise all lanes through
// the public network constructor, not private-field modifications.
void splitWordCoefficientEdges(Batch::Kernel kernel) {
#if CS80_IG00156_EXACT_BATCH_BUILD
 if(!kernel || !Batch::environmentSupported()) return;
 auto network=cs80::kIg00156HighPassNetwork; network.sourceOhms=65536.; // exact binary conductance; ordinary BP coefficient
 Stage model(network);
 model.set(300.f,.6f,96000.f);
 const double half=model.halfSteps().couplings;
 const double gs=1./network.sourceOhms;
 const double center=(half*gs)/1000000.;
 for(std::size_t lane=0;lane<4;++lane) for(int direction:{-1,0,1}) {
  auto a=makeStages(); prepare(a);
  network.inputFarads=direction<0?std::nextafter(center,0.):
                      direction>0?std::nextafter(center,std::numeric_limits<double>::infinity()):center;
  a[lane]=Stage(network);
  a[lane].set(300.f,.6f,96000.f);
  // Match the operations in Stage::set; gs is an exact power of two.
  const double actual=(a[lane].halfSteps().couplings/network.inputFarads)*gs;
  check(direction<0 ? actual>1000000. : direction>0 ? actual<1000000. : actual==1000000.,
        "custom clock straddles inclusive coupling limit");
  if(direction<0)check((std::bit_cast<std::uint64_t>(actual)>>32)==
                       (std::bit_cast<std::uint64_t>(1000000.)>>32),
                       "out-of-range coupling shares the legal endpoint high word");
  auto scalar=a;
  const bool used=compare(a,scalar,{.125f,-.25f,.375f,-.5f},kernel);
  check(used==(actual<=1000000.),"split-word coupling proof preserves endpoint and ordered fallback");
 }
#else
 (void)kernel;
#endif
}
// Exercise ordered static rejection through the public stage API. This
// synthetic network produces a finite subnormal feedforward coefficient;
// it is not an audio model or an accuracy tolerance.
void staticAdmissionOrder(Batch::Kernel kernel) {
#if CS80_IG00156_EXACT_BATCH_BUILD && BATCH_TEST_MXCSR
 if(!kernel || !Batch::environmentSupported())return;
 const unsigned saved=_mm_getcsr();
 const unsigned base=(saved&~(0xe040u|0x3fu))|0x1f80u;
 _mm_setcsr(base); // build subnormal coefficients with DAZ/FTZ disabled
 for(bool negative:{false,true}){
  auto network=cs80::kIg00156HighPassNetwork;
  network.integratorFarads=1.;
  network.feedforwardOhms=negative?-1.e306:1.e306;
  auto normal=makeStages();
  auto unusual=normal; unusual[0]=Stage(network); prepare(unusual);
  const double ff=unusual[0].halfSteps().integrators*
                  (1./(network.feedforwardOhms*network.integratorFarads));
  const auto bits=std::bit_cast<std::uint64_t>(ff);
  const auto magnitude=bits&0x7fffffffffffffffULL;
  check(magnitude>0 && magnitude<0x0010000000000000ULL && (bits>>63)==negative,
        "fixture has genuine signed subnormal static gain");
  prepare(normal);
  for(unsigned mode:{0u,0x40u,0x8000u,0x8040u})for(unsigned seed:{0u,0x15u})
   for(unsigned earlier=0;earlier<3;++earlier){
    // 0 valid pack; 1 a later bad input; 2 an earlier bad input. Start
    // with the already-prepared stage rather than recomputing under DAZ.
    auto a=normal; a[earlier==2?1:0]=unusual[0];const auto before=a;
    Batch::Inputs x{.1f,.2f,.3f,.4f};
    if(earlier)x[earlier==2?0:1]=std::bit_cast<float>(0x7f800001u);
    Batch::Outputs out;for(auto& y:out)y={123.f,456.f,789.f};
    _mm_setcsr(base|mode|seed);const bool used=kernel(pointers(a),x,out);const auto csr=_mm_getcsr();
    _mm_setcsr(base);
    const bool daz=(mode&0x40u)!=0;
    const bool allow=earlier==0&&(!negative||daz);
    check(used==allow,"ordered admission obeys DAZ for signed subnormal gain");
    check((csr&~63u)==((base|mode)&~63u)&&(csr&seed)==seed,
          "static eligibility preserves host controls and sticky seed");
    if(!used){
     check(((csr&2u)!=0)==(!daz&&earlier!=2),"prior denormal comparison observed only if executed");
     check((csr&1u)==(seed&1u),"rejected signalling NaN input is not compared or converted");
     for(int i=0;i<4;++i){
      stateEqual(a[i],before[i]);
      check(out[i].lp==123.f&&out[i].hp==456.f&&out[i].bp==789.f,
            "rejected static gate did not commit output");
     }
    }
   }
 }
 _mm_setcsr(saved);
#else
 (void)kernel;
#endif
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
  dynamicAdmissionEdges(kernel);
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
 movement(kernel);fallbackCases(kernel);independentStageLayouts(kernel);mixedOtaEndpoints(kernel);staggeredNewtonConvergence(kernel);dynamicAdmissionEdges(kernel);splitWordCoefficientEdges(kernel);staticAdmissionOrder(kernel);environments(kernel);
#if CS80_IG00156_EXACT_BATCH_BUILD
 __builtin_cpu_init();
 const bool capable = __builtin_cpu_supports("avx2");
 check((kernel != nullptr) == capable,"qualified build dispatch matches CPU capability");
 if(capable)check(accelerated>0,"capable qualified build genuinely exercises SIMD backend");
#else
 check(kernel == nullptr,"disabled or unsupported build remains scalar");
 check(accelerated == 0,"disabled or unsupported build accelerates no groups");
#endif
 std::printf("Ig00156Batch: %d checks, %d failures, %llu accelerated groups\n",checks,failures,
             static_cast<unsigned long long>(accelerated));
 return failures?1:0;
}
