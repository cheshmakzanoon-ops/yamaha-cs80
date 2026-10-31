#pragma once

// Experimental continuity boundary only. Uses the UNCHANGED production
// oscillator/SVF laws; it does not select ReconstructedOscillator or Engine.
// Electrical inputs deliberately avoid asserting a complete KAS/MIDI mapping.
// See docs/VOICE_CIRCUIT_CONTINUITY.md for evidence and integration gates.
#include "core/Dsp.h"

namespace cs80::experimental
{
class ContinuousVoiceCircuit
{
public:
    static constexpr bool productionIntegrated = false;
    static constexpr const char* modelId = "voice-continuity-electrical-v1";

    struct RankControls
    {
        // 523.2 is the nominal Yamaha sheet42 .5 V ->261.6 Hz anchor, not
        // a recovered temperature/trim law. Caller may supply its interface
        // calibration. VCO-only modulation/footage never alters filter key K.
        float hzPerVolt = 523.2f, vcoMultiplier = 1.0f;
        float pulseWidth = 0.5f;
        bool sawEnabled = true, pulseEnabled = false;
        // Explicit externally supplied additive noise, BEFORE production .9
        // input scaling. No internal noise generator or Engine RNG consumption.
        float noiseInput = 0.0f;
        float hpfEffectiveCv = 5.0f, lpfEffectiveCv = 5.0f;
        float hpfQ = 0.5f, lpfQ = 0.5f;
        bool hpfEnabled = true, lpfEnabled = true;
        float filterLevel = 1.0f, sineLevel = 0.0f;
        // Caller owns EG/touch/mix and audible allocation. Zero does NOT
        // disconnect the oscillator/filter input or stop any physical proxy.
        float outputGain = 0.0f;
    };
    struct Controls
    {
        float keyInputVolts = 0.5f;
        // Electrical high/low, NOT Yamaha's negative-logic key-data labels.
        // TC4011 NAND -> TC4016 high-enable: LL/LH/HL track; HH holds.
        bool trHigh = false, susHigh = true;
        std::array<RankControls, 2> ranks {};
    };
    struct RankOutput
    {
        float audio = 0.0f, filtered = 0.0f, sine = 0.0f;
        // Requested rates before the existing primitives' numerical clamps.
        float oscillatorHz = 0.0f, hpfHz = 0.0f, lpfHz = 0.0f;
    };
    struct Output
    {
        bool accepted = false;
        float heldKeyVolts = 0.0f;
        std::array<RankOutput, 2> ranks {};
    };
    struct RankState
    {
        float phase = 0.0f;
        Svf::State hpf {}, lpf {};
    };
    struct State
    {
        bool prepared = false;
        float sampleRate = 0.0f, heldKeyVolts = 0.0f;
        std::uint64_t samples = 0;
        std::array<RankState, 2> ranks {};
    };

    // No undocumented cold-pitch/phase choice: the harness/adapter supplies
    // them. This is the only reset operation. Invalid setup is non-mutating.
    bool prepare(double internalSampleRate, float initialKeyVolts,
                 std::array<float, 2> initialPhases) noexcept
    {
        if (!std::isfinite(internalSampleRate) || internalSampleRate < 16000.0
            || internalSampleRate > 1536000.0 || !keyInDomain(initialKeyVolts)
            || !std::isfinite(initialPhases[0]) || !std::isfinite(initialPhases[1]))
            return false;
        sampleRate = static_cast<float>(internalSampleRate);
        heldKeyVolts = initialKeyVolts;
        samples = 0;
        for (std::size_t rank = 0; rank < ranks.size(); ++rank)
        {
            ranks[rank].oscillator.reset(initialPhases[rank]);
            ranks[rank].hpf.reset();
            ranks[rank].lpf.reset();
        }
        prepared = true;
        return true;
    }

