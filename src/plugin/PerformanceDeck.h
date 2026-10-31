#pragma once

#include "PanelWidgets.h"

#include <array>

namespace cs80::panel
{
// The performance deck between the control surface and the keys. Left block: the original left-hand panel
// (sustain, portamento/glissando, the two footswitch assigners, tremolo/chorus, the EXP / EXP-WAH selector and
// the on-screen pedal). Centre: nameplate and ribbon. Right block: the rear jack panel (footswitch jack and
// contact, output HIGH/LOW), which the original has on its back, not its front panel.
class PerformanceDeck final : public PanelSection
{
public:
    explicit PerformanceDeck(Cs80AudioProcessor& p);

    juce::Rectangle<float> designArea() const;
    ui::RibbonStrip& getRibbon() noexcept { return ribbon; }
    ui::PerformanceWheel& getExpressionPedal() noexcept { return expressionPedal; }
    void applyCaptions(bool small) override;
    // Message-thread refresh from the editor timer: tremolo/chorus tabs, two-way switch legends, pedal position.
    void refresh();
    void syncChorusState();
    void syncDeckLegends();

    // Static art drawn by the buffered chassis in design space: deck blocks, headers, nameplate, ribbon slot.
    void paintArt(juce::Graphics& g) const override;

private:
    void buildLeftPanel();
    void buildRearPanel();
    // Writes one host parameter as a complete gesture.
    void setParameter(int index, float physicalValue);

    // ON/OFF, TREMOLO and CHORUS tabs (adjustment procedure p21) over chorus_on and chorus_mode.
    KeyButton chorusOnKey { "ON / OFF", KeyButton::Style::selector };
    KeyButton tremoloKey { "TREMOLO", KeyButton::Style::selector };
    KeyButton chorusKey { "CHORUS", KeyButton::Style::selector };
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
