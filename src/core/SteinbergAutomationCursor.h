#pragma once

// Steinberg VST3's IParamValueQueue software contract, without depending on
// the host SDK or JUCE. The producer adapts getPoint() into a non-owning
// PointReader, valid for one process() callback. A cursor streams the queue
// monotonically in O(points + host samples) time and O(1) storage. This is
// only curve arithmetic; no Engine or APVTS state is changed by this header.
//
// Truth model: tests/parity/tolerances-v2/steinberg-automation-cursor-v1.json.
// The implicit previous point lies at host sample -1, per Steinberg's
// pluginterfaces/vst/ivstparameterchanges.h IParamValueQueue description.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace cs80::vst3_automation
{

struct Point
{
    int sampleOffset = 0;
    double normalized = 0.0;
};

struct PointReader
{
    const void* context = nullptr;
    int count = 0;
    bool (*read)(const void* context, int index, Point& result) noexcept = nullptr;
};

enum class Status : std::uint8_t
{
    ok,
    invalidQueue,
    invalidPoint,
    invalidOrder,
    invalidSample
};

enum class Key : std::uint8_t
{
    volume,
    brilliance,
    resonance
};

// The three key CS-80 globals have JUCE NormalisableRange skew=1 in
// src/core/Params.h, so their parameter-unit conversions are affine.
constexpr double normalizedToCore(Key key, double normalized) noexcept
{
    const double value = std::clamp(normalized, 0.0, 1.0);
    switch (key)
    {
        case Key::volume: return value;
        case Key::brilliance: return -2.0 + 4.0 * value;
        case Key::resonance: return value;
    }
    return value;
}

class PiecewiseLinearCursor final
{
public:
    // No allocation, copying, ownership transfer or exception. An empty
    // queue holds the parameter value from the previous host sample.
    void reset(PointReader points, double previousNormalized, int hostBlockFrames) noexcept
    {
        reader = points;
        blockFrames = hostBlockFrames;
        nextIndex = 0;
        lastSample = -1;
        previous = { -1, previousNormalized };
        next = {};
        hasNext = false;
        state = Status::ok;

        // One point per frame is sufficient for every legal linear section
        // and every one-sample jump. Repeated-offset queues are also accepted
        // when their total count fits the bounded host frame count.
        if (hostBlockFrames <= 0 || points.count < 0 || points.count > hostBlockFrames
            || (points.count > 0 && (points.context == nullptr || points.read == nullptr)))
        {
            state = Status::invalidQueue;
            return;
        }

        if (! std::isfinite(previousNormalized) || previousNormalized < 0.0 || previousNormalized > 1.0)
        {
            state = Status::invalidPoint;
            return;
        }

        readNext();
    }

    double sampleAt(int sampleOffset) noexcept
    {
        if (state != Status::ok)
            return previous.normalized;

        if (sampleOffset < 0 || sampleOffset >= blockFrames || sampleOffset < lastSample)
        {
            state = Status::invalidSample;
            return previous.normalized;
        }

        lastSample = sampleOffset;

        // Points at/before the requested sample are applied in queue order.
        // Equal offsets are a declared inference: last one wins, with no
        // divide-by-zero. Other equal-offset interpretations are preregistered.
        while (hasNext && next.sampleOffset <= sampleOffset)
        {
            previous = next;
            readNext();
            if (state != Status::ok)
                return previous.normalized;
        }

        if (! hasNext)
            return previous.normalized;

        const int distance = next.sampleOffset - previous.sampleOffset;
        if (distance <= 0)
        {
            state = Status::invalidOrder;
            return previous.normalized;
        }

        const double fraction = (double) (sampleOffset - previous.sampleOffset) / (double) distance;
        return previous.normalized + (next.normalized - previous.normalized) * fraction;
    }

    Status status() const noexcept { return state; }
    int readerCalls() const noexcept { return nextIndex; }

private:
    void readNext() noexcept
    {
        if (nextIndex == reader.count)
        {
            hasNext = false;
            return;
        }

        Point candidate {};
        if (reader.read == nullptr || ! reader.read(reader.context, nextIndex, candidate))
        {
            state = Status::invalidPoint;
            hasNext = false;
            return;
        }

        ++nextIndex;
        if (candidate.sampleOffset < previous.sampleOffset)
        {
            state = Status::invalidOrder;
            hasNext = false;
            return;
        }
        if (candidate.sampleOffset < 0 || candidate.sampleOffset >= blockFrames
            || ! std::isfinite(candidate.normalized)
            || candidate.normalized < 0.0 || candidate.normalized > 1.0)
        {
            state = Status::invalidPoint;
            hasNext = false;
            return;
        }

        next = candidate;
        hasNext = true;
    }

    PointReader reader {};
    Point previous { -1, 0.0 };
    Point next {};
    int blockFrames = 0;
    int nextIndex = 0;
    int lastSample = -1;
    Status state = Status::invalidQueue;
    bool hasNext = false;
};

static_assert(std::is_trivially_copyable_v<PiecewiseLinearCursor>);
static_assert(sizeof(PiecewiseLinearCursor) <= 96);

} // namespace cs80::vst3_automation
