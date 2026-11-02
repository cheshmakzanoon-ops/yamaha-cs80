// Prospective exact-bit scheduler gate. The original 7c864f `tick()` and its
// private analytic helpers are frozen in reference/WscLoopOriginal.h, in a
// separate namespace; optimizing the live oscillator must not edit that file.
// There is intentionally NO approximate voltage/time acceptance here.
#include "core/ReconstructedOscillator.h"
#include "reference/WscLoopOriginal.h"
#include "Ws1Tolerances.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
namespace old = cs80::wsc_original_reference;
using Current = cs80::ReconstructedOscillator;
using Original = old::ReconstructedOscillator;

struct Gate {
    std::uint64_t checks = 0, mismatches = 0, ticks = 0, events = 0;
    std::uint64_t quietTicks = 0, referenceSearches = 0;
    std::uint32_t rates = 0;
#if defined(CS80_WSC_ORACLE_MUTATE_STATE) || defined(CS80_WSC_ORACLE_MUTATE_EVENT)
    bool mutationApplied = false;
#endif

    template<typename T> void same(const char* field, T observed, T expected) {
        ++checks;
        bool match;
        if constexpr (std::is_same_v<T,double>)
            match = std::bit_cast<std::uint64_t>(observed)
                 == std::bit_cast<std::uint64_t>(expected);
        else match = observed == expected;
        if (!match && mismatches++ < 12) {
            if constexpr (std::is_same_v<T,double>)
                std::fprintf(stderr,"BIT MISMATCH %s: %016llx != %016llx\n",field,
                    static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(observed)),
                    static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(expected)));
            else std::fprintf(stderr,"BIT MISMATCH %s: scalar differs\n",field);
        }
    }

    template<typename A,typename B> void pins(const char* prefix,const A& x,const B& y) {
        (void)prefix;
        same("pins.sawPin5",x.sawPin5,y.sawPin5);
        same("pins.pulsePin7",x.pulsePin7,y.pulsePin7);
        same("pins.sinePin2",x.sinePin2,y.sinePin2);
    }

    void state(const Current::State& x,const Original::State& y) {
#define EQ_STATE(name) same("state." #name,x.name,y.name)
        EQ_STATE(charge); EQ_STATE(resetRemainingSeconds);
        EQ_STATE(inputBaselineVolts); EQ_STATE(triangleBaselineVolts);
        EQ_STATE(heldFrequencyHz); EQ_STATE(heldThresholdVolts);
        EQ_STATE(elapsedSeconds); EQ_STATE(sawEnabled); EQ_STATE(pulseEnabled);
        EQ_STATE(residualCursor); EQ_STATE(resetCount);
#undef EQ_STATE
        for(std::size_t channel=0;channel<3;++channel)
            for(std::size_t slot=0;slot<cs80::OscillatorResidualKernel::length;++slot)
                same("state.residual[3][16]",x.residual[channel][slot],y.residual[channel][slot]);
    }

    void diagnostics(const Current::Diagnostics& x,const Original::Diagnostics& y) {
#define EQ_DIAG(name) same("diagnostics." #name,x.name,y.name)
        EQ_DIAG(invalidControl); EQ_DIAG(frequencyLimited);
        EQ_DIAG(eventCapacityExceeded); EQ_DIAG(eventCount);
        EQ_DIAG(branchSearchCount); EQ_DIAG(rawPin7Volts);
        EQ_DIAG(coupledInputVolts); EQ_DIAG(trianglePin3Volts);
        EQ_DIAG(shaperInputVolts);
#undef EQ_DIAG
        pins("diagnostics.raw",x.raw,y.raw);
        // The entire diagnostic value is a continuation, even when eventCount
        // is zero. Compare stale/inactive slots, not just published events.
        for(std::size_t slot=0;slot<Current::maximumEventsPerTick;++slot) {
            const auto& a=x.events[slot];
            const auto& b=y.events[slot];
            same("events.secondsFromSampleStart",a.secondsFromSampleStart,b.secondsFromSampleStart);
            same("events.absoluteSeconds",a.absoluteSeconds,b.absoluteSeconds);
            same("events.charge",a.charge,b.charge);
            same("events.kind",a.kind,b.kind);
            same("events.resetActive",a.resetActive,b.resetActive);
            pins("events.before",a.before,b.before);
            pins("events.after",a.after,b.after);
        }
    }
};

