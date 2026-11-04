#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <type_traits>
#include <utility>

class Cs80AudioProcessor;

namespace cs80::ui
{
// A parameter's menu is independent of its widget. The host ID is authoritative: a memory
// miniature uses its own bank ID, while displayIndex only supplies the shared units/help.
class ParameterContext final : public std::enable_shared_from_this<ParameterContext>
{
public:
    ParameterContext(Cs80AudioProcessor&, juce::Component&, const juce::String& hostId,
                     int displayIndex = -1, juce::StringArray choices = {});
    ~ParameterContext();
    juce::PopupMenu buildContextMenu();
    void showContextMenu();
    void showValueEntry();
    bool applyTypedValue(const juce::String&);
    void resetToDefault();
    void copyValue();
    bool pasteValue();
    void showHelp();
    bool mouseDown(const juce::MouseEvent&);
    bool keyPressed(const juce::KeyPress&);
    int getHostParameterIndex() const;
    juce::String getParameterId() const;

private:
    bool parse(const juce::String&, float& physical) const;
    juce::String format(float physical) const;
    float value() const;
    void setNormalised(float);

    Cs80AudioProcessor& processor;
    juce::Component::SafePointer<juce::Component> anchor;
    juce::RangedAudioParameter* parameter = nullptr;
    int displayIndex = -1;
    juce::StringArray choices;
    std::unique_ptr<juce::TextEditor> valueEditor;
};

// Intercept the special pointer gesture before the underlying button or combo sees it. A
// MouseListener cannot do this: JUCE notifies it after the widget's own mouseDown has acted.
template <class Base>
class ParameterControl : public Base
{
public:
    template <class... Args>
    explicit ParameterControl(Args&&... args) : Base(std::forward<Args>(args)...) {}

    void bindParameterContext(Cs80AudioProcessor& processor, const juce::String& id,
                              int displayIndex = -1, juce::StringArray choices = {})
    {
        if constexpr (std::is_base_of_v<juce::ComboBox, Base>)
            if (choices.isEmpty())
                for (int i = 0; i < this->getNumItems(); ++i) choices.add(this->getItemText(i));
        context = std::make_shared<ParameterContext>(processor, *this, id, displayIndex, std::move(choices));
    }
    ParameterContext* getParameterContext() const noexcept { return context.get(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        swallowed = this->isEnabled() && context != nullptr && context->mouseDown(e);
        if (!swallowed) Base::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override { if (!swallowed) Base::mouseDrag(e); }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!std::exchange(swallowed, false)) Base::mouseUp(e);
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        return (this->isEnabled() && context != nullptr && context->keyPressed(key)) || Base::keyPressed(key);
    }

private:
    std::shared_ptr<ParameterContext> context;
    bool swallowed = false;
};

using ContextButton = ParameterControl<juce::Button>;
using ContextTextButton = ParameterControl<juce::TextButton>;
using ContextToggleButton = ParameterControl<juce::ToggleButton>;
using ContextComboBox = ParameterControl<juce::ComboBox>;
} // namespace cs80::ui
