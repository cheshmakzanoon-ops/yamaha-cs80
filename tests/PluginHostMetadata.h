#pragma once

#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace cs80::tests
{
// Called by the existing JUCE host smoke harness after GUI initialization.
// These checks concern software capability/tail reporting, not analog fidelity.
template <typename Check>
void testPluginHostMetadata(Check&& check)
{
    Cs80AudioProcessor processor;
    const auto initialValues = processor.currentValues().v;
    const auto initialParameterCount = processor.getParameters().size();
    for (int query = 0; query < 8; ++query)
    {
        check(processor.supportsMPE(), "host advertises implemented MPE capability");
        const double tail = processor.getTailLengthSeconds();
        check(std::isinf(tail) && tail > 0.0,
              "host tail is explicitly unbounded, not a finite silence promise");
    }
    check(processor.currentValues().v == initialValues
              && processor.getParameters().size() == initialParameterCount,
          "metadata queries do not change parameters or defaults");

    // Capability remains advertised with MPE switched off and after obsolete
    // isolation requests, without changing the player's actual MPE setting.
    for (int mode = 0; mode < 3; ++mode)
    {
        ParamValues p;
        p[P_mpeOn] = mode == 0 ? 0.0f : 1.0f;
        p[P_mpeBendRange] = 48.0f;
        p[P_pitchBendRange] = 12.0f;
        p[P_subVco] = p[P_subAtVco] = p[P_modWheelSub] = p[P_initBend] = 0.0f;
        p[P_drift] = p[P_voiceDispersion] = p[P_analogNoise] = 0.0f;
        processor.applyParameterValues(p);
        processor.setParamValue(P_parityMode, mode == 2 ? 1.0f : 0.0f);
        processor.prepareToPlay(48000.0, 64);
        juce::AudioBuffer<float> block(2, 64);
        juce::MidiBuffer notes;
        notes.addEvent(juce::MidiMessage::noteOn(2, 60, (juce::uint8) 100), 0);
        notes.addEvent(juce::MidiMessage::noteOn(3, 60, (juce::uint8) 100), 0);
        processor.processBlock(block, notes);
        const auto before2 = processor.getEngine().getRankInfo(0, 0).oscillatorHz;
        const auto before3 = processor.getEngine().getRankInfo(1, 0).oscillatorHz;
        juce::MidiBuffer expression;
        expression.addEvent(juce::MidiMessage::pitchWheel(2, 12288), 0);
        expression.addEvent(juce::MidiMessage::channelPressureChange(2, 80), 0);
        processor.processBlock(block, expression);
        const auto moving2 = processor.getEngine().getRankInfo(0, 0).oscillatorHz;
        check(moving2 > before2 && moving2 < before2 * (mode == 0 ? std::sqrt(2.0f) : 4.0f),
              "circuit bend moves toward its target during the first 64 samples");
        // More than 30 declared 5 ms time constants lets the circuit's
        // controller dezipper settle before checking the exact musical ratio.
        juce::MidiBuffer noEvents;
        for (int settle = 0; settle < 128; ++settle)
            processor.processBlock(block, noEvents);
        const auto voice2 = processor.getEngine().getVoiceInfo(0);
        const auto voice3 = processor.getEngine().getVoiceInfo(1);
        check(voice2.channel == 2 && voice3.channel == 3,
              "MPE metadata fixture retains separate member-channel notes");
        check(processor.supportsMPE() && processor.getParamValue(P_mpeOn) == p[P_mpeOn]
                  && processor.getParamValue(P_sourcePerformance) == 1.0f
                  && processor.getParamValue(P_parityMode) == 0.0f,
              "capability queries and obsolete mode requests preserve the actual MPE control and circuit policy");
        check(std::abs(voice2.pressure - 80.0f / 127.0f) < 1.0e-6f
                  && voice3.pressure == 0.0f,
              "member channel pressure remains independent");
        const auto after2 = processor.getEngine().getRankInfo(0, 0).oscillatorHz;
        const auto after3 = processor.getEngine().getRankInfo(1, 0).oscillatorHz;
        if (mode != 0)
            check(voice2.bend == 24.0f && voice3.bend == 0.0f
                      && std::abs(after2 / before2 - 4.0f) < 1.0e-5f && after3 == before3,
                  "MPE bends only the addressed member by two octaves, including after retired isolation requests");
        else
            check(voice2.bend == 0.0f && voice3.bend == 0.0f
                      && std::abs(after2 / before2 - std::sqrt(2.0f)) < 1.0e-5f
                      && std::abs(after3 / before3 - std::sqrt(2.0f)) < 1.0e-5f,
                  "MPE-off global bend applies the independent six-semitone target to both notes");
        processor.releaseResources();
    }

    // Exercise the existing DSP at its supported low rate to keep the host
    // smoke bounded. This is not an all-rates FX decay calibration or a claim
    // that a positive-feedback IIR tail becomes mathematically zero.
    constexpr int rate = 8000, blockSize = 256;
    std::vector<float> audio(blockSize * 2);
    struct Window { double rms = 0.0; float peak = 0.0f; bool finite = true; };
    auto renderSecond = [&](Engine& engine)
    {
        Window result;
        double sum = 0.0;
        const int frames = (int) engine.getHostSampleRate();
        for (int frame = 0; frame < frames; frame += blockSize)
        {
            const int count = std::min(blockSize, frames - frame);
            engine.render(audio.data(), count);
            for (int sample = 0; sample < count * 2; ++sample)
            {
                const auto value = audio[(size_t) sample];
                result.finite = result.finite && std::isfinite(value);
                result.peak = std::max(result.peak, std::abs(value));
                sum += static_cast<double>(value) * value;
            }
        }
        result.rms = std::sqrt(sum / (2 * frames));
        return result;
    };
    auto fixture = []
    {
        ParamValues p;
        p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = p[P_chorusOn] = 0.0f;
        p[P_mix] = 0.0f;
        p[P_volume] = 1.0f;
        p[P_keyRelease] = 0.0f;
        for (int rank = 0; rank < 2; ++rank)
        {
            p[chParam(rank, CP_saw)] = 0.0f;
            p[chParam(rank, CP_sine)] = p[chParam(rank, CP_vS)] = 1.0f;
            p[chParam(rank, CP_vA)] = p[chParam(rank, CP_vD)] = 0.001f;
            p[chParam(rank, CP_vR)] = 20.0f;
        }
        return p;
    };
    // Retain the original reported 48 kHz reproduction separately from the
    // slow-macro stress case: 20 s panel R, neutral macros, one-second hold.
    for (bool isolated : { false, true })
    {
        ParamValues p;
        p[P_parityMode] = isolated ? 1.0f : 0.0f;
        p[P_chorusOn] = p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = 0.0f;
        p[P_macro2] = 0.5f;
        p[P_mix] = p[chParam(0, CP_saw)] = 0.0f;
        p[chParam(0, CP_sine)] = p[chParam(0, CP_vS)] = 1.0f;
        p[chParam(0, CP_vR)] = 20.0f;
        Engine engine;
        engine.setParams(p);
        engine.prepare(48000.0);
        engine.noteOn(60, 1.0f, 1);
        renderSecond(engine);
        engine.noteOff(60, 1);
        Window late;
        for (int second = 0; second < 9; ++second) late = renderSecond(engine);
        check(late.finite && late.rms > 1.0e-3 && engine.activeVoices() == 1,
              "original 48 kHz release reproduction is audible eight to nine seconds after note-off");
        std::printf("HOST TAIL REPRO isolation=%d rate=48000 window=8..9 RMS=%.9g peak=%.9g active=%d\n",
                    isolated ? 1 : 0, late.rms, late.peak, engine.activeVoices());
    }
    for (bool isolated : { false, true })
    {
        auto p = fixture();
        p[P_parityMode] = isolated ? 1.0f : 0.0f;
        p[P_macro2] = 0.0f; // Slowest playable macro; neutralized in isolation.
        Engine engine;
        engine.setParams(p);
        engine.prepare(rate);
        engine.noteOn(60, 1.0f, 1);
        renderSecond(engine);
        engine.noteOff(60, 1);
        Window final;
        for (int second = 0; second < 64; ++second)
        {
            final = renderSecond(engine);
            check(final.finite, "long released-note tail remains finite");
            if (second == 8)
                check(final.rms > 1.0e-3 && engine.activeVoices() == 1,
                      "supported release is still audible beyond the old eight-second tail");
            if (second == 20 && !isolated)
                check(final.rms > 1.0e-3 && engine.activeVoices() == 1,
                      "slow macro makes a twenty-second replacement insufficient too");
        }
        check(engine.activeVoices() == 0 && final.peak < 1.0e-6f,
              "noise-free dry released notes do decay to the practical silence threshold");
    }
    for (int type : { 3, 4 })
    {
        auto p = fixture();
        for (int rank = 0; rank < 2; ++rank) p[chParam(rank, CP_vR)] = 0.001f;
        for (int slot = 0; slot < 3; ++slot)
        {
            p[P_fx1Type + slot * 2] = (float) type;
            p[P_fx1Mix + slot * 2] = 1.0f;
        }
        Engine engine;
        engine.setParams(p);
        engine.prepare(rate);
        engine.noteOn(60, 1.0f, 1);
        renderSecond(engine);
        engine.noteOff(60, 1);
        const auto first = renderSecond(engine);
        check(first.rms > 1.0e-4 && engine.activeVoices() == 0,
              "feedback FX can outlive the released dry voice");
        Window final;
        for (int second = 1; second < 16; ++second)
        {
            final = renderSecond(engine);
            check(final.finite, "three shared feedback FX slots produce finite tail samples");
        }
        check(final.peak < 1.0e-6f,
              "fixed feedback FX fixture decays below a stated practical threshold");
    }
    {
        auto p = fixture();
        p[P_analogNoise] = 0.5f;
        Engine engine;
        engine.setParams(p);
        engine.prepare(rate);
        Window late;
        for (int second = 0; second < 10; ++second) late = renderSecond(engine);
        check(engine.activeVoices() == 0 && late.finite && late.rms > 1.0e-4,
              "intentional noise continues without notes: no universal finite silence deadline");
    }
}
} // namespace cs80::tests
