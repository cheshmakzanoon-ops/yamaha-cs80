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
    const juce::String scale="Mapped anchor\n7\n9/8\n5/4\n4/3\n3/2\n5/3\n15/8\n2/1\n";
    auto mapping=[](bool restricted) {
        return juce::String("12\n")+(restricted ? "60\n64\n" : "0\n127\n")
            +"60\n69\n440.0\n7\n0\nx\n1\nx\n2\n3\nx\n4\nx\n5\nx\n6\n";
    };
    for(bool circuit : {false,true}) for(bool extended : {false,true})
    {
        auto full=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        auto limited=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        for(auto* host : {full.get(),limited.get()}) {
            host->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
            host->setPure(false);host->setParamValue(cs80::P_tuningMode,1);
            host->setParamValue(cs80::P_voiceMode,extended ? 1.f:0.f);
            for(int p : std::array<int,6>{cs80::P_arpOn,cs80::P_mpeOn,cs80::P_kasScanTiming,cs80::P_analogNoise,cs80::P_drift,cs80::P_voiceDispersion})host->setParamValue(p,0);
            check(host->loadScalaTuning(scale,mapping(host==limited.get())).isEmpty(),"mapped anchor session loads");
        }
        const auto saved=stateText(*limited);restore(*limited,saved);
        check(stateText(*limited)==saved,"restricted reference mapping survives session roundtrip");
        for(auto* host : {full.get(),limited.get()})host->prepareToPlay(48000,256);
        juce::AudioBuffer<float> a(2,256),b(2,256);juce::MidiBuffer ma,mb;
        for(int note : {60,62,64}) {ma.addEvent(juce::MidiMessage::noteOn(1,note,.8f),0);mb.addEvent(juce::MidiMessage::noteOn(1,note,.8f),0);}
        bool identical=true,audible=false;
        for(int block=0;block<8;++block) {
            full->processBlock(a,ma);limited->processBlock(b,mb);ma.clear();mb.clear();
            for(int c=0;c<2;++c)for(int n=0;n<256;++n) {
                identical=identical && a.getSample(c,n)==b.getSample(c,n);
                audible=audible || std::abs(a.getSample(c,n))>1e-6f;
            }
        }
        check(identical && audible,"playable-range restriction preserves audible mapped pitches through real processor");
        for(auto* midi : {&ma,&mb}) {midi->addEvent(juce::MidiMessage::allSoundOff(1),0);midi->addEvent(juce::MidiMessage::noteOn(1,69,.8f),0);}
        full->processBlock(a,ma);limited->processBlock(b,mb);
        check(full->getEngine().activeVoices()>0 && limited->getEngine().activeVoices()==0 && limited->getExtensionEngine().activeVoices()==0,"out-of-range reference remains unplayable after anchoring tuning");
    }
    std::cout<<"Scala session admission: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures ? 1 : 0;
}
