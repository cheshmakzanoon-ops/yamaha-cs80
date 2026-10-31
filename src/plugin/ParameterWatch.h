#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

namespace cs80::panel
{
// Raises a flag when any host parameter of the processor changes, from whatever thread changed it (host
// automation arrives on the audio thread). The callback only stores an atomic: it never touches the editor.
// The message thread consumes the flag instead of polling every parameter on its timer.
class ParameterChangeWatch final : private juce::AudioProcessorParameter::Listener
{
public:
    explicit ParameterChangeWatch(juce::AudioProcessor& p) : processor(p)
    {
        for (auto* parameter : processor.getParameters())
            parameter->addListener(this);
    }

    ~ParameterChangeWatch() override
    {
        for (auto* parameter : processor.getParameters())
            parameter->removeListener(this);
    }

    // True once after any parameter changed since the previous call (and on the first call).
    bool consume() noexcept { return changed.exchange(false, std::memory_order_acq_rel); }

private:
    void parameterValueChanged(int, float) override { changed.store(true, std::memory_order_release); }
    void parameterGestureChanged(int, bool) override {}

    juce::AudioProcessor& processor;
    std::atomic<bool> changed { true };

    JUCE_DECLARE_NON_COPYABLE(ParameterChangeWatch)
};
} // namespace cs80::panel
