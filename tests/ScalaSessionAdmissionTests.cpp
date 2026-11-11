#include "plugin/PluginProcessor.h"
#include <iostream>
#include <memory>
namespace {
int checks=0, failures=0;
void check(bool ok,const char* text) { ++checks; if(!ok) { ++failures; std::cerr<<"FAIL: "<<text<<'\n'; } }
juce::String stateText(Cs80AudioProcessor& host) {
    juce::MemoryBlock data; host.getStateInformation(data);
    return juce::String::fromUTF8(static_cast<const char*>(data.getData()),int(data.getSize()));
}
void restore(Cs80AudioProcessor& host,const juce::String& text) {
    host.setStateInformation(text.toRawUTF8(),int(text.getNumBytesAsUTF8()));
}
}
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-scala-session-admission");
    auto donor=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    donor->setParamValue(cs80::P_volume,.2f); donor->setParamValue(cs80::P_tuningMode,1);
    donor->setPresetName("Incoming session");
    check(donor->loadScalaTuning("Incoming scale\n1\n2/1\n").isEmpty(),"valid donor scale loaded");
    const auto valid=stateText(*donor);
    for(bool previousScale : {false,true}) for(bool invalidMapping : {false,true})
    {
        auto host=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->setParamValue(cs80::P_volume,.7f); host->setPresetName("Retained session");
        host->selectHardwareSource(0,2);
        if(previousScale) check(host->loadScalaTuning("Previous scale\n1\n3/1\n").isEmpty(),"valid prior scale loaded");
        host->getEditorState().circuitNotes.store(true);
        host->getMidiLearn().assign(20,host->hostParameterIndex(cs80::P_volume));
        const auto beforeMetadata=host->getPresetMetadata();
        const auto before=stateText(*host);
        auto xml=juce::XmlDocument::parse(valid); auto tree=juce::ValueTree::fromXml(*xml);
        auto tuning=tree.getChildWithName("WS3_TUNING");
        tuning.setProperty(invalidMapping ? "kbm" : "scl","malformed tuning",nullptr);
        const auto bad=tree.createXml()->toString();
        restore(*host,bad);
        check(stateText(*host)==before,"malformed session tuning rejects complete restore without side effects");
        check(host->getPresetMetadata().name==beforeMetadata.name && host->getHardwareSource(0)==2,
              "malformed session retains metadata and source selection");
        check(host->hasScalaTuning()==previousScale,"malformed session retains prior tuning presence");
        restore(*host,valid);
        check(host->getScalaDescription()=="Incoming scale" && std::abs(host->getParamValue(cs80::P_volume)-.2f)<1e-6f,
              "valid tuning session still restores scale and parameters");
        check(host->getPresetMetadata().name=="Incoming session","valid session restores metadata");
        tree=juce::ValueTree::fromXml(*juce::XmlDocument::parse(valid));
        tree.removeChild(tree.getChildWithName("WS3_TUNING"),nullptr);
        restore(*host,tree.createXml()->toString());
        check(!host->hasScalaTuning(),"session without tuning retains existing clear behavior");
    }
    std::cout<<"Scala session admission: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures ? 1 : 0;
}
