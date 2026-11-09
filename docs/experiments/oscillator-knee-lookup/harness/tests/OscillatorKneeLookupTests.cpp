#include "core/ReconstructedOscillator.h"
#define cs80 cs80_osc_knee_baseline
#include "fixtures/ReconstructedOscillatorPreKneeLookup.h"
#undef cs80

#include <bit>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <limits>

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
double originalKnee(double a,double aSlope,int side) noexcept {
    for(int k=1;k<=4;++k){const double knot=.25*double(k);if(std::abs(a-knot)<2e-11)a=knot+std::copysign(2e-11,aSlope*double(side));}
    return a;
}
void kneeTruth() {
    const double inf=std::numeric_limits<double>::infinity();
    const std::array<double,13> slopes{0.,-0.,1.,-1.,1e-300,-1e-300,1e300,-1e300,inf,-inf,
        std::bit_cast<double>(std::uint64_t{0x7ff8000000001234}),
        std::numeric_limits<double>::denorm_min(),-std::numeric_limits<double>::denorm_min()};
    const auto compare=[&](double a){for(double slope:slopes)for(int side:{-1,0,1})
        check(equal(cs80::oscillator_detail::orientShaperKnee(a,slope,side),originalKnee(a,slope,side)),
              "direct DUT knee value matches the literal four-knot loop bit-for-bit");};
    for(double value:{-inf,-10.,-0.,0.,std::numeric_limits<double>::denorm_min(),.2,.375,.625,.875,1.1,10.,inf,
                      std::bit_cast<double>(std::uint64_t{0x7ff8000000004321})}){
        compare(value);
        if(std::isfinite(value)){double lower=value,upper=value;for(int i=0;i<128;++i){lower=std::nextafter(lower,-inf);upper=std::nextafter(upper,inf);compare(lower);compare(upper);}}
    }
    for(int k=1;k<=4;++k)for(double offset:{-2e-11,0.,2e-11}){
        const double center=.25*double(k)+offset;compare(center);
        double lower=center,upper=center;for(int i=0;i<256;++i){lower=std::nextafter(lower,-inf);upper=std::nextafter(upper,inf);compare(lower);compare(upper);}
    }
    std::uint64_t random=0x6c8d14cb2940be31ull;
    for(int i=0;i<20000;++i){random=random*6364136223846793005ull+1442695040888963407ull;
        compare(double(random>>11)*0x1p-53*1.5-.125);}
}
using Candidate=cs80::ReconstructedOscillator;
using Baseline=cs80_osc_knee_baseline::ReconstructedOscillator;
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
void tick(Candidate& a,Baseline& b,const Controls& c) {
    const auto x=a.tick(c.frequency,{c.threshold,c.saw,c.pulse});
    const auto y=b.tick(c.frequency,{c.threshold,c.saw,c.pulse});
    check(output(x,y),"all returned pin bits match the frozen baseline");
    check(complete(a,b),"complete public continuation/diagnostic bits match the baseline");
    ++comparedTicks;
}
void matrix() {
    for(double rate:{8000.,22050.,44100.,88200.,96000.,192000.,768000.,1536000.})for(int recipe=0;recipe<9;++recipe){
        contextRate=rate;contextRecipe=recipe;contextFrame=-1;
        Candidate a;Baseline b;
        const auto pa=profile<cs80::OscillatorNominalProfile>(recipe), badA=profile<cs80::OscillatorNominalProfile>(0);
        const auto pb=profile<cs80_osc_knee_baseline::OscillatorNominalProfile>(recipe);
        check(a.prepare(rate,pa)&&b.prepare(rate,pb),"valid profile/rate matrix prepares both oscillators");
        check(complete(a,b),"prepare publishes matching reset state and maximum frequency");
        for(int frame=0;frame<2048;++frame){
            contextFrame=frame;const auto c=controls(frame,a.maximumFrequencyHz());
            tick(a,b,c);
            if(frame%251==0){a.clearDiagnosticFlags();b.clearDiagnosticFlags();check(complete(a,b),"diagnostic clear preserves equality");}
            if(frame%257==0){
                Candidate ac(a);Baseline bc(b);
                for(int j=1;j<=3;++j)tick(ac,bc,controls(frame+j,a.maximumFrequencyHz()));
                Candidate assigned;Baseline assignedBaseline;
                const auto qa=profile<cs80::OscillatorNominalProfile>((recipe+1)%9);
                const auto qb=profile<cs80_osc_knee_baseline::OscillatorNominalProfile>((recipe+1)%9);
                check(assigned.prepare(48000.,qa)&&assignedBaseline.prepare(48000.,qb),"assignment destination has a different prepared profile");
                assigned=ac;assignedBaseline=bc;
                tick(assigned,assignedBaseline,controls(frame+9,a.maximumFrequencyHz()));
                assigned=assigned;assignedBaseline=assignedBaseline;
                check(complete(assigned,assignedBaseline),"self assignment retains profile values and state");
            }
            if(frame%271==0){
                const auto sa=a.getState();const auto sb=b.getState();
                tick(a,b,controls(frame+13,a.maximumFrequencyHz()));
                a.restoreState(sa);b.restoreState(sb);check(complete(a,b),"complete state restore leaves the prepared knee lookup intact");
            }
            if(frame%283==0){
                const double seconds=3./rate;
                const double ia=a.advanceIdle(seconds,c.frequency,{c.threshold,c.saw,c.pulse});
                const double ib=b.advanceIdle(seconds,c.frequency,{c.threshold,c.saw,c.pulse});
                check(equal(ia,ib)&&complete(a,b),"idle integral/state/diagnostics match exactly");
                const auto sa=a.getIdleState();const auto sb=b.getIdleState();
                tick(a,b,controls(frame+17,a.maximumFrequencyHz()));
                a.restoreIdleState(sa);b.restoreIdleState(sb);check(complete(a,b),"compact idle restore retains the prepared profile");
            }
        }
        for(double phase:{0.,-.0,.37,-3.25,std::numeric_limits<double>::quiet_NaN()}){
            a.reset(phase,321.);b.reset(phase,321.);check(complete(a,b),"cold reset retains the selected knee lookup");tick(a,b,controls(7,a.maximumFrequencyHz()));}
        const double maximum=a.maximumFrequencyHz();
        for(double f:{0.,-.0,std::numeric_limits<double>::denorm_min(),maximum,std::nextafter(maximum,0.),std::nextafter(maximum,std::numeric_limits<double>::infinity())})
            for(int side:{-1,0,1}){
                const double u=a.getDiagnostics().coupledInputVolts;
                const double threshold=side==0?u:std::nextafter(u,side<0?-std::numeric_limits<double>::infinity():std::numeric_limits<double>::infinity());
                tick(a,b,{f,threshold,true,true});
            }
        for(double invalid:{0.,7999.,1536001.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
            const auto before=a.getState();const auto maximum=a.maximumFrequencyHz();
            check(!a.prepare(invalid,badA)&&!b.prepare(invalid,profile<cs80_osc_knee_baseline::OscillatorNominalProfile>(0)),"invalid rate prepare fails transactionally");
            check(state(a.getState(),before)&&equal(a.maximumFrequencyHz(),maximum)&&complete(a,b),"invalid rate cannot overwrite the live profile or state");
        }
        auto invalidA=pa;auto invalidB=pb;invalidA.triangleSpanVoltsNominal=0.;invalidB.triangleSpanVoltsNominal=0.;
        check(!a.prepare(rate,invalidA)&&!b.prepare(rate,invalidB)&&complete(a,b),"invalid profile does not publish partial constants");
        // Same basis key (rate/RC), but all other profile constants may change.
        const auto* oldBasis=a.sharedBasis();
        auto changedA=pa;auto changedB=pb;
        changedA.resetSecondsNominal=changedB.resetSecondsNominal=19e-6;
        changedA.referenceHzNominal=changedB.referenceHzNominal=200.;
        changedA.triangleSpanVoltsNominal=changedB.triangleSpanVoltsNominal=1.7;
        changedA.sliceVoltsInferred=changedB.sliceVoltsInferred=.07;
        changedA.shaperOrdinateInferred=changedB.shaperOrdinateInferred={0.,.4,.7,.9,1.};
        check(a.prepare(rate,changedA)&&b.prepare(rate,changedB)&&a.sharedBasis()==oldBasis,"same rate/RC basis is reused while profile constants refresh");
        for(int i=0;i<128;++i)tick(a,b,controls(i,a.maximumFrequencyHz()));
    }
}
void poolAndAllocation() {
    {
        Candidate a;Baseline b;std::array<const cs80::OscillatorExponentialBasis*,15> ap{};
        std::array<const cs80_osc_knee_baseline::OscillatorExponentialBasis*,15> bp{};
        for(std::size_t i=0;i<ap.size();++i){ap[i]=cs80::OscillatorBasisRegistry::acquire(96000.,.01+double(i)*.01);bp[i]=cs80_osc_knee_baseline::OscillatorBasisRegistry::acquire(96000.,.01+double(i)*.01);check(ap[i]&&bp[i],"fill every other basis slot");}
        tick(a,b,controls(77,a.maximumFrequencyHz()));const auto prior=a.getState();const auto maximum=a.maximumFrequencyHz();
        auto pa=profile<cs80::OscillatorNominalProfile>(7);auto pb=profile<cs80_osc_knee_baseline::OscillatorNominalProfile>(7);pa.couplingSecondsNominal=pb.couplingSecondsNominal=.777;
        check(!a.prepare(192000.,pa)&&!b.prepare(192000.,pb),"exhausted registry rejects prepare");
        check(state(a.getState(),prior)&&equal(a.maximumFrequencyHz(),maximum)&&complete(a,b),"failed acquire preserves prepared constants and continuation");
        for(std::size_t i=0;i<ap.size();++i){cs80::OscillatorBasisRegistry::release(ap[i]);cs80_osc_knee_baseline::OscillatorBasisRegistry::release(bp[i]);}
    }
    {
        std::array<const cs80::OscillatorExponentialBasis*,16> ap{};std::array<const cs80_osc_knee_baseline::OscillatorExponentialBasis*,16> bp{};
        for(std::size_t i=0;i<ap.size();++i){ap[i]=cs80::OscillatorBasisRegistry::acquire(96000.,.2+double(i)*.01);bp[i]=cs80_osc_knee_baseline::OscillatorBasisRegistry::acquire(96000.,.2+double(i)*.01);check(ap[i]&&bp[i],"fill registry without a default-key basis");}
        {
            Candidate a;Baseline b;check(a.sharedBasis()==nullptr&&b.sharedBasis()==nullptr&&complete(a,b),"exhausted constructors preserve original unprepared defaults");
            tick(a,b,controls(17,a.maximumFrequencyHz()));a.reset(.37,432.);b.reset(.37,432.);check(complete(a,b),"unprepared reset keeps original default arithmetic");
            check(equal(a.advanceIdle(.0001,440.,{}),b.advanceIdle(.0001,440.,{}))&&complete(a,b),"unprepared idle API retains its original default-member arithmetic");
            Candidate ac(a);Baseline bc(b);check(complete(ac,bc),"copy of an unprepared oscillator retains its default profile");
            ac=a;bc=b;tick(ac,bc,controls(19,ac.maximumFrequencyHz()));
        }
        for(std::size_t i=0;i<ap.size();++i){cs80::OscillatorBasisRegistry::release(ap[i]);cs80_osc_knee_baseline::OscillatorBasisRegistry::release(bp[i]);}
    }
    {
        Candidate a;allocations=0;countAllocations=true;
        for(int i=0;i<4096;++i){const auto c=controls(i,a.maximumFrequencyHz());(void)a.tick(c.frequency,{c.threshold,c.saw,c.pulse});if(i%37==0)(void)a.advanceIdle(.00001,440.,{});}
        countAllocations=false;check(allocations==0,"callback/idle work allocates nothing");
    }
}
}
void* operator new(std::size_t n){if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
int main(){kneeTruth();matrix();poolAndAllocation();std::printf("Oscillator knee lookup: %d checks, %d failures, %zu compared ticks\n",checks,failures,comparedTicks);return failures?1:0;}
