#pragma once

// Voice-mode extensions (IMPROVEMENT_PLAN Phase 6, modern layer; PURE CS-80
// forces the authentic eight voices). Unison runs inside Engine; the
// sixteen-voice mode is two complete eight-voice instruments side by side (a
// second Engine in the processor), so each keeps the original allocator, its
// own source networks and its own output stage. This router decides which
// instrument takes a key.

#include "Engine.h"

namespace cs80
{
struct SixteenVoiceRouter
{
    // The first instrument plays until it has no free circuit; the second
    // then takes the key. If both are full, the first keeps it (its KAS model
    // remembers a still-held key).
    static Engine& noteOnTarget(Engine& first, Engine& second) noexcept
    {
        return first.hasFreeCircuit() || ! second.hasFreeCircuit() ? first : second;
    }
};
} // namespace cs80
