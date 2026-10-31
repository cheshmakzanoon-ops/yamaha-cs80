#pragma once

#include "PanelWidgets.h"

#include <array>

namespace cs80::panel
{
// The front strip of the control surface (PN2): PITCH, DETUNE, ring modulator, sub oscillator, feet, the two
// tone-selector rows, MIX, brilliance, resonance, keyboard control, touch response and VOLUME.
class CentreRow final : public PanelSection
{
public:
    explicit CentreRow(Cs80AudioProcessor& p);

    juce::Rectangle<float> designArea() const;
    ui::ParamKnob* getSubFunction() const noexcept { return subFunction; }
    ui::ParamKnob* getSubSpeed() const noexcept { return subSpeed; }
    juce::TextButton& getToneButton(int channel, int source) { return *toneButtons[(size_t) channel][(size_t) source]; }
    // Lights the tab of each row's selected source.
    void syncToneMarks();
    // Called after a tab press has selected a new source.
    std::function<void()> onToneSelected;
    // The printed hardware name of a selector tab ("ELECTRIC PIANO", "MEMORY 3", "PANEL").
    static juce::String toneTabName(int channel, int source);

    // Static art drawn by the buffered chassis in design space: the strip outline, brackets and tone bezel.
    void paintArt(juce::Graphics& g) const override;

private:
    void build();
    void buildToneSection(float x0);
    void afterLayout() override;
    void paintToneBezel(juce::Graphics& g) const;

    juce::Rectangle<float> toneBezel;
    ui::ParamKnob* subFunction = nullptr;
    ui::ParamKnob* subSpeed = nullptr;
    ui::ParamKnob* pitchKnob = nullptr;
    ui::ParamKnob* volumeKnob = nullptr;
    ui::ParamKnob* legendReference = nullptr;
    std::array<std::array<std::unique_ptr<juce::TextButton>, 14>, 2> toneButtons;
};
} // namespace cs80::panel
