// Test-only original-source WSC independent-lane oracle. This is a RED-first
// prerequisite for a prospective real AVX2 four-double-lane oscillator path;
// there is deliberately no optimized backend or performance claim here.
// The frozen reference is in another namespace and may not be rewritten to
// match any future candidate. Compare fields, never C++ struct padding.
#include "core/ReconstructedOscillator.h"
#include "reference/WscLoopOriginal.h"

#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#include <immintrin.h>
#define CS80_WSC_LANE_X86 1
#else
#define CS80_WSC_LANE_X86 0
#endif

#pragma STDC FENV_ACCESS ON

namespace {
using Current = cs80::ReconstructedOscillator;
using Original = cs80::wsc_original_reference::ReconstructedOscillator;
namespace frozen = cs80::wsc_original_reference;
constexpr int kLanes = 16;
constexpr std::array<int,8> kWidths{1,2,3,4,7,8,15,16};
constexpr std::array<double,4> kRates{88200.,96000.,192000.,384000.};
constexpr std::array<int,4> kRounds{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};

struct Checks {
    std::uint64_t fields=0,failures=0,active=0,masked=0,events=0,noEvents=0;
    std::uint64_t copies=0,resets=0,restores=0,idles=0,fpCases=0;
    std::uint32_t widthMask=0,rateMask=0;
    bool mutationApplied=false;
    int caseIndex=0,sample=0,lane=0;

    template<class T> void same(const char* field,T actual,T expected) {
        static_assert(std::is_integral_v<T> || std::is_same_v<T,double>);
        ++fields;
        bool ok;
        if constexpr(std::is_same_v<T,double>)
            ok=std::bit_cast<std::uint64_t>(actual)==std::bit_cast<std::uint64_t>(expected);
        else ok=actual==expected;
        if(!ok) {
            ++failures;
            if(failures<=8) {
                std::fprintf(stderr,"FAIL %s case=%d sample=%d lane=%d",field,caseIndex,sample,lane);
                if constexpr(std::is_same_v<T,double>)
                    std::fprintf(stderr," bits=0x%016llx/0x%016llx\n",
                                 static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(actual)),
                                 static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(expected)));
                else std::fprintf(stderr," actual=%llu expected=%llu\n",
                                  static_cast<unsigned long long>(actual),
                                  static_cast<unsigned long long>(expected));
            }
            if(failures>=9) throw std::runtime_error("WSC exact bit mismatch budget exceeded");
        }
    }

    template<class A,class B> void pins(const char* name,const A& actual,const B& expected) {
        (void)name;
        same("output.sawPin5",actual.sawPin5,expected.sawPin5);
        same("output.pulsePin7",actual.pulsePin7,expected.pulsePin7);
        same("output.sinePin2",actual.sinePin2,expected.sinePin2);
    }
    void state(const Current::State& a,const Original::State& b) {
#define EQ_FIELD(x) same("lane.state." #x,a.x,b.x)
        EQ_FIELD(charge); EQ_FIELD(resetRemainingSeconds);
        EQ_FIELD(inputBaselineVolts); EQ_FIELD(triangleBaselineVolts);
        EQ_FIELD(heldFrequencyHz); EQ_FIELD(heldThresholdVolts);
        EQ_FIELD(elapsedSeconds); EQ_FIELD(sawEnabled); EQ_FIELD(pulseEnabled);
        EQ_FIELD(residualCursor); EQ_FIELD(resetCount);
#undef EQ_FIELD
        for(std::size_t branch=0;branch<3;++branch)
            for(std::size_t slot=0;slot<cs80::OscillatorResidualKernel::length;++slot)
                same("lane.state.residual",a.residual[branch][slot],b.residual[branch][slot]);
    }
    void diagnostics(const Current::Diagnostics& a,const Original::Diagnostics& b) {
#define EQ_FIELD(x) same("lane.diagnostics." #x,a.x,b.x)
        EQ_FIELD(invalidControl); EQ_FIELD(frequencyLimited);
        EQ_FIELD(eventCapacityExceeded); EQ_FIELD(eventCount);
        EQ_FIELD(branchSearchCount); EQ_FIELD(rawPin7Volts);
        EQ_FIELD(coupledInputVolts); EQ_FIELD(trianglePin3Volts);
        EQ_FIELD(shaperInputVolts);
#undef EQ_FIELD
        pins("raw",a.raw,b.raw);
        // Inactive event slots are part of observable continuation; never
        // compare only eventCount active entries or object padding.
        for(std::size_t index=0;index<Current::maximumEventsPerTick;++index) {
            const auto& x=a.events[index];const auto& y=b.events[index];
            same("events.secondsFromSampleStart",x.secondsFromSampleStart,y.secondsFromSampleStart);
            same("events.absoluteSeconds",x.absoluteSeconds,y.absoluteSeconds);
            same("events.charge",x.charge,y.charge);
            same("events.kind",x.kind,y.kind);
            same("events.resetActive",x.resetActive,y.resetActive);
            pins("events.before",x.before,y.before);
            pins("events.after",x.after,y.after);
        }
    }
};

