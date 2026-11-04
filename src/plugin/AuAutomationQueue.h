#pragma once

#include "HostAutomationBridge.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace cs80
{
// Native Intel evidence: docs/evidence/continuation-2026-10-31/au-intel-endpoint.
// AU's normalized endpoint and JUCE's physical Float32 getter are different
// representations. Keep a coherent pair, with one render-thread publisher and
// arbitrary readers/invalidators. An edit during publication advances revision,
// so an older publisher cannot reclaim ownership after that edit's notification.
class AuNormalizedPublication
{
public:
    using Token = std::uint64_t;
    Token begin() noexcept { return revision.fetch_add(1) + 1; }
    void publish(Token token, float normalized, float readback) noexcept
    {
        values.store((std::uint64_t(std::bit_cast<std::uint32_t>(normalized)) << 32)
            | std::bit_cast<std::uint32_t>(readback));
        committedRevision.store(token);
    }
    void invalidate() noexcept { revision.fetch_add(1); }
    float read(float current) const noexcept
    {
        const auto before = revision.load();
        if (committedRevision.load() != before) return current;
        const auto pair = values.load();
        if (revision.load() != before) return current;
        const auto readback = std::bit_cast<float>(static_cast<std::uint32_t>(pair));
        return readback == current ? std::bit_cast<float>(static_cast<std::uint32_t>(pair >> 32)) : current;
    }
private:
    static_assert(std::atomic<Token>::is_always_lock_free);
    // Sequentially consistent revision checks bracket one indivisible pair.
    // No callback acquires a lock, waits, or allocates.
    std::atomic<Token> revision {0}, committedRevision {std::numeric_limits<Token>::max()}, values {0};
};

// Audio Unit v2 scheduled parameters apply to the current render only:
// https://developer.apple.com/documentation/audiotoolbox/audiounitscheduleparameters(_:_:_:)
// Preallocated storage lets ScheduleParameter report a capacity error before
// accepting an event. Sorting and conversion allocate nothing on the RT path.
class AuAutomationQueue
{
public:
    static constexpr size_t capacity = 2048;
    static constexpr size_t lanes = kSampleAccurateParameters.size();
    struct Event
    {
        int lane = 0, start = 0, duration = 0;
        double first = 0, last = 0;
        bool ramp = false;
        size_t order = 0;
    };
    bool valid(const Event& event) const noexcept
    {
        const auto end = std::int64_t(event.start) + event.duration;
        return event.lane >= 0 && event.lane < static_cast<int>(lanes)
            && (event.ramp || event.start >= 0) && event.duration >= 0
            && end >= std::numeric_limits<int>::min() && end <= std::numeric_limits<int>::max()
            && std::isfinite(event.first) && std::isfinite(event.last);
    }
    size_t available() const noexcept { return capacity - pendingCount; }
    bool add(Event event) noexcept
    {
        if (!valid(event) || pendingCount == capacity) return false;
        event.first = std::clamp(event.first, 0.0, 1.0);
        event.last = std::clamp(event.last, 0.0, 1.0);
        event.order = pendingCount;
        pending[pendingCount++] = event;
        return true;
    }
    void clearPending() noexcept { pendingCount = 0; }
    template<class Function> void forEachPending(Function&& callback) const noexcept
    { for (size_t i = 0; i < pendingCount; ++i) callback(pending[i]); }

    bool prepare(const std::array<double, lanes>& initial, int frames) noexcept
    {
        for (auto& lane : ready) lane = {};
        pointCount = 0;
        for (size_t i = 0; i < pendingCount; ++i)
            if (frames <= 0 || pending[i].start >= frames) { clearPending(); return false; }
        std::sort(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(pendingCount),
            [](const Event& a, const Event& b) {
                if (a.lane != b.lane) return a.lane < b.lane;
                if (a.start != b.start) return a.start < b.start;
                return a.order < b.order;
            });
        size_t eventIndex = 0;
        for (size_t laneIndex = 0; laneIndex < lanes; ++laneIndex)
        {
            const size_t firstPoint = pointCount;
            auto& lane = ready[laneIndex];
            lane.owner = this;
            lane.first = firstPoint;
            const auto valueAt = [&](int position) {
                AutomationPoint previous {-1, initial[laneIndex]};
                for (size_t i = firstPoint; i < pointCount; ++i)
                {
                    const auto point = points[i];
                    if (point.position > position)
                    {
                        const auto weight = double(position - previous.position) / double(point.position - previous.position);
                        return previous.value * (1.0 - weight) + point.value * weight;
                    }
                    previous = point;
                }
                return previous.value;
            };
            const auto append = [&](int position, double value) {
                if (pointCount > firstPoint && points[pointCount - 1].position == position)
                    points[pointCount - 1].value = value;
                else points[pointCount++] = {position, value};
            };
            // AU SetParameter(offset=0) and UI/state changes are already the
            // current value. An explicit first point prevents interpolation
            // from a stale previous-render value before a later immediate event.
            if (eventIndex < pendingCount && pending[eventIndex].lane == static_cast<int>(laneIndex))
                append(0, initial[laneIndex]);
            while (eventIndex < pendingCount && pending[eventIndex].lane == static_cast<int>(laneIndex))
            {
                const auto event = pending[eventIndex++];
                const int start = std::max(0, event.start);
                const int end = event.start + event.duration;
                const double old = valueAt(start - 1);
                while (pointCount > firstPoint && points[pointCount - 1].position >= start) --pointCount;
                if (start > 0) append(start - 1, old);
                if (!event.ramp || event.duration == 0 || end <= 0)
                    append(start, event.last);
                else
                {
                    const double atStart = event.first + (event.last - event.first)
                        * (double(start) - event.start) / event.duration;
                    append(start, atStart);
                    append(end, event.last);
                }
            }
            // A ramp can extend past this render, but it must publish the
            // current render's final value. The host reschedules its progress.
            if (pointCount > firstPoint && points[pointCount - 1].position >= frames)
            {
                const double last = valueAt(frames - 1);
                while (pointCount > firstPoint && points[pointCount - 1].position >= frames - 1) --pointCount;
                append(frames - 1, last);
            }
            lane.count = pointCount - firstPoint;
        }
        clearPending();
        return true;
    }
    HostAutomationQueue queue(size_t index) noexcept
    {
        auto& lane = ready[index];
        return {&lane, static_cast<int>(lane.count),
            [](void* context, int i, int& position, double& value) noexcept {
                const auto& source = *static_cast<const Lane*>(context);
                if (i < 0 || static_cast<size_t>(i) >= source.count) return false;
                const auto point = source.owner->points[source.first + static_cast<size_t>(i)];
                position = point.position; value = point.value; return true;
            }, nullptr, nullptr};
    }
private:
    struct AutomationPoint { int position = 0; double value = 0; };
    struct Lane { const AuAutomationQueue* owner = nullptr; size_t first = 0, count = 0; };
    std::array<Event, capacity> pending {};
    std::array<AutomationPoint, capacity * 3 + lanes> points {};
    std::array<Lane, lanes> ready {};
    size_t pendingCount = 0, pointCount = 0;
};
} // namespace cs80
