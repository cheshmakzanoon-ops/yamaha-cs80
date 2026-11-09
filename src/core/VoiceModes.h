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
    // Returns false only when the original broadcast path must be retained.
    // Immediate dispatch cannot change ownership between query and release.
    // Deferred/ARP-origin candidates remain unqualified after mode changes.
    static bool noteOff(Engine& first, Engine& second, int note, int channel, bool keyboardOnly = false)
    {
        if (!first.canCoordinateImmediateReleaseWith(second))
            return false;
        using Kind = Engine::ReleaseCandidate::Kind;
        const auto a = first.immediateReleaseCandidate(note, channel, keyboardOnly);
        const auto b = second.immediateReleaseCandidate(note, channel, keyboardOnly);
        if ((a.kind != Kind::none && a.order == 0) || (b.kind != Kind::none && b.order == 0))
            return false;
        if (a.kind == Kind::none && b.kind == Kind::none)
            return true;
        if (a.kind == b.kind && a.order == b.order)
            return false; // An aliased/copied ownership identity is not proof.
        const bool chooseSecond = a.kind == Kind::none || (b.kind != Kind::none
            && ((b.kind == Kind::waiting && a.kind != Kind::waiting)
                || (a.kind == b.kind && b.order > a.order)));
        const auto chosen = chooseSecond ? b : a;
        if (keyboardOnly && !chosen.keyboardOrigin)
            return true; // Never fall through to an older UI-owned group.
        auto& target = chooseSecond ? second : first;
        if (keyboardOnly) target.noteOffFromKeyboard(note, channel);
        else target.noteOff(note, channel);
        return true;
    }

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
