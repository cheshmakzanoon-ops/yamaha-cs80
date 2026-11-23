#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace cs80
{
// Retain the two published Bool parameter slots so a host cannot accidentally
// bind an old automation lane to a different control. They are now read-only
// compatibility fields: every playable patch uses the same circuit engine.
// Keeping the AudioParameterBool base also preserves the host's type/range.
class CircuitPolicyParameter final : public juce::AudioParameterBool
{
public:
    CircuitPolicyParameter(const juce::ParameterID& id, const juce::String& name,
                           bool enabled, const juce::String& text)
        : juce::AudioParameterBool(id, name, enabled), value(enabled ? 1.0f : 0.0f), display(text)
    {
    }

    float getValue() const override { return value; }
    void setValue(float) override {}
    bool isAutomatable() const override { return false; }
    juce::String getText(float, int maximumLength) const override
    {
        return maximumLength > 0 ? display.substring(0, maximumLength) : display;
    }
    float getValueForText(const juce::String&) const override { return value; }

private:
    const float value;
    const juce::String display;
};
} // namespace cs80
