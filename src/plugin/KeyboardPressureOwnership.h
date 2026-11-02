#pragma once
#include "KeyboardEventQueue.h"

namespace cs80
{
// Audio-thread-only, bounded attribution; no DSP changes. Snapshot accepts the
// existing Engine VoiceInfo array (or plain test fixtures with the same fields).
template <std::size_t VoiceCount>
class BasicKeyboardPressureOwnership
{
public:
    void clear() noexcept { current.fill({}); origins.fill({}); }
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
        // Acceptance by the FIFO is NOT proof Engine allocated a voice: eight
        // held/pedal-held circuits can refuse an onset. Never claim that voice.
        if (token) noteOff(channel, note);
        bool found = false;
        for (size_t i = 0; i < origins.size(); ++i)
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
        bool currentGesture = false;
        for (const auto& owner : current) currentGesture |= owner == token;
        if (!currentGesture) return false;
        bool exactHeldOrigin = false;
        for (size_t i = 0; i < origins.size(); ++i)
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
    // Only allocated circuits can own pressure. Index by physical slot,
    // not a 16x128 MIDI-key table copied into every processor/test stack frame.
    std::array<KeyPressureToken, VoiceCount> current {};
    std::array<KeyPressureToken, VoiceCount> origins {};
};
using KeyboardPressureOwnership = BasicKeyboardPressureOwnership<8>;
using SixteenVoicePressureOwnership = BasicKeyboardPressureOwnership<16>;
static_assert(sizeof(KeyboardPressureOwnership) <= 1024,
              "Pressure ownership must remain bounded by eight physical voices");
static_assert(sizeof(SixteenVoicePressureOwnership) <= 32 * sizeof(KeyPressureToken),
              "Sixteen-voice pressure ownership remains bounded by its physical slots");
}
