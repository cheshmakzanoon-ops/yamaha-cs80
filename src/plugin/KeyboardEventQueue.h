#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace cs80
{
// Multiple non-audio producers, one audio consumer. Only producers take mutex.
// Lifecycle calls are consumer operations and require stopped audio processing.
template <std::size_t Capacity = 256>
class KeyboardEventQueue
{
public:
    struct Event
    {
        std::uint32_t milliseconds = 0;
        std::uint32_t epoch = 0;
        int channel = 1, note = 0;
        float velocity = 0;
        bool on = false;
    };
    using Batch = std::array<Event, Capacity>;

    bool push(int channel, int note, float velocity, bool on, std::uint32_t milliseconds)
    {
        if (channel < 1 || channel > 16 || note < 0 || note > 127 || !std::isfinite(velocity))
            return false;
        const std::lock_guard<std::mutex> lock(producerMutex);
        const auto current = epoch.load(std::memory_order_acquire);
        if (acknowledged.load(std::memory_order_acquire) != current
            || overflow.load(std::memory_order_acquire)) return false;
        const auto w = write.load(std::memory_order_relaxed);
        if (w - read.load(std::memory_order_acquire) == Capacity)
        {
            overflow.store(true, std::memory_order_release);
            return false;
        }
        // Callers can sample time before contending for the producer mutex.
        // Clamp regressions (including >2^31 ms ambiguous jumps) in publication
        // order; a small unsigned forward delta still supports clock wrap.
        if (timestampEpoch == current
            && std::uint32_t(milliseconds - lastTimestamp) > 0x7fffffffu)
            milliseconds = lastTimestamp;
        lastTimestamp = milliseconds;
        timestampEpoch = current;
        events[w % Capacity] = { milliseconds, current, channel, note, velocity, on };
        write.store(w + 1, std::memory_order_release);
        return true;
    }

    bool hasOverflow() const noexcept { return overflow.load(std::memory_order_acquire); }
    std::uint32_t requestedEpoch() const noexcept { return epoch.load(std::memory_order_acquire); }
    bool needsAcknowledgment() const noexcept
    { return acknowledged.load(std::memory_order_acquire) != requestedEpoch(); }

    // Consumer only. Never resets producer indices or touches unpublished slots.
    void requestReset() noexcept
    {
        // Capture the discard boundary BEFORE advertising the new epoch: a fast
        // UI reset/ack must not let a newly accepted event enter that boundary.
        const auto oldEnd = write.load(std::memory_order_acquire);
        epoch.fetch_add(1, std::memory_order_acq_rel);
        read.store(oldEnd, std::memory_order_release);
    }

    // Non-audio only, AFTER the UI keyboard state has been cleared for this epoch.
    void acknowledge(std::uint32_t clearedEpoch)
    {
        const std::lock_guard<std::mutex> lock(producerMutex);
        if (requestedEpoch() != clearedEpoch) return;
        overflow.store(false, std::memory_order_release);
        acknowledged.store(clearedEpoch, std::memory_order_release);
    }

    std::size_t take(Batch& destination) noexcept
    {
        auto r = read.load(std::memory_order_relaxed);
        const auto end = write.load(std::memory_order_acquire);
        const auto current = requestedEpoch();
        std::size_t count = 0;
        while (r != end)
        {
            const auto event = events[r % Capacity];
            if (event.epoch == current) destination[count++] = event;
            ++r;
        }
        read.store(r, std::memory_order_release);
        return count;
    }

    // JUCE 8.0.6 stretches indirect events over first..last+1 milliseconds.
    // Unsigned deltas keep a batch crossing the 32-bit millisecond wrap ordered.
    static int samplePosition(const Batch& batch, std::size_t count,
                              std::size_t index, int frames) noexcept
    {
        if (frames <= 0 || count == 0) return 0;
        const auto first = batch[0].milliseconds;
        const double span = double(std::uint32_t(batch[count - 1].milliseconds - first)) + 1.0;
        const double elapsed = double(std::uint32_t(batch[index].milliseconds - first));
        const double rounded = std::floor(elapsed * double(frames) / span + 0.5);
        return int(std::clamp(rounded, 0.0, double(frames - 1)));
    }

private:
    static_assert(Capacity > 0);
    static_assert(std::atomic<std::size_t>::is_always_lock_free
                  && std::atomic<std::uint32_t>::is_always_lock_free
                  && std::atomic<bool>::is_always_lock_free,
                  "Keyboard audio consumer requires lock-free index/epoch atomics");
    std::array<Event, Capacity> events {};
    std::mutex producerMutex;
    std::uint32_t lastTimestamp = 0, timestampEpoch = 0; // Producer mutex only.
    std::atomic<std::size_t> read { 0 }, write { 0 };
    std::atomic<std::uint32_t> epoch { 1 }, acknowledged { 1 };
    std::atomic<bool> overflow { false };
};
} // namespace cs80
