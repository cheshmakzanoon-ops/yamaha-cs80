#include "plugin/PluginProcessor.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>
namespace {
int checks=0,failures=0;
void check(bool ok,const char* text) { ++checks; if(!ok) { ++failures; std::cerr<<"FAIL: "<<text<<'\n'; } }
using Setter=void(Cs80AudioProcessor::*)(float) noexcept;
std::vector<float> capture(Setter setter,bool circuit,bool sixteen,bool send,bool malformed,float invalid)
{
    juce::TemporaryFile fixture("-ui-control-mailbox");
    auto host=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    host->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
    host->setParamValue(cs80::P_parityMode,0); host->setParamValue(cs80::P_voiceMode,sixteen ? 1.f : 0.f);
    for(int key : std::array<int,6>{cs80::P_drift,cs80::P_voiceDispersion,cs80::P_analogNoise,cs80::P_detune,cs80::P_kasScanTiming,cs80::P_mpeOn})
        host->setParamValue(key,0);
    host->setParamValue(cs80::P_modWheelVcf,1); host->setParamValue(cs80::P_pitchBendRange,12);
    host->prepareToPlay(48000,256);
    juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer midi;
    for(int n=0;n<(sixteen ? 16 : 2);++n) midi.addEvent(juce::MidiMessage::noteOn(1,48+n,.8f),0);
    host->processBlock(audio,midi); midi.clear();
    if(send) (host.get()->*setter)(.75f);
    if(malformed) (host.get()->*setter)(invalid);
    std::vector<float> result;
    for(int block=0;block<8;++block) {
        host->processBlock(audio,midi);
        for(int frame=0;frame<256;++frame) for(int channel=0;channel<2;++channel)
            result.push_back(audio.getSample(channel,frame));
    }
    return result;
}
}
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    for(auto setter : {&Cs80AudioProcessor::setRibbonFromUI,&Cs80AudioProcessor::setPitchWheelFromUI,&Cs80AudioProcessor::setModWheelFromUI})
    for(bool circuit : {false,true}) for(bool sixteen : {false,true}) {
        const auto expected=capture(setter,circuit,sixteen,true,false,0);
        const auto absent=capture(setter,circuit,sixteen,false,false,0);
        check(expected!=absent,"valid queued UI control changes sounding audio");
        for(float invalid : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
            check(capture(setter,circuit,sixteen,true,true,invalid)==expected,
                  "nonfinite UI input cannot overwrite a valid pending command");
            check(capture(setter,circuit,sixteen,false,true,invalid)==absent,
                  "nonfinite UI input without pending command remains inert");
        }
    }
    std::cout<<"UI control mailboxes: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures ? 1 : 0;
}
