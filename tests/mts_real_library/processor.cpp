// Direct production processor with real official MTS client/library, no mock API.
#include "plugin/PluginProcessor.h"
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
namespace {
void check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
bool near(float a, double b) { return std::abs(a-b)<.00003; }
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    int failed=0;
    {
        juce::TemporaryFile fixture("-real-mts-processor");
        auto host=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->setEngineMode(argc>1 && std::string(argv[1])=="circuit"
            ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        host->setParamValue(cs80::P_parityMode,0);
        host->setParamValue(cs80::P_tuningMode,2);
        host->setParamValue(cs80::P_voiceMode,0);
        for(int parameter : std::array<int,7>{cs80::P_kasScanTiming,cs80::P_arpOn,cs80::P_mpeOn,
                            cs80::P_voiceDispersion,cs80::P_drift,cs80::P_analogNoise,cs80::P_detune})
            host->setParamValue(parameter,0);
        host->prepareToPlay(48000,128);
        juce::AudioBuffer<float> audio(2,128);
        juce::MidiBuffer midi;
        auto tick=[&] { audio.clear();host->processBlock(audio,midi);midi.clear(); };
        auto tables=[&](double offset,bool independent=false) {
            for(const auto* engine : std::array<const cs80::Engine*,2>{&host->getEngine(),&host->getExtensionEngine()})
                for(int c=0;c<16;++c) for(int n=0;n<128;++n) {
                    const auto& t=engine->getMtsTuning(c+1);
                    check(near(t.pitch[n],n+offset+(independent?0:1+c*.1)),"complete primary/extension channel pitch");
                    check(t.mapped[n]==(independent ? n!=60 : !(c==3&&n==61)),"complete primary/extension filter");
                }
        };
        auto active=[&] { int count=0;for(const auto* e:std::array<const cs80::Engine*,2>{&host->getEngine(),&host->getExtensionEngine()})
            for(int v=0;v<8;++v) if(e->getVoiceInfo(v).active) ++count;return count; };
        std::cout<<"READY"<<std::endl;
        std::string command;
        while(std::getline(std::cin,command)) {
            if(command=="quit") break;
            try {
                if(command=="initial" || command=="update") {
                    tick();check(host->isMtsEspConnected(),"processor connected through real IPC");
                    check(host->getMtsEspScaleName()=="Real IPC qualification","real scale name");
                    tables(command=="initial"?.25:.5);
                } else if(command=="filtered") {
                    midi.addEvent(juce::MidiMessage::noteOn(4,61,.8f),0);tick();
                    check(active()==0,"real channel filter blocks note allocation");
                } else if(command=="mpe") {
                    host->setParamValue(cs80::P_mpeOn,1);tick();tables(.5,true);
                } else if(command=="pure") {
                    host->setParamValue(cs80::P_mpeOn,0);host->setPure(true);tick();tables(.5,true);
                } else if(command=="conventional") {
                    host->setPure(false);tick();tables(.5);
                } else if(command=="badchannel") {
                    tick();check(!host->isMtsEspConnected(),"invalid real bank rejected");tables(.5);
                } else if(command=="disconnect") {
                    tick();check(!host->isMtsEspConnected(),"real disconnect observed");tables(.5);
                    check(host->getMtsEspScaleName().isEmpty(),"disconnected scale name empty");
                } else if(command=="activate16") {
                    check(active()==0,"extension starts dormant without voices");
                    host->setParamValue(cs80::P_voiceMode,1);tick();tables(.5);
                    for(int n=0;n<16;++n) midi.addEvent(juce::MidiMessage::noteOn(n+1,48+n,.8f),0);
                    double energy=0;
                    for(int block=0;block<32;++block) {
                        tick();
                        for(int c=0;c<audio.getNumChannels();++c) for(int n=0;n<audio.getNumSamples();++n) {
                            const double value=audio.getSample(c,n);check(std::isfinite(value),"finite real processor audio");energy+=value*value;
                        }
                    }
                    check(active()==16,"sixteen fresh voices after disconnected dormant activation");
                    for(const auto* engine:std::array<const cs80::Engine*,2>{&host->getEngine(),&host->getExtensionEngine()}) for(int v=0;v<8;++v) {
                        const auto info=engine->getVoiceInfo(v);
                        const double expected=cs80::midiToHz(float(info.note+.5+1+(info.channel-1)*.1));
                        const double error=12*std::log2(engine->getRankInfo(v,0).oscillatorHz/expected);
                        check(std::isfinite(error)&&std::abs(error)<.001,"fresh primary/extension voices retain actual channel tuning");
                    }
                    check(energy>1e-8,"nonzero audio positive control");
                    std::cerr<<"AUDIO frames=4096 voices=16 sum_squares="<<energy<<'\n';
                } else check(false,"unknown command");
                std::cout<<"PASS "<<command<<std::endl;
            } catch(const std::exception& error) { failed=1;std::cout<<"FAIL "<<command<<" "<<error.what()<<std::endl; }
        }
        host->releaseResources();
    }
    std::cout<<"EXIT"<<std::endl;
    return failed;
}
