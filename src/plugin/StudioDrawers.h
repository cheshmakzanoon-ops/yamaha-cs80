#pragma once

#include "UiComponents.h"

#include <array>
#include <bit>
#include <functional>
#include <memory>

class Cs80AudioProcessor;

namespace cs80::ui
{

// The voice monitor: an optional drawer that holds what the main panel's signal-flow plate used
// to show, so the channel rows can span the panel as on the hardware. It draws the signal path,
// both envelopes and the filter response of one channel, computed from what is sounding (the
// panel, or the selected tone or memory), and up to sixteen voice cards' activity. It repaints only
// while it is open (15 Hz poll; a frame is drawn only when its inputs changed).
class ScopeDrawer final : public juce::Component, private juce::Timer
{
public:
    explicit ScopeDrawer(Cs80AudioProcessor&);
    ~ScopeDrawer() override;

    void setChannel(int channel);
    int getChannel() const noexcept { return channel; }
    int getDisplayedVoiceCount() const noexcept { return std::popcount(snap.voices); }
    int getDisplayedVoiceCapacity() const noexcept { return snap.capacity; }
    std::function<void()> onClose;

    // Design size of the drawer in the editor's 1600 x 1000 design space.
    static constexpr float kDesignW = 420.0f, kDesignH = 470.0f;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    struct Snapshot
    {
        std::array<float, cs80::CP_count> v {};
        unsigned voices = 0;
        int capacity = 8;
        int channel = 0;
        int source = 13;
        bool operator==(const Snapshot&) const = default;
    };
    void timerCallback() override;
    Snapshot sample() const;
    float p(int local) const { return snap.v[(size_t) local]; }

    Cs80AudioProcessor& processor;
    Snapshot snap;
    int channel = 0;
    std::unique_ptr<juce::Button> closeKey, channelI, channelII;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopeDrawer)
};

// The modern controllers strip: the pitch and mod wheels the CS-80 itself does not have, kept off
// the instrument panel. The editor hides it in PURE CS-80.
class ControllersStrip final : public juce::Component
{
public:
    explicit ControllersStrip(Cs80AudioProcessor&);
    ~ControllersStrip() override;

    PerformanceWheel& getPitchWheel() noexcept { return pitch; }
    PerformanceWheel& getModWheel() noexcept { return mod; }

    static constexpr float kDesignW = 150.0f, kDesignH = 236.0f;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    Cs80AudioProcessor& processor;
    PerformanceWheel pitch { "PITCH", true };
    PerformanceWheel mod { "MOD", false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControllersStrip)
};

} // namespace cs80::ui
