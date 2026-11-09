#pragma once

#include "experimental/MemoryControlAdapter.h"
#include "experimental/MemoryPanelPortMap.h"

namespace cs80::experimental
{
inline constexpr std::uint32_t memoryControlFrameVersion = 1;
// Complete preparation product. No Engine/API/state route consumes it yet.
// Publish immutable complete instances outside the callback in a future route.
struct MemoryControlFrame
{
    MemoryPanelPortMap ports;
    memory_panels::Electrical electrical;
    memory_panels::Switches selectedSwitches;
    std::array<std::uint32_t, 3> saturatedRows {};
    std::uint32_t frameVersion = 0;
    std::uint32_t adapterVersion = 0;
    std::uint32_t sourceModelVersion = 0;
    std::uint64_t sourceHash = 0;
    bool ready = false;
};
} // namespace cs80::experimental
