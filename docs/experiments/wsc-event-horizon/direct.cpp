#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cfenv>
#include <cerrno>
#include <cmath>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <xmmintrin.h>
#include "gen/stock.h"
#define cs80 cs80_candidate
#include "gen/candidate.h"
#undef cs80

using S=cs80::ReconstructedOscillator;
using C=cs80_candidate::ReconstructedOscillator;
static uint64_t b(double d) {return std::bit_cast<uint64_t>(d);}
template<class A,class B> bool out(const A& a,const B& o) {
    return b(a.sawPin5)==b(o.sawPin5)&&b(a.sinePin2)==b(o.sinePin2)&&b(a.pulsePin7)==b(o.pulsePin7);
}
template<class A,class B> bool state(const A&a,const B&o) {
    return b(a.charge)==b(o.charge)&&b(a.resetRemainingSeconds)==b(o.resetRemainingSeconds)
     &&b(a.inputBaselineVolts)==b(o.inputBaselineVolts)&&b(a.triangleBaselineVolts)==b(o.triangleBaselineVolts)
     &&b(a.heldFrequencyHz)==b(o.heldFrequencyHz)&&b(a.heldThresholdVolts)==b(o.heldThresholdVolts)
     &&b(a.elapsedSeconds)==b(o.elapsedSeconds)&&a.sawEnabled==o.sawEnabled&&a.pulseEnabled==o.pulseEnabled
     &&a.residualCursor==o.residualCursor&&a.resetCount==o.resetCount
     &&!std::memcmp(a.residual.data(),o.residual.data(),sizeof(a.residual));
}
template<class A,class B> bool diag(const A&a,const B&o) {
    if(a.invalidControl!=o.invalidControl||a.frequencyLimited!=o.frequencyLimited||a.eventCapacityExceeded!=o.eventCapacityExceeded||a.eventCount!=o.eventCount||
       !out(a.raw,o.raw)||b(a.rawPin7Volts)!=b(o.rawPin7Volts)||b(a.coupledInputVolts)!=b(o.coupledInputVolts)||
       b(a.trianglePin3Volts)!=b(o.trianglePin3Volts)||b(a.shaperInputVolts)!=b(o.shaperInputVolts))return false;
    for(size_t i=0;i<a.events.size();++i) {
        const auto&x=a.events[i]; const auto&y=o.events[i];
        if(b(x.secondsFromSampleStart)!=b(y.secondsFromSampleStart)||b(x.absoluteSeconds)!=b(y.absoluteSeconds)||
            b(x.charge)!=b(y.charge)||x.kind!=y.kind||x.resetActive!=y.resetActive||!out(x.before,y.before)||!out(x.after,y.after))return false;
    }
    return true;
}
#define OBS(NS) namespace NS { struct HorizonObserver {static auto nodes(const ReconstructedOscillator&o){const auto&n=o.endNodes; return std::pair{std::array<double,20>{n.pin7,n.u,n.uSlope,n.triangle,n.triangleSlope,n.w,n.wSlope,n.triangleGain,n.value.sawPin5,n.value.pulsePin7,n.value.sinePin2,n.slope.sawPin5,n.slope.pulsePin7,n.slope.sinePin2,n.constant.sawPin5,n.constant.pulsePin7,n.constant.sinePin2,n.ungatedValue.sawPin5,n.ungatedValue.pulsePin7,n.ungatedValue.sinePin2},o.endNodesValid};} };}
OBS(cs80) OBS(cs80_candidate)
struct Flags { unsigned mx; unsigned short xc,xs; int err; bool operator==(const Flags&) const=default; };
static Flags flags(){ Flags z={_mm_getcsr(),0,0,errno};asm volatile("fnstcw %0":"=m"(z.xc)); asm volatile("fnstsw %0":"=m"(z.xs));return z;}
static void initFp(unsigned mx,unsigned short cw=0x37f) { asm volatile("fnclex");asm volatile("fldcw %0"::"m"(cw));_mm_setcsr(mx);errno=19; }
static long long checks=0, failures=0, pairs=0;
static void ck(bool x,const char* why,int scene,int i){++checks;if(!x){if(failures<8)std::fprintf(stderr,"FAIL %s scene=%d i=%d\n",why,scene,i);++failures;}}
static void compare(S&s,C&c,int scene,int i) {
 ck(state(s.getState(),c.getState()),"continuation",scene,i);ck(diag(s.getDiagnostics(),c.getDiagnostics()),"diagnostics/full stale events",scene,i);
 const auto l=cs80::HorizonObserver::nodes(s),r=cs80_candidate::HorizonObserver::nodes(c);
 ck(l.second==r.second&&!std::memcmp(l.first.data(),r.first.data(),sizeof(l.first)),"private right cache",scene,i);
}
static const char* labels[]={"held-220","held-440","drift-steps/PWM","threshold-runs","near-knots","resets-idle-restore-copy","FP control switches","no-precision","other-rate","other-profile","direction-round","random-segmented","subnormal-FTZ","zero-stop"};
int main() {
 const unsigned savedMx=_mm_getcsr();unsigned short savedX;asm volatile("fnstcw %0":"=m"(savedX));
 // Inputs (including libm/rng) and diagnostic comparison occur outside measured FP environments.
 int nsc=sizeof(labels)/sizeof(*labels); uint64_t totalFast=0, seenFalse=0;
 for(int scene=0;scene<nsc;++scene) {
  initFp(0x1fa0);
  S s; C c; cs80::OscillatorNominalProfile sp;cs80_candidate::OscillatorNominalProfile cp;
  if(scene==9){sp.sliceVoltsInferred=cp.sliceVoltsInferred=.03125;}
  double rate=scene==8?88200.:96000.; s.prepare(rate,sp); c.prepare(rate,cp);
  double f=scene==1?440.:220.,thr=0.;s.reset(.37,f);c.reset(.37,f);
  uint64_t fast0=C::horizonFast,slow0=C::horizonSlow, grants0=C::horizonGrants;
  uint64_t rnd=uint64_t(scene+11);auto next=[&]{rnd=rnd*6364136223846793005ULL+1;return rnd;}; bool saw=true,pulse=true;
  int n=8192;
  for(int i=0;i<n;++i) {
   initFp(0x1fa0);
   if(scene==2){ f=220.+((i/7)%16)*0.03125;thr=.6*std::sin((i/3)*.015);}
   if(scene==3){thr=(int(i/13)%13-6)*.125; saw=(i/71)%2==0;pulse=(i/59)%3!=0;}
   if(scene==4&&i%23==0){auto g=s.getState();f=g.heldFrequencyHz;thr=s.getDiagnostics().coupledInputVolts+((i/23)%3-1)*1e-11;}
   if(scene==5) {
    if(i>0&&i%113==0){auto a=s.getState();auto d=c.getState();s.restoreState(a);c.restoreState(d);}
    if(i%179==0){auto sa=s.getIdleState();auto ca=c.getIdleState();s.restoreIdleState(sa);c.restoreIdleState(ca);}
    if(i%191==0){S sc(s);C ca(c);s=sc;c=ca;}
    if(i%233==0){auto z=double(i%251)/251.;s.reset(z,f);c.reset(z,f);}
    if(i%307==0){cs80::WscControls sc{thr,saw,pulse};cs80_candidate::WscControls cc{thr,saw,pulse};
       initFp(0x9fe0);double a=s.advanceIdle(16./96000.,f,sc);auto af=flags();initFp(0x9fe0);double d=c.advanceIdle(16./96000.,f,cc);auto df=flags();ck(b(a)==b(d)&&af==df,"idle",scene,i);}
   }
   if(scene==11&&i%97==0){f=16.+(next()%2032);thr=double(int(next()%9001)-4500)/1024.;}
   if(scene==12&&i%191==0){thr=std::bit_cast<double>(uint64_t(1)); if(i%382==0)thr=0.;}
   if(scene==13){f=(i/67)%2?0.:220.;}
   unsigned mx=i%2?0x9fe0:0x1fa0; unsigned short cw=0x37f;
   if(scene==6){constexpr unsigned ctl[]={0x1fa0,0x9fe0,0x1f80,0x5fa0,0x3fa0,0x7fa0,0x1fc0,0x9f80}; mx=ctl[(i/41)%8];if(i%127>90)cw=0x077f;}
   if(scene==7)mx=0x1f80;
   if(scene==10)mx=0x3fa0;
   cs80::WscControls sc{thr,saw,pulse};cs80_candidate::WscControls cc{thr,saw,pulse};
   initFp(mx,cw);auto a=s.tick(f,sc);auto af=flags();initFp(mx,cw);auto d=c.tick(f,cc);auto df=flags();++pairs;
   ck(out(a,d),"three pins",scene,i);ck(af==df,"FP status/controls/errno",scene,i);
   compare(s,c,scene,i);
  }
  auto delta=C::horizonFast-fast0;totalFast+=delta;
  if(scene==7||scene==8||scene==9||scene==10)ck(delta==0,"must stay stock",scene,0);
#if defined(CS80_WSC_HORIZON_EXPECT_OFF)
  ck(delta==0,"disabled build must not admit",scene,0);
#else
  if(scene==0||scene==1)ck(delta>0,"positive activation",scene,0);
#endif
  std::printf("%s %d fast=%llu stock=%llu grants=%llu\n",labels[scene],n,(unsigned long long)delta,(unsigned long long)(C::horizonSlow-slow0),(unsigned long long)(C::horizonGrants-grants0));
 }
#ifdef CS80_WSC_HORIZON_VERIFY
 seenFalse=C::horizonFalseWitnesses; ck(seenFalse==0,"actual root/physical contradicts certificate",-1,0);
#endif
 std::printf("pairs=%lld checks=%lld failures=%lld skipped=%llu witnessDisagreements=%llu\n",pairs,checks,failures,(unsigned long long)totalFast,(unsigned long long)seenFalse);
 _mm_setcsr(savedMx);asm volatile("fldcw %0"::"m"(savedX));
 return failures?1:0;
}
