#pragma once
#include "KeyboardEventQueue.h"
#include <tuple>

namespace cs80
{
// Audio-thread-only, bounded attribution; no DSP changes. Snapshot accepts the
// existing Engine VoiceInfo array (or plain test fixtures with the same fields).
class KeyboardPressureOwnership
{
public:
    // Eight original circuits plus the optional second eight-voice instrument.
    static constexpr size_t maximumVoices = 16;
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
        static_assert(std::tuple_size_v<Snapshot> <= maximumVoices);
        // Acceptance by the FIFO is NOT proof Engine allocated a voice: all
        // held/pedal-held circuits can refuse an onset. Never claim that voice.
        if (token) noteOff(channel, note);
        bool found = false;
        for (size_t i = 0; i < after.size(); ++i)
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
        static_assert(std::tuple_size_v<Snapshot> <= maximumVoices);
        if (!token || token.channel < 1 || token.channel > 16 || token.note < 0 || token.note > 127) return false;
        bool currentGesture = false;
        for (const auto& owner : current) currentGesture |= owner == token;
        if (!currentGesture) return false;
        bool exactHeldOrigin = false;
        for (size_t i = 0; i < voices.size(); ++i)
        {
            const auto& voice = voices[i];
            if (!voice.active || voice.note != token.note) continue;
            // Keep the established conservative UI policy: reject ALL host
            // pitch aliases, including other channels and release tails.
            // Positive-channel Engine pressure is channel-scoped, but this
            // ownership correction does not broaden pointer attribution.
            if (!origins[i] || origins[i].note != voice.note || origins[i].channel != voice.channel)
                return false;
            exactHeldOrigin |= voice.held && voice.channel == token.channel && origins[i] == token;
        }
        return exactHeldOrigin;
    }
private:
    // At most sixteen allocated circuits can own pressure. Index by physical slot,
    // not a 16x128 MIDI-key table copied into every processor/test stack frame.
    std::array<KeyPressureToken, maximumVoices> current {};
    std::array<KeyPressureToken, maximumVoices> origins {};
};
static_assert(sizeof(KeyboardPressureOwnership) <= 1024,
              "Pressure ownership must remain bounded by sixteen physical voices");
}
