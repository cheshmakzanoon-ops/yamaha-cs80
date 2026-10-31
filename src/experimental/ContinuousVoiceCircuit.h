#pragma once

// Experimental continuity boundary only. Uses the UNCHANGED production
// oscillator/SVF laws; it does not select ReconstructedOscillator or Engine.
// Electrical inputs deliberately avoid asserting a complete KAS/MIDI mapping.
// See docs/VOICE_CIRCUIT_CONTINUITY.md for evidence and integration gates.
#include "core/Dsp.h"
#include "core/Ig00156Filter.h"
#include "core/ReconstructedOscillator.h"

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
// 0.19-0.23 RTF of extra idle CPU, and the circuit engine's reconstructed
// oscillator and nodal IG00156 stages (plan steps 1.1b/1.2) cost more. The
// circuit engine instead evolves a silent voice cheaply and catches up exactly
// where it matters:
//   1. Oscillator: the reconstructed WSC's physical state (charge, reset
//      one-shot, both 0.1 s coupling baselines) is advanced in closed form by
//      ReconstructedOscillator::advanceIdle, event to event, without antialias
//      residuals. It never resets and never stops (M41: TR reaches only the EG
//      gates), and its slow coupling memories stay exact.
//   2. Filter memories: both IG00156 nodal stages are clocked once per idle
//      step at the step rate, driven by the exact mean of the oscillator's saw
//      and pulse pins over the step interval CENTRED on the step instant (the
//      oscillator runs half a step ahead of the filters while idle). This
//      tracks the slow modes, including the 1 uF couplings, that a short replay
//      cannot settle. The trapezoidal histories are re-referenced whenever the
//      clock rate of a stage changes (Ig00156Stage::reReference), so the node
//      voltages and their derivatives carry over unchanged.
//   3. Wake: the circuit is restored to a stored idle-step snapshot at least W
//      samples old and replayed at the full internal rate with the production
//      primitives up to the note-on, the frequency following the recorded
//      per-step controls. This rebuilds the modes the step rate cannot
//      represent (or mistunes); W is set from those modes.
// A voice that wakes before its first idle step replays its elapsed samples
// exactly instead. Declared simplifications (docs/workstreams/ws3-keyboard.md):
// idle controls are piecewise linear per step, and the decimated step omits
// per-rank noise (the replay window includes it from a per-voice idle stream,
// never the shared Engine RNG). The software filter bypass keeps its existing
// meaning: a bypassed stage is not clocked, idle or active.
// Gate: tests/FreeRunningVoiceTests.cpp against src/reference/FreeRunningVoiceTruth.h.
namespace cs80::voice_continuity
{
// v2: ported from the PolyBLEP oscillator and the first-stage Svf circuit to
// the reconstructed WSC oscillator and the nodal IG00156 stages (ws1 merge).
inline constexpr const char* modelId = "free-running-idle-v2";
// Idle control/step rate. 3 kHz keeps the fundamental and low harmonics of
// every keyboard note inside the step band while costing a few % of a sounding voice.
inline constexpr double kIdleStepHz = 3000.0;
// Replay length: kCatchUpSettleTaus time constants of the slowest mode the
// step rate cannot represent, bounded to [kCatchUpMinSeconds, kCatchUpMaxSeconds].
inline constexpr double kCatchUpSettleTaus = 9.0;
// Implementation revision for v2 (tolerances unchanged): 1 ms left the open-saw
// gate row at -38.8 dB (limit -40) at 88.2 kHz, because the decimated step
// mistunes the forced response of an in-band stage to the fundamental itself,
// not only near the stage's cutoff; 1.5 ms gives -43.2 dB worst.
inline constexpr double kCatchUpMinSeconds = 0.0015;
inline constexpr double kCatchUpMaxSeconds = 0.012;
// A stage whose cutoff is above this fraction of the step rate is treated as a
// fast mode that only the replay settles.
inline constexpr double kStepBandFraction = 0.2;

inline int idleStepSamples(double internalRate) noexcept
{
    return std::max(1, static_cast<int>(std::lround(internalRate / kIdleStepHz)));
}

// Everything the idle path needs to clock one rank, captured from the same
// control computation the sounding voice uses. Cutoffs are the documented
// control-law part passed to Ig00156Stage::set (the stage adds its 4.7 MOhm
// feedforward); Q includes any matrix ratio; frequency includes footage,
// ribbon and the SH hold; pulseWidth is the panel duty proxy.
struct IdleRankControls
{
    float frequency = 0.0f, pulseWidth = 0.5f, noise = 0.0f;
    bool saw = false, pulse = false;
    float hpCutoff = 0.0f, hpQ = 0.5f, lpCutoff = 0.0f, lpQ = 0.5f;
    bool hpOn = false, lpOn = false;
};

inline WscControls wscControls(const IdleRankControls& c) noexcept
{
    WscControls w;
    w.pulseThresholdVolts = ReconstructedOscillator::panelThresholdVoltsInferred(c.pulseWidth);
    w.sawEnabled = c.saw;
    w.pulseEnabled = c.pulse;
    return w;
}

// Engine adapter of the WSC pins (Engine.cpp): saw + pulse in engine units.
inline float sawPulseUnits(const WscOutputVolts& pins) noexcept
{
    return static_cast<float>((pins.sawPin5 + pins.pulsePin7) * kWscEngineUnitsPerVolt);
}

// The rate at which each stage's histories were last clocked (0: never).
struct IdleStageReferences { float hpRate = 0.0f, lpRate = 0.0f; };

// Clock one stage for one interval at `rate`, re-referencing first when its
// previous interval ran at another rate. `idleStep` selects the unwarped
// couplings of the decimated idle clock (Ig00156Stage::set).
inline Ig00156Stage::Output clockStage(Ig00156Stage& stage, float& clockRate, float input,
                                       float cutoff, float q, float rate, bool idleStep) noexcept
{
    const auto previous = stage.halfSteps();
    stage.set(cutoff, q, rate, idleStep);
    if (clockRate != rate)
    {
        if (clockRate > 0.0f)
            stage.reReference(previous);
        clockRate = rate;
    }
    return stage.process(input);
}

// Clock both stages (HP then LP, each only when enabled) and return the rank's
// filtered signal: LP output, else HP output, else the input.
inline float clockStages(Ig00156Stage& hp, Ig00156Stage& lp, IdleStageReferences& refs, float input,
                         const IdleRankControls& c, float rate, bool idleStep = false) noexcept
{
    float x = input;
    if (c.hpOn)
        x = clockStage(hp, refs.hpRate, x, c.hpCutoff, c.hpQ, rate, idleStep).hp;
    if (c.lpOn)
        x = clockStage(lp, refs.lpRate, x, c.lpCutoff, c.lpQ, rate, idleStep).lp;
    return x;
}

// Settling time constant of one stage's slowest resonant pole (total cutoff).
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
// The nodal stage's own fast modes (the AI node near 33-39 kHz) settle within
// microseconds and are covered by kCatchUpMinSeconds.
inline constexpr double kCatchUpTargetError = 1.0e-3;
inline double stageReplaySeconds(double cutoffHz, double q, double stepRate) noexcept
{
    const double total = cutoffHz + kIg00156FeedforwardHz;
    const double tau = stageTau(total, q);
    if (total > kStepBandFraction * stepRate) return kCatchUpSettleTaus * tau;
    const double x = 3.14159265358979323846 * total / stepRate;
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

// Snapshot of one rank at an idle-step instant: a consistent (time,
// oscillator, stages) triple a replay can start from. Kept every
// kHistoryStride steps in a short ring (always ~1.3 ms apart, ~16 ms deep).
// Compact (about 280 bytes): the oscillator's physical state without its
// residual buffer, which is empty after an idle step, and the stages' memories.
struct IdleSnapshot
{
    ReconstructedOscillator::IdleState osc {};
    Ig00156Stage::Memory hp {}, lp {};
    IdleStageReferences refs {};
};
inline constexpr int kHistoryStride = 4;
inline constexpr int kHistoryLength = 12;
static_assert((kHistoryLength - 2) * kHistoryStride >= kCatchUpMaxSeconds * kIdleStepHz,
              "idle history must cover the longest replay");
// Per-step oscillator frequency, for the replay's frequency trajectory.
inline constexpr int kFrequencyHistory = kHistoryStride * kHistoryLength;
struct IdleHistory
{
    std::array<IdleSnapshot, kHistoryLength> entries {};
    std::array<float, kFrequencyHistory> frequency {};
    // Step 0 (the silence instant) keeps the complete oscillator state: its
    // residual buffer still holds the last sounding samples' antialias tails.
    ReconstructedOscillator::State silence {};
};
// Step counts wrap at a multiple of the ring period so slot indices stay valid.
inline constexpr int kIdleStepWrap = (1 << 28) - ((1 << 28) % (kHistoryStride * kHistoryLength));

template <class Rank>
void storeSnapshot(Rank& r, int slot, const ReconstructedOscillator::IdleState& osc) noexcept
{
    auto& e = r.idleHistory.entries[static_cast<std::size_t>(slot)];
    e.osc = osc;
    e.hp = r.hpStage.getMemory();
    e.lp = r.lpStage.getMemory();
    e.refs = r.idleRefs;
}

inline float recordedFrequency(const IdleHistory& h, int step) noexcept
{
    return h.frequency[static_cast<std::size_t>(((step % kFrequencyHistory) + kFrequencyHistory) % kFrequencyHistory)];
}

// Full-rate replay with the production primitives, forward from the present
// state. The frequency ramps linearly from f0 to f1 over `samples`.
template <class Rank>
void replayRank(Rank& r, const IdleRankControls& c, float sampleRate, int samples, float f0, float f1,
                Rng& noise) noexcept
{
    auto controls = wscControls(c);
    for (int i = 0; i < samples; ++i)
    {
        const float f = f0 + (f1 - f0) * (static_cast<float>(i + 1) / static_cast<float>(samples));
        float x = sawPulseUnits(r.wsc.tick(f, controls));
        if (c.noise > 0.0f) x += noise.nextBipolar() * c.noise * 0.8f;
        clockStages(r.hpStage, r.lpStage, r.idleRefs, x * 0.9f, c, sampleRate);
    }
}

// One idle step of one rank (members wsc, hpStage, lpStage, idle, idleRefs,
// idleLead, idleFirstSamples, idleHistory as in Engine::Rank). `next` are this step's controls;
// the rank's stored controls are the previous step's (or the last sounding
// sample's). `stepIndex` counts idle steps since the circuit went silent, this
// one included (1 for the first). The oscillator is advanced over the window
// (t - D/2, t + D/2] around this step instant t at this step's frequency (the
// midpoint rule), so it leads the filters by D/2 between steps.
template <class Rank>
void idleStepRank(Rank& r, const IdleRankControls& next, float sampleRate, int samples,
                  int stepIndex) noexcept
{
    const double delta = static_cast<double>(samples) / sampleRate, half = 0.5 * delta;
    if (stepIndex == 1)
    {
        // Until the first step the silent circuit is untouched, so it still
        // holds the state it had when it went silent: that is step 0.
        r.idleHistory.silence = r.wsc.getState();
        storeSnapshot(r, 0, r.wsc.getIdleState());
        r.idleHistory.frequency[0] = r.idle.frequency;
        r.idleFirstSamples = samples;
        r.idleLead = 0.0;
    }
    const float previousFrequency = r.idle.frequency;
    const auto previousControls = wscControls(r.idle);
    r.idle = next;
    const auto controls = wscControls(next);
    // The oscillator is at (t - D) + lead; bring it to the window start t - D/2
    // under the controls it had (zero for regular steps; D/2 on the first).
    const double gap = half - r.idleLead;
    if (gap > 0.0)
        (void) r.wsc.advanceIdle(gap, previousFrequency, previousControls);
    const double toInstant = gap > 0.0 ? half : std::max(1.0e-12, delta - r.idleLead);
    const double meanA = r.wsc.advanceIdle(toInstant, next.frequency, controls);
    const bool snapshot = stepIndex % kHistoryStride == 0;
    ReconstructedOscillator::IdleState atInstant {};
    if (snapshot)
        atInstant = r.wsc.getIdleState();
    const double meanB = r.wsc.advanceIdle(half, next.frequency, controls);
    r.idleLead = half;
    const double mean = (meanA * toInstant + meanB * half) / (toInstant + half);
    const float input = static_cast<float>(mean * kWscEngineUnitsPerVolt) * 0.9f;
    clockStages(r.hpStage, r.lpStage, r.idleRefs, input, r.idle, static_cast<float>(1.0 / delta), true);
    r.idleHistory.frequency[static_cast<std::size_t>(stepIndex % kFrequencyHistory)] = next.frequency;
    if (snapshot)
        storeSnapshot(r, (stepIndex / kHistoryStride) % kHistoryLength, atInstant);
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
            replayRank(ranks[i], ranks[i].idle, sampleRate, pending, ranks[i].idle.frequency,
                       ranks[i].idle.frequency, noise);
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
    for (int i = 0; i < count; ++i)
    {
        auto& r = ranks[i];
        const auto& snap = r.idleHistory.entries[static_cast<std::size_t>((start / kHistoryStride) % kHistoryLength)];
        if (start == 0)
            r.wsc.restoreState(r.idleHistory.silence);
        else
            r.wsc.restoreIdleState(snap.osc);
        r.hpStage.restoreMemory(snap.hp);
        r.lpStage.restoreMemory(snap.lp);
        r.idleRefs = snap.refs;
        // Step by step along the recorded idle frequency, then hold the last
        // step's controls up to the note-on. A stage switched on or off while
        // silent follows the present controls for the whole replay.
        for (int k = start; k < steps; ++k)
            replayRank(r, r.idle, sampleRate, k == 0 ? r.idleFirstSamples : stepSamples,
                       recordedFrequency(r.idleHistory, k), recordedFrequency(r.idleHistory, k + 1), noise);
        replayRank(r, r.idle, sampleRate, pending, r.idle.frequency, r.idle.frequency, noise);
        r.idleLead = 0.0;
    }
}
} // namespace cs80::voice_continuity
