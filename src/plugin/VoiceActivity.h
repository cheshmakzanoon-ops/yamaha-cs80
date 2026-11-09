#pragma once

#include <atomic>
#include <bit>
#include <cstdint>

namespace cs80
{
// One completed-block observation. Bits 0..7 are the original instrument,
// 8..15 the optional extension. Capacity is audio-applied, not an APVTS request.
struct VoiceActivityReading
{
    std::uint32_t mask = 0;
    int capacity = 8;
    int count() const noexcept { return std::popcount(mask); }
    bool active(int slot) const noexcept
    { return slot >= 0 && slot < 16 && slot < capacity && (mask & (1u << slot)) != 0; }
    bool operator==(const VoiceActivityReading&) const = default;
};

// Presentation-only. Publish capacity and activity in ONE lock-free value so a
// UI poll cannot combine a sixteen-voice mask with an eight-voice denominator.
class VoiceActivity
{
public:
    void publish(unsigned primaryMask, unsigned extensionMask, bool extensionActive) noexcept
    {
        const auto value = (primaryMask & 0xffu)
            | (extensionActive ? ((extensionMask & 0xffu) << 8) | extensionBit : 0u);
        packed.store(value, std::memory_order_relaxed);
    }
    void reset() noexcept { packed.store(0, std::memory_order_relaxed); }
    VoiceActivityReading read() const noexcept
    {
        const auto value = packed.load(std::memory_order_relaxed);
        return { value & 0xffffu, (value & extensionBit) != 0 ? 16 : 8 };
    }
private:
    static constexpr std::uint32_t extensionBit = 1u << 16;
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
                  "Voice activity publication must remain lock-free");
    std::atomic<std::uint32_t> packed { 0 };
};
} // namespace cs80
