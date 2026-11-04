#pragma once

// Real JUCE processor audio->UI model-tap handoff. Engine::RankInfo is
// render-thread-only and is compared solely AFTER a processBlock completes.
#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace cs80::tests
{
template <typename Check>
void testVoiceMonitorHost(Check check)
{
    Cs80AudioProcessor host;
    host.setParamValue(P_analogNoise, 0.0f);
    host.setParamValue(P_voiceDispersion, 0.0f);
    host.prepareToPlay(48000.0, 256);
    const auto empty = host.getVoiceMonitorSnapshot();
    check(! empty.ranks[0].active && ! empty.ranks[1].active,
          "freshly prepared processor has no stale audio rank taps");

    juce::AudioBuffer<float> output(2, 256);
    juce::MidiBuffer midi;
    auto process = [&]
    {
        output.clear();
        host.processBlock(output, midi);
        midi.clear();
    };
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    process();
    const auto sounding = host.getVoiceMonitorSnapshot();
    const auto rankI = host.getEngine().getRankInfo(0, 0);
    const auto rankII = host.getEngine().getRankInfo(0, 1);
    const auto match = [](const VoiceMonitorRank& tap, const Engine::RankInfo& truth)
    {
        const bool matched = tap.active && tap.ampEnvelope == truth.ampEnvelope
            && tap.filterEnvelope == truth.filterEnvelope
            && tap.hpfHz == truth.hpfHz
            && tap.lpfHz == truth.lpfHz;
        if (! matched)
            std::printf("TAP DIAG: active=%d amp=%.6g/%.6g filter=%.6g/%.6g HPF=%.6g/%.6g LPF=%.6g/%.6g\n",
                        tap.active, tap.ampEnvelope, truth.ampEnvelope,
                        tap.filterEnvelope, truth.filterEnvelope,
                        tap.hpfHz, truth.hpfHz, tap.lpfHz, truth.lpfHz);
        return matched;
    };
    check(match(sounding.ranks[0], rankI) && match(sounding.ranks[1], rankII),
          "audio callback publishes both real Engine rank filter/envelope nodes bit-exactly");

    // Sixteen voices are two Engine instances. Force the loudest velocity-
    // weighted envelope to belong to a note in slots 8..15, then require its
    // two monitor ranks to match the second Engine, not stale first-engine taps.
    host.requestPanicFromUI();
    process();
    host.setParamValue(P_voiceMode, 1.0f);
    process();
    for (int i = 0; i < 16; ++i)
        midi.addEvent(juce::MidiMessage::noteOn(1, 40 + i, (juce::uint8) (i < 8 ? 10 : 120)), i);
    process();
    const auto sixteen = host.getVoiceMonitorSnapshot();
    check(host.getVoiceActivitySnapshot().capacity == 16
              && host.getVoiceActivitySnapshot().activeCount() == 16,
          "voice-monitor sixteen-note fixture actually owns both eight-voice engines");

    float selectedScore = -1.0f;
    int selectedSlot = -1;
    for (int slot = 0; slot < 16; ++slot)
    {
        auto& engine = slot < 8 ? host.getEngine() : host.getExtensionEngine();
        const int voice = slot % 8;
        if (! engine.getVoiceInfo(voice).active) continue;
        const auto a = engine.getRankInfo(voice, 0), b = engine.getRankInfo(voice, 1);
        const float score = std::max(a.ampEnvelope, b.ampEnvelope) * engine.getVoiceInfo(voice).velocity;
        if (! std::isfinite(score) || score <= selectedScore) continue;
        selectedScore = score;
        selectedSlot = slot;
    }
    check(selectedSlot >= 8, "high-velocity ninth-to-sixteenth voices own the monitor over quiet first-engine voices");
    if (selectedSlot >= 0)
    {
        auto& engine = selectedSlot < 8 ? host.getEngine() : host.getExtensionEngine();
        check(match(sixteen.ranks[0], engine.getRankInfo(selectedSlot % 8, 0))
                  && match(sixteen.ranks[1], engine.getRankInfo(selectedSlot % 8, 1)),
              "rank-model monitor follows the loudest note across both engine instances at one audio sample boundary");
    }

    host.requestPanicFromUI();
    process();
    const auto cleared = host.getVoiceMonitorSnapshot();
    check(! cleared.ranks[0].active && ! cleared.ranks[1].active,
          "audio panic clears previously active rank telemetry without a UI timer or processor race");
    host.releaseResources();
    const auto released = host.getVoiceMonitorSnapshot();
    check(! released.ranks[0].active && ! released.ranks[1].active,
          "releasing the plugin clears rank taps before later editor reads");
}
} // namespace cs80::tests
