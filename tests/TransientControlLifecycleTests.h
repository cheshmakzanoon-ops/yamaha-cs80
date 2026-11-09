#pragma once

#include "plugin/PerformanceDeck.h"
#include "plugin/StudioDrawers.h"

#include <cmath>
#include <memory>
#include <vector>

namespace cs80::tests
{
namespace transient_control_detail
{
inline juce::MouseEvent mouse(juce::Component& c, float x, float y, bool down = true)
{
    const juce::Point<float> point(x, y);
    return { juce::Desktop::getInstance().getMainMouseSource(), point,
             juce::ModifierKeys(down ? juce::ModifierKeys::leftButtonModifier : 0),
             1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c,
             juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, false };
}
}

template <class Check>
void testTransientControlLifecycle(Check check)
{
    using transient_control_detail::mouse;
    juce::TemporaryFile fixture("-cs80-transient-controls");
    auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    std::vector<float> pitch, mod, ribbon;
    auto makeStrip = [&] {
        auto strip = std::make_unique<cs80::ui::ControllersStrip>(*host);
        strip->setSize(150, 236);
        strip->setVisible(true);
        strip->getPitchWheel().onValueChange = [&](float v) { pitch.push_back(v); };
        strip->getModWheel().onValueChange = [&](float v) { mod.push_back(v); };
        return strip;
    };
    {
        auto strip = makeStrip();
        auto& pw = strip->getPitchWheel(); auto& mw = strip->getModWheel();
        pw.mouseDown(mouse(pw, 10, 0)); mw.mouseDown(mouse(mw, 10, 0));
        check(pw.getValue() > 0.9f && mw.getValue() > 0.9f, "the fixture begins real wheel pointer gestures");
        strip->setVisible(false);
        check(pw.getValue() == 0.0f && pitch.size() == 2 && pitch.back() == 0.0f,
              "hiding the controllers strip releases an active spring pitch gesture exactly once");
        check(mw.getValue() > 0.9f && mod.size() == 1, "hiding the strip retains latched modulation without a new event");
        const auto pitchCount = pitch.size(), modCount = mod.size();
        pw.mouseDrag(mouse(pw, 10, 176)); mw.mouseDrag(mouse(mw, 10, 176));
        pw.mouseUp(mouse(pw, 10, 176, false)); mw.mouseUp(mouse(mw, 10, 176, false));
        check(pitch.size() == pitchCount && mod.size() == modCount,
              "late drag/up delivery after cancellation cannot revive a hidden gesture");
        strip->setVisible(true);
        pw.mouseDown(mouse(pw, 10, 176));
        check(pw.getValue() < -0.9f, "a new gesture works after showing the strip again");
        pw.mouseUp(mouse(pw, 10, 176, false));
        check(pw.getValue() == 0.0f && pitch.back() == 0.0f, "normal pointer release still centres the wheel");
        const auto released = pitch.size();
        strip.reset();
        check(pitch.size() == released, "destroying an idle strip does not emit a redundant pitch reset");
    }
    {
        auto strip = makeStrip(); auto& pw = strip->getPitchWheel();
        pw.mouseDown(mouse(pw, 10, 0));
        const auto count = pitch.size();
        strip.reset();
        check(pitch.size() == count + 1 && pitch.back() == 0.0f,
              "destroying the owning strip releases its active pitch gesture");
    }
    {
        auto strip = makeStrip();
        strip->getPitchWheel().setValue(0.7f); // display/restore is not an owned UI gesture
        const auto count = pitch.size();
        strip->setVisible(false); strip.reset();
        check(pitch.size() == count, "an idle restored wheel never sends a reset that could overwrite host pitch");
    }
    auto makeDeck = [&] {
        auto deck = std::make_unique<cs80::panel::PerformanceDeck>(*host);
        deck->getRibbon().setSize(800, 36);
        deck->getRibbon().onValueChange = [&](float v) { ribbon.push_back(v); };
        return deck;
    };
    {
        auto deck = makeDeck(); auto& strip = deck->getRibbon();
        strip.mouseDown(mouse(strip, 400, 18)); strip.mouseDrag(mouse(strip, 650, 18));
        check(ribbon.back() > 0.1f, "the fixture begins a real ribbon pointer bend");
        const auto count = ribbon.size();
        deck.reset();
        check(ribbon.size() == count + 1 && ribbon.back() == 0.0f,
              "destroying the performance deck releases an active ribbon pointer bend");
    }
    {
        auto deck = makeDeck(); auto& strip = deck->getRibbon();
        check(strip.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)) && ribbon.back() > 0.0f,
              "the fixture begins a keyboard-owned ribbon bend");
        const auto count = ribbon.size();
        deck.reset();
        check(ribbon.size() == count + 1 && ribbon.back() == 0.0f,
              "destroying the performance deck also releases its keyboard ribbon bend");
    }
    {
        auto deck = makeDeck(); auto& strip = deck->getRibbon();
        strip.keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
        strip.mouseDown(mouse(strip, 400, 18)); strip.mouseDrag(mouse(strip, 650, 18));
        const auto pointerValue = ribbon.back();
        const auto count = ribbon.size();
        strip.focusLost(juce::Component::focusChangedDirectly);
        check(ribbon.size() == count && ribbon.back() == pointerValue,
              "pointer takeover ends keyboard ownership so focus loss cannot release the new pointer gesture");
        strip.mouseUp(mouse(strip, 650, 18, false));
        const auto released = ribbon.size();
        strip.mouseDrag(mouse(strip, 700, 18)); strip.mouseUp(mouse(strip, 700, 18, false));
        deck.reset();
        check(ribbon.size() == released && ribbon.back() == 0.0f,
              "completed ribbon release is idempotent and rejects a late drag");
    }
    {
        auto deck = makeDeck(); auto& pedal = deck->getExpressionPedal();
        pedal.setSize(32, 79); pedal.mouseDown(mouse(pedal, 10, 45));
        const auto value = host->getExpressionValue();
        const auto count = ribbon.size();
        deck.reset();
        check(host->getExpressionValue() == value && ribbon.size() == count,
              "closing an idle-ribbon deck retains latched expression and emits no ribbon reset");
    }

    // Verify the actual owners' callbacks through the processor's mailboxes,
    // not only callback counters. Both engine policies retain a held note.
    for (bool circuit : { false, true })
    {
        auto audioHost = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        audioHost->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        const int disabled[] { cs80::P_parityMode, cs80::P_mpeOn, cs80::P_arpOn, cs80::P_kasScanTiming,
                               cs80::P_voiceDispersion, cs80::P_drift, cs80::P_initBend, cs80::P_portamento };
        for (int p : disabled)
            audioHost->setParamValue(p, 0.0f);
        audioHost->setParamValue(cs80::P_pitchBendRange, 12.0f);
        audioHost->prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> audio(2, 256); juce::MidiBuffer midi;
        auto settle = [&] { for (int i = 0; i < 16; ++i) { audioHost->processBlock(audio, midi); midi.clear(); } };
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8) 100), 0); settle();
        const float baseHz = audioHost->getEngine().getRankInfo(0, 0).oscillatorHz;
        auto strip = std::make_unique<cs80::ui::ControllersStrip>(*audioHost);
        strip->setSize(150, 236); strip->setVisible(true);
        auto& wheel = strip->getPitchWheel(); wheel.mouseDown(mouse(wheel, 10, 0)); settle();
        check(audioHost->getEngine().getRankInfo(0, 0).oscillatorHz > baseHz * 1.5f,
              "an owned wheel gesture actually bends the running engine");
        strip->setVisible(false); settle();
        check(std::abs(audioHost->getEngine().getRankInfo(0, 0).oscillatorHz / baseHz - 1.0f) < 0.005f,
              "strip hide returns the running engine to its prior pitch");
        // Isolate the following ribbon assertion even in the pre-fix negative run.
        audioHost->setPitchWheelFromUI(0.0f); settle();
        auto deck = std::make_unique<cs80::panel::PerformanceDeck>(*audioHost);
        auto& ribbonStrip = deck->getRibbon(); ribbonStrip.setSize(800, 36);
        ribbonStrip.mouseDown(mouse(ribbonStrip, 400, 18)); ribbonStrip.mouseDrag(mouse(ribbonStrip, 650, 18)); settle();
        check(audioHost->getEngine().getVoiceInfo(0).ribbonRatio > 1.1f, "an owned ribbon gesture reaches the running engine");
        const auto parameters = audioHost->currentValues();
        deck.reset(); settle();
        check(std::abs(audioHost->getEngine().getVoiceInfo(0).ribbonRatio - 1.0f) < 0.001f
                  && audioHost->getEngine().getVoiceInfo(0).held,
              "owner destruction centres the running ribbon without stopping the held note");
        const auto after = audioHost->currentValues();
        bool unchanged = true;
        for (int p = 0; p < cs80::P_numParams; ++p) unchanged &= after[p] == parameters[p];
        check(unchanged, "transient cleanup does not edit any host parameter");

        // Closing an idle view must not manufacture a UI reset over a host bend.
        audioHost->setRibbonFromUI(0.0f); settle();
        midi.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0); settle();
        const float hostBentHz = audioHost->getEngine().getRankInfo(0, 0).oscillatorHz;
        check(hostBentHz > baseHz * 1.5f, "the idle-owner fixture starts with a real host MIDI pitch bend");
        auto idleStrip = std::make_unique<cs80::ui::ControllersStrip>(*audioHost);
        idleStrip->setVisible(true);
        idleStrip->getPitchWheel().setValue(0.7f);
        idleStrip->setVisible(false); idleStrip.reset();
        auto idleDeck = std::make_unique<cs80::panel::PerformanceDeck>(*audioHost);
        idleDeck.reset(); settle();
        check(std::abs(audioHost->getEngine().getRankInfo(0, 0).oscillatorHz / hostBentHz - 1.0f) < 0.005f,
              "idle owner hide/destruction preserves the running engine's host MIDI pitch bend");
    }
}
} // namespace cs80::tests
