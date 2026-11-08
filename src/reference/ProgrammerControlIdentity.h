#pragma once

#include "core/Params.h"
#include "core/SourceFactoryTones.h"

namespace cs80::reference::programmer_identity
{
// Identity only: this does NOT map values to electrical wiper travel, voltage,
// resistance, or source impedance. Physical row and channel-storage order differ.
constexpr int channelParamForRole(source_factory::Role role) noexcept
{
    using R = source_factory::Role;
    switch (role)
    {
        case R::pwmRate: return CP_pwmSpeed;
        case R::pwmDepth: return CP_pwmDepth;
        case R::pulseWidth: return CP_pw;
        case R::pulseGate: return CP_pulse;
        case R::sawGate: return CP_saw;
        case R::noise: return CP_noise;
        case R::highpassCutoff: return CP_hpfCutoff;
        case R::highpassQ: return CP_hpfRes;
        case R::lowpassCutoff: return CP_lpfCutoff;
        case R::lowpassQ: return CP_lpfRes;
        case R::filterInitial: return CP_fIl;
        case R::filterAttackTarget: return CP_fAl;
        case R::filterAttackTime: return CP_fA;
        case R::filterDecayTime: return CP_fD;
        case R::filterReleaseTime: return CP_fR;
        case R::filterGain: return CP_vcfLevel;
        case R::sineGain: return CP_sine;
        case R::amplitudeAttackTime: return CP_vA;
        case R::amplitudeDecayTime: return CP_vD;
        case R::amplitudeSustain: return CP_vS;
        case R::amplitudeReleaseTime: return CP_vR;
        case R::level: return CP_vLevel;
        case R::initialBrilliance: return CP_initBrill;
        case R::initialLevel: return CP_initLevel;
        case R::afterBrilliance: return CP_afterBrill;
        case R::afterLevel: return CP_afterLevel;
    }
    return -1;
}

// One-based physical output row, as used in MemoryPanelNetwork and the source
// ledger. Returns a channel-local CP_* index, not a host/global parameter index.
constexpr int channelParamForOutput(int output) noexcept
{
    if (output < 1 || output > static_cast<int>(source_factory::roles.size())) return -1;
    return channelParamForRole(source_factory::roles[static_cast<size_t>(output - 1)]);
}

// Feet, pan and HPF/LPF bypass have no programmer output row. Invalid indices
// also return -1, rather than accidentally aliasing physical row zero.
constexpr int outputForChannelParam(int channelParam) noexcept
{
    for (int output = 1; output <= static_cast<int>(source_factory::roles.size()); ++output)
        if (channelParamForOutput(output) == channelParam) return output;
    return -1;
}
} // namespace cs80::reference::programmer_identity
