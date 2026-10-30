#pragma once
#include "KeyboardEventQueue.h"

namespace cs80
{
// Audio-thread-only, bounded attribution; no DSP changes. Snapshot accepts the
// existing Engine VoiceInfo array (or plain test fixtures with the same fields).
class KeyboardPressureOwnership
{
public:
    void clear() noexcept { for (auto& channel : current) channel.fill({}); origins.fill({}); }
    void noteOff(int channel, int note) noexcept { current[(size_t) channel - 1][(size_t) note] = {}; }
    void hostNote(int note) noexcept { for (auto& channel : current) channel[(size_t) note] = {}; }
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
                if (token) current[(size_t) channel - 1][(size_t) note] = token;
                found = true;
            }
        return found;
    }
    template <class Snapshot>
    bool allows(KeyPressureToken token, const Snapshot& voices) const noexcept
    {
        if (!token || token.channel < 1 || token.channel > 16 || token.note < 0 || token.note > 127
            || !(current[(size_t) token.channel - 1][(size_t) token.note] == token)) return false;
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
    std::array<std::array<KeyPressureToken, 128>, 16> current {};
    std::array<KeyPressureToken, 8> origins {};
};
}
