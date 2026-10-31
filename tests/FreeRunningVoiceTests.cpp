// Phase 2.1 free-running voices (circuit engine).
//
// Part A is the stage gate: the realtime idle path (production primitives plus
// the shared voice_continuity step/wake helpers) against the offline truth
// model src/reference/FreeRunningVoiceTruth.h, which clocks the same declared
// chain every sample in double precision. Tolerances are pre-registered in
// tests/Ws3Tolerances.h. Part B checks the Engine integration contract. These
// are numerical checks of the declared model, not a hardware comparison.
#include "core/Engine.h"
#include "experimental/ContinuousVoiceCircuit.h"
#include "reference/FreeRunningVoiceTruth.h"
#include "Ws3Tolerances.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace cs80;
namespace truth = cs80::reference::free_running;
namespace tol = cs80::ws3_tolerances;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what.c_str()); }
}

// v2 (ws1 + ws3 merge): the circuit engine's rank is the reconstructed WSC
// oscillator into the two nodal IG00156 stages; the gate procedure, scenarios
// and pre-registered tolerances are unchanged.
struct TestRank
{
    explicit TestRank(double rate) { (void) wsc.prepare(rate, OscillatorNominalProfile {}); }
    ReconstructedOscillator wsc;
    Ig00156Stage hpStage { kIg00156HighPassNetwork }, lpStage { kIg00156LowPassNetwork };
    voice_continuity::IdleRankControls idle;
    voice_continuity::IdleStageReferences idleRefs;
    voice_continuity::IdleHistory idleHistory;
    double idleLead = 0.0;
    int idleFirstSamples = 0;
};

voice_continuity::IdleRankControls toIdle(const truth::RankControls& c)
{
    return { static_cast<float>(c.frequency), static_cast<float>(c.pulseWidth), 0.0f, c.saw, c.pulse,
             static_cast<float>(c.hpCutoff), static_cast<float>(c.hpQ),
             static_cast<float>(c.lpCutoff), static_cast<float>(c.lpQ), c.hpOn, c.lpOn };
}

// Mirrors the sounding-voice path of Engine::processInternalSample for one rank.
float activeTick(TestRank& r, const truth::RankControls& c, float sr)
{
    const auto idle = toIdle(c);
    const float x = voice_continuity::sawPulseUnits(r.wsc.tick(idle.frequency, voice_continuity::wscControls(idle))) * 0.9f;
    r.idle = idle;
    return voice_continuity::clockStages(r.hpStage, r.lpStage, r.idleRefs, x, idle, sr);
}

std::array<double, 4> asArray(const Ig00156Stage::State& s) { return { s.input, s.feedback, s.bp, s.lp }; }
Ig00156Stage::State asState(const std::array<double, 4>& a) { return { a[0], a[1], a[2], a[3] }; }
double stateNorm(const TestRank& r)
{
    double sum = 0.0;
    for (const auto* stage : { &r.hpStage, &r.lpStage })
        for (double x : asArray(stage->getState())) sum += x * x;
    return std::sqrt(sum);
}

double circularDistance(double a, double b)
{
    double d = std::abs(a - b);
    d -= std::floor(d);
    return std::min(d, 1.0 - d);
}

struct GateResult { double residualDb = 0.0, phaseError = 0.0, wakeStateRatio = 0.0; };
using ControlsAt = std::function<truth::RankControls(int sample)>;

