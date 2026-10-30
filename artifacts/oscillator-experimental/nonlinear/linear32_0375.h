#pragma once
// Source-informed topology; all opaque-IC transfer choices are an INFERRED
// nominal instrument, not recovered Yamaha die parameters or a specimen fit.
// See docs/OSCILLATOR_REALTIME_PROPOSAL.md, profile w3-current-fold-v1.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace cs80 {
struct OscillatorNominalProfile {
    static constexpr const char* id = "w3-current-fold-v1";
    double resetSecondsNominal = 30e-6;
    double referenceHzNominal = 261.6;
    double resetVoltsNominal = 3.5;
    double resetExcessVoltsNominal = .45;
    double couplingSecondsNominal = 4.7e-6 * 22000.0;
    double triangleSpanVoltsNominal = 3.2;
    double sliceVoltsInferred = 0.0;
    double pulseHalfSpanVoltsInferred = 1.5;
    // Frozen from independent periodic FFT quadrature, 2^19 samples/period;
    // proposal's calibration version 2026-10-29, not an installed resistor.
    double sineGainVoltsInferred = 1.5017767959;
    std::array<double,5> shaperOrdinateInferred {
        0., .3826834323650898, .7071067811865475, .9238795325112867, 1.
    };
};
struct WscControls {
    double pulseThresholdVolts = 0.;
    bool sawEnabled = true, pulseEnabled = true;
};
// Returned audio is a gate-selected pin-voltage readout. Physical pins keep
// oscillating behind the downstream panel gates; Diagnostics::raw is ungated.
struct WscOutputVolts {
    double sawPin5 = 0., pulsePin7 = 0., sinePin2 = 0.;
};

// Shared linear-phase numerical antialias kernel, not an analog circuit.
// All branches and controls acquire 8 INTERNAL samples of delay. Integration
// must align the complete isolation path and notify the host; default-off is
// unaffected. This short bank is optimized for the existing 2x output path,
// not for ideal response up to the direct internal Nyquist frequency. The Blackman-Harris window, cutoff and table resolution are
// explicit numerical choices whose quality is measured independently.
class OscillatorResidualKernel {
public:
    static constexpr std::size_t length=32,phases=64,points=length*phases+1;
    static constexpr double latencySamples=16.,cutoffCyclesPerSample=.375;
    struct Residual { double step=0.,ramp=0.; };
    static const OscillatorResidualKernel& shared() noexcept { static const OscillatorResidualKernel bank;return bank; }
    Residual at(double ageSamples) const noexcept {
        if(ageSamples>=double(length))return {};
        const double position=std::max(0.,ageSamples)*double(phases);
        const auto i=std::min(points-2,static_cast<std::size_t>(position));const double f=position-double(i);
        return {step[i]+f*(step[i+1]-step[i]),ramp[i]+f*(ramp[i+1]-ramp[i])};
    }
    double firstMomentSamples() const noexcept { return moment; }
    double secondMomentSamplesSquared() const noexcept { return secondMoment; }
    const std::array<double,points>& impulseTable() const noexcept { return impulse; }
    const std::array<double,points>& stepTable() const noexcept { return step; }
private:
    OscillatorResidualKernel() noexcept {
        constexpr double pi=3.1415926535897932384626433832795,dx=1./double(phases);
        for(std::size_t i=0;i<points;++i){const double t=double(i)*dx-latencySamples,a=2*pi*double(i)/double(points-1);
            const double window=.35875-.48829*std::cos(a)+.14128*std::cos(2*a)-.01168*std::cos(3*a);
            impulse[i]=(t==0.?2*cutoffCyclesPerSample:std::sin(2*pi*cutoffCyclesPerSample*t)/(pi*t))*window;}
        double area=0.;for(std::size_t i=1;i<points;++i)area+=(impulse[i-1]+impulse[i])*.5*dx;
        for(auto& x:impulse)x/=area;
        double s=0.,m=0.,m2=0.;step[0]=-1.;
        for(std::size_t i=1;i<points;++i){const double t0=double(i-1)*dx,t1=double(i)*dx,h0=impulse[i-1],h1=impulse[i];
            s+=(h0+h1)*.5*dx;m+=dx*((2*t0+t1)*h0+(t0+2*t1)*h1)/6.;
            m2+=dx*(h0*(3*t0*t0+2*t0*t1+t1*t1)+h1*(t0*t0+2*t0*t1+3*t1*t1))/12.;
            step[i]=s-1.;ramp[i]=t1*(s-1.)-m;}
        moment=m;secondMoment=m2;for(auto& x:ramp)x+=moment;step.back()=0.;ramp.back()=0.;
    }
    std::array<double,points> impulse{},step{},ramp{};
    double moment=0.,secondMoment=0.;
};

