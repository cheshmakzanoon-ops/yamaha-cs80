#pragma once

#include "Dsp.h"

#include <utility>

namespace cs80
{
// Software anti-imaging reconstruction, not an analog input bandwidth model.
// Reuse the established Kaiser coefficient design of the current oversampling
// factor, with independent fixed-size history. Zero insertion and gain N retain
// unity passband gain. Group delay (64N - 1) internal samples: 63.5 host samples
// at 2x, 63.75 at 4x, 63.875 at 8x, EXTERNAL CONTROL ONLY. The synth's declared
// audio output latency remains 63 host samples at every factor.
//
// Store only host samples and omit products whose zero-inserted history is
// identically zero. Preserve the full FIR's center-first, ascending symmetric
// pair summation; a conventional reordered polyphase sum can round differently.
class ExternalModulationInterpolator
{
public:
    static constexpr double kHostDelay = (double) Decimator::kCenter / 2.0; // 2x
    using Frame = std::array<float, OversamplingDecimator::kMaxFactor>;

    void prepare(float hostRate, int oversampling = 2)
    {
        (void) hostRate; // normalized design, like the output decimator
        OversamplingDecimator design;
        design.prepare(oversampling);
        factor = design.getFactor();
        for (int i = 0; i < design.getTaps(); ++i)
            coefficients[(size_t) i] = design.getCoefficient(i);
        reset();
    }

    void reset() noexcept
    {
        clearHistory();
        connected = false;
    }

    void setConnected(bool enabled) noexcept
    {
        if (enabled != connected)
        {
            clearHistory();
            connected = enabled;
        }
    }

    static float sanitise(float input) noexcept
    {
        // Explicit software protection, NOT measured input-stage clipping.
        return std::isfinite(input) ? clampf(input, -1.0f, 1.0f) : 0.0f;
    }

    int getFactor() const noexcept { return factor; }
    double getHostDelay() const noexcept
    { return (double) OversamplingDecimator::centerFor(factor) / (double) factor; }

    // The first getFactor() entries are this host frame's internal samples.
    Frame tick(float input) noexcept
    {
        Frame out {};
        if (!connected) return out;
        const float x = static_cast<float>(factor) * sanitise(input);
        history[(size_t) next] = history[(size_t) (next + kHistory)] = x;
        if (++next == kHistory) next = 0;
        switch (factor)
        {
            case 4: render<4>(out); break;
            case 8: render<8>(out); break;
            default: render<2>(out); break;
        }
        return out;
    }

private:
    // taps = 128N - 1, so no phase reaches farther than 127 host frames.
    // Mirroring makes all lookbacks contiguous, including at the ring wrap.
    static constexpr int kHistory = 128;
    void clearHistory() noexcept { history.fill(0.0f); next = 0; }

    template <int N, int phase>
    float renderPhase() const noexcept
    {
        constexpr int taps = OversamplingDecimator::tapsFor(N);
        constexpr int center = OversamplingDecimator::centerFor(N);
        const float* newest = history.data() + next - 1 + kHistory;
        // At phase p, only taps congruent to p (mod N) contain an input.
        // The center 64N - 1 is occupied only at p = N - 1.
        double y = (double) coefficients[(size_t) center]
            * (phase == N - 1 ? newest[-(center / N)] : 0.0f);
        // A pair (i, 128N - 2 - i) can contribute at either of these two
        // residues. Visit their union in the original ascending tap order;
        // when both residues coincide, evaluate the pair just once.
        constexpr int mirroredPhase = (N - 2 - phase + N) % N;
        constexpr int first = std::min(phase, mirroredPhase);
        constexpr int second = std::max(phase, mirroredPhase);
        const auto add = [&](int tap, int residue) {
            const double a = residue == phase ? newest[-((tap - phase) / N)] : 0.0;
            const double b = residue == mirroredPhase
                ? newest[-((taps - 1 - tap - phase) / N)] : 0.0;
            y += (double) coefficients[(size_t) tap] * (a + b);
        };
        for (int tap = first; tap < center; tap += N)
        {
            add(tap, first);
            if constexpr (first != second)
                if (tap + second - first < center)
                    add(tap + second - first, second);
        }
        // The omitted zero terms also canonicalise an exact signed-zero sum.
        // Add in double before casting so negative nonzero subnormal results
        // still round to the same float (including -0) as the full FIR.
        return (float) (y + 0.0);
    }

    // Compile-time phases remove modulo/division and absent-input branches
    // from the inner loop. No coefficient or history allocation is needed.
    template <int N, std::size_t... phase>
    void renderPhases(Frame& out, std::index_sequence<phase...>) const noexcept
    { ((out[phase] = renderPhase<N, (int) phase>()), ...); }

    template <int N>
    void render(Frame& out) const noexcept
    { renderPhases<N>(out, std::make_index_sequence<N>{}); }

    std::array<float, OversamplingDecimator::kMaxTaps> coefficients {};
    std::array<float, 2 * kHistory> history {};
    int factor = 2, next = 0;
    bool connected = false;
};
} // namespace cs80
