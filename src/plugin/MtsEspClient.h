#pragma once

// MTS-ESP client shim (IMPROVEMENT_PLAN Phase 6, modern layer).
//
// ODDSound's pinned 0BSD client is vendored unmodified in deps/mts-esp/Client/
// with its licence and SHA-256 provenance (deps/mts-esp/README.vendor.md).
// The adapter also has an explicit no-client build branch, compile-tested
// alongside the shipping branch. Without a master, the processor's existing
// MIDI Tuning Standard SysEx path remains available.
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

    // True when the client library was compiled in.
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
