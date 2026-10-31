#pragma once

// MTS-ESP client shim (IMPROVEMENT_PLAN Phase 6, modern layer).
//
// ODDSound's MTS-ESP client (libMTSClient.h/.cpp, 0BSD licence per the
// ODDSound/MTS-ESP repository page) is NOT vendored: its source could not be
// fetched byte-exactly from this session. The shim compiles the client in
// automatically when the two files are placed at deps/mts-esp/Client/ (deps/
// is untracked; vendor them in-tree with their licence to ship). Without them
// it reports "not available" and the processor falls back to MIDI Tuning
// Standard SysEx, which needs no library (Engine::handleSysEx).
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