#if CS80_WSC_LANE_X86 && (defined(__GNUC__) || defined(__clang__))
std::uint16_t x87Control() { std::uint16_t v=0; __asm__ volatile("fnstcw %0":"=m"(v)); return v; }
std::uint16_t x87Status() { std::uint16_t v=0; __asm__ volatile("fnstsw %0":"=am"(v)); return v; }
#else
std::uint16_t x87Control() { return 0; }
std::uint16_t x87Status() { return 0; }
#endif

struct FpState {
    std::fenv_t env{};
    int rounding=FE_TONEAREST,exceptions=0;
    std::uint16_t x87Control=0,x87Status=0;
#if CS80_WSC_LANE_X86
    unsigned mxcsr=0;
#endif
};
FpState captureFp() {
    FpState x;
    std::fegetenv(&x.env);
    x.rounding=std::fegetround();
    x.exceptions=std::fetestexcept(FE_ALL_EXCEPT);
    x.x87Control=x87Control();x.x87Status=x87Status();
#if CS80_WSC_LANE_X86
    x.mxcsr=_mm_getcsr();
#endif
    return x;
}
void restoreFp(const FpState& x) {
    std::fesetenv(&x.env);
#if CS80_WSC_LANE_X86
    _mm_setcsr(x.mxcsr);
#endif
}
struct RestoreCallerFp {
    FpState before=captureFp();
    ~RestoreCallerFp(){restoreFp(before);}
};
void checkFp(Checks& g,const FpState& a,const FpState& b) {
    g.same("fp.rounding",a.rounding,b.rounding);
    g.same("fp.exceptions",a.exceptions,b.exceptions);
    g.same("fp.x87Control",a.x87Control,b.x87Control);
    g.same("fp.x87StatusExceptions",static_cast<unsigned>(a.x87Status&0x3fu),
           static_cast<unsigned>(b.x87Status&0x3fu));
#if CS80_WSC_LANE_X86
    g.same("fp.mxcsr",a.mxcsr,b.mxcsr);
#endif
}

std::vector<unsigned> denormalModes() {
#if CS80_WSC_LANE_X86
    alignas(16) std::array<unsigned char,512> registers{};
    _fxsave(registers.data());
    unsigned mask=0;
    std::memcpy(&mask,registers.data()+28,sizeof(mask));
    if(mask==0)mask=0xffbfu;
    std::vector<unsigned> modes{0};
    if(mask&(1u<<15))modes.push_back(1u<<15);
    if(mask&(1u<<6))modes.push_back(1u<<6);
    if((mask&((1u<<15)|(1u<<6)))==((1u<<15)|(1u<<6)))
        modes.push_back((1u<<15)|(1u<<6));
    return modes;
#else
    return {0};
#endif
}

frozen::WscControls originalControls(const cs80::WscControls& c) {
    return {c.pulseThresholdVolts,c.sawEnabled,c.pulseEnabled};
}
double frequencyFor(int lane,int sample) {
    const double f=75.0+static_cast<double>(lane)*31.0
                   +static_cast<double>((sample/37)%5)*0.125;
    if(sample==47&&lane%3==0)return 0.0;
    if(sample==67&&lane%7==0)return std::numeric_limits<double>::infinity();
    return f;
}
cs80::WscControls controlsFor(int lane,int sample) {
    const double base=(static_cast<double>(lane)-7.0)*0.11;
    cs80::WscControls c;
    c.pulseThresholdVolts=base+static_cast<double>((sample/23)%3)*0.09;
    c.sawEnabled=((sample/41+lane)%3)!=0;
    c.pulseEnabled=((sample/29+lane)%4)!=0;
    if(sample==89&&lane==3)c.pulseThresholdVolts=std::numeric_limits<double>::quiet_NaN();
    if(sample==83&&lane==1)c.pulseThresholdVolts=std::numeric_limits<double>::denorm_min();
    if(sample==117&&lane==2)c.pulseThresholdVolts=-0.0;
    return c;
}

