#include "plugin/PluginProcessor.h"
#include <clocale>
#include <cstdio>
#include <memory>
#include <string>
int main()
{
    juce::ScopedJuceInitialiser_GUI messageManagerOnly;
    const std::string original=std::setlocale(LC_NUMERIC,nullptr);
    int checks=0,failures=0;
    for(const char* locale:{"C","de_DE.utf8","fr_FR.utf8"})
    {
        if(std::setlocale(LC_NUMERIC,locale)==nullptr)return 2;
        juce::TemporaryFile root("-cs80-old-policy-locale");
        auto host=std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
        host->setParamValue(cs80::P_sourcePerformance,0);
        host->setParamValue(cs80::P_parityMode,1);
        host->prepareToPlay(48000,32);
        juce::MemoryBlock saved;host->getStateInformation(saved);
        auto xml=juce::parseXML(saved.toString());auto tree=juce::ValueTree::fromXml(*xml);
        const auto source=tree.getChildWithProperty("id","source_performance").getProperty("value").toString();
        const auto pure=tree.getChildWithProperty("id","parity_mode").getProperty("value").toString();
        host->setParamValue(cs80::P_sourcePerformance,1);host->setParamValue(cs80::P_parityMode,0);
        juce::AudioBuffer<float> audio(2,0);juce::MidiBuffer midi;
        host->processBlock(audio,midi);
        host->setStateInformation(saved.getData(),(int)saved.getSize());host->processBlock(audio,midi);
        const float actualSource=host->getParamValue(cs80::P_sourcePerformance);
        const float actualPure=host->getParamValue(cs80::P_parityMode);
        const bool actualPolicy=cs80::isPureCs80(host->getEngine().getRenderPolicy());
        std::printf("locale=%s source_xml=%s pure_xml=%s restored_source=%g restored_pure=%g effective_pure=%d\n",
                    locale,source.toRawUTF8(),pure.toRawUTF8(),actualSource,actualPure,actualPolicy?1:0);
        ++checks;if(actualSource!=0){++failures;std::puts("FAIL: saved Legacy ENGINE must not become Circuit under host numeric locale");}
        ++checks;if(actualPure!=1||!actualPolicy){++failures;std::puts("FAIL: saved PURE must not disable isolation under host numeric locale");}
    }
    std::setlocale(LC_NUMERIC,original.c_str());
    std::printf("Existing policy locale probe: %d checks, %d failures\n",checks,failures);
    return failures==0?0:1;
}
