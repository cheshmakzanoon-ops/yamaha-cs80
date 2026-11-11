#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdint>
#include <functional>

class Cs80AudioProcessor;

namespace cs80::ui
{
// Context routing for the Studio's one-parameter buttons. The processor must
// outlive this component and its external ButtonAttachment; their IDs must agree.
// Painting, ordinary clicks and attachment ownership remain with the control.
class ParameterContextButton : public juce::Button
{
public:
    explicit ParameterContextButton(const juce::String& name) : juce::Button(name) {}
    bool bindParameterContext(Cs80AudioProcessor&, const juce::String& parameterId);
    void clearParameterContext();
    int getContextHostParameterIndex() const;
    const juce::String& getContextParameterId() const noexcept { return parameterId; }
    juce::PopupMenu buildContextMenu();
    void showContextMenu();
    std::function<void()> onContextMenu;

    juce::String getTooltip() override;
    void mouseDown(const juce::MouseEvent&) final;
    void mouseDrag(const juce::MouseEvent&) final;
    void mouseUp(const juce::MouseEvent&) final;
    bool keyPressed(const juce::KeyPress&) final;

protected:
    // Hooks run only for admitted ordinary input. mouse release runs after
    // Button::mouseUp, preserving clicked() -> attachment-listener ordering.
    virtual void ordinaryMouseDown(const juce::MouseEvent&) {}
    virtual void ordinaryMouseUp(const juce::MouseEvent&) {}
    virtual bool ordinaryKeyPressed(const juce::KeyPress&);
    virtual void presentContextMenu(juce::PopupMenu, std::function<void()> dismissed);

private:
    bool ordinaryInputActive() const;
    juce::RangedAudioParameter* contextParameter() const;
    bool canOpenContext() const;
    bool contextMatches(Cs80AudioProcessor*, const juce::String&, int, std::uint64_t) const;
    Cs80AudioProcessor* processor = nullptr;
    juce::String parameterId, originalDescription;
    std::uint64_t bindingGeneration = 0;
    unsigned queuedKeyboardActions = 0;
    bool contextPointer = false, ordinaryPointer = false, contextPopupActive = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterContextButton)
};
}
