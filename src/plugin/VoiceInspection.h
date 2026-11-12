#pragma once
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>

namespace cs80
{
// Read-only callback telemetry, not a parameter or saved state. One UI request
// owner and one audio/setup writer under the usual stopped-audio lifecycle.
struct VoiceInspectionReading
{
    std::uint64_t request = 0;
    bool ready = false, available = false, active = false, held = false, pedalHeld = false, rendered = false;
    int capacity = 8, note = -1, channel = 1;
    float amplitude = 0, filter = 0, oscillatorHz = 0, hpfHz = 0, lpfHz = 0;
    bool operator==(const VoiceInspectionReading&) const = default;
};
class VoiceInspection
{
public:
    static int slot(std::uint64_t q) noexcept { return int(q & 15); }
    static int rank(std::uint64_t q) noexcept { return int((q >> 4) & 1); }
    static bool enabled(std::uint64_t q) noexcept { return (q & 32) != 0; }
    bool request(int slotIndex, int rankIndex, bool on) noexcept
    {
        if (slotIndex < 0 || slotIndex > 15 || rankIndex < 0 || rankIndex > 1) return false;
        const auto old = requested();
        const auto fields = std::uint64_t(slotIndex | (rankIndex << 4) | (int(on) << 5));
        if ((old & 63) != fields) selection.store(((old & ~std::uint64_t(63)) + 64) | fields);
        return true;
    }
    std::uint64_t requested() const noexcept { return selection.load(); }
    void reset() noexcept { publish({}); }
    void publish(const VoiceInspectionReading& r) noexcept
    {
        // All payload words are atomic. Sequential consistency gives one total
        // order: a reader accepts only a complete, unchanged even generation.
        // Neither thread retries, waits, allocates or accesses an unowned buffer.
        sequence.fetch_add(1);
        identity.store(r.request);
        flags.store(unsigned(r.ready) | (unsigned(r.available) << 1) | (unsigned(r.active) << 2)
                    | (unsigned(r.held) << 3) | (unsigned(r.pedalHeld) << 4) | (unsigned(r.rendered) << 5)
                    | (unsigned(r.capacity == 16) << 6) | (unsigned(r.note + 1) << 7) | (unsigned(r.channel) << 15));
        const std::array values {r.amplitude,r.filter,r.oscillatorHz,r.hpfHz,r.lpfHz};
        for (size_t i=0;i<values.size();++i) payload[i].store(std::bit_cast<std::uint32_t>(values[i]));
        sequence.fetch_add(1);
    }
    VoiceInspectionReading read() const noexcept
    {
        const auto before = sequence.load();
        if (before & 1) return {};
        VoiceInspectionReading r;
        r.request = identity.load(); const auto f = flags.load();
        r.ready=f&1; r.available=f&2; r.active=f&4; r.held=f&8; r.pedalHeld=f&16; r.rendered=f&32;
        r.capacity=(f&64)?16:8; r.note=int((f>>7)&255)-1; r.channel=int((f>>15)&31);
        std::array<float*,5> values {&r.amplitude,&r.filter,&r.oscillatorHz,&r.hpfHz,&r.lpfHz};
        for (size_t i=0;i<values.size();++i) *values[i]=std::bit_cast<float>(payload[i].load());
        if (sequence.load()!=before || r.request!=requested()) return {};
        return r;
    }
private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free && std::atomic<std::uint32_t>::is_always_lock_free);
    std::atomic<std::uint64_t> selection {0}, sequence {0}, identity {0};
    std::atomic<std::uint32_t> flags {0};
    std::array<std::atomic<std::uint32_t>,5> payload {};
};
}
