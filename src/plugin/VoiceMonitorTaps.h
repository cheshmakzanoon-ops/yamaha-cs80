#pragma once

// Engine-to-Studio voice monitor handoff. RankInfo is render-thread-only and
// MUST NOT be polled directly by the JUCE message/UI thread. This mailbox
// publishes the loudest active voice's two rank nodes from one audio callback.
// Each rank is a single lock-free 64-bit packet; the even/odd sequence keeps
// the two packets in the same rendered callback, without audio-thread locks.
// The values are reconstructed MODEL nodes, never measured hardware voltages.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace cs80
{
struct VoiceMonitorRank
{
    bool active = false;
    float ampEnvelope = 0.0f; // actual Engine::RankInfo normalized envelope
    float filterEnvelope = 0.0f;
    float hpfHz = 0.0f;       // effective model cutoff, including controls
    float lpfHz = 0.0f;
    bool operator==(const VoiceMonitorRank&) const = default;
};

struct VoiceMonitorSnapshot
{
    std::array<VoiceMonitorRank, 2> ranks {};
    bool operator==(const VoiceMonitorSnapshot&) const = default;
};

class VoiceMonitorTaps
{
public:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "voice monitor telemetry requires a lock-free 64-bit atomic");

    void publish(const VoiceMonitorSnapshot& snapshot) noexcept
    {
        // Sequential consistency makes the two-packet seqlock race-free under
        // the C++ memory model. The audio writer does four bounded atomics,
        // with no retry and no allocation; only the UI reader may retry.
        revision.fetch_add(1u, std::memory_order_seq_cst); // odd / publishing
        packets[0].store(pack(snapshot.ranks[0]), std::memory_order_seq_cst);
        packets[1].store(pack(snapshot.ranks[1]), std::memory_order_seq_cst);
        revision.fetch_add(1u, std::memory_order_seq_cst); // even / committed
    }

    VoiceMonitorSnapshot read() const noexcept
    {
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            const auto before = revision.load(std::memory_order_seq_cst);
            if ((before & 1u) != 0u) continue;
            const auto first = packets[0].load(std::memory_order_seq_cst);
            const auto second = packets[1].load(std::memory_order_seq_cst);
            if (before == revision.load(std::memory_order_seq_cst))
                return { { unpack(first), unpack(second) } };
        }
        return {}; // a contended UI frame may be dropped; DSP never waits
    }

    void reset() noexcept { publish({}); }

private:
    // One packet is [active:1][LPF Hz:15][HPF Hz:16][filter env:16][amp env:16].
    // Display resolution, not a DSP law: envelope <=~3.1e-5, cutoff <=1 Hz.
    static std::uint32_t quantise(float value, float lo, float hi, std::uint32_t max) noexcept
    {
        if (! std::isfinite(value)) value = lo;
        const float unit = (std::clamp(value, lo, hi) - lo) / (hi - lo);
        return (std::uint32_t) (unit * (float) max + 0.5f);
    }

    static std::uint64_t pack(const VoiceMonitorRank& r) noexcept
    {
        if (! r.active) return 0u;
        const auto amp = quantise(r.ampEnvelope, 0.0f, 1.0f, 65535u);
        const auto filter = quantise(r.filterEnvelope, -1.0f, 1.0f, 65535u);
        const auto hpf = quantise(r.hpfHz, 0.0f, 32767.0f, 65535u);
        const auto lpf = quantise(r.lpfHz, 0.0f, 32767.0f, 32767u);
        return (std::uint64_t{ 1 } << 63) | ((std::uint64_t) lpf << 48)
             | ((std::uint64_t) hpf << 32) | ((std::uint64_t) filter << 16) | amp;
    }

    static VoiceMonitorRank unpack(std::uint64_t value) noexcept
    {
        if ((value >> 63) == 0u) return {};
        return { true,
                 (float) (value & 0xffffu) / 65535.0f,
                 (float) ((value >> 16) & 0xffffu) * (2.0f / 65535.0f) - 1.0f,
                 (float) ((value >> 32) & 0xffffu) * (32767.0f / 65535.0f),
                 (float) ((value >> 48) & 0x7fffu) };
    }

    std::atomic<std::uint32_t> revision { 0u };
    std::array<std::atomic<std::uint64_t>, 2> packets {};
};
} // namespace cs80
