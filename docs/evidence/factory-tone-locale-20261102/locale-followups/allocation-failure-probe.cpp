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
    auto check=[&](bool ok,const char* text){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",text);}};
    for(const char* name:{"C","de_DE.utf8"})
    {
        const auto* selected=std::setlocale(LC_NUMERIC,name);
        check(selected!=nullptr,"required locale exists");
        if(selected==nullptr)return 2;
        const std::string expectedLocale=selected;
        juce::TemporaryFile root("-cs80-null-numeric-locale");
        auto host=std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
        host->selectHardwareSource(0,0);host->selectHardwareSource(1,1);
        auto* p=host->getAPVTS().getParameter("factory_tone_revision");
        if(p==nullptr)return 3;
        p->setValueNotifyingHost(1);
        check(host->getAPVTS().getRawParameterValue("factory_tone_revision")->load()==1,
              "fallback probe begins with a real live V2 opt-in");
        juce::MemoryBlock saved;host->getStateInformation(saved);
        auto xml=juce::parseXML(saved.toString());auto tree=juce::ValueTree::fromXml(*xml);
        check(tree.getChildWithProperty("id","factory_tone_revision").getProperty("value").toString()=="1.0",
              "fallback probe restores actual V2 serialization");
        host->setStateInformation(saved.getData(),(int)saved.getSize());
        check(host->getAPVTS().getRawParameterValue("factory_tone_revision")->load()==0,
              "simulated locale-allocation failure falls back to V1, not live V2");
        host->prepareToPlay(48000,32);
        for(int rank=0;rank<2;++rank)
        {
            cs80::source_factory::ToneControls expected;
            check(cs80::source_factory::compileTone(rank,rank,expected)
                      && host->getEngine().getSourceBusReadout(rank)==expected.busVolts,
                  "allocation-failure fallback reaches actual V1 source buses");
        }
        check(expectedLocale==std::setlocale(LC_NUMERIC,nullptr),
              "allocation-failure fallback leaves caller locale unchanged");
    }
    std::setlocale(LC_NUMERIC,original.c_str());
    std::printf("Locale allocation failure simulation: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
