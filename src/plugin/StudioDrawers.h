#pragma once

#include "UiComponents.h"
#include "VoiceMonitor.h"

#include <array>
#include <functional>
#include <memory>

class Cs80AudioProcessor;

namespace cs80::ui
{

// The voice monitor: an optional drawer that holds what the main panel's signal-flow plate used
// to show, so the channel rows can span the panel as on the hardware. It draws the signal path,
// both envelopes and effective filter cutoff histories of one physical voice/rank, copied
// from the engine's audio-thread diagnostics. It polls at 15 Hz while open and shows all
// sixteen physical slots when the optional second instrument is active.
class ScopeDrawer final : public juce::Component, private juce::Timer
{
public:
    explicit ScopeDrawer(Cs80AudioProcessor&);
    ~ScopeDrawer() override;

    void setChannel(int channel);
    int getChannel() const noexcept { return channel; }
    void setVoice(int voice);
    int getVoice() const noexcept { return voice; }
    cs80::Engine::RankInfo displayedRank() const;
    void refresh() { timerCallback(); }
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
        float oscillatorHz = 0.0f, hpfHz = 0.0f, lpfHz = 0.0f;
        float filterEnvelope = 0.0f, ampEnvelope = 0.0f, keyVolts = 0.0f, hpfQ = 0.0f, lpfQ = 0.0f;
        bool hpfEnabled = false, lpfEnabled = false;
        unsigned voices = 0;
        int channel = 0, voice = 0, capacity = 8, note = -1;
        int source = 13;
        std::uint64_t serial = 0, assignmentSerial = 0;
        bool operator==(const Snapshot&) const = default;
    };
    void timerCallback() override;
    Snapshot sample() const;
    void updateControls();

    Cs80AudioProcessor& processor;
    Snapshot snap;
    int channel = 0, voice = 0;
    std::array<Snapshot, 120> history {};
    std::size_t historyCount = 0, historyNext = 0;
    juce::ComboBox voiceSelect;
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
