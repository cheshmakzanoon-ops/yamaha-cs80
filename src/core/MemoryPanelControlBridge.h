#pragma once

#include "MemoryPanels.h"
#include "Params.h"

#include <algorithm>
#include <array>
#include <cmath>

// Input-only bridge for the future circuit-engine memory/PANEL source route.
// The plugin already copies selected M1/M3 or M2/M4 physical parameter values
// into ControlFrame::effectiveValues; source 13 carries the current rank's
// PANEL parameters. No new APVTS bank, host ID, or audio path is introduced.
//
// A panel control's NormalisableRange position is the electrical pot fraction;
// converting the physical value requires the inverse of JUCE's skew law.
// This assignment of software slider travel to hardware wiper travel remains a
// DECLARED INFERENCE. Evidence for the selected rail/pots/diodes is BA27/BA28,
// MBK63 (source image titled MKB) and S31-S38, as reviewed in docs/workstreams/ws6-library.md.
namespace cs80::memory_panels::control_bridge
{
inline constexpr std::array<ChannelParam, 26> rowParameter {{
    CP_pwmSpeed, CP_pwmDepth, CP_pw, CP_pulse, CP_saw, CP_noise,
    CP_hpfCutoff, CP_hpfRes, CP_lpfCutoff, CP_lpfRes,
    CP_fIl, CP_fAl, CP_fA, CP_fD, CP_fR,
    CP_vcfLevel, CP_sine, CP_vA, CP_vD, CP_vS, CP_vR, CP_vLevel,
    CP_initBrill, CP_initLevel, CP_afterBrill, CP_afterLevel
}};

constexpr bool validRowParameters() noexcept
{
    for (std::size_t i = 0; i < rowParameter.size(); ++i)
    {
        if (rowParameter[i] == CP_feet || rowParameter[i] == CP_pan
            || rowParameter[i] == CP_hpfOn || rowParameter[i] == CP_lpfOn)
            return false;
        for (std::size_t j = i + 1; j < rowParameter.size(); ++j)
            if (rowParameter[i] == rowParameter[j]) return false;
    }
    return rowParameter[3] == CP_pulse && rowParameter[4] == CP_saw
        && rowParameter[6] == CP_hpfCutoff && rowParameter[8] == CP_lpfCutoff
        && rowParameter[21] == CP_vLevel;
}
static_assert(validRowParameters(), "Each of the 26 schematic outputs must map to exactly one panel control");

inline double position(const ParamDef& def, float physical) noexcept
{
    if (!std::isfinite(physical)) physical = def.def;
    if (!(def.max > def.min) || !(def.skew > 0.0f)) return 0.0;
    const double linear = std::clamp((static_cast<double>(physical) - def.min)
                                     / (static_cast<double>(def.max) - def.min), 0.0, 1.0);
    return std::pow(linear, 1.0 / def.skew);
}

inline Positions positions(const ParamValues& effective, int rank) noexcept
{
    Positions result {};
    if (rank < 0 || rank > 1) return result;
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto param = rowParameter[i];
        result[i] = position(kChannelDefs[param], effective[chParam(rank, param)]);
    }
    return result;
}
} // namespace cs80::memory_panels::control_bridge
