#pragma once

// MTS-ESP client shim (IMPROVEMENT_PLAN Phase 6, modern layer).
//
// ODDSound's 0BSD client is pinned byte-exactly in src/vendor/mts-esp with its
// licence and provenance. A live master and the upstream shared library must
// be installed separately. The adapter does not install or configure either.
//
// Register/deregister off the audio thread, then query a channel-independent
// or channel-aware tuning tables on the audio thread. MIDI Tuning Standard SysEx stays in the
// existing validated Engine::handleSysEx path, not the client's second parser.

#include "core/Tuning.h"

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
    // Audio thread. Fills all 128 notes from the master. An observed disconnect
    // or unsafe mapped Hz leaves the table unchanged. Source reads are not an
    // atomic snapshot of concurrent master edits or rapid reconnects.
    bool fillTable(tuning::TuningTable& table) const noexcept;
    using ChannelTables = std::array<tuning::TuningTable, 16>;
    // Complete transactional bank. Independent mode preserves upstream's
    // recommended global tuning while MPE is enabled.
    bool fillChannelTables(ChannelTables& tables, bool channelIndependent = false) const noexcept;

private:
    void* client = nullptr;
};
} // namespace cs80
