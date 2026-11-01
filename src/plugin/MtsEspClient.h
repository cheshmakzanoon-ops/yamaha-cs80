#pragma once

// MTS-ESP client shim (IMPROVEMENT_PLAN Phase 6, modern layer).
//
// ODDSound's 0BSD-licensed MTS-ESP client is vendored, byte-for-byte, in
// src/vendor/mts-esp/Client/. The client dynamically searches for the MTS-ESP
// master library on the user's system and retains its own MTS SysEx fallback.
// If there is no master the engine's MIDI Tuning Standard parser still works
// without any external installation (Engine::handleSysEx).
//
// Client API used (as documented in the MTS-ESP client header): RegisterClient,
// DeregisterClient, HasMaster, NoteToFrequency(note, channel), ShouldFilterNote,
// ParseMIDIDataU. All are designed by ODDSound to be called on the audio thread.

#include "core/Tuning.h"

#include <cstdint>

namespace cs80
{
class MtsEspClient
{
public:
    MtsEspClient();
    ~MtsEspClient();
    MtsEspClient(const MtsEspClient&) = delete;
    MtsEspClient& operator=(const MtsEspClient&) = delete;

    // True when the vendored client was compiled in (even without a master).
    static bool available() noexcept;
    // Audio thread. True while an MTS-ESP master is connected.
    bool hasMaster() const noexcept;
    // Audio thread. Fills all 128 notes from the master; false without one.
    bool fillTable(tuning::TuningTable& table) const noexcept;
    // Audio thread. Lets the client see MTS SysEx too (no-op without the library).
    void parseMidi(const std::uint8_t* data, int size) noexcept;

private:
    void* client = nullptr;
};
} // namespace cs80
