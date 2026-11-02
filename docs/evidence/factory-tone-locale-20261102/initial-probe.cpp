#include "plugin/PluginProcessor.h"
#include <clocale>
#include <cstdio>
#include <memory>
#include <string>

int main()
{
    juce::ScopedJuceInitialiser_GUI messageManagerOnly;
    int checks=0, failures=0;
    const std::string original=std::setlocale(LC_NUMERIC,nullptr);
    for(const char* locale:{"C","de_DE.utf8","fr_FR.utf8"})
    {
        const char* selected=std::setlocale(LC_NUMERIC,locale);
        if(selected==nullptr){std::printf("UNAVAILABLE locale=%s\n",locale);return 2;}
        juce::TemporaryFile root("-cs80-tone-locale");
        auto host=std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
        auto* p=host->getAPVTS().getParameter("factory_tone_revision");
        if(p==nullptr)return 3;
        p->setValueNotifyingHost(1);
        juce::MemoryBlock saved;
        host->getStateInformation(saved);
        auto xml=juce::parseXML(saved.toString());
        auto tree=juce::ValueTree::fromXml(*xml);
        const auto encoded=tree.getChildWithProperty("id","factory_tone_revision").getProperty("value").toString();
        p->setValueNotifyingHost(0);
        host->setStateInformation(saved.getData(),(int)saved.getSize());
        const float restored=host->getAPVTS().getRawParameterValue("factory_tone_revision")->load();
        std::printf("locale=%s decimal=%s saved=%s restored=%g expected=1\n",
                    locale,std::localeconv()->decimal_point,encoded.toRawUTF8(),restored);
        ++checks;
        if(restored!=1){++failures;std::puts("FAIL: actual saved V2 session must restore V2 independently of host numeric locale");}
    }
    std::setlocale(LC_NUMERIC,original.c_str());
    std::printf("Locale processor probe: %d checks, %d failures\n",checks,failures);
    return failures==0?0:1;
}
