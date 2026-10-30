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