old::OscillatorNominalProfile originalProfile(const cs80::OscillatorNominalProfile& p) {
    old::OscillatorNominalProfile q;
#define COPY_PROFILE(name) q.name = p.name
    COPY_PROFILE(resetSecondsNominal); COPY_PROFILE(referenceHzNominal);
    COPY_PROFILE(resetVoltsNominal); COPY_PROFILE(resetExcessVoltsNominal);
    COPY_PROFILE(couplingSecondsNominal); COPY_PROFILE(triangleSpanVoltsNominal);
    COPY_PROFILE(sliceVoltsInferred); COPY_PROFILE(pulseHalfSpanVoltsInferred);
    COPY_PROFILE(sineGainVoltsInferred); COPY_PROFILE(shaperOrdinateInferred);
#undef COPY_PROFILE
    return q;
}
old::WscControls originalControls(const cs80::WscControls& c) {
    return {c.pulseThresholdVolts,c.sawEnabled,c.pulseEnabled};
}
Original::State originalState(const Current::State& p) {
    Original::State q;
#define COPY_STATE(name) q.name = p.name
    COPY_STATE(charge); COPY_STATE(resetRemainingSeconds);
    COPY_STATE(inputBaselineVolts); COPY_STATE(triangleBaselineVolts);
    COPY_STATE(heldFrequencyHz); COPY_STATE(heldThresholdVolts);
    COPY_STATE(elapsedSeconds); COPY_STATE(sawEnabled); COPY_STATE(pulseEnabled);
    COPY_STATE(residual); COPY_STATE(residualCursor); COPY_STATE(resetCount);
#undef COPY_STATE
    return q;
}
Original::IdleState originalIdle(const Current::IdleState& p) {
    Original::IdleState q;
#define COPY_IDLE(name) q.name = p.name
    COPY_IDLE(charge); COPY_IDLE(resetRemainingSeconds);
    COPY_IDLE(inputBaselineVolts); COPY_IDLE(triangleBaselineVolts);
    COPY_IDLE(heldFrequencyHz); COPY_IDLE(heldThresholdVolts);
    COPY_IDLE(elapsedSeconds); COPY_IDLE(resetCount);
    COPY_IDLE(sawEnabled); COPY_IDLE(pulseEnabled);
#undef COPY_IDLE
    return q;
}

struct Pair {
    Current current;
    Original original;
    Gate& gate;
    std::uint64_t precedingSearches = 0;
    explicit Pair(Gate& g) : gate(g) {}

