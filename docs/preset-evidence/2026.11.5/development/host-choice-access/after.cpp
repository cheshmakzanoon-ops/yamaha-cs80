#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
bool probe(juce::RangedAudioParameter* parameter) {
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter);
    if (choice == nullptr) return false;
    const auto& range = choice->getNormalisableRange();
    return choice->choices.joinIntoString("|") == "Standard|Xylophone bar|Steelpan shell|Choir ah|Choir oo"
        && parameter->getNumSteps() == 5 && range.start == 0.0f && range.end == 4.0f
        && choice->convertFrom0to1(parameter->getDefaultValue()) == 0.0f
        && parameter->getValue() == 0.25f;
}
