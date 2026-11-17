#include "core/ReconstructedOscillator.h"
#define cs80 cs80_osc_profile_baseline
#include "fixtures/ReconstructedOscillatorPreProfileCache.h"
#undef cs80

#include <bit>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <limits>
#include <cfenv>
#include <cerrno>
#include <xmmintrin.h>

namespace {
bool countAllocations=false; std::size_t allocations=0;
int checks=0,failures=0; std::size_t comparedTicks=0;
double contextRate=0.;int contextRecipe=-1,contextFrame=-1;
void check(bool ok,const char* what) { ++checks;if(!ok){if(failures<16)std::fprintf(stderr,"FAIL: rate=%.0f profile=%d frame=%d: %s\n",contextRate,contextRecipe,contextFrame,what);++failures;} }
bool equal(double a,double b) { return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b); }
template<class A,class B> bool output(const A& a,const B& b) { return equal(a.sawPin5,b.sawPin5)&&equal(a.pulsePin7,b.pulsePin7)&&equal(a.sinePin2,b.sinePin2); }
template<class A,class B> bool state(const A& a,const B& b) {
    if(!equal(a.charge,b.charge)||!equal(a.resetRemainingSeconds,b.resetRemainingSeconds)
       ||!equal(a.inputBaselineVolts,b.inputBaselineVolts)||!equal(a.triangleBaselineVolts,b.triangleBaselineVolts)
       ||!equal(a.heldFrequencyHz,b.heldFrequencyHz)||!equal(a.heldThresholdVolts,b.heldThresholdVolts)
       ||!equal(a.elapsedSeconds,b.elapsedSeconds)||a.sawEnabled!=b.sawEnabled||a.pulseEnabled!=b.pulseEnabled
       ||a.residualCursor!=b.residualCursor||a.resetCount!=b.resetCount)return false;
    for(std::size_t branch=0;branch<a.residual.size();++branch)
        for(std::size_t i=0;i<a.residual[branch].size();++i)if(!equal(a.residual[branch][i],b.residual[branch][i]))return false;
    return true;
}
template<class A,class B> bool diagnostics(const A& a,const B& b) {
    if(a.invalidControl!=b.invalidControl||a.frequencyLimited!=b.frequencyLimited||a.eventCapacityExceeded!=b.eventCapacityExceeded
       ||a.eventCount!=b.eventCount||!equal(a.rawPin7Volts,b.rawPin7Volts)||!equal(a.coupledInputVolts,b.coupledInputVolts)
       ||!equal(a.trianglePin3Volts,b.trianglePin3Volts)||!equal(a.shaperInputVolts,b.shaperInputVolts)||!output(a.raw,b.raw))return false;
    // Include retained entries beyond eventCount: the public diagnostic object exposes them too.
    for(std::size_t i=0;i<a.events.size();++i){const auto& x=a.events[i];const auto& y=b.events[i];
        if(!equal(x.secondsFromSampleStart,y.secondsFromSampleStart)||!equal(x.absoluteSeconds,y.absoluteSeconds)||!equal(x.charge,y.charge)
           ||x.kind!=y.kind||x.resetActive!=y.resetActive||!output(x.before,y.before)||!output(x.after,y.after))return false;}
    return true;
}
template<class A,class B> bool complete(const A& a,const B& b) {
    return equal(a.sampleRate(),b.sampleRate())&&equal(a.maximumFrequencyHz(),b.maximumFrequencyHz())
        &&state(a.getState(),b.getState())&&diagnostics(a.getDiagnostics(),b.getDiagnostics());
}
template<class P> P profile(int recipe) {
    P p;
    switch(recipe){
    case 1:p.resetSecondsNominal=1e-6;p.referenceHzNominal=10000.;p.couplingSecondsNominal=.001;break;
    case 2:p.resetSecondsNominal=100e-6;p.referenceHzNominal=100.;p.couplingSecondsNominal=10.;break;
    case 3:p.resetVoltsNominal=.1;p.resetExcessVoltsNominal=.09;p.triangleSpanVoltsNominal=20.;p.sineGainVoltsInferred=10.;break;
    case 4:p.resetVoltsNominal=20.;p.resetExcessVoltsNominal=0.;p.triangleSpanVoltsNominal=.1;p.sineGainVoltsInferred=.1;break;
    case 5:p.sliceVoltsInferred=-10.;p.pulseHalfSpanVoltsInferred=10.;break;
    case 6:p.sliceVoltsInferred=10.;p.pulseHalfSpanVoltsInferred=.1;break;
    case 7:p.resetSecondsNominal=37e-6;p.referenceHzNominal=432.;p.triangleSpanVoltsNominal=4.7;p.shaperOrdinateInferred={0.,.25,.5,.75,1.};break;
    case 8:p.resetSecondsNominal=21e-6;p.resetVoltsNominal=4.2;p.resetExcessVoltsNominal=.3;p.sliceVoltsInferred=-.04;p.shaperOrdinateInferred={0.,.7,.9,.97,1.};break;
    }
    return p;
}
using Candidate=cs80::ReconstructedOscillator;
using Baseline=cs80_osc_profile_baseline::ReconstructedOscillator;
struct Controls { double frequency,threshold;bool saw,pulse; };
Controls controls(int frame,double maximum) {
    const double angle=double(frame)*.017;
    double frequency=261.6*std::exp2(4.*std::sin(angle*.19));
    if(frame%89==0)frequency=maximum*.999;
    if(frame%97==0)frequency=maximum*1.01;
    if(frame%109==0)frequency=-10.;
    if(frame%113==0)frequency=0.;
    if(frame%127==0)frequency=std::numeric_limits<double>::quiet_NaN();
    if(frame%149==0)frequency=std::numeric_limits<double>::infinity();
    const double threshold=frame%151==0?std::numeric_limits<double>::quiet_NaN():1.52*std::sin(angle);
    return {frequency,threshold,frame%31<23,frame%37<29};
}
unsigned fpMode=0;int fpSeed=0;unsigned long fpPairs=0;
struct FpStatus {unsigned mxcsr;unsigned short sw,cw;int error;};
FpStatus fpStatus(){FpStatus f{};f.mxcsr=_mm_getcsr();asm volatile("fnstsw %0":"=am"(f.sw));asm volatile("fnstcw %0":"=m"(f.cw));f.error=errno;return f;}
bool fpEqual(const FpStatus& a,const FpStatus& b){return a.mxcsr==b.mxcsr && (a.sw&0x7f)==(b.sw&0x7f) && a.cw==b.cw && a.error==b.error;}
// Every pair receives a newly clean or newly seeded environment. Flags from
// setup/control generation/previous ticks cannot mask a missing exception.
struct FpPair {
 std::fenv_t saved{};unsigned mxcsr;int savedErrno;FpStatus a{};const char* label;bool restored=false;
 explicit FpPair(const char* name):mxcsr(_mm_getcsr()),savedErrno(errno),label(name){check(std::fegetenv(&saved)==0,"save caller FPenv");seed();}
 void seed(){check(std::fesetenv(FE_DFL_ENV)==0,"seed default FPenv");_mm_setcsr((_mm_getcsr()&~0x8040u)|fpMode);if(fpSeed==1)std::feraiseexcept(FE_INVALID);if(fpSeed==2)std::feraiseexcept(FE_INEXACT|FE_UNDERFLOW);errno=fpSeed?ERANGE:17;}
 void between(){a=fpStatus();seed();}
 void finish(){const auto b=fpStatus();restore();check(fpEqual(a,b),label);++fpPairs;}
 void restore(){if(!restored){std::fesetenv(&saved);_mm_setcsr(mxcsr);errno=savedErrno;restored=true;}}
 ~FpPair(){restore();}
};
template<class A,class B> bool pairedPrepare(A&& a,B&& b){FpPair fp("prepare FP/status/errno");const bool x=a();fp.between();const bool y=b();fp.finish();check(x==y,"prepare results match");return x&&y;}
void tick(Candidate& a,Baseline& b,const Controls& c) {
    FpPair fp("tick FP/status/errno");
    const auto x=a.tick(c.frequency,{c.threshold,c.saw,c.pulse});
    fp.between();const auto y=b.tick(c.frequency,{c.threshold,c.saw,c.pulse});
    fp.finish();check(output(x,y),"all returned pin bits match the frozen baseline");
    check(complete(a,b),"complete public continuation/diagnostic bits match the baseline");
    ++comparedTicks;
}
void matrix() {
    for(double rate:{8000.,22050.,44100.,88200.,96000.,192000.,768000.,1536000.})for(int recipe=0;recipe<9;++recipe){
        contextRate=rate;contextRecipe=recipe;contextFrame=-1;
        FpPair constructor("constructor FP/status/errno");Candidate a;constructor.between();Baseline b;constructor.finish();
        const auto pa=profile<cs80::OscillatorNominalProfile>(recipe), badA=profile<cs80::OscillatorNominalProfile>(0);
        const auto pb=profile<cs80_osc_profile_baseline::OscillatorNominalProfile>(recipe);
        check(pairedPrepare([&]{return a.prepare(rate,pa);},[&]{return b.prepare(rate,pb);}),"valid profile/rate matrix prepares both oscillators");
        check(complete(a,b),"prepare publishes matching reset state and maximum frequency");
        for(int frame=0;frame<2048;++frame){
            contextFrame=frame;const auto c=controls(frame,a.maximumFrequencyHz());
            tick(a,b,c);
            if(frame%251==0){{FpPair fp("clear diagnostics FP/status/errno");a.clearDiagnosticFlags();fp.between();b.clearDiagnosticFlags();fp.finish();}check(complete(a,b),"diagnostic clear preserves equality");}
            if(frame%257==0){
                FpPair copy("copy constructor FP/status/errno");Candidate ac(a);copy.between();Baseline bc(b);copy.finish();
                for(int j=1;j<=3;++j)tick(ac,bc,controls(frame+j,a.maximumFrequencyHz()));
                FpPair assignedCtor("assignment destination constructor FP/status/errno");Candidate assigned;assignedCtor.between();Baseline assignedBaseline;assignedCtor.finish();
                const auto qa=profile<cs80::OscillatorNominalProfile>((recipe+1)%9);
                const auto qb=profile<cs80_osc_profile_baseline::OscillatorNominalProfile>((recipe+1)%9);
                check(pairedPrepare([&]{return assigned.prepare(48000.,qa);},[&]{return assignedBaseline.prepare(48000.,qb);}),"assignment destination has a different prepared profile");
                {FpPair fp("copy assignment FP/status/errno");assigned=ac;fp.between();assignedBaseline=bc;fp.finish();}
                tick(assigned,assignedBaseline,controls(frame+9,a.maximumFrequencyHz()));
                {FpPair fp("self assignment FP/status/errno");assigned=assigned;fp.between();assignedBaseline=assignedBaseline;fp.finish();}
                check(complete(assigned,assignedBaseline),"self assignment retains cached constants and state");
            }
            if(frame%271==0){
                const auto sa=a.getState();const auto sb=b.getState();
                tick(a,b,controls(frame+13,a.maximumFrequencyHz()));
                {FpPair fp("restore state FP/status/errno");a.restoreState(sa);fp.between();b.restoreState(sb);fp.finish();}check(complete(a,b),"complete state restore leaves the prepared profile cache intact");
            }
            if(frame%283==0){
                const double seconds=3./rate;
                FpPair idle("advance idle FP/status/errno");const double ia=a.advanceIdle(seconds,c.frequency,{c.threshold,c.saw,c.pulse});
                idle.between();const double ib=b.advanceIdle(seconds,c.frequency,{c.threshold,c.saw,c.pulse});
                idle.finish();check(equal(ia,ib)&&complete(a,b),"idle integral/state/diagnostics match exactly");
                const auto sa=a.getIdleState();const auto sb=b.getIdleState();
                tick(a,b,controls(frame+17,a.maximumFrequencyHz()));
                {FpPair fp("restore idle FP/status/errno");a.restoreIdleState(sa);fp.between();b.restoreIdleState(sb);fp.finish();}check(complete(a,b),"compact idle restore retains the prepared profile");
            }
        }
        for(double phase:{0.,-.0,.37,-3.25,std::numeric_limits<double>::quiet_NaN()}){
            {FpPair fp("reset FP/status/errno");a.reset(phase,321.);fp.between();b.reset(phase,321.);fp.finish();}check(complete(a,b),"cold reset retains the selected profile cache");tick(a,b,controls(7,a.maximumFrequencyHz()));}
        const double maximum=a.maximumFrequencyHz();
        for(double f:{0.,-.0,std::numeric_limits<double>::denorm_min(),maximum,std::nextafter(maximum,0.),std::nextafter(maximum,std::numeric_limits<double>::infinity())})
            for(int side:{-1,0,1}){
                const double u=a.getDiagnostics().coupledInputVolts;
                const double threshold=side==0?u:std::nextafter(u,side<0?-std::numeric_limits<double>::infinity():std::numeric_limits<double>::infinity());
                tick(a,b,{f,threshold,true,true});
            }
        for(double invalid:{0.,7999.,1536001.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
            const auto before=a.getState();const auto maximum=a.maximumFrequencyHz();
            check(!pairedPrepare([&]{return a.prepare(invalid,badA);},[&]{return b.prepare(invalid,profile<cs80_osc_profile_baseline::OscillatorNominalProfile>(0));}),"invalid rate prepare fails transactionally");
            check(state(a.getState(),before)&&equal(a.maximumFrequencyHz(),maximum)&&complete(a,b),"invalid rate cannot overwrite the live cache or state");
        }
        auto invalidA=pa;auto invalidB=pb;invalidA.triangleSpanVoltsNominal=0.;invalidB.triangleSpanVoltsNominal=0.;
        check(!pairedPrepare([&]{return a.prepare(rate,invalidA);},[&]{return b.prepare(rate,invalidB);})&&complete(a,b),"invalid profile does not publish partial constants");
        // Same basis key (rate/RC), but all other profile constants may change.
        const auto* oldBasis=a.sharedBasis();
        auto changedA=pa;auto changedB=pb;
        changedA.resetSecondsNominal=changedB.resetSecondsNominal=19e-6;
        changedA.referenceHzNominal=changedB.referenceHzNominal=200.;
        changedA.triangleSpanVoltsNominal=changedB.triangleSpanVoltsNominal=1.7;
        changedA.sliceVoltsInferred=changedB.sliceVoltsInferred=.07;
        changedA.shaperOrdinateInferred=changedB.shaperOrdinateInferred={0.,.4,.7,.9,1.};
        check(pairedPrepare([&]{return a.prepare(rate,changedA);},[&]{return b.prepare(rate,changedB);})&&a.sharedBasis()==oldBasis,"same rate/RC basis is reused while profile constants refresh");
        for(int i=0;i<128;++i)tick(a,b,controls(i,a.maximumFrequencyHz()));
    }
}
void poolAndAllocation() {
    {
        FpPair constructor("constructor FP/status/errno");Candidate a;constructor.between();Baseline b;constructor.finish();std::array<const cs80::OscillatorExponentialBasis*,15> ap{};
        std::array<const cs80_osc_profile_baseline::OscillatorExponentialBasis*,15> bp{};
        for(std::size_t i=0;i<ap.size();++i){ap[i]=cs80::OscillatorBasisRegistry::acquire(96000.,.01+double(i)*.01);bp[i]=cs80_osc_profile_baseline::OscillatorBasisRegistry::acquire(96000.,.01+double(i)*.01);check(ap[i]&&bp[i],"fill every other basis slot");}
        tick(a,b,controls(77,a.maximumFrequencyHz()));const auto prior=a.getState();const auto maximum=a.maximumFrequencyHz();
        auto pa=profile<cs80::OscillatorNominalProfile>(7);auto pb=profile<cs80_osc_profile_baseline::OscillatorNominalProfile>(7);pa.couplingSecondsNominal=pb.couplingSecondsNominal=.777;
        check(!pairedPrepare([&]{return a.prepare(192000.,pa);},[&]{return b.prepare(192000.,pb);}),"exhausted registry rejects prepare");
        check(state(a.getState(),prior)&&equal(a.maximumFrequencyHz(),maximum)&&complete(a,b),"failed acquire preserves prepared constants and continuation");
        for(std::size_t i=0;i<ap.size();++i){cs80::OscillatorBasisRegistry::release(ap[i]);cs80_osc_profile_baseline::OscillatorBasisRegistry::release(bp[i]);}
    }
    {
        std::array<const cs80::OscillatorExponentialBasis*,16> ap{};std::array<const cs80_osc_profile_baseline::OscillatorExponentialBasis*,16> bp{};
        for(std::size_t i=0;i<ap.size();++i){ap[i]=cs80::OscillatorBasisRegistry::acquire(96000.,.2+double(i)*.01);bp[i]=cs80_osc_profile_baseline::OscillatorBasisRegistry::acquire(96000.,.2+double(i)*.01);check(ap[i]&&bp[i],"fill registry without a default-key basis");}
        {
            FpPair constructor("constructor FP/status/errno");Candidate a;constructor.between();Baseline b;constructor.finish();check(a.sharedBasis()==nullptr&&b.sharedBasis()==nullptr&&complete(a,b),"exhausted constructors preserve original unprepared defaults");
            tick(a,b,controls(17,a.maximumFrequencyHz()));{FpPair fp("unprepared reset FP/status/errno");a.reset(.37,432.);fp.between();b.reset(.37,432.);fp.finish();}check(complete(a,b),"unprepared reset keeps original default arithmetic");
            FpPair idle("unprepared idle FP/status/errno");const auto ia=a.advanceIdle(.0001,440.,{});idle.between();const auto ib=b.advanceIdle(.0001,440.,{});idle.finish();check(equal(ia,ib)&&complete(a,b),"unprepared idle API retains its original default-member arithmetic");
            FpPair copy("copy constructor FP/status/errno");Candidate ac(a);copy.between();Baseline bc(b);copy.finish();check(complete(ac,bc),"copy of an unprepared oscillator retains its default cache");
            {FpPair fp("unprepared assignment FP/status/errno");ac=a;fp.between();bc=b;fp.finish();}tick(ac,bc,controls(19,ac.maximumFrequencyHz()));
        }
        for(std::size_t i=0;i<ap.size();++i){cs80::OscillatorBasisRegistry::release(ap[i]);cs80_osc_profile_baseline::OscillatorBasisRegistry::release(bp[i]);}
    }
    {
        Candidate a;allocations=0;countAllocations=true;
        for(int i=0;i<4096;++i){const auto c=controls(i,a.maximumFrequencyHz());(void)a.tick(c.frequency,{c.threshold,c.saw,c.pulse});if(i%37==0)(void)a.advanceIdle(.00001,440.,{});}
        countAllocations=false;check(allocations==0,"cached callback/idle work allocates nothing");
    }
}
}
void* operator new(std::size_t n){if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
int main(){std::fenv_t original;std::fegetenv(&original);const auto originalMxcsr=_mm_getcsr();const int originalErrno=errno;for(unsigned mode:{0u,0x8040u})for(int seeded:{0,1,2}){fpMode=mode;fpSeed=seeded;matrix();poolAndAllocation();}std::fesetenv(&original);_mm_setcsr(originalMxcsr);errno=originalErrno;std::printf("Oscillator API FP: %d checks, %d failures, %zu compared ticks, %lu paired FP calls\n",checks,failures,comparedTicks,fpPairs);return failures?1:0;}