class ReconstructedOscillator {
public:
    static constexpr double minimumRate=8000.,maximumRate=1536000.;
    static constexpr std::size_t maximumEventsPerTick=64;
    enum EventKind : unsigned { resetStart=1,resetEnd=2,branchCorner=4,controlChange=8 };
    struct Event {
        double secondsFromSampleStart=0.,absoluteSeconds=0.,charge=0.;
        unsigned kind=0;
        bool resetActive=false;
        WscOutputVolts before{},after{};
    };
    struct Diagnostics {
        bool invalidControl=false,frequencyLimited=false,eventCapacityExceeded=false;
        std::size_t eventCount=0;
        std::array<Event,maximumEventsPerTick> events{};
        double rawPin7Volts=0.,coupledInputVolts=0.,trianglePin3Volts=0.,shaperInputVolts=0.;
        WscOutputVolts raw{};
    };
    // Full continuation state, including capacitor memory and overlapping tails.
    // Copying the oscillator itself also preserves profile/rate and this state.
    struct State {
        double charge=0.,resetRemainingSeconds=0.;
        double inputBaselineVolts=0.,triangleBaselineVolts=0.;
        double heldFrequencyHz=261.6,heldThresholdVolts=0.,elapsedSeconds=0.;
        bool sawEnabled=true,pulseEnabled=true;
        std::array<std::array<double,OscillatorResidualKernel::length>,3> residual{};
        std::size_t residualCursor=0;
        std::uint64_t resetCount=0;
    };
    ReconstructedOscillator() noexcept { (void)prepare(96000.,OscillatorNominalProfile{}); }
    static bool validProfile(const OscillatorNominalProfile& p) noexcept {
        const auto valid=[](double x,double low,double high){return std::isfinite(x)&&x>=low&&x<=high;};
        if(!valid(p.resetSecondsNominal,1e-6,100e-6)||!valid(p.referenceHzNominal,1.,10000.)
            ||p.referenceHzNominal*p.resetSecondsNominal>=.95
            ||!valid(p.resetVoltsNominal,.1,20.)||!valid(p.resetExcessVoltsNominal,0.,p.resetVoltsNominal*.9)
            ||!valid(p.couplingSecondsNominal,.001,10.)||!valid(p.triangleSpanVoltsNominal,.1,20.)
            ||!valid(p.sliceVoltsInferred,-10.,10.)||!valid(p.pulseHalfSpanVoltsInferred,.1,10.)
            ||!valid(p.sineGainVoltsInferred,.1,10.)||p.shaperOrdinateInferred[0]!=0.||p.shaperOrdinateInferred[4]!=1.)return false;
        double lastSlope=std::numeric_limits<double>::infinity();
        for(std::size_t i=1;i<5;++i){double slope=4.*(p.shaperOrdinateInferred[i]-p.shaperOrdinateInferred[i-1]);if(!std::isfinite(slope)||slope<=0.||slope>lastSlope)return false;lastSlope=slope;}
        return true;
    }
    // Setup only; invalid prepare is transactional and preserves live state.
    bool prepare(double sampleRate,const OscillatorNominalProfile& selected) noexcept {
        if(!validProfile(selected)||!std::isfinite(sampleRate)||sampleRate<minimumRate||sampleRate>maximumRate)return false;
        (void)OscillatorResidualKernel::shared();
        profile=selected;rate=sampleRate;dt=1./rate;
        converterGain=(profile.resetVoltsNominal-profile.resetExcessVoltsNominal)/(1.-profile.referenceHzNominal*profile.resetSecondsNominal);
        rampHalf=(profile.resetVoltsNominal-profile.resetExcessVoltsNominal)*.5;
        prepareExponentialResiduals();reset(0.,profile.referenceHzNominal);return true;
    }
    // Cold-start convention only. No noteOn() method deliberately exists: the
    // inspected M41 graph has no note-trigger phase/coupling reset connection.
    void reset(double phase=0.,double initialHz=261.6) noexcept {
        state={};diagnostics={};
        state.heldFrequencyHz=std::isfinite(initialHz)?std::clamp(initialHz,0.,maximumFrequencyHz()):std::clamp(profile.referenceHzNominal,0.,maximumFrequencyHz());
        phase=std::isfinite(phase)?phase-std::floor(phase):0.;
        const double f=state.heldFrequencyHz;
        if(f>0.&&phase<f*profile.resetSecondsNominal){const double elapsed=phase/f;state.charge=(1./profile.resetSecondsNominal-f)*elapsed;state.resetRemainingSeconds=profile.resetSecondsNominal-elapsed;}
        else state.charge=1.-phase;
        if(f==0.)state.charge=phase==0.?0.:1.-phase;
        // Explicit zero AUDIO prehistory. Capacitor/charge initial conditions
        // are independent physical-model conventions, not hidden past sound.
        recordEvent(0.,controlChange,Nodes{},nodes(1));
        diagnostics.eventCount=0;
    }
    WscOutputVolts tick(double requestedHz,const WscControls& controls={}) noexcept {
        diagnostics.eventCount=0;
        Nodes before;
        double f=state.heldFrequencyHz,threshold=state.heldThresholdVolts;
        if(std::isfinite(requestedHz)){f=std::clamp(requestedHz,0.,maximumFrequencyHz());if(f!=requestedHz)diagnostics.frequencyLimited=true;}
        else diagnostics.invalidControl=true;
        if(std::isfinite(controls.pulseThresholdVolts))threshold=controls.pulseThresholdVolts;
        else diagnostics.invalidControl=true;
        const bool changed=f!=state.heldFrequencyHz||threshold!=state.heldThresholdVolts||controls.sawEnabled!=state.sawEnabled||controls.pulseEnabled!=state.pulseEnabled;
        if(changed)before=nodes(1);
        state.heldFrequencyHz=f;state.heldThresholdVolts=threshold;state.sawEnabled=controls.sawEnabled;state.pulseEnabled=controls.pulseEnabled;
        auto after=nodes(1);
        if(changed)recordEvent(0.,controlChange,before,after);
        double elapsed=0.;
        for(std::size_t iteration=0;iteration<maximumEventsPerTick&&elapsed<dt;++iteration){
            // A stopped q=0 capacitor does NOT retrigger the one-shot.
            if(state.resetRemainingSeconds==0.&&state.charge<=0.&&f>0.){
                before=after;state.charge=0.;state.resetRemainingSeconds=profile.resetSecondsNominal;++state.resetCount;
                after=nodes(1);recordEvent(elapsed,resetStart,before,after);
            }
            const bool inReset=state.resetRemainingSeconds>0.;
            const double physical=inReset?state.resetRemainingSeconds:(f>0.?std::max(0.,state.charge)/f:std::numeric_limits<double>::infinity());
            double span=std::min(dt-elapsed,physical);
            const auto start=after;
            bool branchEvent=false;
            span=nextBranchEvent(start,span,branchEvent);
            // Roots closer than round-off to the previous event are represented
            // by its right-sided branch derivative, never a zero-progress loop.
            if(!(span>0.)){diagnostics.eventCapacityExceeded=true;break;}
            advance(span,start);
            elapsed+=span;
            const bool physicalReached=std::abs(span-physical)<=2e-15;
            if(physicalReached&&inReset)state.resetRemainingSeconds=std::numeric_limits<double>::min();
            if(physicalReached||branchEvent)before=nodes(-1);
            if(physicalReached){
                if(inReset){state.resetRemainingSeconds=0.;after=nodes(1);recordEvent(elapsed,resetEnd,before,after);}
                else {state.charge=0.;state.resetRemainingSeconds=profile.resetSecondsNominal;++state.resetCount;after=nodes(1);recordEvent(elapsed,resetStart,before,after);}
            } else {after=nodes(1);if(branchEvent)recordEvent(elapsed,branchCorner,before,after);}
            if(dt-elapsed<=2e-15)elapsed=dt;
        }
        if(elapsed<dt){ // Explicit diagnostic, bounded numerical failure policy.
            diagnostics.eventCapacityExceeded=true;advance(dt-elapsed,after);after=nodes(1);
        }
        state.elapsedSeconds+=dt;
        const auto raw=after;
        diagnostics.rawPin7Volts=raw.pin7;diagnostics.coupledInputVolts=raw.u;
        diagnostics.trianglePin3Volts=raw.triangle;diagnostics.shaperInputVolts=raw.w;diagnostics.raw=raw.ungatedValue;
        WscOutputVolts output;
        double* destinations[3]={&output.sawPin5,&output.pulsePin7,&output.sinePin2};
        const auto values=asArray(raw.value),slopes=asArray(raw.slope),constants=asArray(raw.constant);
        for(std::size_t branch=0;branch<3;++branch){const double a=values[branch]-constants[branch],b=slopes[branch]/rate+inverseTauSamples*a;*destinations[branch]=constants[branch]+a*weightedArea-b*weightedMoment+state.residual[branch][state.residualCursor];state.residual[branch][state.residualCursor]=0.;}
        state.residualCursor=(state.residualCursor+1)%OscillatorResidualKernel::length;
        return output;
    }
    double maximumFrequencyHz() const noexcept { return std::min(.45*rate,.95/profile.resetSecondsNominal); }
    static constexpr double latencyInternalSamples() noexcept { return OscillatorResidualKernel::latencySamples; }
    double sampleRate() const noexcept { return rate; }
    const OscillatorNominalProfile& getProfile() const noexcept { return profile; }
    const State& getState() const noexcept { return state; }
    const Diagnostics& getDiagnostics() const noexcept { return diagnostics; }
    void clearDiagnosticFlags() noexcept { diagnostics.invalidControl=diagnostics.frequencyLimited=diagnostics.eventCapacityExceeded=false; }
    // Diagnostic phase coordinate, not a replacement for complete state copying.
    double getPhase() const noexcept {
        if(state.resetRemainingSeconds>0.)return state.heldFrequencyHz*(profile.resetSecondsNominal-state.resetRemainingSeconds);
        return 1.-state.charge;
    }
    static double panelThresholdVoltsInferred(double dutyProxy) noexcept { return 1.525*(1.-2.*dutyProxy); }
    static double shape(double x,const OscillatorNominalProfile& p=OscillatorNominalProfile{}) noexcept {
        const double a=std::abs(x);if(a>=1.)return std::copysign(1.,x);
        const auto i=std::min(std::size_t(3),static_cast<std::size_t>(a*4.));const double f=a*4.-double(i);
        return std::copysign(p.shaperOrdinateInferred[i]+f*(p.shaperOrdinateInferred[i+1]-p.shaperOrdinateInferred[i]),x);
    }
private:
    struct Nodes {
        double pin7=0.,u=0.,uSlope=0.,triangle=0.,triangleSlope=0.,w=0.,wSlope=0.;
        double triangleGain=0.;
        WscOutputVolts value{},slope{},constant{},ungatedValue{};
    };
    static std::array<double,3> asArray(const WscOutputVolts& x) noexcept { return {x.sawPin5,x.pulsePin7,x.sinePin2}; }
    // |x| <= 1/(8k*.001)=.125 under supported profiles. The polynomial is
    // machine-close at nominal RC; use libm outside the cheap nominal domain.
    static double decay(double x) noexcept {
        if(x>.002)return std::exp(-x);
        return 1.-x*(1.-x*(.5-x*(1./6.-x*(1./24.-x/120.))));
    }
    static double sided(double x,double derivative,int side) noexcept {
        return derivative!=0.&&std::abs(x)<2e-11?std::copysign(2e-11,derivative*double(side)==0.?x:derivative*double(side)):x;
    }
    Nodes nodes(int side) const noexcept {
        Nodes n;
        const double tau=profile.couplingSecondsNominal;
        const double pinSlope=state.resetRemainingSeconds>0.?0.:-converterGain*state.heldFrequencyHz;
        n.pin7=state.resetRemainingSeconds>0.?profile.resetVoltsNominal:converterGain*state.charge;
        n.u=n.pin7-state.inputBaselineVolts;n.uSlope=pinSlope-n.u/tau;
        const double fold=n.u-profile.sliceVoltsInferred;
        const double oriented=sided(fold,n.uSlope,side);
        const bool limited=sided(std::abs(fold)-rampHalf,(oriented>=0.?1.:-1.)*n.uSlope,side)>=0.;
        n.triangle=profile.triangleSpanVoltsNominal*std::min(1.,std::abs(fold)/rampHalf);
        n.triangleGain=limited?0.:std::copysign(profile.triangleSpanVoltsNominal/rampHalf,oriented);
        n.triangleSlope=n.triangleGain*n.uSlope;
        n.w=n.triangle-state.triangleBaselineVolts;n.wSlope=n.triangleSlope-n.w/tau;
        const double half=profile.triangleSpanVoltsNominal*.5,x=n.w/half;
        double a=std::abs(x);const double aSlope=std::copysign(1.,x)*n.wSlope/half;
        for(int k=1;k<=4;++k){const double knot=.25*double(k);if(std::abs(a-knot)<2e-11)a=knot+std::copysign(2e-11,aSlope*double(side));}
        double shapeSlope=0.;if(a<1.){const auto i=std::min(std::size_t(3),static_cast<std::size_t>(std::max(0.,a)*4.));shapeSlope=4.*(profile.shaperOrdinateInferred[i+1]-profile.shaperOrdinateInferred[i]);}
        n.ungatedValue={-n.u,sided(n.u-state.heldThresholdVolts,n.uSlope,side)>0.?profile.pulseHalfSpanVoltsInferred:-profile.pulseHalfSpanVoltsInferred,-profile.sineGainVoltsInferred*shape(x,profile)};
        n.value={state.sawEnabled?-n.u:0.,state.pulseEnabled?(sided(n.u-state.heldThresholdVolts,n.uSlope,side)>0.?profile.pulseHalfSpanVoltsInferred:-profile.pulseHalfSpanVoltsInferred):0.,-profile.sineGainVoltsInferred*shape(x,profile)};
        n.slope={state.sawEnabled?-n.uSlope:0.,0.,-profile.sineGainVoltsInferred*shapeSlope*n.wSlope/half};
        n.constant={state.sawEnabled?-pinSlope*tau:0.,n.value.pulsePin7,n.value.sinePin2+profile.sineGainVoltsInferred*shapeSlope*x};
        return n;
    }
    // Both physical coupling poles use the explicitly selected same ideal RC.
    // On a fold segment tri=A+B exp(-t/RC), so the second pole is exact too.
    void advance(double seconds,const Nodes& start) noexcept {
        const double tau=profile.couplingSecondsNominal,e=decay(seconds/tau);
        const bool reset=state.resetRemainingSeconds>0.;
        const double pinSlope=reset?0.:-converterGain*state.heldFrequencyHz;
        const double uEnd=pinSlope*tau+(start.u-pinSlope*tau)*e;
        const double b=start.triangleGain*(start.u-pinSlope*tau),a=start.triangle-b;
        state.triangleBaselineVolts=a+(state.triangleBaselineVolts-a)*e+b*(seconds/tau)*e;
        state.charge+=(reset?1./profile.resetSecondsNominal-state.heldFrequencyHz:-state.heldFrequencyHz)*seconds;
        if(reset)state.resetRemainingSeconds=std::max(0.,state.resetRemainingSeconds-seconds);
        const double pinEnd=start.pin7+pinSlope*seconds;
        state.inputBaselineVolts=pinEnd-uEnd;
        if(std::abs(state.inputBaselineVolts)<1e-300)state.inputBaselineVolts=0.;
        if(std::abs(state.triangleBaselineVolts)<1e-300)state.triangleBaselineVolts=0.;
    }
    double nextBranchEvent(const Nodes& n,double limit,bool& event) const noexcept {
        constexpr double epsilon=3e-14;
        const double tau=profile.couplingSecondsNominal;
        const double pinSlope=state.resetRemainingSeconds>0.?0.:-converterGain*state.heldFrequencyHz;
        const double base=pinSlope*tau,b=n.u-base;
        const double end=base+b*decay(limit/tau);
        const std::array<double,4> levels {profile.sliceVoltsInferred-rampHalf,profile.sliceVoltsInferred,profile.sliceVoltsInferred+rampHalf,state.heldThresholdVolts};
        for(double level:levels){if((level-n.u)*(level-end)<=0.&&level!=n.u&&b!=0.){const double root=-tau*std::log1p((level-n.u)/b);if(root>epsilon&&root<=limit){limit=root;event=true;}}}
        const double triangleB=n.triangleGain*b;
        const auto w=[&](double t){return (n.w-triangleB*t/tau)*decay(t/tau);};
        // w has at most one turning point within an analytic fold segment.
        if(triangleB!=0.){const double turn=tau*(n.w+triangleB)/triangleB;if(turn>epsilon&&turn<limit){limit=turn;event=true;}}
        const double searchLimit=limit,wEnd=w(searchLimit),half=profile.triangleSpanVoltsNominal*.5;
        const bool increasing=wEnd>n.w;
        const double spacing=.25*half;
        int knot=increasing?int(std::floor((n.w+2e-11*half)/spacing))+1:int(std::ceil((n.w-2e-11*half)/spacing))-1;
        if(knot==0)knot=increasing?1:-1;
        if(increasing)knot=std::max(-4,knot);else knot=std::min(4,knot);
        if(knot>=-4&&knot<=4){const double level=spacing*double(knot);
            if((level-n.w)*(level-wEnd)<=0.&&level!=n.w){
                double lo=0.,hi=searchLimit,t=searchLimit*(level-n.w)/(wEnd-n.w);
                for(int iteration=0;iteration<5;++iteration){const double e=decay(t/tau),v=(n.w-triangleB*t/tau)*e,d=(-triangleB-n.w+triangleB*t/tau)*e/tau;
                    if((v<level)==increasing)lo=t;else hi=t;
                    const double candidate=d!=0.?t-(v-level)/d:(lo+hi)*.5;
                    if(std::abs(v-level)<1e-13&&std::abs(v-level)<=std::abs(d)*1e-11)break;
                    t=candidate>lo&&candidate<hi?candidate:(lo+hi)*.5;
                }
                // Tangential knees can have tiny voltage error but large time
                // error. Certify time error; rare shallow crossings get a
                // fixed-count long-double bisection, never an unbounded loop.
                const double ev=decay(t/tau),rv=(n.w-triangleB*t/tau)*ev-level,rd=(-triangleB-n.w+triangleB*t/tau)*ev/tau;
                if(std::abs(rd)<1e-3||std::abs(rv)>1e-13||std::abs(rv)>std::abs(rd)*1e-11){
                    long double lower=0.,upper=searchLimit;
                    for(int iteration=0;iteration<48;++iteration){const long double mid=(lower+upper)*.5L;
                        const long double value=(static_cast<long double>(n.w)-static_cast<long double>(triangleB)*mid/tau)*std::exp(-mid/tau);
                        if((value<level)==increasing)lower=mid;else upper=mid;}
                    t=static_cast<double>((lower+upper)*.5L);
                }
                if(t>epsilon&&t<=limit){limit=t;event=true;}
            }
        }
        return limit;
    }
    void recordEvent(double time,unsigned kind,const Nodes& before,const Nodes& after) noexcept {
        const auto v0=asArray(before.value),v1=asArray(after.value),s0=asArray(before.slope),s1=asArray(after.slope),c0=asArray(before.constant),c1=asArray(after.constant);
        std::array<double,3> dc{},da{},db{};
        bool nonzero=false;
        for(std::size_t branch=0;branch<3;++branch){dc[branch]=c1[branch]-c0[branch];da[branch]=(v1[branch]-v0[branch])-dc[branch];db[branch]=(s1[branch]-s0[branch])/rate+inverseTauSamples*da[branch];nonzero|=dc[branch]!=0.||da[branch]!=0.||db[branch]!=0.;}
        if(nonzero||kind==resetStart||kind==resetEnd){if(diagnostics.eventCount<maximumEventsPerTick){auto& e=diagnostics.events[diagnostics.eventCount++];e={time,state.elapsedSeconds+time,state.charge,kind,state.resetRemainingSeconds>0.,before.ungatedValue,after.ungatedValue};}else diagnostics.eventCapacityExceeded=true;}
        if(!nonzero)return;
        const double firstAge=std::clamp(1.-time*rate,0.,1.);
        const auto& step=OscillatorResidualKernel::shared().stepTable();
        const double position=firstAge*double(OscillatorResidualKernel::phases);
        const auto base=std::min(OscillatorResidualKernel::phases-1,static_cast<std::size_t>(position));
        const double frac=position-double(base);
        for(std::size_t i=0;i<OscillatorResidualKernel::length;++i){const auto table=base+i*OscillatorResidualKernel::phases;
            const auto index=(state.residualCursor+i)%OscillatorResidualKernel::length;
            const double st=step[table]+frac*(step[table+1]-step[table]);
            const double e=exponentialResidual[table]+frac*(exponentialResidual[table+1]-exponentialResidual[table]);
            const double te=timeExponentialResidual[table]+frac*(timeExponentialResidual[table+1]-timeExponentialResidual[table]);
            for(std::size_t branch=0;branch<3;++branch)if(dc[branch]!=0.||da[branch]!=0.||db[branch]!=0.)state.residual[branch][index]+=dc[branch]*st+da[branch]*e+db[branch]*te;
        }
    }
    // Exact event basis for this nominal model: every final branch between
    // events is C+(A+B*t)exp(-t/RC). Integrate the SAME kernel against that
    // basis, rather than truncate a BLAMP tail or approximate RC curvature.
    // Simpson integration on each linearly interpolated kernel cell has error
    // O((1/(phases*rate*RC))^4); lookup interpolation is tested separately.
    void prepareExponentialResiduals() noexcept {
        constexpr double dx=1./double(OscillatorResidualKernel::phases);
        inverseTauSamples=1./(rate*profile.couplingSecondsNominal);
        const auto& h=OscillatorResidualKernel::shared().impulseTable();
        weightedArea=weightedMoment=0.;exponentialResidual[0]=timeExponentialResidual[0]=0.;
        for(std::size_t i=1;i<OscillatorResidualKernel::points;++i){const double a=double(i-1)*dx,b=double(i)*dx,m=(a+b)*.5;
            const double fa=h[i-1]*std::exp(a*inverseTauSamples),fb=h[i]*std::exp(b*inverseTauSamples),fm=(h[i-1]+h[i])*.5*std::exp(m*inverseTauSamples);
            weightedArea+=dx*(fa+4*fm+fb)/6.;weightedMoment+=dx*(a*fa+4*m*fm+b*fb)/6.;
            exponentialResidual[i]=weightedArea;timeExponentialResidual[i]=weightedMoment;}
        for(std::size_t i=0;i<OscillatorResidualKernel::points;++i){const double t=double(i)*dx,e=std::exp(-t*inverseTauSamples),a=exponentialResidual[i]-weightedArea,b=timeExponentialResidual[i]-weightedMoment;
            exponentialResidual[i]=e*a;timeExponentialResidual[i]=e*(t*a-b);}
        exponentialResidual.back()=timeExponentialResidual.back()=0.;
    }
    std::array<double,OscillatorResidualKernel::points> exponentialResidual{},timeExponentialResidual{};
    double inverseTauSamples=0.,weightedArea=0.,weightedMoment=0.;
    OscillatorNominalProfile profile{};
    State state{};
    Diagnostics diagnostics{};
    double rate=96000.,dt=1./96000.,converterGain=0.,rampHalf=1.525;
};
} // namespace cs80