void oneCase(Checks& g,int width,double rate,int samples,int round,unsigned denorm,int caseNumber) {
    RestoreCallerFp caller;
    if(std::fesetround(round)!=0)throw std::runtime_error("unsupported FE rounding mode");
#if CS80_WSC_LANE_X86
    unsigned mxcsr=_mm_getcsr();
    mxcsr=(mxcsr&~((1u<<15)|(1u<<6)))|denorm;
    _mm_setcsr(mxcsr);
#else
    (void)denorm;
#endif
    std::array<Current,kLanes> candidate;
    std::array<Original,kLanes> original;
    Current::State savedCurrent{};
    Original::State savedOriginal{};
    for(int i=0;i<kLanes;++i) {
        g.same("lane.prepare.candidate",candidate[static_cast<std::size_t>(i)].prepare(rate,{}),true);
        g.same("lane.prepare.original",original[static_cast<std::size_t>(i)].prepare(rate,{}),true);
        candidate[static_cast<std::size_t>(i)].reset(0.0375*static_cast<double>(i),frequencyFor(i,0));
        original[static_cast<std::size_t>(i)].reset(0.0375*static_cast<double>(i),frequencyFor(i,0));
    }
    for(int sample=0;sample<samples;++sample) {
        g.sample=sample;
        if(sample==19) {
            savedCurrent=candidate[3].getState();savedOriginal=original[3].getState();
        }
        if(sample==31) {candidate[7]=candidate[0];original[7]=original[0];++g.copies;}
        if(sample==63) {
            candidate[6].reset(0.17,frequencyFor(6,sample));
            original[6].reset(0.17,frequencyFor(6,sample));++g.resets;
        }
        if(sample==91) {
            const double seconds=0.0013;
            const auto control=controlsFor(5,sample);
            const FpState before=captureFp();
            const double a=candidate[5].advanceIdle(seconds,frequencyFor(5,sample),control);
            const FpState afterCandidate=captureFp();restoreFp(before);
            const double b=original[5].advanceIdle(seconds,frequencyFor(5,sample),originalControls(control));
            const FpState afterOriginal=captureFp();
            g.lane=5;g.same("lane.idle.integral",a,b);checkFp(g,afterCandidate,afterOriginal);
            restoreFp(afterCandidate);++g.idles;
        }
        if(sample==97) {
            candidate[3].restoreState(savedCurrent);original[3].restoreState(savedOriginal);
            ++g.restores;
        }
        // Exact physical event at the end of the internal sample, one ULP
        // below/above it, a one-sample reset plateau and signed-zero charge.
        // Both namespaces receive independent copies of the same physical
        // state, but the reference continues through its frozen scheduler.
        if(sample>=113&&sample<=115) {
            const double boundary=frequencyFor(2,sample)/rate;
            const double charge=sample==113?boundary:
                std::nextafter(boundary,sample==114?0.0:std::numeric_limits<double>::infinity());
            auto c=candidate[2].getState();auto o=original[2].getState();
            c.charge=o.charge=charge;
            c.resetRemainingSeconds=o.resetRemainingSeconds=0.0;
            candidate[2].restoreState(c);original[2].restoreState(o);
        }
        if(sample==116||sample==118) {
            auto c=candidate[2].getState();auto o=original[2].getState();
            c.charge=o.charge=sample==116?0.4:-0.0;
            c.resetRemainingSeconds=o.resetRemainingSeconds=sample==116?1.0/rate:0.0;
            candidate[2].restoreState(c);original[2].restoreState(o);
        }
        // Every sample has a different lane presentation order. A future
        // SIMD kernel must not assume that physical lane i owns voice i.
        for(int pos=0;pos<kLanes;++pos) {
            const int lane=(pos*3+sample)%kLanes;
            g.lane=lane;
            const bool masked=lane>=width||(sample+3*lane)%13==0;
            const double f=frequencyFor(lane,sample);
            const auto c=controlsFor(lane,sample);
            if(masked) {
                ++g.masked;
#if defined(CS80_WSC_LANE_MUTATE_MASK)
                if(!g.mutationApplied&&caseNumber==0&&sample==13&&lane==0) {
                    candidate[static_cast<std::size_t>(lane)].tick(f,c);
                    g.mutationApplied=true;
                }
#endif
                continue;
            }
            ++g.active;
            if(sample%19==0)std::feraiseexcept(FE_INEXACT);
            const FpState before=captureFp();
            double candidateHz=f;
#if defined(CS80_WSC_LANE_MUTATE_CROSS_LANE)
            if(!g.mutationApplied&&caseNumber==0&&sample==17&&lane==0) {
                candidateHz=frequencyFor(1,sample);g.mutationApplied=true;
            }
#endif
            const auto a=candidate[static_cast<std::size_t>(lane)].tick(candidateHz,c);
#if defined(CS80_WSC_LANE_MUTATE_FP)
            if(!g.mutationApplied&&caseNumber==0&&sample==19&&lane==0) {
                std::feclearexcept(FE_INEXACT);g.mutationApplied=true;
            }
#endif
            const FpState afterCandidate=captureFp();restoreFp(before);
            const auto b=original[static_cast<std::size_t>(lane)].tick(f,originalControls(c));
            const FpState afterOriginal=captureFp();
            g.pins("lane.output",a,b);
            checkFp(g,afterCandidate,afterOriginal);
            restoreFp(afterCandidate);
            const auto& diag=original[static_cast<std::size_t>(lane)].getDiagnostics();
            g.events+=diag.eventCount;
            g.noEvents+=diag.eventCount==0;
        }
        // Compare all sixteen lanes, including masked and never-clocked lanes.
        // Full inactive event-slot diagnostic continuation is checked too.
        for(int lane=0;lane<kLanes;++lane) {
            g.lane=lane;
            g.state(candidate[static_cast<std::size_t>(lane)].getState(),
                    original[static_cast<std::size_t>(lane)].getState());
#if defined(CS80_WSC_LANE_MUTATE_EVENT)
            auto corrupted=candidate[static_cast<std::size_t>(lane)].getDiagnostics();
            if(!g.mutationApplied&&corrupted.eventCount>0) {
                corrupted.events[0].absoluteSeconds=std::nextafter(
                    corrupted.events[0].absoluteSeconds,std::numeric_limits<double>::infinity());
                g.mutationApplied=true;
            }
            g.diagnostics(corrupted,original[static_cast<std::size_t>(lane)].getDiagnostics());
#else
            g.diagnostics(candidate[static_cast<std::size_t>(lane)].getDiagnostics(),
                          original[static_cast<std::size_t>(lane)].getDiagnostics());
#endif
        }
    }
    (void)caseNumber;
    ++g.fpCases;
}

} // namespace