    void prepare(double rate,const cs80::OscillatorNominalProfile& p={}) {
        const bool a=current.prepare(rate,p);
        const bool b=original.prepare(rate,originalProfile(p));
        gate.same("prepare",a,b);
        gate.same("valid.prepare",a,true);
        gate.same("sampleRate",current.sampleRate(),original.sampleRate());
        check();
    }
    void reset(double phase,double frequency) {
        current.reset(phase,frequency);
        original.reset(phase,frequency);
        precedingSearches = 0;
        check();
    }
    void restore(const Current::State& state) {
        current.restoreState(state);
        original.restoreState(originalState(state));
        precedingSearches = original.getDiagnostics().branchSearchCount;
        check();
    }
    void restoreIdle(const Current::IdleState& state) {
        current.restoreIdleState(state);
        original.restoreIdleState(originalIdle(state));
        precedingSearches = original.getDiagnostics().branchSearchCount;
        check();
    }
    void check() {
        const auto& x = current.getState();
        const auto& y = original.getState();
#if defined(CS80_WSC_ORACLE_MUTATE_STATE)
        auto mutated = y;
        if(!gate.mutationApplied && gate.ticks > 16) {
            mutated.charge=std::nextafter(mutated.charge,std::numeric_limits<double>::infinity());
            gate.mutationApplied=true;
        }
        gate.state(x,mutated);
#else
        gate.state(x,y);
#endif
#if defined(CS80_WSC_ORACLE_MUTATE_EVENT)
        auto events = original.getDiagnostics();
        if(!gate.mutationApplied && events.eventCount > 0) {
            events.events[0].absoluteSeconds=std::nextafter(
                events.events[0].absoluteSeconds,std::numeric_limits<double>::infinity());
            gate.mutationApplied=true;
        }
        gate.diagnostics(current.getDiagnostics(),events);
#else
        gate.diagnostics(current.getDiagnostics(),original.getDiagnostics());
#endif
        gate.same("phase",current.getPhase(),original.getPhase());
        gate.same("maximumFrequency",current.maximumFrequencyHz(),original.maximumFrequencyHz());
    }
    void tick(double hz,const cs80::WscControls& controls={}) {
        const auto a=current.tick(hz,controls);
        const auto b=original.tick(hz,originalControls(controls));
        gate.pins("output",a,b);
        const auto& d=original.getDiagnostics();
        gate.events+=d.eventCount;
        gate.quietTicks+=(d.eventCount==0);
        gate.referenceSearches+=d.branchSearchCount-precedingSearches;
        precedingSearches=d.branchSearchCount;
        ++gate.ticks;
        check();
    }
    void idle(double seconds,double hz,const cs80::WscControls& controls) {
        const auto a=current.advanceIdle(seconds,hz,controls);
        const auto b=original.advanceIdle(seconds,hz,originalControls(controls));
        gate.same("idle.integral",a,b);
        precedingSearches=original.getDiagnostics().branchSearchCount;
        check();
    }
    void invalidPrepare(double rate,const cs80::OscillatorNominalProfile& p) {
        gate.same("invalid.prepare",current.prepare(rate,p),
                  original.prepare(rate,originalProfile(p)));
        check();
    }
};

void stableAndChanging(Gate& gate,double rate) {
    Pair p(gate);
    p.prepare(rate);
    p.reset(0.317,261.6);
    for(int n=0;n<8192;++n) p.tick(261.6);

    Pair copied=p; // Copy must preserve the private future-event cache.
    Pair assigned(gate);
    assigned.current=p.current;
    assigned.original=p.original;
    assigned.check(); // Assignment must also retain the entire oscillator.
    for(int n=0;n<256;++n) { p.tick(261.6); copied.tick(261.6); }

    for(int n=0;n<4096;++n) {
        const double hz=n<128 ? 0. : n<512 ? 440. : n<768 ? 4186.
                       : n<1024 ? 1e9 : n%257==0 ? std::numeric_limits<double>::quiet_NaN()
                       : n%331==0 ? std::numeric_limits<double>::infinity()
                       : n%339==0 ? -0. : 261.6+100.*std::sin(.006*double(n));
        cs80::WscControls control;
        control.pulseThresholdVolts=n%197==0 ? std::numeric_limits<double>::quiet_NaN()
            : n%201==0 ? std::numeric_limits<double>::infinity()
            : n%251==0 ? -0. : .7*std::sin(.005*double(n));
        control.sawEnabled=(n/37)%2==0;
        control.pulseEnabled=(n/43)%2!=0;
        p.tick(hz,control);
        if(n==1171) p.idle(.002,261.6,control);
        if(n==1200) p.restore(p.current.getState());
        if(n==1400) p.restoreIdle(p.current.getIdleState());
        if(n==1700) {
            cs80::OscillatorNominalProfile invalid;
            invalid.resetSecondsNominal=0.;
            p.invalidPrepare(rate,invalid);
            p.invalidPrepare(std::numeric_limits<double>::quiet_NaN(),{});
        }
        if(n==2300) {
            auto snap=p.current.getState();
            snap.inputBaselineVolts=std::numeric_limits<double>::denorm_min();
            snap.triangleBaselineVolts=-std::numeric_limits<double>::denorm_min();
            snap.charge=std::numeric_limits<double>::denorm_min();
            p.restore(snap);
        }
    }
    p.reset(0.,261.6);
    for(int n=0;n<192;++n) {
        cs80::WscControls controls;
        const double input=p.current.getDiagnostics().coupledInputVolts;
        controls.pulseThresholdVolts=input + (n%3==0 ? 0. : n%3==1 ? 2e-11 : -2e-11);
        controls.sawEnabled=n%5!=0;
        controls.pulseEnabled=n%7!=0;
        p.tick(n%9==0 ? -10. : 261.6,controls);
    }
}

