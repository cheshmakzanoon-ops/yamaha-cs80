#pragma once

#include <atomic>
#include <cstdint>

namespace cs80
{
// One completed-block observation of audio-applied MPE state. Ranges are in
// semitones, rounded to 0.01 for display only; DSP values are never quantized.
struct MpeStatusReading
{
    bool ready = false, enabled = false;
    int lowerMembers = 0, upperMembers = 0;
    float lowerManagerRange = 0, lowerMemberRange = 0;
    float upperManagerRange = 0, upperMemberRange = 0;
    bool operator==(const MpeStatusReading&) const = default;
};

// Four 14-bit cent ranges plus two 4-bit member counts fit one atomic word.
// Counts 15/15 cannot occur in a valid negotiated layout, so reserve them for
// not-ready/disabled. No allocation, locks, retry loops, or shared Engine reads.
class MpeStatus
{
public:
    void reset() noexcept { packed.store(notReady, std::memory_order_relaxed); }
    void publish(bool enabled, int lowerMembers, int upperMembers,
                 float lowerManager, float lowerMember,
                 float upperManager, float upperMember) noexcept
    {
        if (!enabled)
        {
            packed.store(disabled, std::memory_order_relaxed);
            return;
        }
        const auto value = quantize(lowerManager) | (quantize(lowerMember) << 14)
            | (quantize(upperManager) << 28) | (quantize(upperMember) << 42)
            | (std::uint64_t(lowerMembers & 15) << 56)
            | (std::uint64_t(upperMembers & 15) << 60);
        packed.store(value, std::memory_order_relaxed);
    }
    MpeStatusReading read() const noexcept
    {
        const auto value = packed.load(std::memory_order_relaxed);
        if ((value & countMask) == countMask)
            return { value != notReady, false };
        const auto range = [value](int shift) noexcept
        { return float((value >> shift) & rangeMask) / 100.0f; };
        return { true, true, int((value >> 56) & 15), int(value >> 60),
                 range(0), range(14), range(28), range(42) };
    }
private:
    static constexpr std::uint64_t rangeMask = 0x3fffu;
    static constexpr std::uint64_t countMask = std::uint64_t(0xff) << 56;
    static constexpr std::uint64_t notReady = countMask, disabled = countMask | 1;
    static std::uint64_t quantize(float semitones) noexcept
    {
        // MIDI RPN can reach 127 + 127/100 = 128.27. Bound defensively,
        // including NaN, before integer conversion; the DSP state is untouched.
        if (!(semitones > 0)) return 0;
        if (semitones >= 128.27f) return 12827;
        return std::uint64_t(semitones * 100.0f + 0.5f);
    }
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "MPE status publication must remain lock-free");
    std::atomic<std::uint64_t> packed { notReady };
};
} // namespace cs80
