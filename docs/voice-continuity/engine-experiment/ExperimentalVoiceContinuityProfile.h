#pragma once

#include "experimental/ContinuousVoiceCircuit.h"

namespace cs80::experimental
{
// Test-only adapter profile. No host parameter, preset field or production
// selection consumes it. All choices below are nominal interface assumptions,
// not a recovered complete KAS, touch or common-noise model.
struct ExperimentalVoiceContinuityProfile
{
    static constexpr const char* id = "engine-continuity-nominal-v1";
    int version = 1;
    bool enabled = false;
    int coldMidiNote = 60;
    float initialKeyVolts = 0.5f;
    std::uint32_t phaseSeed = 0xc5800101u;
    std::uint32_t idleNoiseSeed = 0xc5800201u;

    bool valid() const noexcept
    {
        return version == 1 && coldMidiNote >= 0 && coldMidiNote <= 127
            && std::isfinite(initialKeyVolts) && initialKeyVolts >= 0
            && initialKeyVolts <= 1024 && phaseSeed != 0 && idleNoiseSeed != 0;
    }
    bool operator==(const ExperimentalVoiceContinuityProfile&) const = default;
};
} // namespace cs80::experimental