    // Exactly two production oscillator ticks and four SVF updates per valid
    // internal sample, independent of output gain or bypass. There is no
    // allocation, locking, adaptive integration or callback-dependent loop.
    // Invalid controls are rejected as a WHOLE before key/audio state changes.
    Output processSample(const Controls& controls) noexcept
    {
        if (!prepared || !keyInDomain(controls.keyInputVolts)) return {};
        for (const auto& rank : controls.ranks)
            if (!inDomain(rank)) return {};

        const float nextKey = !(controls.trHigh && controls.susHigh)
            ? controls.keyInputVolts : heldKeyVolts;
        std::array<float, 2> frequency {}, hpCutoff {}, lpCutoff {};
        for (std::size_t rank = 0; rank < ranks.size(); ++rank)
        {
            const auto& c = controls.ranks[rank];
            frequency[rank] = nextKey * c.hzPerVolt * c.vcoMultiplier;
            hpCutoff[rank] = nominalFilterCutoff(nextKey, c.hpfEffectiveCv);
            lpCutoff[rank] = nominalFilterCutoff(nextKey, c.lpfEffectiveCv);
            if (!std::isfinite(frequency[rank]) || !std::isfinite(hpCutoff[rank])
                || !std::isfinite(lpCutoff[rank])) return {};
        }

        heldKeyVolts = nextKey;
        Output out;
        out.accepted = true;
        out.heldKeyVolts = heldKeyVolts;
        for (std::size_t rank = 0; rank < ranks.size(); ++rank)
        {
            auto& r = ranks[rank];
            const auto& c = controls.ranks[rank];
            const auto osc = r.oscillator.tick(frequency[rank], sampleRate,
                c.pulseWidth, c.sawEnabled, c.pulseEnabled, 0.0f, unusedRng);
            const float input = (osc.saw + osc.pulse + c.noiseInput) * 0.9f;
            r.hpf.set(hpCutoff[rank], c.hpfQ, sampleRate);
            const float hp = r.hpf.process(input).hp;
            // Software routing overlay: bypass chooses the input to the next
            // stage, but the bypassed stage itself remains clocked. This is
            // not a claim that the original instrument has filter bypasses.
            const float lpInput = c.hpfEnabled ? hp : input;
            r.lpf.set(lpCutoff[rank], c.lpfQ, sampleRate);
            const float lp = r.lpf.process(lpInput).lp;
            const float filtered = c.lpfEnabled ? lp : lpInput;
            out.ranks[rank] = {
                (filtered * c.filterLevel + osc.sine * c.sineLevel) * c.outputGain,
                filtered, osc.sine, frequency[rank], hpCutoff[rank], lpCutoff[rank]
            };
        }
        ++samples;
        return out;
    }

    State getState() const noexcept
    {
        State state { prepared, sampleRate, heldKeyVolts, samples, {} };
        for (std::size_t rank = 0; rank < ranks.size(); ++rank)
            state.ranks[rank] = { ranks[rank].oscillator.getPhase(),
                                ranks[rank].hpf.getState(), ranks[rank].lpf.getState() };
        return state;
    }

private:
    // Deliberately wide SOFTWARE test domain, not electrical component limits
    // or a host-control range. It avoids overflow before the unchanged DSP's
    // own frequency/cutoff/Q guards. Negative key voltage is not modeled.
    static bool bounded(float value, float low, float high) noexcept
    { return std::isfinite(value) && value >= low && value <= high; }
    static bool keyInDomain(float value) noexcept { return bounded(value, 0.0f, 1024.0f); }
    static bool inDomain(const RankControls& c) noexcept
    {
        return bounded(c.hzPerVolt, 0.0f, 4096.0f)
            && bounded(c.vcoMultiplier, 0.0f, 4096.0f)
            && bounded(c.pulseWidth, 0.05f, 0.95f)
            && bounded(c.noiseInput, -64.0f, 64.0f)
            && bounded(c.hpfEffectiveCv, -64.0f, 64.0f)
            && bounded(c.lpfEffectiveCv, -64.0f, 64.0f)
            && bounded(c.hpfQ, 0.35f, 9.0f) && bounded(c.lpfQ, 0.35f, 9.0f)
            && bounded(c.filterLevel, 0.0f, 64.0f)
            && bounded(c.sineLevel, 0.0f, 64.0f)
            && bounded(c.outputGain, 0.0f, 64.0f);
    }
    struct Rank { Oscillator oscillator; Svf hpf, lpf; };
    std::array<Rank, 2> ranks {};
    Rng unusedRng { 1u }; // Oscillator::tick noiseAmount=0 never reads it.
    float sampleRate = 0.0f, heldKeyVolts = 0.0f;
    std::uint64_t samples = 0;
    bool prepared = false;
};
} // namespace cs80::experimental

