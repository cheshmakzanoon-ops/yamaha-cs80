#pragma once
// Uniform-drive specialization of the factory cutoff evaluator.
// No persistent state, no changed audio/control clock. Keep the validation and
// ordered arithmetic in sync with source_factory::evaluateCutoff.
#include "SourceFactoryTones.h"
#include "HeterogeneousSourceCutoff.h"
#include <bit>
#if defined(__x86_64__) && defined(__GNUC__) && defined(__SSE2_MATH__) && !defined(__clang__) \
    && !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER) && !defined(__NVCOMPILER) \
    && !defined(__PGI) && !defined(__ibmxl__) && !defined(_MSC_VER) \
    && !defined(__FMA__) && !defined(__FMA4__) && !defined(__AVX512F__) && !defined(__FP_FAST_FMA) && !defined(__FP_FAST_FMAF) && !defined(__FAST_MATH__) \
    && !defined(__ASSOCIATIVE_MATH__) && !defined(__RECIPROCAL_MATH__) \
    && !defined(__NO_SIGNED_ZEROS__) && (!defined(__FINITE_MATH_ONLY__) || !__FINITE_MATH_ONLY__) \
    && defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ == 0 && !defined(CS80_DISABLE_UNIFORM_CUTOFF)
#include <xmmintrin.h>
#endif

namespace cs80::source_factory::uniform_cutoff {
namespace sf = cs80::source_factory;

static inline bool admitted(const sf::RankDrives& input) noexcept {
#if defined(__x86_64__) && defined(__GNUC__) && defined(__SSE2_MATH__) && !defined(__clang__) \
    && !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER) && !defined(__NVCOMPILER) \
    && !defined(__PGI) && !defined(__ibmxl__) && !defined(_MSC_VER) \
    && !defined(__FMA__) && !defined(__FMA4__) && !defined(__AVX512F__) && !defined(__FP_FAST_FMA) && !defined(__FP_FAST_FMAF) && !defined(__FAST_MATH__) \
    && !defined(__ASSOCIATIVE_MATH__) && !defined(__RECIPROCAL_MATH__) \
    && !defined(__NO_SIGNED_ZEROS__) && (!defined(__FINITE_MATH_ONLY__) || !__FINITE_MATH_ONLY__) \
    && defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ == 0 && !defined(CS80_DISABLE_UNIFORM_CUTOFF)
    // Compare representations. Never conflate signed zeros, a subnormal
    // with zero when DAZ is enabled, or distinct NaN encodings.
    const auto z = std::bit_cast<std::uint64_t>(input[0].filterEnvelopeZ);
    const auto v = std::bit_cast<std::uint64_t>(input[0].velocity);
    const auto p = std::bit_cast<std::uint64_t>(input[0].pressure);
    for (std::size_t index=1; index<input.size(); ++index) {
        const auto& voice = input[index];
        if (z != std::bit_cast<std::uint64_t>(voice.filterEnvelopeZ)
            || v != std::bit_cast<std::uint64_t>(voice.velocity)
            || p != std::bit_cast<std::uint64_t>(voice.pressure)) return false;
    }
    // GNU x86-64 ordinary SSE arithmetic, nearest, all exceptions masked.
    // DAZ and FTZ may be independently set; the scalar operations retained
    // below are still performed in the current thread's environment.
    return (_mm_getcsr() & 0x7f80u) == 0x1f80u;
#else
    (void) input;
    return false;
#endif
}

namespace detail {
template<bool compactPublication = false>
static inline bool evaluateForEngine(const sf::ToneControls& tone, const sf::RankDrives& input,
                     sf::CutoffReadout& out, bool* tookFast, bool precheckedEngineTone) noexcept {
    if (!admitted(input)) return sf::heterogeneous_cutoff::detail::evaluateForEngine(tone, input, out, precheckedEngineTone);
    if (tookFast) *tookFast = true;

    if (!precheckedEngineTone && (!tone.compiled || tone.provenance.version != sf::modelVersion
        || tone.provenance.sourceHash != sf::sourceFingerprint
        || (tone.provenance.inferredCells != 0
            && (tone.provenance.hypothesisVersion != cs80::factory_hypotheses::version
                || tone.provenance.hypothesisHash != cs80::factory_hypotheses::fingerprint)))) return false;

    // All channel drive bits agree. Keep the first stock touch/envelope
    // calculation and the original ordered eight-step accumulations.
    sf::VoiceDrives d;
    bool saturated = false;
    if (!sf::sanitizeDrive(input[0], d, saturated)) return false;
    const double touch = tone.initialBrillianceVolts * d.velocity + tone.afterBrillianceVolts * d.pressure;
    const double envelope = sf::envelopeVoltsFromState(d.filterEnvelopeZ);
    double touchSum = 0.0, envelopeSum = 0.0;
    for (int voice = 0; voice < sf::voiceCount; ++voice) {
        touchSum += touch;
        envelopeSum += envelope;
    }

    std::array<double, 2> pins{}, cvs{}, buses{};
    for (std::size_t side = 0; side < 2; ++side) {
        const auto& m = tone.cutoff[side];
        const double bus = m.busBaseVolts + m.busPerTouchSum * touchSum
            + m.busPerEnvelopeVoltSum * envelopeSum;
        if (!std::isfinite(bus)) return false;
        const double pin = m.pinPerLoadedBus * bus + m.pinPerTouch * touch
            + m.pinPerEnvelopeVolt * envelope;
        const double cv = m.effectiveCvOffset + m.effectiveCvPerPinVolt * (pin - m.referencePinVolts);
        if (!std::isfinite(pin) || !std::isfinite(cv)) return false;
        pins[side] = pin; cvs[side] = cv; buses[side] = bus;
    }
    // All failure checks, even LP after an otherwise valid HP, have now
    // completed. On the engine's qualified path publish the two distinct
    // values of each field directly, without staging eight duplicate pairs.
    if constexpr (compactPublication) {
        out.pinVolts.fill(pins);
        out.effectiveCv.fill(cvs);
        out.loadedBusVolts = buses;
        out.saturatedVoices = saturated ? (1u << sf::voiceCount) - 1u : 0u;
    } else {
        sf::CutoffReadout next;
        if (saturated) next.saturatedVoices = (1u << sf::voiceCount) - 1;
        next.pinVolts.fill(pins);
        next.effectiveCv.fill(cvs);
        next.loadedBusVolts = buses;
        out = next;
    }
    return true;
}
} // namespace detail
static inline bool evaluate(const sf::ToneControls& tone,const sf::RankDrives& input,
                            sf::CutoffReadout& out, bool* tookFast=nullptr) noexcept {
    return detail::evaluateForEngine(tone,input,out,tookFast,false);
}
}
