// Named IEEE-754 comparisons for the separately registered exact event gate.
// This utility does no rendering and introduces no sound law.
//
// A zero-time comparison is legitimate for a sounding tail or a noteOff.
// For an inactive Bass reassignment it is legitimate ONLY after wakeIdleVoice
// has completed pending elapsed time; never compare a stale idle snapshot.
#pragma once

#include "core/Engine.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace click_continuity {

static_assert(sizeof(float) == sizeof(std::uint32_t)
              && sizeof(double) == sizeof(std::uint64_t));
static_assert(std::numeric_limits<float>::is_iec559
              && std::numeric_limits<double>::is_iec559);

inline bool sameBits(float a, float b) noexcept
{
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}
inline bool sameBits(double a, double b) noexcept
{
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

struct Continuation
{
    cs80::ReconstructedOscillator::State oscillator;
    cs80::Engine::RankAudioState audio;
    cs80::Engine::RankEnvelopeState envelopes;
    cs80::Ig00151Vca::State vca;
};

inline Continuation observe(const cs80::Engine& engine, int voice, int rank)
{
    return {
        engine.getRankOscillatorState(voice, rank),
        engine.getRankAudioState(voice, rank),
        engine.getRankEnvelopeState(voice, rank),
        engine.getRankVcaState(voice, rank)
    };
}

struct Mismatches
{
    int oscillator = 0; // 59 exposed members, including all 48 residual samples
    int nodal = 0;      // 8 HP/LP integrator history members
    int envelopes = 0;  // 2 exposed charge members
    int vca = 0;        // 2 level and coupling members
    int phase = 0;      // 1 duplicate engine phase proxy

    int total() const noexcept { return oscillator + nodal + envelopes + vca + phase; }
};

inline Mismatches compare(const Continuation& before, const Continuation& after)
{
    Mismatches diff;
    const auto& a = before.oscillator;
    const auto& b = after.oscillator;
    const auto scalar = [&diff](double x, double y) {
        diff.oscillator += !sameBits(x, y);
    };
    scalar(a.charge, b.charge);
    scalar(a.resetRemainingSeconds, b.resetRemainingSeconds);
    scalar(a.inputBaselineVolts, b.inputBaselineVolts);
    scalar(a.triangleBaselineVolts, b.triangleBaselineVolts);
    scalar(a.heldFrequencyHz, b.heldFrequencyHz);
    scalar(a.heldThresholdVolts, b.heldThresholdVolts);
    scalar(a.elapsedSeconds, b.elapsedSeconds);
    diff.oscillator += a.sawEnabled != b.sawEnabled;
    diff.oscillator += a.pulseEnabled != b.pulseEnabled;
    for (std::size_t j = 0; j < a.residual.size(); ++j)
        for (std::size_t k = 0; k < a.residual[j].size(); ++k)
            scalar(a.residual[j][k], b.residual[j][k]);
    diff.oscillator += a.residualCursor != b.residualCursor;
    diff.oscillator += a.resetCount != b.resetCount;

    const auto nodal = [&diff](cs80::Ig00156Stage::State x,
                               cs80::Ig00156Stage::State y) {
        diff.nodal += !sameBits(x.input, y.input);
        diff.nodal += !sameBits(x.feedback, y.feedback);
        diff.nodal += !sameBits(x.bp, y.bp);
        diff.nodal += !sameBits(x.lp, y.lp);
    };
    nodal(before.audio.hpCircuit, after.audio.hpCircuit);
    nodal(before.audio.lpCircuit, after.audio.lpCircuit);
    diff.envelopes += !sameBits(before.envelopes.amplitude.value,
                                after.envelopes.amplitude.value);
    diff.envelopes += !sameBits(before.envelopes.filter.value,
                                after.envelopes.filter.value);
    diff.vca += !sameBits(before.vca.level, after.vca.level);
    diff.vca += !sameBits(before.vca.coupling, after.vca.coupling);
    diff.phase += !sameBits(before.audio.phase, after.audio.phase);
    return diff;
}

// A fixed field count makes the registered coverage reviewable. Whenever
// public Engine/WSC state changes, review/update both the explicit member
// comparisons above and the preregistered field count before evaluation.
inline constexpr int exposedFieldsPerRank = 59 + 8 + 2 + 2 + 1;
static_assert(exposedFieldsPerRank == 72);

} // namespace click_continuity
