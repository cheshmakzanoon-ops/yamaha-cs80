#pragma once
#include "plugin/PluginProcessor.h"
#include <cmath>
#include <array>
#include <memory>
#include <vector>

namespace cs80::tests
{
template<class Check> void testMtsDormantEngineHost(Check check)
{
    juce::TemporaryFile fixture("-mts-dormant-engine");
    for (bool circuit : {false, true}) for (bool realtime : {false, true})
    for (bool previouslyActive : {false, true}) for (int frames : {0, 64}) for (int sub : {8, 9})
    {
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        host->setParamValue(P_parityMode, 0); host->setParamValue(P_tuningMode, 2);
        host->setParamValue(P_voiceMode, previouslyActive ? 1.f : 0.f);
        host->setParamValue(P_kasScanTiming, 0); host->setParamValue(P_arpOn, 0);
        host->setParamValue(P_mpeOn, 0); host->setParamValue(P_voiceDispersion, 0);
        host->setParamValue(P_drift, 0); host->setParamValue(P_analogNoise, 0); host->setParamValue(P_detune, 0);
        host->prepareToPlay(48000, 128);
        juce::AudioBuffer<float> audio(2, frames); juce::MidiBuffer midi;
        auto tick = [&] { host->processBlock(audio, midi); midi.clear(); };
        tick();
        if (previouslyActive) { host->setParamValue(P_voiceMode, 0); tick(); }
        std::vector<std::uint8_t> bytes {0xF0, static_cast<std::uint8_t>(realtime ? 0x7F : 0x7E),
                                        0x7F, 8, static_cast<std::uint8_t>(sub), 0, 0, 2};
        for (int pc=0; pc<12; ++pc)
            if(sub==8) bytes.push_back(89); else { bytes.push_back(80); bytes.push_back(0); }
        bytes.push_back(0xF7);
        midi.addEvent(bytes.data(), int(bytes.size()), 0); tick();
        for (const auto* engine : std::array<const Engine*, 2>{&host->getEngine(), &host->getExtensionEngine()})
            check(engine->getMtsTuning(2).pitch[60] == 60.25f && engine->getMtsTuning(1).pitch[60] == 60.f,
                  "host SysEx stages selected-channel tuning in primary and dormant extension");
        host->setParamValue(P_voiceMode, 1); tick();
        for(int i=0;i<16;++i) midi.addEvent(juce::MidiMessage::noteOn(1+i%2, 48+i, .8f), 0);
        audio.setSize(2,128); tick();
        int active=0;
        for(const auto* engine : std::array<const Engine*, 2>{&host->getEngine(), &host->getExtensionEngine()})
            for(int v=0;v<8;++v) if(const auto info=engine->getVoiceInfo(v); info.active)
            {
                ++active;
                const float expected=float(info.note)+(info.channel==2 ? .25f : 0.f);
                const float error=12.f*std::log2(engine->getRankInfo(v,0).oscillatorHz / midiToHz(expected));
                check(std::isfinite(error) && std::abs(error)<1e-3f,
                      "new sixteen-voice notes use prior channel tuning regardless of engine allocation");
            }
        check(active==16, "transition fixture exercises all sixteen allocated voices");
    }
}
}