// Sound `activeSamples`, go silent for `idleSamples`, wake into `wake` controls
// and sound for the residual window. Realtime and truth see identical controls.
// Registered procedure (tests/Ws3Tolerances.h erratum): the truth is
// synchronised to the realtime state at silence; the window compares two
// production chains started from the realtime and from the truth wake state.
GateResult runGate(double internalRate, const truth::RankControls& sounding, int activeSamples,
                   const ControlsAt& idleControls, int idleSamples, const truth::RankControls& wake)
{
    const float sr = static_cast<float>(internalRate);
    const int step = voice_continuity::idleStepSamples(internalRate);
    TestRank rt(internalRate);
    truth::RankTruth ref(internalRate);
    Rng idleNoise(11u);
    rt.wsc.reset(0.25, sounding.frequency);
    for (int i = 0; i < activeSamples; ++i) activeTick(rt, sounding, sr);
    ref.reset(rt.wsc.getState(), asArray(rt.hpStage.getState()), asArray(rt.hpStage.getNodes()),
              asArray(rt.lpStage.getState()), asArray(rt.lpStage.getNodes()));

    int clock = 0, steps = 0;
    for (int i = 0; i < idleSamples; ++i)
    {
        const auto c = idleControls(i);
        ref.tick(c, internalRate);
        if (++clock >= step)
        {
            voice_continuity::idleStepRank(rt, toIdle(c), sr, clock, ++steps);
            clock = 0;
        }
    }
    voice_continuity::wakeCircuit(&rt, 1, sr, step, clock, steps, idleNoise);
    GateResult result;
    result.phaseError = circularDistance(rt.wsc.getPhase(), ref.getPhase());
    result.wakeStateRatio = stateNorm(rt);

    // Production chain started from the truth's wake state.
    TestRank fromTruth(internalRate);
    fromTruth.wsc.restoreState(ref.oscillatorState());
    fromTruth.hpStage.setState(asState(ref.hpStage().z), asState(ref.hpStage().v));
    fromTruth.lpStage.setState(asState(ref.lpStage().z), asState(ref.lpStage().v));
    fromTruth.idleRefs = { sr, sr };
    const int window = static_cast<int>(std::lround(tol::kWakeResidualWindowSeconds * internalRate));
    double error = 0.0, power = 0.0;
    for (int i = 0; i < window; ++i)
    {
        const double a = activeTick(rt, wake, sr);
        const double b = activeTick(fromTruth, wake, sr);
        error += (a - b) * (a - b);
        power += b * b;
    }
    result.residualDb = 10.0 * std::log10(std::max(error, 1.0e-300) / std::max(power, 1.0e-300));
    return result;
}

truth::RankControls withPitch(truth::RankControls c, double hz) { c.frequency = hz; return c; }

void testGate()
{
    truth::RankControls openSaw;                         // C4 saw, open LP
    truth::RankControls resonantLow = openSaw;           // low resonant LP
    resonantLow.frequency = 130.8127827; resonantLow.lpCutoff = 400.0; resonantLow.lpQ = 3.0;
    truth::RankControls pulse = openSaw;                 // C2 30% pulse, raised HP
    pulse.frequency = 65.40639133; pulse.saw = false; pulse.pulse = true; pulse.pulseWidth = 0.3;
    pulse.hpCutoff = 200.0; pulse.hpQ = 1.0; pulse.lpCutoff = 5000.0; pulse.lpQ = 0.5;
    truth::RankControls highQ = openSaw;                 // resonance above the idle step band
    highQ.pulse = true; highQ.lpCutoff = 3000.0; highQ.lpQ = 5.0;
    truth::RankControls highHp = openSaw;                // HP above the fundamental
    highHp.frequency = 440.0; highHp.hpCutoff = 1500.0; highHp.hpQ = 2.0; highHp.lpCutoff = 9000.0;
    const struct { const char* name; truth::RankControls c; } scenarios[] = {
        {"open saw", openSaw}, {"resonant low LP", resonantLow}, {"pulse", pulse},
        {"high-Q LP", highQ}, {"high HP", highHp} };

    double worstResidual = -1.0e9, worstPhase = 0.0;
    for (double host : {44100.0, 48000.0, 96000.0, 192000.0})
    {
        const double rate = 2.0 * host;
        const int active = static_cast<int>(0.05 * rate);
        for (const auto& s : scenarios)
            for (double idleSeconds : {0.05, 0.5, 3.0})
            {
                if (idleSeconds == 3.0 && host != 48000.0) continue;
                const auto c = s.c;
                const auto r = runGate(rate, c, active, [c](int) { return c; },
                                       static_cast<int>(idleSeconds * rate), withPitch(c, c.frequency * 1.259921));
                worstResidual = std::max(worstResidual, r.residualDb);
                worstPhase = std::max(worstPhase, r.phaseError);
                check(r.residualDb <= tol::kWakeResidualDbMax,
                      std::string("wake residual ") + s.name + " " + std::to_string(host) + " Hz idle "
                      + std::to_string(idleSeconds) + " s: " + std::to_string(r.residualDb) + " dB");
                check(r.phaseError <= tol::kIdlePhaseErrorCyclesMax,
                      std::string("idle phase ") + s.name + " " + std::to_string(host) + ": "
                      + std::to_string(r.phaseError));
            }
        // Shorter than one idle step: exact forward replay.
        const int step = voice_continuity::idleStepSamples(rate);
        for (int gap : {1, 3, step - 1})
        {
            const auto c = openSaw;
            const auto r = runGate(rate, c, active, [c](int) { return c; }, gap, withPitch(c, 329.6275569));
            check(r.residualDb <= tol::kShortGapResidualDbMax,
                  "short-gap exact replay " + std::to_string(host) + " gap " + std::to_string(gap)
                  + ": " + std::to_string(r.residualDb) + " dB");
            check(r.phaseError <= tol::kIdlePhaseErrorCyclesMax, "short-gap phase");
        }
        // Vibrato while silent (5 Hz, +/-0.5 semitone): trapezoidal idle phase.
        {
            const auto c = openSaw;
            const auto vib = [c, rate](int i) {
                auto v = c;
                v.frequency = c.frequency * std::exp2(0.5 / 12.0 * std::sin(2.0 * truth::kPi * 5.0 * i / rate));
                return v;
            };
            const auto r = runGate(rate, c, active, vib, static_cast<int>(0.5 * rate), c);
            check(r.phaseError <= tol::kIdlePhaseErrorCyclesMaxWithVibrato,
                  "vibrato idle phase " + std::to_string(host) + ": " + std::to_string(r.phaseError));
        }
        // Waveforms off while silent: the filter memories decay with the circuit.
        {
            auto silentInput = openSaw;
            silentInput.saw = silentInput.pulse = false;
            TestRank probe(rate);
            probe.wsc.reset(0.25, openSaw.frequency);
            for (int i = 0; i < active; ++i) activeTick(probe, openSaw, static_cast<float>(rate));
            const double before = stateNorm(probe);
            const auto r = runGate(rate, openSaw, active, [silentInput](int) { return silentInput; },
                                   static_cast<int>(1.0 * rate), openSaw);
            check(before > 0.05 && r.wakeStateRatio <= tol::kZeroWaveformDecayRatioMax * before,
                  "zero-waveform decay " + std::to_string(host) + ": " + std::to_string(r.wakeStateRatio)
                  + " of " + std::to_string(before));
        }
    }
    std::printf("free-running gate: worst wake residual %.2f dB, worst idle phase %.3g cycles\n",
                worstResidual, worstPhase);
}

