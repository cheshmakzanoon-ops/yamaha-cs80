#include "plugin/PluginProcessor.h"
#include <clocale>
#include <cstdio>
#include <memory>
#include <string>
int main()
{
    juce::ScopedJuceInitialiser_GUI messageManagerOnly;
    const std::string original = std::setlocale(LC_NUMERIC, nullptr);
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char* message) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
    };
    for (const char* name : {"C", "de_DE.utf8"})
    {
        const auto* selected = std::setlocale(LC_NUMERIC, name);
        check(selected != nullptr, "required allocation-fallback locale exists");
        if (selected == nullptr) continue;
        const std::string expectedLocale = selected;
        juce::TemporaryFile root("-cs80-policy-null-locale");
        auto host = std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
        host->prepareToPlay(48000, 32);
        juce::AudioBuffer<float> audio(2, 0);
        juce::MidiBuffer midi;
        for (int schema : {0, 1, 2, 3})
            for (float savedEngine : {0.0f, 1.0f})
            {
                const float fallback = schema == 3 ? 1.0f : 0.0f;
                host->setParamValue(cs80::P_sourcePerformance, savedEngine);
                host->setParamValue(cs80::P_parityMode, 1-savedEngine);
                juce::MemoryBlock bytes;
                host->getStateInformation(bytes);
                const auto xml = juce::parseXML(bytes.toString());
                auto state = juce::ValueTree::fromXml(*xml);
                check(state.getChildWithProperty("id", "source_performance").getProperty("value").toString()
                          == (savedEngine == 1 ? "1.0" : "0.0")
                          && state.getChildWithProperty("id", "parity_mode").getProperty("value").toString()
                          == (savedEngine == 1 ? "0.0" : "1.0"),
                      "allocation-failure probe starts from actual binary-policy XML");
                if (schema == 0) state.removeProperty("stateSchemaVersion", nullptr);
                else state.setProperty("stateSchemaVersion", schema, nullptr);
                host->setParamValue(cs80::P_sourcePerformance, 1-fallback);
                host->setParamValue(cs80::P_parityMode, 1);
                host->processBlock(audio, midi);
                const auto previous = host->getEngine().getRenderPolicy();
                const auto text = state.createXml()->toString();
                host->setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
                check(host->getEngine().getRenderPolicy() == previous,
                      "allocation failure never mutates Engine on the restoring thread");
                check(host->getParamValue(cs80::P_sourcePerformance) == fallback,
                      "allocation failure uses writing-schema ENGINE fallback, not live value");
                check(host->getParamValue(cs80::P_parityMode) == 0,
                      "allocation failure restores PURE off, not live isolation");
                host->processBlock(audio, midi);
                check(host->getEngine().getRenderPolicy() == (schema == 3
                          ? cs80::RenderPolicy::sourcePerformance : cs80::RenderPolicy::legacy),
                      "allocation failure publishes exact schema fallback policy on audio transaction");
                check(expectedLocale == std::setlocale(LC_NUMERIC, nullptr),
                      "allocation failure preserves caller numeric locale");
            }
    }
    std::setlocale(LC_NUMERIC, original.c_str());
    std::printf("Policy locale allocation failure simulation: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