// ---------------------------------------------------------------------------
// Production idle path (IMPROVEMENT_PLAN 2.1, circuit engine only).
//
// The kernel above clocks every circuit on every sample; a whole-Engine
// measurement of that (docs/voice-continuity/engine-experiment) cost
// 0.19-0.23 RTF of extra idle CPU. The circuit engine instead evolves a silent
// voice cheaply and catches up exactly where it matters:
//   1. Oscillator phase: advanced by the sum of the per-sample increments at
//      the idle control rate (trapezoidal frequency within an idle step), so it
//      never resets and never stops (M41: TR reaches only the EG gates).
//   2. Filter memories: both IG00156 stages are clocked once per idle step at
//      the step rate, driven by the exact block average of the oscillator's
//      waveform over that step. This tracks the slow (low-cutoff) modes, the
//      ones a short replay cannot settle. The trapezoidal memories are
//      re-referenced whenever the clock rate of a stage changes, so the
//      physical node voltages and OTA currents carry over unchanged.
//   3. Wake: the circuit is restored to a stored idle-step snapshot at least W
//      samples old and replayed at the full internal rate with the production
//      primitives up to the note-on, ending on the continuous phase. This
//      rebuilds the modes the step rate cannot represent (or mistunes); W is
//      set from those modes. Replaying from a snapshot, never from a rewound
//      phase, keeps memories and oscillator time-aligned.
// A voice that wakes before its first idle step replays its elapsed samples
// exactly instead. Declared simplifications (docs/workstreams/ws3-keyboard.md):
// idle controls are piecewise linear per step, and the decimated step omits
// per-rank noise (the replay window includes it from a per-voice idle stream,
// never the shared Engine RNG). The software filter bypass keeps its existing
// meaning: a bypassed stage is not clocked, idle or active.
// Gate: tests/FreeRunningVoiceTests.cpp against src/reference/FreeRunningVoiceTruth.h.
namespace cs80::voice_continuity
{
inline constexpr const char* modelId = "free-running-idle-v1";
// Idle control/step rate. 3 kHz keeps the fundamental and low harmonics of
// every keyboard note inside the step band while costing ~3% of a sounding voice.
inline constexpr double kIdleStepHz = 3000.0;
// Replay length: kCatchUpSettleTaus time constants of the slowest mode the
// step rate cannot represent, bounded to [kCatchUpMinSeconds, kCatchUpMaxSeconds].
inline constexpr double kCatchUpSettleTaus = 9.0;
inline constexpr double kCatchUpMinSeconds = 0.001;
inline constexpr double kCatchUpMaxSeconds = 0.012;
// A stage whose cutoff is above this fraction of the step rate is treated as a
// fast mode that only the replay settles.
inline constexpr double kStepBandFraction = 0.2;

inline int idleStepSamples(double internalRate) noexcept
{
    return std::max(1, static_cast<int>(std::lround(internalRate / kIdleStepHz)));
}

// Everything the idle path needs to clock one rank, captured from the same
// control computation the sounding voice uses (cutoffs INCLUDE the feedforward
// floor; frequency includes footage and ribbon).
struct IdleRankControls
{
    float frequency = 0.0f, pulseWidth = 0.5f, noise = 0.0f;
    bool saw = false, pulse = false;
    float hpCutoff = 18.0f, hpQ = 0.5f, lpCutoff = 18.0f, lpQ = 0.5f;
    bool hpOn = false, lpOn = false;
};

// The rate at which one stage's trapezoidal memories were last clocked and the
// node values of that interval (bp/lp are the capacitor node voltages, hp the
// OTA input). Updated only when the stage is actually clocked.
struct StageReference
{
    float rate = 0.0f;
    Svf::Output nodes {};
};
struct IdleStageReferences { StageReference hp, lp; };

namespace detail
{
// Antiderivatives over one period [0,1) of the oscillator's naive waveforms.
inline double sawIntegral(double phase) noexcept { return phase - phase * phase; }
inline double pulseIntegral(double phase, double width) noexcept
{
    return phase < width ? phase : 2.0 * width - phase;
}
// Integral of a periodic function with per-period antiderivative F (F(0)=0)
// and full-period integral `period` over (a, a + delta].
template <class F>
double periodicIntegral(double a, double delta, double period, F&& f) noexcept
{
    const double b = a + delta;
    const double fa = std::floor(a), fb = std::floor(b);
    return f(b - fb) - f(a - fa) + (fb - fa) * period;
}
inline double stageGain(float cutoffHz, float rate) noexcept
{
    const double c = std::clamp(static_cast<double>(cutoffHz), 18.0, 0.45 * rate);
    return std::tan(3.14159265358979323846 * c / rate);
}
} // namespace detail

// Re-express a circuit stage's trapezoidal memories for clock rate `toRate`.
// Svf::processCircuit leaves s = node + g * OTA current for each integrator;
// the node voltages and currents are the physical state, the gain g is a
// property of the clock. Keeping the nodes and currents and replacing g is the
// rate-independent transfer.
inline void reReferenceStage(Svf& stage, const StageReference& ref, float cutoffHz, float toRate) noexcept
{
    if (ref.rate == toRate || !(ref.rate > 0.0f)) return;
    const double g = detail::stageGain(cutoffHz, toRate);
    const double v = kIg00156OtaLevel;
    const double u1 = v * fastTanh(static_cast<double>(ref.nodes.hp) / v);
    const double u2 = v * fastTanh(static_cast<double>(ref.nodes.bp) / v);
    stage.setState({ static_cast<float>(ref.nodes.bp + g * u1), static_cast<float>(ref.nodes.lp + g * u2) });
}

// Clock both circuit stages for one interval at `rate`, re-referencing first
// when the previous interval of a stage ran at another rate.
inline void clockStages(Svf& hp, Svf& lp, IdleStageReferences& refs, float input,
                        const IdleRankControls& c, float rate) noexcept
{
    float x = input;
    if (c.hpOn)
    {
        reReferenceStage(hp, refs.hp, c.hpCutoff, rate);
        hp.set(c.hpCutoff, c.hpQ, rate);
        refs.hp = { rate, hp.processCircuit(x) };
        x = refs.hp.nodes.hp;
    }
    if (c.lpOn)
    {
        reReferenceStage(lp, refs.lp, c.lpCutoff, rate);
        lp.set(c.lpCutoff, c.lpQ, rate);
        refs.lp = { rate, lp.processCircuit(x) };
    }
}

// Exact average over phase interval (phase, phase + delta] of the oscillator's
// waveform without its band-limiting residuals (polyBLEP residuals integrate to
// ~0 per edge): falling saw plus its 30 us reset pulse, and the comparator pulse.
inline double waveformBlockAverage(double phase, double delta, float frequency,
                                   float pulseWidth, bool saw, bool pulse) noexcept
{
    if (!saw && !pulse) return 0.0;
    const double narrow = std::clamp(30.0e-6 * static_cast<double>(frequency), 0.0001, 0.45);
    const double width = std::clamp(static_cast<double>(pulseWidth), 0.000001, 0.999999);
    if (!(delta > 1.0e-12))
    {
        const double p = phase - std::floor(phase);
        double x = 0.0;
        if (saw) x += (1.0 - 2.0 * p) + 0.70 * 0.55 * (p < narrow ? 1.0 : -1.0);
        if (pulse) x += p < width ? 1.0 : -1.0;
        return x;
    }
    double sum = 0.0;
    if (saw)
    {
        const double sawPart = detail::periodicIntegral(phase, delta, 0.0,
            [](double p) { return detail::sawIntegral(p); });
        const double resetPart = detail::periodicIntegral(phase, delta, 2.0 * narrow - 1.0,
            [narrow](double p) { return detail::pulseIntegral(p, narrow); });
        sum += sawPart + 0.70 * 0.55 * resetPart;
    }
    if (pulse)
        sum += detail::periodicIntegral(phase, delta, 2.0 * width - 1.0,
            [width](double p) { return detail::pulseIntegral(p, width); });
    return sum / delta;
}

// Advance the oscillator by `samples` internal samples, using the same
// per-sample increment law as Oscillator::tick. The frequency over the step is
// the trapezoid of the previous step's and this step's control values, so a
// modulated idle pitch integrates to second order. Returns the oscillator's
// average contribution over the step interval CENTRED on the step instant
// (before the .9 input scale): the trapezoidal step treats its input as a
// point value at that instant, and a causal average would delay it by half a
// step (0.4 rad at 400 Hz for a 3 kHz step).
inline float advanceIdleOscillator(Oscillator& osc, const IdleRankControls& c,
                                   float sampleRate, int samples,
                                   float previousFrequency) noexcept
{
    const double f0 = std::clamp(static_cast<double>(previousFrequency) / sampleRate, 0.0, 0.45);
    const double f1 = std::clamp(static_cast<double>(c.frequency) / sampleRate, 0.0, 0.45);
    const double dt = 0.5 * (f0 + f1);
    const double start = osc.getPhase();
    const double delta = dt * samples;
    const double end = start + delta;
    const double average = waveformBlockAverage(end - 0.5 * delta, delta, c.frequency, c.pulseWidth,
                                                c.saw, c.pulse);
    osc.reset(static_cast<float>(end - std::floor(end)));
    return static_cast<float>(average);
}

// Settling time constant of one SVF stage's slowest pole.
inline double stageTau(double cutoffHz, double q) noexcept
{
    const double w = 2.0 * 3.14159265358979323846 * std::max(cutoffHz, 1.0);
    const double zeta = 0.5 / std::clamp(q, 0.35, 9.0);
    if (zeta < 1.0) return 1.0 / (zeta * w);
    return 1.0 / (w * (zeta - std::sqrt(zeta * zeta - 1.0)));
}

// Replay a stage needs after decimated evolution. A fast mode (above the step
// band) is not represented at all and settles over kCatchUpSettleTaus time
// constants. An in-band mode is represented, but bilinear frequency warping at
// the step rate mistunes its response by about (pi fc / fstep)^2, amplified by
// its resonance Q; it needs ln(error / kCatchUpTargetError) time constants.
inline constexpr double kCatchUpTargetError = 1.0e-3;
inline double stageReplaySeconds(double cutoffHz, double q, double stepRate) noexcept
{
    const double tau = stageTau(cutoffHz, q);
    if (cutoffHz > kStepBandFraction * stepRate) return kCatchUpSettleTaus * tau;
    const double x = 3.14159265358979323846 * cutoffHz / stepRate;
    const double error = x * x * std::max(q, 0.5);
    return error > kCatchUpTargetError ? tau * std::log(error / kCatchUpTargetError) : 0.0;
}

// Replay length for a wake after decimated idle evolution.
inline int catchUpSamples(const std::array<IdleRankControls, 2>& ranks, float sampleRate,
                          float stepRate) noexcept
{
    double seconds = 0.0;
    for (const auto& c : ranks)
    {
        if (c.hpOn) seconds = std::max(seconds, stageReplaySeconds(c.hpCutoff, c.hpQ, stepRate));
        if (c.lpOn) seconds = std::max(seconds, stageReplaySeconds(c.lpCutoff, c.lpQ, stepRate));
    }
    seconds = std::clamp(seconds, kCatchUpMinSeconds, kCatchUpMaxSeconds);
    return std::max(1, static_cast<int>(std::lround(seconds * sampleRate)));
}

// Snapshot of one rank at an idle-step instant: a consistent (time, phase,
// memories) triple a replay can start from. Kept every kHistoryStride steps in
// a short ring (rate independent: always ~1.3 ms apart, ~16 ms deep).
struct IdleSnapshot
{
    float phase = 0.0f, frequency = 0.0f;
    Svf::State hp {}, lp {};
    IdleStageReferences refs {};
};
inline constexpr int kHistoryStride = 4;
inline constexpr int kHistoryLength = 12;
static_assert((kHistoryLength - 2) * kHistoryStride >= kCatchUpMaxSeconds * kIdleStepHz,
              "idle history must cover the longest replay");
struct IdleHistory { std::array<IdleSnapshot, kHistoryLength> entries {}; };
// Step counts wrap at a multiple of the ring period so slot indices stay valid.
inline constexpr int kIdleStepWrap = (1 << 28) - ((1 << 28) % (kHistoryStride * kHistoryLength));

template <class Rank>
IdleSnapshot snapshotOf(const Rank& r) noexcept
{
    return { r.osc.getPhase(), r.idle.frequency, r.hpf.getState(), r.lpf.getState(), r.idleRefs };
}

// Full-rate replay with the production primitives, forward from the present
// state. The frequency ramps linearly from f0 to the controls' frequency over
// the first `rampSamples` samples (the idle trajectory between snapshots).
inline void catchUpRank(Oscillator& osc, Svf& hp, Svf& lp, IdleStageReferences& refs,
                        const IdleRankControls& c, float sampleRate, int samples,
                        float f0, int rampSamples, Rng& noise) noexcept
{
    for (int i = 0; i < samples; ++i)
    {
        const float f = i < rampSamples
            ? f0 + (c.frequency - f0) * (static_cast<float>(i + 1) / static_cast<float>(rampSamples))
            : c.frequency;
        const auto o = osc.tick(f, sampleRate, c.pulseWidth, c.saw, c.pulse, 0.0f, noise);
        float x = o.saw + o.pulse;
        if (c.noise > 0.0f) x += noise.nextBipolar() * c.noise * 0.8f;
        clockStages(hp, lp, refs, x * 0.9f, c, sampleRate);
    }
}

// One idle step of one rank (members osc, hpf, lpf, idle, idleRefs,
// idleHistory as in Engine::Rank). `next` are this step's controls; the rank's
// stored controls are the previous step's (or the last sounding sample's) and
// supply the trapezoid frequency. `stepIndex` counts idle steps since the
// circuit went silent, this one included (1 for the first).
template <class Rank>
void idleStepRank(Rank& r, const IdleRankControls& next, float sampleRate, int samples,
                  int stepIndex) noexcept
{
    // Until the first step the silent circuit is untouched, so it still holds
    // the state and phase it had when it went silent: that is step 0.
    if (stepIndex == 1) r.idleHistory.entries[0] = snapshotOf(r);
    const float previousFrequency = r.idle.frequency;
    r.idle = next;
    const float input = advanceIdleOscillator(r.osc, r.idle, sampleRate, samples, previousFrequency) * 0.9f;
    clockStages(r.hpf, r.lpf, r.idleRefs, input, r.idle, sampleRate / static_cast<float>(samples));
    if (stepIndex % kHistoryStride == 0)
        r.idleHistory.entries[static_cast<std::size_t>((stepIndex / kHistoryStride) % kHistoryLength)] = snapshotOf(r);
}

// Bring a silent circuit (both ranks) up to date before it sounds again.
// `pending` internal samples have elapsed since the last idle step (or since it
// went silent); `steps` idle steps have run since it went silent.
template <class Rank>
void wakeCircuit(Rank* ranks, int count, float sampleRate, int stepSamples, int pending,
                 int steps, Rng& noise) noexcept
{
    stepSamples = std::max(1, stepSamples);
    if (steps <= 0)
    {
        // Silent for less than one idle step: replay the elapsed samples
        // exactly, forward from the state the circuit had when it went silent.
        for (int i = 0; i < count; ++i)
            catchUpRank(ranks[i].osc, ranks[i].hpf, ranks[i].lpf, ranks[i].idleRefs, ranks[i].idle,
                        sampleRate, pending, ranks[i].idle.frequency, 0, noise);
        return;
    }
    std::array<IdleRankControls, 2> controls {};
    for (int i = 0; i < count && i < 2; ++i) controls[static_cast<std::size_t>(i)] = ranks[i].idle;
    const int replay = catchUpSamples(controls, sampleRate, sampleRate / static_cast<float>(stepSamples));
    // Newest snapshot old enough to cover the replay, else the oldest kept.
    const int latest = (steps / kHistoryStride) * kHistoryStride;
    const int oldest = std::max(0, latest - (kHistoryLength - 1) * kHistoryStride);
    const int needSteps = (std::max(0, replay - pending) + stepSamples - 1) / stepSamples;
    int start = ((steps - needSteps) / kHistoryStride) * kHistoryStride;
    if (steps - needSteps < 0) start = 0;
    start = std::clamp(start, oldest, latest);
    const int span = (steps - start) * stepSamples;
    for (int i = 0; i < count; ++i)
    {
        auto& r = ranks[i];
        // The continuous phase at the wake instant.
        const double dt = std::clamp(static_cast<double>(r.idle.frequency) / sampleRate, 0.0, 0.45);
        const double wakePhase = static_cast<double>(r.osc.getPhase()) + dt * pending;
        const auto& snap = r.idleHistory.entries[static_cast<std::size_t>((start / kHistoryStride) % kHistoryLength)];
        r.osc.reset(snap.phase);
        r.hpf.setState(snap.hp);
        r.lpf.setState(snap.lp);
        r.idleRefs = snap.refs;
        catchUpRank(r.osc, r.hpf, r.lpf, r.idleRefs, r.idle, sampleRate, span + pending,
                    snap.frequency, span, noise);
        r.osc.reset(static_cast<float>(wakePhase - std::floor(wakePhase)));
    }
}
} // namespace cs80::voice_continuity
