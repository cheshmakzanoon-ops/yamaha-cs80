#pragma once

// Engine-to-Studio voice monitor handoff. RankInfo is render-thread-only and
// MUST NOT be polled directly by the JUCE message/UI thread. This mailbox
// publishes the loudest active voice's two rank nodes from one audio callback.
// Each pair of IEEE754 model floats occupies one lock-free 64-bit packet;
// the even/odd sequence keeps both ranks in the same rendered callback.
// The values are reconstructed MODEL nodes, never measured hardware voltages.

#include <array>
#include <atomic>
#include <bit>
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
    static_assert(sizeof(float) == sizeof(std::uint32_t),
                  "voice monitor preserves exact 32-bit model node values");

    void publish(const VoiceMonitorSnapshot& snapshot) noexcept
    {
        // Sequential consistency makes the multi-packet seqlock race-free under
        // the C++ memory model. The audio writer performs bounded atomics,
        // with no retry and no allocation; only the UI reader may retry.
        revision.fetch_add(1u, std::memory_order_seq_cst); // odd / publishing
        const auto& a = snapshot.ranks[0];
        const auto& b = snapshot.ranks[1];
        activeMask.store((a.active ? 1u : 0u) | (b.active ? 2u : 0u), std::memory_order_seq_cst);
        envelopes[0].store(packFloats(a.ampEnvelope, a.filterEnvelope), std::memory_order_seq_cst);
        cutoffs[0].store(packFloats(a.hpfHz, a.lpfHz), std::memory_order_seq_cst);
        envelopes[1].store(packFloats(b.ampEnvelope, b.filterEnvelope), std::memory_order_seq_cst);
        cutoffs[1].store(packFloats(b.hpfHz, b.lpfHz), std::memory_order_seq_cst);
        revision.fetch_add(1u, std::memory_order_seq_cst); // even / committed
    }

    VoiceMonitorSnapshot read() const noexcept
    {
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            const auto before = revision.load(std::memory_order_seq_cst);
            if ((before & 1u) != 0u) continue;
            const auto activity = activeMask.load(std::memory_order_seq_cst);
            const auto firstEnvelopes = envelopes[0].load(std::memory_order_seq_cst);
            const auto firstCutoffs = cutoffs[0].load(std::memory_order_seq_cst);
            const auto secondEnvelopes = envelopes[1].load(std::memory_order_seq_cst);
            const auto secondCutoffs = cutoffs[1].load(std::memory_order_seq_cst);
            if (before == revision.load(std::memory_order_seq_cst))
                return { { unpack(firstEnvelopes, firstCutoffs, (activity & 1u) != 0u),
                           unpack(secondEnvelopes, secondCutoffs, (activity & 2u) != 0u) } };
        }
        return {}; // a contended UI frame may be dropped; DSP never waits
    }

    void reset() noexcept { publish({}); }

private:
    // Preserve the *actual* model floats, including signed filter-envelope
    // excursions beyond [-1,1]. Fixed-point clipping would falsify that tap.
    static float finite(float value) noexcept
    {
        return std::isfinite(value) ? value : 0.0f;
    }

    static std::uint64_t packFloats(float first, float second) noexcept
    {
        return (std::uint64_t) std::bit_cast<std::uint32_t>(finite(first))
             | ((std::uint64_t) std::bit_cast<std::uint32_t>(finite(second)) << 32);
    }

    static VoiceMonitorRank unpack(std::uint64_t envelopePair, std::uint64_t cutoffPair,
                                   bool active) noexcept
    {
        if (! active) return {};
        return { true,
                 std::bit_cast<float>((std::uint32_t) envelopePair),
                 std::bit_cast<float>((std::uint32_t) (envelopePair >> 32)),
                 std::bit_cast<float>((std::uint32_t) cutoffPair),
                 std::bit_cast<float>((std::uint32_t) (cutoffPair >> 32)) };
    }

    std::atomic<std::uint32_t> revision { 0u };
    std::atomic<std::uint32_t> activeMask { 0u };
    std::array<std::atomic<std::uint64_t>, 2> envelopes {};
    std::array<std::atomic<std::uint64_t>, 2> cutoffs {};
};
} // namespace cs80
