#pragma once

#include "experimental/MemoryControlFrame.h"
#include "reference/MemoryPanelPortCompiler.h"

namespace cs80::reference::memory_panel
{
// OFFLINE only. Electrical assumptions are explicit, not implied by the stored
// host controls or by the source support for 10k memory (not universal PANEL).
// Banks contain channel-local CP_count values, not packed 26-control storage:
// [0] the rank's first memory, [1] its second memory, [2] the manual panel.
inline bool compileControlFrame(int rank, int selector,
    const std::array<experimental::memory_control::ChannelValues, 3>& banks,
    const memory_panels::Electrical& electrical, experimental::MemoryControlFrame& result)
{
    if (rank < 0 || rank > 1 || selector < 11 || selector > 13) return false;
    Setup setup;
    setup.rank = rank; setup.selector = selector; setup.electrical = electrical;
    experimental::MemoryControlFrame next;
    next.electrical = electrical;
    for (std::size_t bank = 0; bank < banks.size(); ++bank)
    {
        experimental::memory_control::AdaptedBank adapted;
        if (!experimental::memory_control::adapt(banks[bank], adapted)) return false;
        setup.banks[bank] = adapted.positions;
        next.saturatedRows[bank] = adapted.saturatedRows;
    }
    if (!compileLoadedPortMap(setup, next.ports)) return false;
    next.selectedSwitches = memory_panels::switchStates(rank, selector, setup.banks[static_cast<std::size_t>(selector - 11)]);
    next.frameVersion = experimental::memoryControlFrameVersion;
    next.adapterVersion = experimental::memory_control::version;
    next.sourceModelVersion = source_factory::modelVersion;
    next.sourceHash = source_factory::sourceFingerprint;
    next.ready = true;
    result = next;
    return true;
}
} // namespace cs80::reference::memory_panel
