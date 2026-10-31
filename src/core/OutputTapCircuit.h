#pragma once
// Source-informed ideal passive output graph with a LABELED INFERRED profile.
// Not a recovered installed revision, measured output calibration, or amplifier
// model. See docs/OUTPUT_REALTIME_PROPOSAL.md for evidence/conflicts/boundaries.
#include <algorithm>
#include <array>
#include <cmath>

namespace cs80 {
// Circuit-engine gain staging (docs/IMPROVEMENT_PLAN.md Phase 1.5).
//
// kCircuitOutputReference is a SOFTWARE CALIBRATION, not a recovered gain: the
// engine's normalized preamp-source units are not volts, so a fixed mapping to
// digital full scale must be chosen. -6 dB was chosen from cs80_engine_audit
// (phrase suite, 48 kHz, 2026-10-31): source tone tabs peaked at up to
// +3.2 dBFS and their loudest chord was 5 dB above the legacy tabs. After the
// trim the worst peak is about -2.8 dBFS and loudness sits with legacy.
//
// circuitOutputDriver is an INFERRED headroom model for the preamp/driver that
// feeds the passive output network: unity below the knee, then a smooth
// (C1-continuous) approach to a ceiling at digital full scale, as a real driver
// approaching its supply rails would. Its knee and ceiling are not measured;
// it only engages on peaks the trim leaves above -1 dBFS.
inline constexpr double kCircuitOutputReference = 0.5;
inline constexpr double kCircuitDriverKnee = 0.891250938; // -1 dBFS
inline constexpr double kCircuitDriverCeiling = 1.0;
inline double circuitOutputDriver(double x) noexcept {
    const double magnitude = std::abs(x);
    if (!(magnitude > kCircuitDriverKnee)) return x;
    const double span = kCircuitDriverCeiling - kCircuitDriverKnee;
    const double shaped = kCircuitDriverKnee + span * std::tanh((magnitude - kCircuitDriverKnee) / span);
    return std::copysign(shaped, x);
}

class OutputTapCircuit {
public:
    enum class Mode { high, low };
    enum class LoadKind { open, shorted, finite };
    struct Load {
        LoadKind kind = LoadKind::finite;
        double ohms = 100000.0; // Inferred receiver; ignored for explicit open/short.
    };
    struct Profile {
        double couplingFarads = 10e-6; // Documented nominal; ideal capacitor.
        double sourceOhms = 100.0; // INFERRED Thevenin driver boundary.
        double highSeriesOhms = 560.0; // RIGHT 56k literal alternative retained in tests.
        double lowSeriesOhms = 4700.0;
        double lowShuntOhms = 560.0; // GENERAL 560k literal alternative retained in tests.
        Load load{};
    };
    struct Nodes {
        double capacitorInputVolts=0.0, capacitorVolts=0.0, commonVolts=0.0;
        double highTapVolts=0.0, lowTapVolts=0.0, jackVolts=0.0;
        // Positive toward common, toward taps, then toward reference/load.
        double sourceAmps=0.0, highSeriesAmps=0.0, lowSeriesAmps=0.0;
        double shuntAmps=0.0, loadAmps=0.0;
        double audibleJackVolts=0.0; // SOFTWARE readout, not another physical node.
    };
    struct State {
        double capacitorVolts=0.0; // The ONLY physical energy-storage state.
        double previousSourceVolts=0.0;
        double readoutGain=0.0; // Separate software dezipper, not capacitor charge.
    };
    struct Coefficients {
        double parallelOhms=0.0, totalOhms=0.0, tauSeconds=0.0;
        double jackGain=0.0, integrationB=0.0, lowBranchOhms=0.0;
        double loadedLowShuntOhms=0.0;
    };
    static constexpr double minimumRate=8000.0, maximumRate=1536000.0;
    static constexpr double maximumInputMagnitude=1000000.0;
    static constexpr double readoutTauSeconds=0.005; // INFERRED software policy.

