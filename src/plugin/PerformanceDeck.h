#pragma once

#include "PanelWidgets.h"

#include <array>

namespace cs80::panel
{
// The performance deck between the control surface and the keys: sustain, portamento/glissando and the
// chorus/tremolo group; the nameplate and the ribbon controller; keyboard contour, the foot controller and
// the footswitch, assigner and output switches.
class PerformanceDeck final : public PanelSection
{
public:
    explicit PerformanceDeck(Cs80AudioProcessor& p);

    juce::Rectangle<float> designArea() const;
    ui::RibbonStrip& getRibbon() noexcept { return ribbon; }
    ui::PerformanceWheel& getExpressionPedal() noexcept { return expressionPedal; }
    void applyCaptions(bool small) override;
    // Message-thread refresh from the editor timer: chorus selector, two-way switch legends, pedal position.
    void refresh();
    void syncChorusState();
    void syncDeckLegends();

    // Static art drawn by the buffered chassis in design space: deck blocks, headers, nameplate, ribbon slot.
    static void paintArt(juce::Graphics& g);

private:
    void buildPerformance();
    void buildKeyboardControls();
    int readChorusState() const;
    void setChorusState(int state);

    std::array<KeyButton, 3> chorusKeys { KeyButton("OFF", KeyButton::Style::selector), KeyButton("CHORUS", KeyButton::Style::selector),
                                          KeyButton("TREMOLO", KeyButton::Style::selector) };
    ui::ParamKnob* chorusSpeedKnob = nullptr;
    ui::ParamKnob* chorusDepthKnob = nullptr;
    ui::LedButton* susToggle = nullptr;
    ui::LedButton* holdToggle = nullptr;
    ui::LedButton* wahToggle = nullptr;
    ui::LedButton* outputToggle = nullptr;
    ui::PerformanceWheel expressionPedal { "PEDAL", false };
    ui::RibbonStrip ribbon;
};
} // namespace cs80::panel
