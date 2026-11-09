#pragma once

#include <cstdint>
#include <limits>

namespace cs80
{
// Event-time metadata only. A processor owns one stable source before both
// Engines, and accesses it solely on the render thread. It never changes DSP
// ages or resets with one instrument. Exhaustion disables qualification.
class ReleaseOrderClock
{
public:
    explicit ReleaseOrderClock(std::uint64_t initial = 0) noexcept : serial(initial) {}
    ReleaseOrderClock(const ReleaseOrderClock&) = delete;
    ReleaseOrderClock& operator=(const ReleaseOrderClock&) = delete;
    bool valid() const noexcept { return !exhausted; }
    std::uint64_t next() noexcept
    {
        if (serial == std::numeric_limits<std::uint64_t>::max())
        {
            exhausted = true;
            return 0;
        }
        return ++serial;
    }
private:
    std::uint64_t serial = 0;
    bool exhausted = false;
};
} // namespace cs80
