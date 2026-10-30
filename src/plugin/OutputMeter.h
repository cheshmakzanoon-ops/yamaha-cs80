#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <limits>

namespace cs80
{
struct OutputMeterReading
{
    float samplePeak = 0.0f; // Finite absolute sample peak, not true peak or loudness.
    bool overRange = false; // At least one finite sample exceeded |1|; no clipping claim.
    bool nonFinite = false; // NaN/Inf observed separately, never mixed into samplePeak.
};

// Local audio-callback accumulator. Only read samples already mapped to actual
// host output channels. This helper never modifies audio, normalizes or limits.
class OutputMeterBlock
{
public:
    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559,
                  "Output metering requires 32-bit IEEE754 float samples");

    void addSample(float sample) noexcept
    {
        // Positive IEEE754 magnitude bits sort identically to finite values.
        // Integer classification/max also preserves subnormal samples while the
        // audio callback uses FTZ/DAZ, and never performs arithmetic on NaN/Inf.
        const auto magnitude = std::bit_cast<std::uint32_t>(sample) & 0x7fffffffu;
        if (magnitude < 0x7f800000u)
            peakBits = std::max(peakBits, magnitude);
        else
            invalid = true;
    }

    void addSamples(const float* samples, int count) noexcept
    {
        if (samples == nullptr) return;
        for (int sample = 0; sample < count; ++sample) addSample(samples[sample]);
    }

    OutputMeterReading reading() const noexcept
    { return { std::bit_cast<float>(peakBits), peakBits > 0x3f800000u, invalid }; }

private:
    friend class OutputMeter;
    std::uint32_t peakBits = 0u;
    bool invalid = false;
};

namespace detail
{
// Factored only for deterministic interleaving tests. Production passes the
// lock-free atomic<uint32_t> below; magnitude bits are finite/nonnegative.
// Exactly one producer may publish nonzero values; consumers only exchange to 0.
template <typename PeakAtomic>
void publishOutputPeakBits(PeakAtomic& mailbox, std::uint32_t peak) noexcept
{
    if (peak == 0u) return;
    std::uint32_t previous = mailbox.load(std::memory_order_relaxed);
    const auto maximum = std::max(previous, peak);
    // Validate even a non-increasing positive peak. A relaxed load may observe
    // an older maximum after a consumer clear; trusting it and skipping the CAS
    // could otherwise discard a smaller but new post-clear observation.
    if (!mailbox.compare_exchange_strong(previous, maximum, std::memory_order_relaxed))
    {
        // A strong CAS cannot fail spuriously. With one producer, its only
        // competing write is a consumer clearing to zero. No later producer
        // value can be overwritten, so one store preserves this publication.
        // A consumer during/after that store either receives it now or leaves
        // it pending for the next read. No retry/spin loop is required.
        mailbox.store(peak, std::memory_order_relaxed);
    }
}
} // namespace detail

// Single audio producer / UI consumer telemetry, not a coherent audio snapshot.
// Peak and event bits are consumed independently: a publication overlapping a
// poll may put its peak and flags in adjacent polls. Neither controls the DSP.
// The UI should also infer over-range from samplePeak > 1 and own wall-time
// decay/hold. With no polling, finite peaks and event flags remain accumulated.
class OutputMeter
{
public:
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
                  "Output metering requires lock-free peak/event atomics on this target");

    void publish(const OutputMeterBlock& block) noexcept
    {
        detail::publishOutputPeakBits(pendingPeak, block.peakBits);
        const std::uint32_t flags = (block.peakBits > 0x3f800000u ? overRangeBit : 0u)
                                  | (block.invalid ? nonFiniteBit : 0u);
        if (flags != 0) pendingEvents.fetch_or(flags, std::memory_order_relaxed);
    }

    OutputMeterReading consume() noexcept
    {
        const auto peak = pendingPeak.exchange(0u, std::memory_order_relaxed);
        const auto flags = pendingEvents.exchange(0u, std::memory_order_relaxed);
        return { std::bit_cast<float>(peak), (flags & overRangeBit) != 0, (flags & nonFiniteBit) != 0 };
    }

    // prepare/release only, with no concurrent audio publication. Opening an
    // editor instead consumes/discards once: an in-flight publication may then
    // appear at the next poll, and post-clear publications remain observable.
    void reset() noexcept
    {
        pendingPeak.store(0u, std::memory_order_relaxed);
        pendingEvents.store(0u, std::memory_order_relaxed);
    }

private:
    static constexpr std::uint32_t overRangeBit = 1u, nonFiniteBit = 2u;
    std::atomic<std::uint32_t> pendingPeak { 0u };
    std::atomic<std::uint32_t> pendingEvents { 0u };
};
} // namespace cs80
