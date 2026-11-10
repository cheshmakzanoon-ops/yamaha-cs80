#pragma once
#include "plugin/ModernModulationUi.h"
#include "plugin/PluginProcessor.h"
#include <vector>

namespace cs80::tests
{
template<class Check>
void testModernSliderGestures(Check check)
{
    juce::TemporaryFile fixture("-cs80-modern-slider-gestures");
    auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    struct Probe final : juce::AudioProcessorParameter::Listener
    {
        std::vector<int> kinds, parameters;
        void parameterValueChanged(int index, float) override { kinds.push_back(0); parameters.push_back(index); }
        void parameterGestureChanged(int index, bool begin) override { kinds.push_back(begin ? 1 : -1); parameters.push_back(index); }
        void clear() { kinds.clear(); parameters.clear(); }
    } probe;
    for (auto* parameter : host->getParameters()) parameter->addListener(&probe);
    int count = 0;
    {
        using namespace cs80::ui::modern_layer;
        MsegPage mseg(*host); LfoPanel lfo(*host); ModMatrixPage matrix(*host);
        KeyboardModulationPage curves(*host); ModernFxPage fx(*host);
        for (juce::Component* page : std::initializer_list<juce::Component*>{&mseg,&lfo,&matrix,&curves,&fx})
            for (auto* child : page->getChildren())
                if (auto* slider = dynamic_cast<juce::Slider*>(child))
                {
                    ++count;
                    // Exercise each attached control independently of model-dependent UI availability.
                    slider->setEnabled(true);
                    slider->setValue((slider->getMinimum()+slider->getMaximum())*.5, juce::sendNotificationSync);
                    probe.clear(); const auto before = slider->getValue();
                    check(slider->keyPressed(juce::KeyPress(juce::KeyPress::rightKey)) && slider->getValue()>before
                              && probe.kinds == std::vector<int>({1,0,-1}) && probe.parameters.size()==3
                              && probe.parameters[0]==probe.parameters[1] && probe.parameters[1]==probe.parameters[2],
                          "modern slider arrow reaches one attached parameter with ordered begin/value/end");
                    probe.clear(); slider->setEnabled(false); const auto disabled = slider->getValue();
                    check(!slider->keyPressed(juce::KeyPress(juce::KeyPress::rightKey))
                              && slider->getValue()==disabled && probe.kinds.empty(),
                          "disabled modern slider rejects arrow mutation and host notifications");
                    slider->setEnabled(true); probe.clear();
                    {
                        juce::Slider::ScopedDragNotification gesture(*slider);
                        const auto active = slider->getValue();
                        check(!slider->keyPressed(juce::KeyPress(juce::KeyPress::rightKey))
                                  && slider->getValue()==active && probe.kinds==std::vector<int>({1}),
                              "modern arrow cannot nest or mutate an active edit");
                    }
                    check(probe.kinds==std::vector<int>({1,-1}), "existing modern edit finishes balanced");
                }
    }
    check(count==36, "regression covers all 36 attached sliders across five modern page families");
    for (auto* parameter : host->getParameters()) parameter->removeListener(&probe);
}
}