    OutputTapCircuit() noexcept { (void)prepare(96000.0,Profile{},Mode::high); }
    static bool validMode(Mode m) noexcept { return m==Mode::high || m==Mode::low; }
    static bool validProfile(const Profile& p) noexcept {
        const auto within=[](double x,double lo,double hi) {
            return std::isfinite(x) && x>=lo && x<=hi;
        };
        const bool load=p.load.kind==LoadKind::open || p.load.kind==LoadKind::shorted
            || (p.load.kind==LoadKind::finite && within(p.load.ohms,100.0,1e9));
        return load && within(p.couplingFarads,1e-6,100e-6)
            && within(p.sourceOhms,0.0,10000.0)
            && within(p.highSeriesOhms,100.0,100000.0)
            && within(p.lowSeriesOhms,1000.0,100000.0)
            && within(p.lowShuntOhms,100.0,1000000.0);
    }
    // Setup-only. Complete validation precedes every write to the live object.
    // Valid prepare intentionally resets charge/history; seamless rate changes
    // and reactive loads are not supported by this single-capacitor model.
    bool prepare(double sampleRate,const Profile& p,Mode selected) noexcept {
        if (!validProfile(p) || !validMode(selected) || !std::isfinite(sampleRate)
            || sampleRate<minimumRate || sampleRate>maximumRate) return false;
        const std::array<Coefficients,2> next {
            coefficients(p,sampleRate,Mode::high),coefficients(p,sampleRate,Mode::low)
        };
        profile=p; rate=sampleRate; endpoints=next;
        readoutStep=-std::expm1(-1.0/(readoutTauSeconds*rate));
        return reset(selected);
    }
    void reset() noexcept {
        state={0.0,0.0,selectedCoefficients().jackGain};
    }
    bool reset(Mode selected) noexcept {
        if (!validMode(selected)) return false;
        mode=selected; reset(); return true;
    }
    // Selects the topology for the NEXT integration interval. The event itself
    // preserves voltage/Uprev/Ks. The old-end derivative is rebased onto this
    // new topology; no stale old-topology capacitor-current state is carried.
    bool setMode(Mode selected) noexcept {
        if (!validMode(selected)) return false;
        mode=selected; return true;
    }
    static double boundedInput(double input) noexcept {
        return std::isfinite(input)
            ? std::clamp(input,-maximumInputMagnitude,maximumInputMagnitude) : 0.0;
    }
    Nodes process(double input) noexcept {
        const double u=boundedInput(input);
        const auto& c=selectedCoefficients();
        state.capacitorVolts+=c.integrationB*(u+state.previousSourceVolts-2.0*state.capacitorVolts);
        state.previousSourceVolts=u;
        state.readoutGain+=readoutStep*(c.jackGain-state.readoutGain);
        const double residual=u-state.capacitorVolts;
        Nodes out;
        out.capacitorVolts=state.capacitorVolts;
        out.sourceAmps=residual/c.totalOhms;
        out.commonVolts=c.parallelOhms*out.sourceAmps;
        out.capacitorInputVolts=out.capacitorVolts+out.commonVolts;
        out.lowSeriesAmps=out.commonVolts/c.lowBranchOhms;
        out.lowTapVolts=out.lowSeriesAmps*c.loadedLowShuntOhms;
        const bool loadedHigh=mode==Mode::high && profile.load.kind!=LoadKind::open;
        const double loadOhms=profile.load.kind==LoadKind::finite?profile.load.ohms:0.0;
        if (loadedHigh) {
            out.highSeriesAmps=out.commonVolts/(profile.highSeriesOhms+loadOhms);
            out.highTapVolts=out.highSeriesAmps*loadOhms;
        } else out.highTapVolts=out.commonVolts;
        out.jackVolts=mode==Mode::high?out.highTapVolts:out.lowTapVolts;
        out.shuntAmps=out.lowTapVolts/profile.lowShuntOhms;
        if (profile.load.kind==LoadKind::shorted)
            out.loadAmps=mode==Mode::high?out.highSeriesAmps:out.lowSeriesAmps;
        else if (profile.load.kind==LoadKind::finite) out.loadAmps=out.jackVolts/loadOhms;
        out.audibleJackVolts=state.readoutGain*residual;
        return out;
    }
    // Render-thread-only diagnostics. No lock-free cross-thread access promise.
    const State& getState() const noexcept { return state; }
    const Profile& getProfile() const noexcept { return profile; }
    const Coefficients& selectedCoefficients() const noexcept {
        return endpoints[mode==Mode::high?0:1];
    }
    double sampleRate() const noexcept { return rate; }
    Mode getMode() const noexcept { return mode; }
private:
    static Coefficients coefficients(const Profile& p,double fs,Mode selected) noexcept {
        const auto parallel=[](double a,double b) { return a/(a+b)*b; };
        Coefficients c;
        const bool high=selected==Mode::high, open=p.load.kind==LoadKind::open;
        const double rl=p.load.kind==LoadKind::finite?p.load.ohms:0.0;
        c.loadedLowShuntOhms=high || open?p.lowShuntOhms:
            p.load.kind==LoadKind::shorted?0.0:parallel(p.lowShuntOhms,rl);
        c.lowBranchOhms=p.lowSeriesOhms+c.loadedLowShuntOhms;
        c.parallelOhms=high && !open?parallel(c.lowBranchOhms,p.highSeriesOhms+rl):c.lowBranchOhms;
        const double m=high?(open?1.0:rl/(p.highSeriesOhms+rl)):
            c.loadedLowShuntOhms/c.lowBranchOhms;
        c.totalOhms=p.sourceOhms+c.parallelOhms;
        c.tauSeconds=p.couplingFarads*c.totalOhms;
        c.jackGain=m*c.parallelOhms/c.totalOhms;
        c.integrationB=1.0/(2.0*c.tauSeconds*fs+1.0);
        return c;
    }
    Profile profile{};
    State state{};
    std::array<Coefficients,2> endpoints{};
    double rate=96000.0,readoutStep=0.0;
    Mode mode=Mode::high;
};

// Named engineering choice, NOT an assertion that three installed channels had
// equal trims/resistors. Each tap remains independently configurable at setup.
struct InferredSymmetricOutputProfile {
    OutputTapCircuit::Profile left{},right{},general{};
    double generalToLeftFeedbackRatio=1.0; // INFERRED RfG/RfL.
    double generalToRightFeedbackRatio=1.0; // INFERRED RfG/RfR.
};

class OriginalOutputCircuit {
public:
    using Mode=OutputTapCircuit::Mode;
    using Profile=InferredSymmetricOutputProfile;
    struct Outputs {
        OutputTapCircuit::Nodes left{},right{},general{};
    };
    OriginalOutputCircuit() noexcept { (void)prepare(96000.0,Profile{},Mode::high); }
    static bool validProfile(const Profile& p) noexcept {
        const auto ratio=[](double x){return std::isfinite(x) && x>=0.25 && x<=4.0;};
        return OutputTapCircuit::validProfile(p.left) && OutputTapCircuit::validProfile(p.right)
            && OutputTapCircuit::validProfile(p.general)
            && ratio(p.generalToLeftFeedbackRatio) && ratio(p.generalToRightFeedbackRatio);
    }
    bool prepare(double rate,const Profile& p,Mode selected) noexcept {
        if (!validProfile(p) || !OutputTapCircuit::validMode(selected) || !std::isfinite(rate)
            || rate<OutputTapCircuit::minimumRate || rate>OutputTapCircuit::maximumRate) return false;
        // All parameters have been validated; no partial replacement is possible.
        (void)left.prepare(rate,p.left,selected);
        (void)right.prepare(rate,p.right,selected);
        (void)general.prepare(rate,p.general,selected);
        profile=p; return true;
    }
    bool setMode(Mode selected) noexcept {
        if (!OutputTapCircuit::validMode(selected)) return false;
        (void)left.setMode(selected);(void)right.setMode(selected);(void)general.setMode(selected);
        return true;
    }
    void reset() noexcept { left.reset();right.reset();general.reset(); }
    bool reset(Mode selected) noexcept {
        if (!OutputTapCircuit::validMode(selected)) return false;
        (void)left.reset(selected);(void)right.reset(selected);(void)general.reset(selected);
        return true;
    }
    Outputs process(double leftSource,double rightSource) noexcept {
        const double l=OutputTapCircuit::boundedInput(leftSource);
        const double r=OutputTapCircuit::boundedInput(rightSource);
        // Inputs are normalized ideal LEFT/RIGHT preamp-source units, NOT LI/RI
        // volts. Derive GENERAL BEFORE any coupling/loading. Its independent
        // module input is bounded by the same explicit numerical guard.
        const double g=0.47*(profile.generalToLeftFeedbackRatio*l
            +profile.generalToRightFeedbackRatio*r);
        return {left.process(l),right.process(r),general.process(g)};
    }
    const Profile& getProfile() const noexcept { return profile; }
    const OutputTapCircuit& leftCircuit() const noexcept { return left; }
    const OutputTapCircuit& rightCircuit() const noexcept { return right; }
    const OutputTapCircuit& generalCircuit() const noexcept { return general; }
private:
    Profile profile{};
    OutputTapCircuit left{},right{},general{};
};
} // namespace cs80
