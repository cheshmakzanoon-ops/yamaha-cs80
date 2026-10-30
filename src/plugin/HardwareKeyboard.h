#pragma once

#include "PluginProcessor.h"
#include "UiComponents.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

// On-screen keyboard. Owner of everything about how the keys look and animate.
// Behaviour (gesture capture, pressure, host key mirroring) must stay as written:
// tests in src/tools/ui_snapshot.cpp and tests/Keyboard*Tests exercise it.
namespace cs80::ui
{

// Software-rendered key surfaces keep snapshots identical to the interactive
// editor. The key depth, moulded front lip and black-key bevel are drawn here.
class HardwareKeyboard final : public juce::MidiKeyboardComponent, public juce::SettableTooltipClient
{
public:
    HardwareKeyboard(Cs80AudioProcessor& p, Orientation orientation)
        : MidiKeyboardComponent(p.getKeyboardState(), orientation), processor(p) {}

    // Pointer gesture standing in for the original per-key pressure sensor.
    std::function<bool(int)> hostKeyDown;

    void mouseDown(const juce::MouseEvent& e) override
    {
        token = processor.captureKeyboardGesture(getMidiChannel(), getNoteAndVelocityAtPosition(e.position).note, [&]
        { MidiKeyboardComponent::mouseDown(e); });
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const int note = getNoteAndVelocityAtPosition(e.position).note;
        const bool newStrike = note >= 0 && note != gesture.getNote();
        const auto accepted = processor.captureKeyboardGesture(getMidiChannel(), note, [&]
        { MidiKeyboardComponent::mouseDrag(e); });
        // A same-key drag retains its original token even after reset+ack.
        // Cross-key slides must bind the actual new onset or become invalid.
        // A genuine accepted onset can also follow JUCE's enter/release path
        // at the same pitch. Do not discard its proven new identity.
        if (accepted && !newStrike) gesture.begin(note, e.position.y);
        if (accepted || newStrike) token = accepted;
    }

    bool mouseDownOnKey(int note, const juce::MouseEvent& e) override
    {
        gesture.begin(note, e.position.y);
        return true;
    }
    bool mouseDraggedToKey(int note, const juce::MouseEvent& e) override
    {
        const float pressure = gesture.drag(note, e.position.y, (float) getHeight());
        if (pressure >= 0.0f)
            processor.setKeyPressureFromUI(token, pressure);
        return true;
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        MidiKeyboardComponent::mouseUp(e);
        gesture.end();
        token = {};
    }
    void mouseExit(const juce::MouseEvent& e) override
    {
        // JUCE mouseExit releases its held pointer note. Re-entry onto even
        // the same pitch is therefore a fresh strike, not the old gesture.
        MidiKeyboardComponent::mouseExit(e);
        gesture.end();
        token = {};
    }

    void drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area,
                       bool down, bool over, juce::Colour, juce::Colour) override
    {
        down = down || (hostKeyDown && hostKeyDown(note));
        auto key = area.reduced(0.7f, 0.0f);
        const float lip = key.getHeight() * 0.07f;
        const float sink = down ? 2.0f : 0.0f;
        const auto ivoryTop = down ? juce::Colour(0xffd9c58e) : juce::Colour(0xfff5f0e0);
        // Key top: shaded under the panel lip, bright mid-section.
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff8f8a7a), key.getX(), key.getY(),
                                               ivoryTop, key.getX(), key.getY() + key.getHeight() * 0.22f, false));
        g.fillRect(key.withTrimmedBottom(lip));
        g.setGradientFill(juce::ColourGradient(ivoryTop, key.getX(), key.getY() + key.getHeight() * 0.22f,
                                               ivoryTop.darker(0.10f), key.getX(), key.getBottom() - lip, false));
        g.fillRect(key.withTrimmedTop(key.getHeight() * 0.22f).withTrimmedBottom(lip));
        // Rounded front lip in shade, with a bright upper edge.
        auto front = key.withTop(key.getBottom() - lip + sink);
        g.setGradientFill(juce::ColourGradient(ivoryTop.darker(0.12f), front.getX(), front.getY(),
                                               ivoryTop.darker(0.42f), front.getX(), front.getBottom(), false));
        g.fillRect(front);
        g.setColour(juce::Colours::white.withAlpha(down ? 0.2f : 0.75f));
        g.drawHorizontalLine((int) front.getY(), key.getX() + 1, key.getRight() - 1);
        // Gaps between keys: dark seam plus a soft edge shadow into the key.
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawVerticalLine((int) area.getRight() - 1, area.getY(), area.getBottom());
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.20f), key.getX(), 0.0f,
                                               juce::Colours::transparentBlack, key.getX() + 4.0f, 0.0f, false));
        g.fillRect(key.withWidth(4.0f).withTrimmedBottom(lip));
        if (over && !down) { g.setColour(juce::Colour(0xfff3d691).withAlpha(0.17f)); g.fillRect(area); }
    }

    void drawBlackNote(int note, juce::Graphics& g, juce::Rectangle<float> area,
                       bool down, bool over, juce::Colour) override
    {
        down = down || (hostKeyDown && hostKeyDown(note));
        // Cast shadow on the neighbouring white keys.
        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.fillRect(area.getX() + area.getWidth() * 0.18f, area.getY(), area.getWidth() * 1.0f, area.getHeight() + 3.0f);
        g.setColour(juce::Colour(0xff030303)); g.fillRect(area);
        auto cap = area.reduced(area.getWidth() * 0.12f, 0);
        auto slope = cap.removeFromBottom(area.getHeight() * 0.15f);
        const float sink = down ? 2.0f : 0.0f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3c3c41), cap.getX(), cap.getY(),
                                               down ? juce::Colour(0xff7a6a46) : juce::Colour(0xff131316),
                                               cap.getX(), cap.getBottom(), false));
        g.fillRect(cap);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff58585e), slope.getX(), slope.getY() + sink,
                                               juce::Colour(0xff0d0d0f), slope.getX(), slope.getBottom(), false));
        g.fillRect(slope.withTrimmedTop(sink));
        g.setColour(juce::Colour(0xff8b8b92).withAlpha(0.8f));
        g.drawHorizontalLine((int) slope.getY() + (int) sink, cap.getX(), cap.getRight());
        g.setColour(juce::Colours::white.withAlpha(over ? 0.28f : 0.14f));
        g.drawVerticalLine((int) cap.getX(), cap.getY(), cap.getBottom());
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawVerticalLine((int) cap.getRight() - 1, cap.getY(), cap.getBottom());
    }

private:
    Cs80AudioProcessor& processor;
    Cs80AudioProcessor::KeyPressureToken token {};
    cs80::ui::KeyPressureGesture gesture;
};

// The keybed assembly (frame, key slip, end blocks) around the keys. The editor
// places this one component; `keys` is the playable MIDI keyboard inside it.
class Keybed final : public juce::Component
{
public:
    explicit Keybed(Cs80AudioProcessor& p) : keys(p, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        addAndMakeVisible(keys);
    }

    // Insets of the key area inside the assembly, in pixels.
    juce::BorderSize<int> keyInsets { 0, 0, 0, 0 };

    void resized() override
    {
        keys.setBounds(keyInsets.subtractedFrom(getLocalBounds()));
        if (keys.getWidth() > 0)
            keys.setKeyWidth((float) keys.getWidth() / 36.0f);
    }

    HardwareKeyboard keys;
};

} // namespace cs80::ui
