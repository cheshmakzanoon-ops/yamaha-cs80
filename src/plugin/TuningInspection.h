#pragma once

#include <atomic>
#include <bit>
#include <cstdint>

namespace cs80
{
// Table pitch in fractional MIDI semitones, before master/rank/bend offsets.
// Unmapped notes still have a table value, but are not admitted for playback.
struct TuningInspectionReading
{
    bool ready = false;
    int note = 69, channel = 1, mode = 0;
    bool mapped = false;
    float pitch = 0;
    bool operator==(const TuningInspectionReading&) const = default;
};

// The UI owns the request; the audio callback publishes one complete response.
// Echoed selection allows the UI to distinguish pending from answered requests.
// Neither side locks, allocates, retries, or reads shared mutable Engine state.
class TuningInspection
{
public:
    struct Selection { int note, channel; };
    bool request(int note, int channel) noexcept
    {
        if (note < 0 || note > 127 || channel < 1 || channel > 16) return false;
        selection.store(unsigned(note) | (unsigned(channel - 1) << 7), std::memory_order_relaxed);
        return true;
    }
    Selection requested() const noexcept
    {
        const auto value = selection.load(std::memory_order_relaxed);
        return { int(value & 127), int((value >> 7) & 15) + 1 };
    }
    // Lifecycle invalidation retains the selection for the next callback.
    void reset() noexcept { packed.store(0, std::memory_order_relaxed); }
    void publish(int note, int channel, int mode, bool mapped, float pitch) noexcept
    {
        const auto value = std::uint64_t(std::bit_cast<std::uint32_t>(pitch))
            | (std::uint64_t(note & 127) << 32)
            | (std::uint64_t((channel - 1) & 15) << 39)
            | (std::uint64_t(mode & 3) << 43)
            | (std::uint64_t(mapped) << 45) | readyMask;
        packed.store(value, std::memory_order_relaxed);
    }
    TuningInspectionReading read() const noexcept
    {
        const auto value = packed.load(std::memory_order_relaxed);
        if ((value & readyMask) == 0) return {};
        return { true, int((value >> 32) & 127), int((value >> 39) & 15) + 1,
                 int((value >> 43) & 3), bool((value >> 45) & 1),
                 std::bit_cast<float>(std::uint32_t(value)) };
    }
private:
    static constexpr std::uint64_t readyMask = std::uint64_t(1) << 46;
    static_assert(std::atomic<unsigned>::is_always_lock_free
                  && std::atomic<std::uint64_t>::is_always_lock_free,
                  "Tuning inspection must remain lock-free");
    std::atomic<unsigned> selection { 69 };
    std::atomic<std::uint64_t> packed { 0 };
};
} // namespace cs80
