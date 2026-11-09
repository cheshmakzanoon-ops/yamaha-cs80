#pragma once

#include "reference/ProgrammerControlIdentity.h"
#include "core/MemoryPanels.h"
#include "core/Dsp.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace cs80::experimental::memory_control
{
inline constexpr std::uint32_t version = 1;
inline constexpr std::string_view modelId = "semantic-linear-travel-v1";
using ChannelValues = std::array<double, CP_count>;

struct AdaptedBank
{
    memory_panels::Positions positions {};
    std::array<double, 26> requestedNominalVolts {};
    std::uint32_t saturatedRows = 0;
    std::uint32_t adapterVersion = version;
};

// Invert named nominal receiver semantics, THEN assume linear rail-oriented
// travel. This does not invert the loaded pot/diode network or recover a taper.
// Represented host values are validated; excluded controls remain untouched.
inline bool adapt(const ChannelValues& values, AdaptedBank& result) noexcept
{
    namespace sf = source_factory;
    namespace identity = reference::programmer_identity;
    AdaptedBank next;
    for (int output = 1; output <= 26; ++output)
    {
        const auto row = static_cast<std::size_t>(output - 1);
        const int parameter = identity::channelParamForOutput(output);
        if (parameter < 0 || parameter >= CP_count) return false;
        const auto param = static_cast<std::size_t>(parameter);
        const auto& definition = kChannelDefs[param];
        const double value = values[param];
        if (!std::isfinite(value) || value < definition.min || value > definition.max) return false;
        double fraction = value;
        switch (sf::roles[row])
        {
            case sf::Role::pwmRate: fraction = std::log(value / 0.05) / std::log(400.0); break;
            case sf::Role::pulseWidth: fraction = (value - 0.5) / 0.4; break;
            case sf::Role::pulseGate:
            case sf::Role::sawGate:
                next.positions[row] = value >= 0.5 ? 1.0 : 0.0;
                continue; // no fabricated analog voltage for logical outputs
            case sf::Role::highpassCutoff:
                fraction = (sf::alignment.hpBusVolts + std::log2(value / (125.0 * kReferenceKeyVolts)) - sf::alignment.hpEffectiveCv) / 10.0;
                break;
            case sf::Role::lowpassCutoff:
                fraction = (sf::alignment.lpBusVolts + std::log2(value / (125.0 * kReferenceKeyVolts)) - sf::alignment.lpEffectiveCv) / 10.0;
                break;
            case sf::Role::highpassQ:
            case sf::Role::lowpassQ: fraction = 1.0 - value; break;
            case sf::Role::filterAttackTime:
            case sf::Role::amplitudeAttackTime: fraction = -std::log10(value) / 3.0; break;
            case sf::Role::filterDecayTime:
            case sf::Role::filterReleaseTime:
            case sf::Role::amplitudeDecayTime:
            case sf::Role::amplitudeReleaseTime: fraction = std::log10(10.0 / value) / 3.0; break;
            case sf::Role::pwmDepth:
            case sf::Role::noise:
            case sf::Role::filterInitial:
            case sf::Role::filterAttackTarget:
            case sf::Role::filterGain:
            case sf::Role::sineGain:
            case sf::Role::amplitudeSustain:
            case sf::Role::level:
            case sf::Role::initialBrilliance:
            case sf::Role::initialLevel:
            case sf::Role::afterBrilliance:
            case sf::Role::afterLevel: break;
            default: return false; // a new role needs its own reviewed mapping
        }
        if (!std::isfinite(fraction)) return false;
        next.requestedNominalVolts[row] = 10.0 * fraction;
        if (fraction < 0.0 || fraction > 1.0) next.saturatedRows |= 1u << row;
        next.positions[row] = std::clamp(fraction, 0.0, 1.0);
    }
    result = next;
    return true;
}
} // namespace cs80::experimental::memory_control
