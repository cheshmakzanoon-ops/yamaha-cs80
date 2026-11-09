#pragma once
#include "plugin/PluginEditor.h"
#include "plugin/StudioDrawers.h"

#include <chrono>
#include <cstdint>

namespace cs80::tests
{
namespace voice_activity_ui
{
inline juce::Component* find(juce::Component& root, const char* id)
{
    if (root.getComponentID() == id) return &root;
    for (auto* child : root.getChildren()) if (auto* found = find(*child, id)) return found;
    return nullptr;
}
inline std::uint64_t region(const juce::Image& image, float designW, float designH,
                            int x0, int x1, int y0, int y1)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (int y = (int) (image.getHeight() * y0 / designH); y < (int) (image.getHeight() * y1 / designH); ++y)
        for (int x = (int) (image.getWidth() * x0 / designW); x < (int) (image.getWidth() * x1 / designW); ++x)
        { hash ^= image.getPixelAt(x, y).getARGB(); hash *= 1099511628211ull; }
    return hash;
}
}

template <class Check, class Flush>
void testVoiceActivityUi(Check check, Flush flush)
{
    juce::TemporaryFile fixture("-cs80-voice-activity-ui");
    Cs80AudioProcessor host(fixture.getFile());
    host.setParamValue(P_parityMode, 0.0f);
    host.setParamValue(P_voiceMode, 1.0f);
    host.setParamValue(P_kasScanTiming, 0.0f);
    host.setParamValue(P_arpOn, 0.0f);
    host.prepareToPlay(48000.0, 32);
    juce::AudioBuffer<float> audio(2, 0);
    juce::MidiBuffer midi;
    auto tick = [&] { host.processBlock(audio, midi); midi.clear(); };
    tick();
    Cs80AudioProcessorEditor editor(host);
    editor.setVisible(true);
    cs80::ui::ScopeDrawer drawer(host);
    drawer.setVisible(true);
    auto* lamps = voice_activity_ui::find(editor, "activity_lamps");
    auto* label = dynamic_cast<juce::Label*>(voice_activity_ui::find(editor, "voice_status"));
    check(lamps != nullptr && label != nullptr, "voice telemetry has toolbar lamps and accessible count");
    if (lamps == nullptr || label == nullptr) return;
    const auto waitCount = [&](int count, int capacity) {
        const auto expected = "Active voices: " + juce::String(count) + " of " + juce::String(capacity);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (label->getText() != expected && std::chrono::steady_clock::now() < deadline) flush(20);
        check(label->getText() == expected, "accessible count includes applied sixteen-voice capacity");
        flush(100); // drawer's independent 15 Hz poll
    };
    for (const auto size : { juce::Point<int>(1600, 1000), juce::Point<int>(1120, 700) })
    {
        editor.setSize(size.x, size.y);
        drawer.setSize(size.x * 420 / 1600, size.y * 470 / 1000);
        midi.addEvent(juce::MidiMessage::allSoundOff(1), 0);
        midi.addEvent(juce::MidiMessage::allSoundOff(2), 0); tick(); waitCount(0, 16);
        for (int i = 0; i < 8; ++i) midi.addEvent(juce::MidiMessage::noteOn(1, 48 + i, (juce::uint8)100), 0);
        tick();
        (void) lamps->createComponentSnapshot(lamps->getLocalBounds());
        check(label->getText() == "Active voices: 8 of 16", "paint-before-timer also updates the accessible voice count");
        waitCount(8, 16);
        const auto primaryLamps = lamps->createComponentSnapshot(lamps->getLocalBounds());
        const auto primaryDrawer = drawer.createComponentSnapshot(drawer.getLocalBounds());
        for (int i = 0; i < 8; ++i) midi.addEvent(juce::MidiMessage::noteOn(2, 60 + i, (juce::uint8)100), 0);
        tick(); waitCount(16, 16);
        const auto bothLamps = lamps->createComponentSnapshot(lamps->getLocalBounds());
        auto bothDrawer = drawer.createComponentSnapshot(drawer.getLocalBounds());
        const auto lh = [](const juce::Image& image, bool extension) {
            return voice_activity_ui::region(image, 128.0f, 40.0f, 8, 92, extension ? 29 : 18, extension ? 37 : 25);
        };
        const auto dh = [](const juce::Image& image, bool extension) {
            return voice_activity_ui::region(image, 420.0f, 470.0f, 20, 198, extension ? 75 : 60, extension ? 84 : 69);
        };
        check(primaryLamps.isValid() && bothLamps.isValid() && primaryDrawer.isValid() && bothDrawer.isValid(),
              "both telemetry views render at full and minimum editor sizes");
        check(lh(primaryLamps, false) == lh(bothLamps, false) && lh(primaryLamps, true) != lh(bothLamps, true),
              "extension activity changes toolbar lower row, not primary row or just text");
        // The jewel glow legitimately spills into the other row. Compare the
        // lower card area, excluding count text, and wait for its 15 Hz poll.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (dh(primaryDrawer, true) == dh(bothDrawer, true) && std::chrono::steady_clock::now() < deadline)
        {
            flush(20);
            bothDrawer = drawer.createComponentSnapshot(drawer.getLocalBounds());
        }
        check(dh(primaryDrawer, true) != dh(bothDrawer, true),
              "extension activity changes monitor lower card area, not just count text");
        const auto state = host.getAPVTS().copyState();
        (void) drawer.createComponentSnapshot(drawer.getLocalBounds());
        (void) lamps->createComponentSnapshot(lamps->getLocalBounds());
        check(state.isEquivalentTo(host.getAPVTS().copyState()), "telemetry painting does not edit parameter state");
    }
    host.setParamValue(P_parityMode, 1.0f); tick(); waitCount(0, 8);
}
} // namespace cs80::tests
