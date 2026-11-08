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
    // The first instrument plays until it has no available circuit; the
    // second then takes the key. Active direct KAS scanning also accounts for
    // occupancy after the existing queue, without borrowing future releases.
    // If both are full, the first keeps it (its KAS model remembers a held key).
    // This is not incoming-event admission: queue/waiter limits still apply.
    // Scan-disabled routing stays unchanged, including deferred UI events
    // left behind by that transition; this API has no event-origin argument.
    static Engine& noteOnTarget(Engine& first, Engine& second) noexcept
    {
        return first.hasFreeCircuitForRouting() || ! second.hasFreeCircuitForRouting() ? first : second;
    }
};
} // namespace cs80