void boundaries(Gate& gate,double rate) {
    constexpr double f=261.6;
    const double dt=1./rate;
    const std::array<double,10> edges{
        0.,dt,std::nextafter(dt,0.),
        std::nextafter(dt,std::numeric_limits<double>::infinity()),
        dt-3e-15,dt-2e-15,dt+2e-15,dt+3e-15,
        30e-6,std::numeric_limits<double>::denorm_min()
    };
    Pair p(gate);
    p.prepare(rate);
    for(const double boundary: edges) {
        for(const bool reset: {false,true}) {
            p.reset(.411,f);
            auto state=p.current.getState();
            state.resetRemainingSeconds=reset ? boundary : 0.;
            state.charge=reset ? .38 : f*boundary;
            state.inputBaselineVolts=reset ? -0. : 0.;
            state.triangleBaselineVolts=reset ? 0. : -0.;
            p.restore(state);
            for(int j=0;j<32;++j) p.tick(f);
        }
    }
    // The original reset plateau can itself be exactly one sample long.
    cs80::OscillatorNominalProfile profile;
    profile.resetSecondsNominal=dt;
    p.prepare(rate,profile);
    p.reset(0.,f);
    for(int n=0;n<512;++n) p.tick(f);
}

void verifyContract(Gate& g,const ws1::Tolerances& rules) {
    if(g.mismatches > rules["maximum_bit_mismatches"])
        throw std::runtime_error("bit mismatch budget exceeded");
    if(g.ticks != rules["expected_compared_ticks"])
        throw std::runtime_error("fixture tick count changed");
    if(g.events < rules["minimum_observed_events"])
        throw std::runtime_error("event fixture was not reached");
    if(g.quietTicks < rules["minimum_observed_no_event_ticks"])
        throw std::runtime_error("no-event fixture was not reached");
    if(g.referenceSearches < rules["minimum_reference_branch_searches"])
        throw std::runtime_error("branch-cache fixture was not reached");
    if(g.rates != rules["expected_independent_rates"])
        throw std::runtime_error("fixture rate count changed");
}
} // namespace

int main(int argc,char** argv) {
    try {
        const ws1::Tolerances rule(argc>1 ? argv[1] : "tests/cpu/wsc-steady-fastpath.json",
                                     "wsc_steady_fast_path");
        Gate gate;
        for(double rate: {88200.,96000.,192000.,384000.}) {
            stableAndChanging(gate,rate);
            boundaries(gate,rate);
            ++gate.rates;
        }
        std::printf("WSC original-loop oracle: rates=%u ticks=%llu events=%llu quiet=%llu "
                    "searches=%llu checks=%llu mismatches=%llu\n",gate.rates,
                    static_cast<unsigned long long>(gate.ticks),
                    static_cast<unsigned long long>(gate.events),
                    static_cast<unsigned long long>(gate.quietTicks),
                    static_cast<unsigned long long>(gate.referenceSearches),
                    static_cast<unsigned long long>(gate.checks),
                    static_cast<unsigned long long>(gate.mismatches));
#if defined(CS80_WSC_ORACLE_MUTATE_STATE) || defined(CS80_WSC_ORACLE_MUTATE_EVENT)
        if (!gate.mutationApplied) throw std::runtime_error("mutation witness not reached");
#endif
        verifyContract(gate,rule);
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"WSC oracle FAIL: %s\n",e.what());
        return 1;
    }
}
