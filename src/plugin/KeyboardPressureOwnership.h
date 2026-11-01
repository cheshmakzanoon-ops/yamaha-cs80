#pragma once
#include "KeyboardEventQueue.h"
#include <algorithm>

namespace cs80
{
// Audio-thread-only, bounded attribution; no DSP changes. Snapshot accepts the
// existing Engine VoiceInfo array (or plain test fixtures with the same fields).
class KeyboardPressureOwnership
{
public:
    void clear() noexcept { current.fill({}); origins.fill({}); }
    // Dropping the second instrument must not discard valid first-engine
    // gestures, but no old extension origin may survive a later re-entry.
    void clearExtension() noexcept
    {
        for (std::size_t i = 8; i < current.size(); ++i)
        {
            current[i] = {};
            origins[i] = {};
        }
    }
    void noteOff(int channel, int note) noexcept
    {
        for (auto& owner : current)
            if (owner.channel == channel && owner.note == note) owner = {};
    }
    void hostNote(int note) noexcept
    {
        for (auto& owner : current)
            if (owner.note == note) owner = {};
    }
    template <class Snapshot>
    bool allocated(const Snapshot& before, const Snapshot& after, KeyPressureToken token,
                   int channel, int note) noexcept
    {
        // Acceptance by the FIFO is NOT proof an Engine allocated a voice:
        // eight or sixteen held/pedal-held circuits can refuse an onset.
        if (token) noteOff(channel, note);
        bool found = false;
        const auto slots = std::min({ origins.size(), before.size(), after.size() });
        for (size_t i = 0; i < slots; ++i)
            if ((!before[i].active || (!before[i].held && !before[i].pedalHeld))
                && after[i].active && after[i].held
                && after[i].channel == channel && after[i].note == note)
            {
                origins[i] = token; // Host allocation/reuse clears old UI origin.
                current[i] = token; // Host reuse also revokes the slot's old current gesture.
                found = true;
            }
        return found;
    }
    template <class Snapshot>
    bool allows(KeyPressureToken token, const Snapshot& voices) const noexcept
    {
        if (!token || token.channel < 1 || token.channel > 16 || token.note < 0 || token.note > 127) return false;
        const auto slots = std::min(origins.size(), voices.size());
        bool currentGesture = false;
        for (size_t i = 0; i < slots; ++i) currentGesture |= current[i] == token;
        if (!currentGesture) return false;
        bool exactHeldOrigin = false;
        for (size_t i = 0; i < slots; ++i)
        {
            const auto& voice = voices[i];
            if (!voice.active || voice.note != token.note) continue;
            // Non-MPE Engine pressure groups pitches across channels and also
            // reaches release tails. Conservatively reject ALL host aliases,
            // even when MPE would have allowed a narrower channel-only group.
            if (!origins[i] || origins[i].note != voice.note || origins[i].channel != voice.channel)
                return false;
            exactHeldOrigin |= voice.held && voice.channel == token.channel && origins[i] == token;
        }
        return exactHeldOrigin;
    }
private:
    // 16-mode adds another physical instrument. Index its eight circuits as
    // slots 8..15; eight-voice snapshots use only 0..7. Never index by the
    // much larger 16x128 MIDI-key space.
    std::array<KeyPressureToken, 16> current {};
    std::array<KeyPressureToken, 16> origins {};
};
static_assert(sizeof(KeyboardPressureOwnership) <= 1024,
              "Pressure ownership must remain bounded by sixteen physical voices");
}
