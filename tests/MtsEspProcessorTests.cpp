// Real processor/adapter, deterministic external C API. Not a live MTS service.
#include "plugin/PluginProcessor.h"
#include "vendor/mts-esp/Client/libMTSClient.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

struct MTSClient { bool channelFiltering=false; };
namespace {
bool master = false, invalid = false;
double transpose = .25;
unsigned channelQueries=0;
bool multichannel=false;
int registrations = 0, deregistrations = 0, queries = 0, checks = 0, failures = 0;
void check(bool pass, const char* text) { ++checks; if (!pass) { ++failures; std::cerr << "FAIL: " << text << '\n'; } }
}
extern "C" {
MTSClient* MTS_RegisterClient() { ++registrations; return new MTSClient; }
void MTS_DeregisterClient(MTSClient* c) { ++deregistrations; delete c; }
bool MTS_HasMaster(MTSClient*) { return master; }
double MTS_NoteToFrequency(MTSClient* client, char note, signed char channel)
{
    ++queries;
    if(channel>=0 && channel<16) channelQueries |= 1u<<channel;
    if (invalid && note == 64 && channel==15) return std::numeric_limits<double>::quiet_NaN();
    const double offset=multichannel && channel>=0 && client->channelFiltering ? (channel+1)*.03125 : 0.;
    return 440.0 * std::exp2((static_cast<unsigned char>(note) + transpose + offset - 69.) / 12.);
}
bool MTS_ShouldFilterNote(MTSClient* client, char note, signed char channel) {
    client->channelFiltering=channel>=0;
    return note==23 && (!multichannel || channel<0 || channel==1 || channel==15);
}
}
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-mts-esp-processor");
    for (bool channels : {false,true}) for (bool circuit : {false, true}) for (bool priorActive : {false, true}) for (int frames : {0, 64})
    {
        master = false; invalid = false; transpose = .25; multichannel=channels; channelQueries=0;
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        host->setParamValue(cs80::P_parityMode, 0); host->setParamValue(cs80::P_tuningMode, 2);
        host->setParamValue(cs80::P_voiceMode, priorActive ? 1.f : 0.f);
        for (int parameter : std::array<int,7>{cs80::P_kasScanTiming, cs80::P_arpOn, cs80::P_mpeOn,
                              cs80::P_voiceDispersion, cs80::P_drift, cs80::P_analogNoise, cs80::P_detune})
            host->setParamValue(parameter, 0);
        host->prepareToPlay(48000, 128);
        juce::AudioBuffer<float> audio(2, frames); juce::MidiBuffer midi;
        auto tick = [&] { host->processBlock(audio, midi); midi.clear(); };
        auto tables = [&](float offset, bool independent=false) {
            for (const auto* e : std::array<const cs80::Engine*,2>{&host->getEngine(), &host->getExtensionEngine()})
                for (int channel=1; channel<=16; ++channel)
                    check(std::abs(e->getMtsTuning(channel).pitch[64] - (64.f+offset+(channels && !independent ? channel*.03125f : 0.f))) < 1e-5f
                          && e->getMtsTuning(channel).mapped[23]==(channels && !independent && channel!=2 && channel!=16), "both engines retain complete master table on every channel");
        };
        tick();
        if (priorActive) { host->setParamValue(cs80::P_voiceMode, 0); tick(); }
        master = true; tick();
        check(host->isMtsEspConnected(), "valid master update reports connected"); tables(.25f);
        check(channelQueries==0xffffu,"conventional MIDI queries every upstream channel");
        transpose = .5; invalid = true; tick();
        check(!host->isMtsEspConnected(), "invalid master table is not accepted"); tables(.25f);
        invalid = false; tick(); tables(.5f);
        for(int n=0;n<8;++n) midi.addEvent(juce::MidiMessage::noteOn(n+1,48+n,.8f),0);
        audio.setSize(2,256); tick();
        transpose=.75;
        for(int block=0;block<16;++block) tick();
        int held=0;
        for(int v=0;v<8;++v) if(const auto info=host->getEngine().getVoiceInfo(v); info.active) {
            ++held;
            const float expected=float(info.note)+.75f+(channels ? info.channel*.03125f : 0.f);
            const float error=12.f*std::log2(host->getEngine().getRankInfo(v,0).oscillatorHz/cs80::midiToHz(expected));
            check(std::isfinite(error) && std::abs(error)<1e-3f,"held voices follow their live channel updates");
        }
        check(held==8,"held retuning fixture exercises every primary voice");
        transpose=.5; tick();
        std::array<unsigned char,21> sysex {0xf0,0x7f,0x7f,8,8,0,0,2,89,89,89,89,89,89,89,89,89,89,89,89,0xf7};
        midi.addEvent(sysex.data(),int(sysex.size()),0);tick();
        check(host->getEngine().getMtsTuning(2).pitch[64]==64.25f,"same-block SysEx follows master polling");
        tick();tables(.5f);
        host->setParamValue(cs80::P_mpeOn,1); tick(); tables(.5f,true);
        host->setParamValue(cs80::P_mpeOn,0); tick(); tables(.5f);
        host->setPure(true); channelQueries=0; tick(); tables(.5f,true);
        check(channelQueries==0,"PURE preserves channel-independent polling");
        host->setPure(false); tick(); tables(.5f);
        master = false; tick();
        check(!host->isMtsEspConnected(), "disconnect is observed"); tables(.5f);
        host->setParamValue(cs80::P_voiceMode, 1); tick(); tables(.5f);
        for (int n=0;n<16;++n) midi.addEvent(juce::MidiMessage::noteOn(1+n,48+n,.8f),0);
        audio.setSize(2,128); tick();
        int active=0;
        for (const auto* e : std::array<const cs80::Engine*,2>{&host->getEngine(), &host->getExtensionEngine()})
            for(int v=0;v<8;++v) if (const auto info=e->getVoiceInfo(v); info.active)
            {
                ++active;
                const float error=12.f*std::log2(e->getRankInfo(v,0).oscillatorHz / cs80::midiToHz(float(info.note)+.5f+(channels ? info.channel*.03125f : 0.f)));
                check(std::isfinite(error) && std::abs(error)<1e-3f, "both active engines sound retained master pitch after disconnect");
            }
        check(active==16, "all sixteen voices exercise retained master pitch");
    }
    check(registrations == 16 && registrations == deregistrations && queries > 0,
          "processor clients register and deregister through fixture backend");
    std::cout << "MTS-ESP processor fixture: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
