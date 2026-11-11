#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdint>
#include <functional>

class Cs80AudioProcessor;

namespace cs80::ui
{
// Discrete parameter choices retain JUCE's normal selection and attachment
// behavior. The processor must outlive this control, just as for its external
// ComboBoxAttachment. Keep that attachment and this context ID aligned.
class ParameterComboBox : public juce::ComboBox
{
public:
    ParameterComboBox();
    bool bindParameterContext(Cs80AudioProcessor&, const juce::String& parameterId);
    void clearParameterContext();
    int getContextHostParameterIndex() const;
    const juce::String& getContextParameterId() const noexcept { return parameterId; }
    juce::PopupMenu buildContextMenu();
    void showContextMenu();
    std::function<void()> onContextMenu;

    juce::String getTooltip() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void showPopup() override;
    void lookAndFeelChanged() override;

protected:
    // Presentation is separate from routing so an embedded editor or a
    // headless test can supply its own presenter. Complete once on dismissal.
    virtual void presentContextMenu(juce::PopupMenu, std::function<void()> dismissed);

private:
    void routeLabelInput();
    juce::RangedAudioParameter* contextParameter() const;
    bool canOpenContext() const;
    bool contextMatches(Cs80AudioProcessor*, const juce::String&, int, std::uint64_t) const;
    Cs80AudioProcessor* processor = nullptr;
    juce::String parameterId;
    std::uint64_t bindingGeneration = 0;
    bool contextPointer = false, ordinaryPointer = false, contextPopupActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterComboBox)
};
}
