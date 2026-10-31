#pragma once

#include "PanelWidgets.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace cs80::panel
{
// The lid's live readout: a small black window that names the control under the pointer (or being dragged)
// with its value, and otherwise says which source each channel is playing. It repaints on its own, so
// dragging a slider never repaints the whole lid.
class LidReadout final : public juce::Component
{
public:
    LidReadout();
    void setLines(const juce::String& main, const juce::String& hint);
    void paint(juce::Graphics& g) override;

private:
    juce::String mainLine, hintLine;
};

// Magnified view of the four independent analog-memory panels (owner index 46). This does not copy a live
// patch: moving each miniature control edits that bank directly. The component is the whole lid: a hinge band
// above it, the rimmed lid itself and the shadow the raised lid throws on the deck below, so it is laid out in
// 1504 x 590 design units.
class MemoryLid final : public juce::Component
{
public:
    explicit MemoryLid(Cs80AudioProcessor& p);
    ~MemoryLid() override;

    void refresh();
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent& e) override { track(e); }
    void mouseMove(const juce::MouseEvent& e) override { track(e); }
    void mouseDrag(const juce::MouseEvent& e) override { track(e); }
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    std::function<void()> onClose;

private:
    static constexpr float kW = 1504.0f, kH = 590.0f, kOffsetX = 16.0f;

    struct Item
    {
        int bank = 0, local = 0;
        std::unique_ptr<ui::FocusSlider> slider;
        std::unique_ptr<ui::LedButton> toggle;
        // Destroy attachments before their controls.
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };

    // The lid's own art (hinge, rim, crinkle plate, captions, row frames) for the current rows.
    void paintArt(juce::Graphics& g);
    unsigned activeRowMask() const noexcept;
    Item* findItem(const juce::Component* c);
    void track(const juce::MouseEvent& e);
    static juce::String sourceName(int source, int channel);
    void showIdle();
    void describe(const Item& item, bool lastTouch);
    // The front-panel name of a channel control (the lid prints the same names as the main panel).
    static juce::String panelName(int local);
    static float rowY(int bank) { return 118.0f + (float) bank * 104.0f; }
    static float colX(int col) { return 145.0f + (float) col * 50.0f; }

    Cs80AudioProcessor& processor;
    LidReadout readout;
    Item* hovered = nullptr;
    Item* lastTouched = nullptr;
    std::array<bool, 4> rowActive { false, false, false, false };
    // The art is expensive (brushed metal, crinkle tile, four etched rows) and every slider move repaints the
    // part of the lid under it, so it is rendered once per size, pixel scale and lit-row set and then blitted.
    juce::Image artCache;
    juce::Rectangle<int> artBounds;
    float artPixelScale = 0.0f;
    unsigned artRows = ~0u;
    KeyButton close { "CLOSE LID", KeyButton::Style::plate };
    std::array<std::unique_ptr<KeyButton>, 4> bankButtons;
    std::vector<Item> items;
    const std::array<int, 26> order {{
        cs80::CP_pwmSpeed, cs80::CP_pwmDepth, cs80::CP_pw, cs80::CP_pulse, cs80::CP_saw, cs80::CP_noise,
        cs80::CP_hpfCutoff, cs80::CP_hpfRes, cs80::CP_lpfCutoff, cs80::CP_lpfRes,
        cs80::CP_fIl, cs80::CP_fAl, cs80::CP_fA, cs80::CP_fD, cs80::CP_fR,
        cs80::CP_vcfLevel, cs80::CP_sine, cs80::CP_vA, cs80::CP_vD, cs80::CP_vS, cs80::CP_vR, cs80::CP_vLevel,
        cs80::CP_initBrill, cs80::CP_initLevel, cs80::CP_afterBrill, cs80::CP_afterLevel
    }};
    const std::array<const char*, 26> labels {{
        "SPEED","PWM","PW","PULSE","SAW","NOISE","HPF","RES H","LPF","RES L",
        "IL","AL","A","D","R","VCF LV","SINE","A","D","S","R","LEVEL",
        "BRILL","LEVEL","BRILL","LEVEL"
    }};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MemoryLid)
};

// Slider cap colours as printed on the main panel, so a memory row reads like a miniature of it.
juce::Colour memoryCapColour(int local);
} // namespace cs80::panel