int main() {
    try {
        Checks g;
        int caseNumber=0;
        const auto denorm=denormalModes();
        for(std::size_t rate=0;rate<kRates.size();++rate) {
            for(std::size_t w=0;w<kWidths.size();++w) {
                g.caseIndex=caseNumber;
                oneCase(g,kWidths[w],kRates[rate],192,FE_TONEAREST,0,caseNumber++);
                g.widthMask|=1u<<w;g.rateMask|=1u<<rate;
            }
        }
        for(double rate:kRates)
            for(int round:kRounds)
                for(unsigned mxcsr:denorm) {
                    g.caseIndex=caseNumber;
                    oneCase(g,4,rate,128,round,mxcsr,caseNumber++);
                }
        const std::uint64_t expectedCases=32u+16u*denorm.size();
        const std::uint64_t expectedSamples=32u*192u+16u*denorm.size()*128u;
        const std::uint64_t scheduledSlots=expectedSamples*kLanes;
        std::printf("WSC independent-lane frozen oracle: cases=%d widths=%u rates=%u "
                    "denormal-modes=%zu active=%llu masked=%llu events=%llu no-events=%llu "
                    "copies=%llu resets=%llu restores=%llu idle-wakes=%llu "
                    "bit-field-checks=%llu mismatches=%llu vector-backend-calls=0\n",
                    caseNumber,static_cast<unsigned>(kWidths.size()),
                    static_cast<unsigned>(kRates.size()),denorm.size(),
                    static_cast<unsigned long long>(g.active),
                    static_cast<unsigned long long>(g.masked),
                    static_cast<unsigned long long>(g.events),
                    static_cast<unsigned long long>(g.noEvents),
                    static_cast<unsigned long long>(g.copies),
                    static_cast<unsigned long long>(g.resets),
                    static_cast<unsigned long long>(g.restores),
                    static_cast<unsigned long long>(g.idles),
                    static_cast<unsigned long long>(g.fields),
                    static_cast<unsigned long long>(g.failures));
        if(g.widthMask!=0xffu||g.rateMask!=0xfu
           ||static_cast<std::uint64_t>(caseNumber)!=expectedCases
           ||g.active+g.masked!=scheduledSlots||g.active<10000||g.masked<10000
           ||g.fields<scheduledSlots*775u||g.events<100||g.noEvents<10000
           ||g.copies!=expectedCases||g.resets!=expectedCases
           ||g.restores!=expectedCases||g.idles!=expectedCases||g.failures!=0)
            throw std::runtime_error("WSC registered lane/work/bit acceptance failed");
#if defined(CS80_WSC_LANE_MUTATE_MASK) || defined(CS80_WSC_LANE_MUTATE_CROSS_LANE) || \
    defined(CS80_WSC_LANE_MUTATE_EVENT) || defined(CS80_WSC_LANE_MUTATE_FP)
        if(!g.mutationApplied)throw std::runtime_error("WSC named mutation not reached");
#endif
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"WSC independent-lane FAIL: %s\n",e.what());
        return 1;
    }
}
