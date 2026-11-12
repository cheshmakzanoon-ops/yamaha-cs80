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
    chParam(1, CP_lpfRes), chParam(1, CP_vLevel), chParam(1, CP_pw),
    // Append capability lanes so the original eighteen keep their indices.
    chParam(0, CP_pan), chParam(1, CP_pan),
    // Original-panel SUB controls: append without moving existing lanes.
    int(P_subOriginalSpeed), int(P_subVco),
    // Complete the three continuous original SUB depth controls.
    int(P_subVcf), int(P_subVca),
    // Existing global touch amounts; keep the complete 24-lane prefix stable.
    int(P_subAtSpeed), int(P_subAtVco), int(P_subAtVcf),
    // Compatibility-mode SUB speed, distinct from original-panel speed.
    int(P_subSpeed),
    // Existing global ring controls; retain every preceding capability lane.
    int(P_ringSpeed), int(P_ringAmount),
    // Existing global envelope depth; preserve the complete 30-lane prefix.
    int(P_ringDepth),
    // Existing trigger-latched times; do not reshape an active ring envelope.
    int(P_ringAttack), int(P_ringDecay),
    // Existing global chorus controls; retain the complete 33-lane prefix.
    int(P_chorusSpeed), int(P_chorusDepth),
    // Existing global glide time; preserve the complete 35-lane prefix.
    int(P_portamento),
    // Existing global performance release; retain the 36-lane prefix.
    int(P_masterSustain),
    // Existing smoothed initial-bend amount; retain all 37 previous lanes.
    int(P_initBend),
    // Existing smoothed LOW level contour; preserve the 38-lane prefix.
    int(P_kbdLevelLow),
    // Existing HIGH level contour; retain the complete 39-lane prefix.
    int(P_kbdLevelHigh),
    // Existing LOW/HIGH brilliance contours; retain the complete 40-lane prefix.
    int(P_kbdBrillianceLow), int(P_kbdBrillianceHigh),
    // Existing rank INITIAL TOUCH amounts; preserve source authority and 42-lane prefix.
    chParam(0, CP_initBrill), chParam(0, CP_initLevel),
    chParam(1, CP_initBrill), chParam(1, CP_initLevel),
    // Existing rank AFTER TOUCH amounts; preserve source authority and 46-lane prefix.
    chParam(0, CP_afterBrill), chParam(0, CP_afterLevel),
    chParam(1, CP_afterBrill), chParam(1, CP_afterLevel)
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
        // A malformed APVTS/host readback may be NaN or infinite before the
        // first queue point. std::clamp alone does not reject NaN, and must
        // not leak a non-finite initial value into sample interpolation.
        previousValue = std::isfinite(initial) ? std::clamp(initial, 0.0, 1.0) : 0.0;
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
        // Widen before subtracting: an out-of-range host knot at INT_MAX
        // follows the implicit previous position -1, whose difference does
        // not fit in int. All ordinary offsets retain the exact same ratio.
        const auto fraction = (double(position) - double(previousPosition))
                            / (double(nextPosition) - double(previousPosition));
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
        // Complete the same accepted-knot traversal on a copy: a regressing
        // point rejected during rendering must not reappear at publication.
        // The live cursor stays untouched, including on early block exits.
        auto endpoint = *this;
        return endpoint.valueAt(std::numeric_limits<int>::max());
    }

    void commit() const noexcept
    {
        if (queue.commitParameter != nullptr)
            commitFinalValue(finalValue());
    }

    // Publish an already computed endpoint without walking the unread suffix
    // again. The caller can store its full precision before the notification.
    void commitFinalValue(double value) const noexcept
    {
        if (queue.commitParameter != nullptr)
            queue.commitParameter(queue.parameter, static_cast<float>(value));
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
    virtual bool usesHostAutomationTiming() const noexcept = 0;
    virtual int sampleAccurateParameterIndex(const juce::AudioProcessorParameter&) const noexcept = 0;
    virtual double sampleAccurateParameterValue(size_t lane) const noexcept = 0;
    virtual void beginHostAutomationBlock(int frameCount) noexcept = 0;
    // VST3 commits final points in protected policies; AU has different scheduling rules.
    virtual void beginVst3HostAutomationBlock(int frameCount) noexcept
    { beginHostAutomationBlock(frameCount); }
    virtual bool attachHostAutomationQueue(juce::AudioProcessorParameter&, HostAutomationQueue) noexcept = 0;
    virtual void endHostAutomationBlock() noexcept = 0;
};

// Also flushes the last values on wrapper early returns and suspended renders.
// A zero-frame parameter flush keeps the original JUCE path.
class HostAutomationBlock
{
public:
    HostAutomationBlock(HostAutomationClient* candidate, int frames, bool vst3 = false) noexcept
        : client(frames > 0 ? candidate : nullptr)
    {
        if (client != nullptr)
        {
            if (vst3) client->beginVst3HostAutomationBlock(frames);
            else client->beginHostAutomationBlock(frames);
        }
    }
    ~HostAutomationBlock() { if (client != nullptr) client->endHostAutomationBlock(); }
    HostAutomationClient* get() const noexcept { return client; }
    HostAutomationBlock(const HostAutomationBlock&) = delete;
    HostAutomationBlock& operator=(const HostAutomationBlock&) = delete;
private:
    HostAutomationClient* client = nullptr;
};
} // namespace cs80