// ---------------------------------------------------------------------------
// Part B: Engine integration.

ParamValues quiet(float parity, float source)
{
    ParamValues p;
    p[P_parityMode] = parity;
    p[P_sourcePerformance] = source;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_keyRelease] = 0.0f;
    for (int r = 0; r < 2; ++r) { p[chParam(r, CP_vR)] = 0.05f; p[chParam(r, CP_fR)] = 0.05f; }
    return p;
}

std::vector<float> render(Engine& e, int frames, int block = 256)
{
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    for (int i = 0; i < frames; i += block) e.render(out.data() + static_cast<std::size_t>(i) * 2, std::min(block, frames - i));
    return out;
}

void testEngine()
{
    for (double host : {44100.0, 48000.0, 96000.0})
        for (int mode = 0; mode < 3; ++mode) // legacy, source performance, isolation
        {
            const bool circuit = mode != 0;
            Engine e;
            e.prepare(host);
            e.setParams(quiet(mode == 2 ? 1.0f : 0.0f, mode == 1 ? 1.0f : 0.0f));
            check(e.getRenderPolicy() == (mode == 0 ? RenderPolicy::legacy : mode == 1
                  ? RenderPolicy::sourcePerformance : RenderPolicy::isolation), "policy fixture");
            // Cold circuits: distinct declared phases in the circuit engine.
            if (circuit)
            {
                bool distinct = true;
                for (int v = 1; v < 8; ++v)
                    distinct = distinct && e.getRankAudioState(v, 0).phase != e.getRankAudioState(0, 0).phase;
                check(distinct, "cold circuits start at distinct declared phases");
            }
            else check(e.getRankAudioState(3, 1).phase == 0.0f, "legacy cold phase unchanged");

            e.noteOn(60, 0.8f);
            render(e, static_cast<int>(host * 0.3));
            e.noteOff(60);
            render(e, static_cast<int>(host * 0.5));
            check(!e.getVoiceInfo(0).active, "voice retired");
            const auto a = e.getRankAudioState(0, 0);
            const auto info = e.getRankInfo(0, 0);
            const int idleFrames = static_cast<int>(host * 0.25);
            render(e, idleFrames);
            const auto b = e.getRankAudioState(0, 0);
            if (circuit)
            {
                check(b.phase != a.phase, "silent circuit oscillator keeps running");
                check(b.lpf.ic1 != a.lpf.ic1 || b.lpf.ic2 != a.lpf.ic2, "silent circuit filter keeps evolving");
                // Constant idle pitch: phase advance equals the exact increment sum
                // up to the idle-step granularity (the wake catch-up adds the rest).
                const double dt = std::min(0.45, double(info.oscillatorHz) / (2.0 * host));
                const int step = voice_continuity::idleStepSamples(2.0 * host);
                const double samples = 2.0 * idleFrames;
                double predicted = double(a.phase) + dt * samples;
                double error = circularDistance(b.phase, predicted - std::floor(predicted));
                check(error <= dt * step + tol::kIdlePhaseErrorCyclesMax, "idle phase follows the increment law");
            }
            else check(b.phase == a.phase && b.lpf.ic1 == a.lpf.ic1, "legacy silent voice stays frozen");

            // Reassignment after a long silence: no reset in the circuit engine.
            const auto before = e.getRankAudioState(0, 0);
            for (int k = 0; k < 8; ++k) e.noteOn(48 + k, 0.7f); // cycles back to voice 0 last
            const auto after = e.getRankAudioState(0, 0);
            if (circuit)
                check(!(after.hpf.ic1 == 0.0f && after.hpf.ic2 == 0.0f && after.lpf.ic1 == 0.0f
                        && after.lpf.ic2 == 0.0f), "assignment keeps filter memories");
            else
                check(after.hpf.ic1 == 0.0f && after.lpf.ic2 == 0.0f, "legacy assignment resets");
            (void) before;
            for (float x : render(e, 4096)) check(std::isfinite(x), "finite after wake");
        }

    // Silent circuits contribute nothing to the mix, idle or waking.
    {
        Engine e;
        e.prepare(48000.0);
        auto p = quiet(1.0f, 0.0f);
        p[P_chorusOn] = 0.0f;
        e.setParams(p);
        const auto out = render(e, 48000);
        double peak = 0.0;
        for (float x : out) peak = std::max(peak, double(std::abs(x)));
        check(peak == 0.0, "eight free-running cold circuits are silent");
    }

    // Exact block-partition invariance with idle periods and wakes.
    for (int mode : {1, 2})
    {
        std::vector<float> reference;
        for (int block : {997, 1, 7, 64})
        {
            Engine e;
            e.prepare(48000.0);
            e.setParams(quiet(mode == 2 ? 1.0f : 0.0f, mode == 1 ? 1.0f : 0.0f));
            std::vector<float> all;
            auto append = [&](int frames) { auto x = render(e, frames, block); all.insert(all.end(), x.begin(), x.end()); };
            e.noteOn(55, 0.9f); append(3000);
            e.noteOff(55); append(20000);
            e.noteOn(62, 0.5f); e.noteOn(67, 0.6f); append(5000);
            e.noteOff(62); e.noteOff(67); append(9000);
            for (int k = 0; k < 9; ++k) e.noteOn(40 + 3 * k, 0.8f);
            append(4000);
            if (reference.empty()) reference = all;
            else check(all == reference, "block partition " + std::to_string(block) + " mode " + std::to_string(mode));
        }
    }

    // Hard panic leaves a VCA stage behind; waking that circuit restarts from zero.
    {
        Engine e;
        e.prepare(48000.0);
        e.setParams(quiet(1.0f, 0.0f));
        e.noteOn(60, 1.0f);
        render(e, 4800);
        e.allNotesOff(false, 0);
        check(e.getRankEnvelopeState(0, 0).amplitude.value > 0.1f, "hard panic left a VCA stage behind");
        render(e, 4800);
        for (int k = 1; k < 8; ++k) e.noteOn(60 + k, 1.0f); // cyclic cursor: voices 1..7
        e.noteOn(72, 1.0f);                                  // then voice 0 again
        check(e.getVoiceInfo(0).note == 72, "fixture reassigns voice 0");
        check(e.getRankEnvelopeState(0, 0).amplitude.value == 0.0f
              && e.getRankEnvelopeState(0, 0).amplitude.stage == Adsr::Stage::Attack,
              "woken circuit's VCA attacks from zero after hard panic");
    }

    // Sustain II termination resets the filter EG: the next wake starts at IL.
    {
        Engine e;
        e.prepare(48000.0);
        auto p = quiet(1.0f, 0.0f);
        p[P_sustainMode] = 1.0f;
        p[chParam(0, CP_fIl)] = 0.8f;
        e.setParams(p);
        for (int k = 0; k < 8; ++k) e.noteOn(48 + k, 0.8f);
        render(e, 2400);
        for (int k = 0; k < 8; ++k) e.noteOff(48 + k);
        render(e, 240);
        e.noteOn(70, 0.8f); // terminates the sustained tails, takes voice 0
        check(std::abs(e.getRankEnvelopeState(0, 0).filter.value - filterInitialOffset(0.8f)) < 1.0e-6f,
              "Sustain II terminated circuit restarts its filter EG from IL");
    }
}
} // namespace

int main()
{
    testGate();
    testEngine();
    std::printf("Free-running voices: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
