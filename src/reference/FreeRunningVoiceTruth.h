#pragma once

// Offline truth model for free-running voice continuity (IMPROVEMENT_PLAN 2.1).
// NOT used by Engine. One rank chain (oscillator -> IG00156 HP stage -> LP stage)
// advanced at the full internal rate on EVERY sample, in double precision, with
// the nodal-stage implicit equation solved to convergence. It restates the
// declared realtime stage law (src/core/Ig00156Filter.h: the M41 graph with
// the fourth-order linear phase correction and the inferred OTA tanh) without the
// realtime model's float controls, cached Newton start, fastTanh or iteration
// cap, so the cheap idle path in src/experimental/ContinuousVoiceCircuit.h can
// be held to it.
//
// v2 (ws1 + ws3 merge, 2026-10-31): the circuit engine's oscillator is the
// event-driven reconstructed WSC (src/core/ReconstructedOscillator.h). It is
// already a double-precision continuous-time model whose own accuracy is gated
// by cs80_reconstructed_oscillator_tests and cs80_oscillator_promotion_tests,
// so the truth ticks that model on every sample rather than restating its
// 300-line event solver; the idle path is held to per-sample ticking of it.
// v1 restated the PolyBLEP oscillator and the first-stage Svf circuit, which
// the circuit engine no longer runs.
//
// It is a numerical reference for the declared model, not a measured CS-80.

#include "core/Ig00156Filter.h"
#include "core/ReconstructedOscillator.h"
#include "IG00156PhaseCorrection.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace cs80::reference::free_running
{
inline constexpr double kPi = 3.14159265358979323846;

struct RankControls
{
    double frequency = 261.6255653;
    double pulseWidth = 0.5;              // panel duty proxy
    bool saw = true, pulse = false;
    double noise = 0.0;                   // the truth model is noise-free; must stay 0
    double hpCutoff = 25.0, hpQ = 0.5;    // control-law part; the stage adds 4.7 MOhm feedforward
    double lpCutoff = 2200.0, lpQ = 0.7;
    bool hpOn = true, lpOn = true;
};

// Full-matrix phase-corrected stage. It solves all four nonlinear nodal
// equations to convergence rather than using the production scalar reduction.
class NodalStage
{
public:
    explicit NodalStage(const Ig00156Network& n) : net(n) {}
    std::array<double, 4> z {}, v {}, inputs {};
    struct Output { double lp = 0.0, hp = 0.0, bp = 0.0; };
    Output tick(double input, double gmCutoffHz, double q, double fs)
    {
        const double gs=1/net.sourceOhms, gg=1/net.groundOhms, gf=1/net.feedbackOhms;
        const double gt=gs+gg+gf, scale=gt/(2*gs), ff=1/(net.feedforwardOhms*net.integratorFarads);
        if(gmCutoffHz!=lastCutoff||q!=lastQ||fs!=lastRate)
        {
            lastCutoff=gmCutoffHz;lastQ=q;lastRate=fs;
            const double total=std::clamp(gmCutoffHz,0.0,std::max(0.0,.45*fs-ff/(2*kPi)))+ff/(2*kPi);
            gq=std::max(0.0,gt/(2*std::clamp(q,.35,9.0))-gf);
            gm=std::max(0.0,ig00156IntegratorRate(net,total,gq)-ff);
            const ig00156::StageNetwork network{net.sourceOhms,net.groundOhms,net.inputFarads,
                net.feedbackOhms,net.feedbackFarads,net.integratorFarads,net.integratorFarads,
                net.feedforwardOhms,net.feedforwardOhms};
            const double siemens=gm*net.integratorFarads;
            oracle.configure(ig00156::stageSystem(network,{siemens,siemens,gq,0.0}),fs);
        }
        oracle.x=v;oracle.history=inputs;
        const double u=input*scale;
        oracle.nonlinearStep(u,gm,kIg00156OtaLevel,false);
        v=oracle.x;inputs=oracle.history;
        const double hp=2*(v[1]+v[2])-v[3],half=.5/fs;
        z[0]=v[0]+half*gs/net.inputFarads*(u-v[0]-v[1]-v[2]);
        z[1]=v[1]+half/net.feedbackFarads*(gs*(u-v[0])-gt*v[1]+(gq-gs-gg)*v[2]);
        z[2]=v[2]-half*(gm*kIg00156OtaLevel*std::tanh(hp/kIg00156OtaLevel)+ff*hp);
        z[3]=v[3]-half*(gm*kIg00156OtaLevel*std::tanh(v[2]/kIg00156OtaLevel)+ff*v[2]);
        return {v[3],hp,-v[2]};
    }
private:
    Ig00156Network net;
    ig00156::PhaseCorrectedLinearStage oracle;
    double lastCutoff=-1,lastQ=-1,lastRate=-1,gm=0,gq=0;
};

class RankTruth
{
public:
    explicit RankTruth(double rate)
    {
        (void) osc.prepare(rate, OscillatorNominalProfile {});
    }
    // Synchronise to a realtime chain: oscillator state and both stages.
    void reset(const ReconstructedOscillator::State& oscillator,
               const std::array<double, 4>& hpZ, const std::array<double, 4>& hpV,
               const std::array<double, 4>& lpZ, const std::array<double, 4>& lpV,
               const std::array<double, 4>& hpInputs, const std::array<double, 4>& lpInputs)
    {
        osc.restoreState(oscillator);
        hp.z = hpZ; hp.v = hpV;
        lp.z = lpZ; lp.v = lpV;
        hp.inputs = hpInputs; lp.inputs = lpInputs;
    }
    // One internal sample. Returns the filtered (pre-VCA) rank signal.
    double tick(const RankControls& c, double rate)
    {
        WscControls w;
        w.pulseThresholdVolts = ReconstructedOscillator::panelThresholdVoltsInferred(c.pulseWidth);
        w.sawEnabled = c.saw;
        w.pulseEnabled = c.pulse;
        const auto pins = osc.tick(c.frequency, w);
        double filtered = (pins.sawPin5 + pins.pulsePin7) * kWscEngineUnitsPerVolt * 0.9;
        if (c.hpOn) filtered = hp.tick(filtered, c.hpCutoff, c.hpQ, rate).hp;
        if (c.lpOn) filtered = lp.tick(filtered, c.lpCutoff, c.lpQ, rate).lp;
        return filtered;
    }
    double getPhase() const { return osc.getPhase(); }
    const ReconstructedOscillator::State& oscillatorState() const { return osc.getState(); }
    const NodalStage& hpStage() const { return hp; }
    const NodalStage& lpStage() const { return lp; }

private:
    ReconstructedOscillator osc;
    NodalStage hp { kIg00156HighPassNetwork }, lp { kIg00156LowPassNetwork };
};
} // namespace cs80::reference::free_running
