#pragma once
// Acceptance-only oracle, registered before dormant-state implementation.
// This evaluates the original cascade with both memories of every section.
// It neither imports nor calls production reduced-state / matrix / QR helpers.
// The Gaussian law is the existing declared model (SUB29 and ws2-engine.md),
// not a measurement of hardware. Limits: tolerances-v2/dormant-noise.json.
#include "fixtures/dormant_noise/NoiseSourceBeforeDecimation.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace cs80::dormant_noise_truth
{
namespace frozen = cs80::dormant_noise_frozen;

template <std::size_t Size>
using Vector = std::array<long double, Size>;
template <std::size_t Size>
using Matrix = std::array<Vector<Size>, Size>;

template <std::size_t Size>
struct Transition
{
    Matrix<Size> mean {};
    Matrix<Size> covariance {};
};

template <std::size_t Sections>
class FullSectionCascade
{
public:
    static constexpr std::size_t size = Sections + 1;
    using State = Vector<size>;
    using Law = Transition<size>;
    struct FullState
    {
        std::array<long double, Sections> previousInput {};
        std::array<long double, Sections> previousOutput {};
    };

    explicit FullSectionCascade(const std::array<frozen::FirstOrderSection, Sections>& original)
        : sections(original) {}

    // Lift the shared section-boundary values to every original x[-1],y[-1].
    static FullState expand(const State& boundary) noexcept
    {
        FullState state;
        for (std::size_t section = 0; section < Sections; ++section)
        {
            state.previousInput[section] = boundary[section];
            state.previousOutput[section] = boundary[section + 1];
        }
        return state;
    }
    static State project(const FullState& state) noexcept
    {
        State boundary;
        boundary[0] = state.previousInput[0];
        for (std::size_t section = 0; section < Sections; ++section)
            boundary[section + 1] = state.previousOutput[section];
        return boundary;
    }

    void step(FullState& state, long double innovation) const noexcept
    {
        // The original serial section recurrence, with every old input/output
        // stored separately. Coefficients are frozen doubles, evaluated here
        // in long double. This does not assemble the production reduced A.
        long double input = innovation;
        for (std::size_t section = 0; section < Sections; ++section)
        {
            const auto& c = sections[section];
            const long double output = static_cast<long double>(c.b0) * input
                + static_cast<long double>(c.b1) * state.previousInput[section]
                + static_cast<long double>(c.a1) * state.previousOutput[section];
            state.previousInput[section] = input;
            state.previousOutput[section] = output;
            input = output;
        }
    }

    Law transition(std::size_t samples, long double sigma) const noexcept
    {
        Law law;
        // Conditional mean: advance each independent boundary basis through
        // the full original section memories with zero future excitation.
        for (std::size_t column = 0; column < size; ++column)
        {
            State basis {};
            basis[column] = 1.0L;
            auto state = expand(basis);
            for (std::size_t sample = 0; sample < samples; ++sample)
                step(state, 0.0L);
            const auto out = project(state);
            for (std::size_t row = 0; row < size; ++row)
                law.mean[row][column] = out[row];
        }
        // Conditional covariance: each distinct Gaussian input contributes
        // one outer product. Start with the last input's impulse, then move it
        // one original sample earlier per iteration. No covariance factor is
        // involved in this independent oracle.
        FullState impulse {};
        if (samples != 0)
            step(impulse, sigma);
        for (std::size_t age = 0; age < samples; ++age)
        {
            const auto out = project(impulse);
            for (std::size_t row = 0; row < size; ++row)
                for (std::size_t column = 0; column < size; ++column)
                    law.covariance[row][column] += out[row] * out[column];
            step(impulse, 0.0L);
        }
        return law;
    }

private:
    std::array<frozen::FirstOrderSection, Sections> sections;
};

inline FullSectionCascade<4> panel(double rate)
{
    using namespace frozen::noise_design;
    return FullSectionCascade<4>({ frozen::FirstOrderSection::highPass(kCouplingHz, rate),
        frozen::FirstOrderSection::lowPass(kStage1Hz, rate),
        frozen::FirstOrderSection::highPass(kInterstageHz, rate),
        frozen::FirstOrderSection::lowPass(kStage2Hz, rate) });
}
inline FullSectionCascade<2> hiss(double rate)
{
    using namespace frozen::noise_design;
    return FullSectionCascade<2>({ frozen::FirstOrderSection::shelf(kHissZero1Hz, kHissPole1Hz, rate),
        frozen::FirstOrderSection::shelf(kHissZero2Hz, kHissPole2Hz, rate) });
}
inline long double panelSigma(double rate)
{
    return std::sqrt(frozen::CircuitNoiseSources::panelDensity() * rate * 0.5);
}
inline long double hissSigma(double rate)
{
    return std::sqrt(frozen::CircuitNoiseSources::hissDensity() * rate * 0.5);
}

template <std::size_t Size>
Vector<Size> conditionalMean(const Transition<Size>& law, const Vector<Size>& initial) noexcept
{
    Vector<Size> result {};
    for (std::size_t row = 0; row < Size; ++row)
        for (std::size_t column = 0; column < Size; ++column)
            result[row] += law.mean[row][column] * initial[column];
    return result;
}

// Independent Gaussian composition: first A, then B. Used only for long-interval
// oracle checks; the N<=32 production factors are compared with the direct
// full-section impulse expansion above, so this cannot hide a shared matrix bug.
template <std::size_t Size>
Transition<Size> compose(const Transition<Size>& a, const Transition<Size>& b) noexcept
{
    Transition<Size> result;
    Matrix<Size> propagated {};
    for (std::size_t i = 0; i < Size; ++i)
        for (std::size_t j = 0; j < Size; ++j)
            for (std::size_t k = 0; k < Size; ++k)
            {
                result.mean[i][j] += b.mean[i][k] * a.mean[k][j];
                propagated[i][j] += b.mean[i][k] * a.covariance[k][j];
            }
    for (std::size_t i = 0; i < Size; ++i)
        for (std::size_t j = 0; j < Size; ++j)
        {
            result.covariance[i][j] = b.covariance[i][j];
            for (std::size_t k = 0; k < Size; ++k)
                result.covariance[i][j] += propagated[i][k] * b.mean[j][k];
        }
    return result;
}

template <std::size_t Size>
Transition<Size> repeat(Transition<Size> step, std::uint64_t count) noexcept
{
    Transition<Size> result;
    for (std::size_t i = 0; i < Size; ++i)
        result.mean[i][i] = 1.0L;
    while (count != 0)
    {
        if ((count & 1u) != 0)
            result = compose(result, step);
        count >>= 1u;
        if (count != 0)
            step = compose(step, step);
    }
    return result;
}

inline std::uint32_t momentSeed(std::uint32_t rateIndex, std::uint32_t intervalIndex,
                                std::uint32_t initialIndex, std::uint32_t chainIndex) noexcept
{
    std::uint32_t x = 0x4e4f4953u ^ rateIndex * 0x9e3779b9u ^ intervalIndex * 0x85ebca6bu
        ^ initialIndex * 0xc2b2ae35u ^ chainIndex * 0x27d4eb2du;
    x ^= x >> 16u; x *= 0x7feb352du;
    x ^= x >> 15u; x *= 0x846ca68bu;
    x ^= x >> 16u;
    return x;
}
} // namespace cs80::dormant_noise_truth
