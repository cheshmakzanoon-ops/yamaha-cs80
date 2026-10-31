#pragma once

#include "core/Params.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace juce { class AudioProcessorParameter; }

namespace cs80
{
// Host protocol, not a circuit law. Steinberg's IParamValueQueue specifies
// linear normalized segments, including the prior value at sample -1:
// https://steinbergmedia.github.io/vst3_doc/vstinterfaces/classSteinberg_1_1Vst_1_1IParamValueQueue.html
inline constexpr std::array kSampleAccurateParameters {
    int(P_volume), int(P_mix), int(P_brilliance), int(P_resonance), int(P_masterTune), int(P_detune),
    chParam(0, CP_hpfCutoff), chParam(0, CP_hpfRes), chParam(0, CP_lpfCutoff),
    chParam(0, CP_lpfRes), chParam(0, CP_vLevel), chParam(0, CP_pw),
    chParam(1, CP_hpfCutoff), chParam(1, CP_hpfRes), chParam(1, CP_lpfCutoff),
    chParam(1, CP_lpfRes), chParam(1, CP_vLevel), chParam(1, CP_pw)
};

// The wrapper owns the queue for the entire synchronous process call. Borrow
// its points instead of imposing a fixed event-count limit or allocating on
// the audio thread. The number of parameter lanes is fixed by the enum list.
struct HostAutomationQueue
{
    void* context = nullptr;
    int count = 0;
    bool (*readPoint)(void*, int, int&, double&) noexcept = nullptr;
    void* parameter = nullptr;
    void (*commitParameter)(void*, float) noexcept = nullptr;
};

class HostAutomationCurve
{
public:
    void begin(HostAutomationQueue source, double initial) noexcept
    {
        queue = source;
        previousPosition = -1;
        previousValue = std::clamp(initial, 0.0, 1.0);
        nextIndex = 0;
        haveNext = false;
        readNext();
    }

    bool active() const noexcept { return queue.readPoint != nullptr && queue.count > 0; }

    double valueAt(int position) noexcept
    {
        while (haveNext && nextPosition <= position)
        {
            previousPosition = nextPosition;
            previousValue = nextValue;
            readNext();
        }
        if (!haveNext) return previousValue;
        const auto fraction = double(position - previousPosition) / double(nextPosition - previousPosition);
        return previousValue + (nextValue - previousValue) * fraction;
    }

    // Call valueAt(position) first. Constant spans can remain one engine render;
    // nonzero slopes advance at the host sample rate, before the DSP smoother.
    int samplesUntilChange(int position, int remaining) const noexcept
    {
        if (!haveNext) return remaining;
        return nextValue != previousValue ? 1 : std::max(1, std::min(remaining, nextPosition - position));
    }

    double finalValue() const noexcept
    {
        for (int i = queue.count; i-- > 0;)
        {
            int position = 0;
            double value = 0.0;
            if (queue.readPoint(queue.context, i, position, value) && position >= 0 && std::isfinite(value))
                return std::clamp(value, 0.0, 1.0);
        }
        return previousValue;
    }

    void commit() const noexcept
    {
        if (queue.commitParameter != nullptr)
            queue.commitParameter(queue.parameter, static_cast<float>(finalValue()));
    }

private:
    void readNext() noexcept
    {
        haveNext = false;
        while (queue.readPoint != nullptr && nextIndex < queue.count)
        {
            int position = 0;
            double value = 0.0;
            const bool valid = queue.readPoint(queue.context, nextIndex++, position, value);
            if (!valid || position < 0 || position < previousPosition || !std::isfinite(value)) continue;
            nextPosition = position;
            nextValue = std::clamp(value, 0.0, 1.0);
            haveNext = true;
            return;
        }
    }

    HostAutomationQueue queue;
    int nextIndex = 0, previousPosition = -1, nextPosition = 0;
    double previousValue = 0.0, nextValue = 0.0;
    bool haveNext = false;
};

class HostAutomationClient
{
public:
    virtual ~HostAutomationClient() = default;
    virtual void beginHostAutomationBlock(int frameCount) noexcept = 0;
    virtual bool attachHostAutomationQueue(juce::AudioProcessorParameter&, HostAutomationQueue) noexcept = 0;
    virtual void endHostAutomationBlock() noexcept = 0;
};

// Also flushes the last values on wrapper early returns and suspended renders.
// A zero-frame parameter flush keeps the original JUCE path.
class HostAutomationBlock
{
public:
    HostAutomationBlock(HostAutomationClient* candidate, int frames) noexcept
        : client(frames > 0 ? candidate : nullptr)
    { if (client != nullptr) client->beginHostAutomationBlock(frames); }
    ~HostAutomationBlock() { if (client != nullptr) client->endHostAutomationBlock(); }
    HostAutomationClient* get() const noexcept { return client; }
    HostAutomationBlock(const HostAutomationBlock&) = delete;
    HostAutomationBlock& operator=(const HostAutomationBlock&) = delete;
private:
    HostAutomationClient* client = nullptr;
};
} // namespace cs80
