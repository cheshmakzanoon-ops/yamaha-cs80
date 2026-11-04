#pragma once

#include "StudioUiTests.h"

namespace cs80::tests::ui_completion
{
using namespace cs80::tests::studio;

inline bool sameRank(const cs80::Engine::RankInfo& a, const cs80::Engine::RankInfo& b)
{
    return a.oscillatorHz == b.oscillatorHz && a.hpfHz == b.hpfHz && a.lpfHz == b.lpfHz
        && a.ampEnvelope == b.ampEnvelope && a.filterEnvelope == b.filterEnvelope
        && a.keyVolts == b.keyVolts && a.hpfQ == b.hpfQ && a.lpfQ == b.lpfQ
        && a.hpfEnabled == b.hpfEnabled && a.lpfEnabled == b.lpfEnabled;
}

inline void testVoiceMonitorAndPressure(const Check& check, const Flush& flush, const juce::File& captures = {})
{
    for (int policy = 0; policy < 3; ++policy)
    {
        auto quiet = std::make_unique<Cs80AudioProcessor>();
        auto monitored = std::make_unique<Cs80AudioProcessor>();
        for (auto* p : { quiet.get(), monitored.get() })
        {
            p->setEngineMode(policy == 0 ? Cs80AudioProcessor::EngineMode::legacy : Cs80AudioProcessor::EngineMode::circuit);
            p->setPure(policy == 2);
            p->setParamValue(cs80::P_voiceMode, 1.0f);
            p->prepareToPlay(48000.0, 128);
        }
        cs80::VoiceMonitorFrame frame;
        check(!monitored->readVoiceMonitor(frame), "engine tap publication is off until a monitor subscribes");
        monitored->setVoiceMonitorEnabled(true);
        juce::AudioBuffer<float> a(2, 128), b(2, 128);
        juce::MidiBuffer ma, mb;
        for (int i = 0; i < 16; ++i)
        {
            ma.addEvent(juce::MidiMessage::noteOn(1, 40 + i, (juce::uint8) (80 + i)), i);
            mb.addEvent(juce::MidiMessage::noteOn(1, 40 + i, (juce::uint8) (80 + i)), i);
        }
        bool neutral = true, exact = true;
        for (int block = 0; block < 4; ++block)
        {
            quiet->processBlock(a, ma);
            monitored->processBlock(b, mb);
            ma.clear(); mb.clear();
            for (int ch = 0; ch < 2; ++ch)
                neutral &= std::memcmp(a.getReadPointer(ch), b.getReadPointer(ch), 128 * sizeof(float)) == 0;
            exact &= monitored->readVoiceMonitor(frame);
            exact &= frame.capacity == (policy == 2 ? 8 : 16);
            for (int i = 0; i < frame.capacity; ++i)
            {
                const auto& engine = i < 8 ? monitored->getEngine() : monitored->getExtensionEngine();
                const auto truth = engine.getVoiceInfo(i % 8);
                const auto& v = frame.voices[(size_t) i];
                exact &= v.info.active == truth.active && v.info.held == truth.held && v.info.note == truth.note
                    && v.info.channel == truth.channel && v.info.pressure == truth.pressure && v.info.assignmentSerial == truth.assignmentSerial;
                for (int rank = 0; rank < 2; ++rank) exact &= sameRank(v.ranks[(size_t) rank], engine.getRankInfo(i % 8, rank));
            }
        }
        check(neutral, "monitor publication leaves every rendered sample byte-identical in Legacy, Circuit and PURE");
        check(exact, "published voice and rank fields exactly match both engines' current audio-thread taps");
        check(policy != 2 || (frame.capacity == 8 && monitored->getActiveVoiceCount() == 8 && !monitored->isVoiceActive(8)),
              "PURE CS-80 publishes only the original eight physical voices");
        Cs80AudioProcessorEditor editor(*monitored);
        editor.setSize(1600, 1000);
        editor.setVisible(true);
        auto* scope = findFirst<cs80::ui::ScopeDrawer>(editor);
        check(scope != nullptr, "editor owns the engine-tap voice monitor");
        if (scope != nullptr)
        {
            scope->setVisible(true);
            monitored->processBlock(b, mb);
            scope->setVoice(policy == 2 ? 3 : 11);
            scope->setChannel(1);
            scope->refresh();
            const auto& instrument = policy == 2 ? monitored->getEngine() : monitored->getExtensionEngine();
            check(scope->getVoice() == (policy == 2 ? 3 : 11) && sameRank(scope->displayedRank(), instrument.getRankInfo(3, 1)),
                  "voice monitor displays the chosen physical slot and rank's real engine taps");
            auto* selector = dynamic_cast<juce::ComboBox*>(findById(editor, "scope_voice"));
            const int selectedSlot = policy == 2 ? 4 : 12;
            check(selector != nullptr && selector->getNumItems() == (policy == 2 ? 8 : 16)
                      && selector->getSelectedId() == selectedSlot
                      && selector->getText() == "VOICE " + juce::String(selectedSlot)
                      && scope->getDescription().contains("voice " + juce::String(selectedSlot) + ", rank II"),
                  "voice selector and accessible text match the current frame without waiting for another audio block");
            flush(80);
            if (policy == 1 && captures.isDirectory())
            {
                // The image contains real history at 48 kHz / 3200 frames = 15 Hz,
                // using the same engine-tap path as the live drawer.
                juce::AudioBuffer<float> historyAudio(2, 3200);
                for (int point = 0; point < 18; ++point)
                {
                    monitored->processBlock(historyAudio, mb);
                    scope->refresh();
                }
                save(editor, captures, "studio-engine-taps-sixteen-voices-1600x1000", check);
            }
        }
    }

    Cs80AudioProcessor host;
    host.setParamValue(cs80::P_voiceMode, 1.0f);
    host.prepareToPlay(48000.0, 32);
    host.serviceKeyboardUI();
    juce::AudioBuffer<float> audio(2, 32);
    juce::MidiBuffer midi;
    auto tick = [&] { host.processBlock(audio, midi); midi.clear(); };
    tick(); // enable the second instrument before the UI event is captured
    for (int note = 40; note < 48; ++note) midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 90), 0);
    tick();
    auto strike = [&]
    {
        return host.captureKeyboardGesture(1, 60, [&] { host.getKeyboardState().noteOn(1, 60, 0.8f); });
    };
    auto pressure = [&]
    {
        for (int i = 0; i < 8; ++i)
        {
            const auto v = host.getExtensionEngine().getVoiceInfo(i);
            if (v.active && v.held && v.note == 60) return v.pressure;
        }
        return -1.0f;
    };
    const auto old = strike();
    host.setKeyPressureFromUI(old, 0.4f); tick();
    check(old && std::abs(pressure() - 0.4f) < 1e-6f && host.getActiveVoiceCount() == 9,
          "UI key allocated to the second engine owns and applies its pressure");
    host.getKeyboardState().noteOff(1, 60, 0.0f);
    const auto fresh = strike();
    host.setKeyPressureFromUI(old, 0.9f);
    host.setKeyPressureFromUI(fresh, 0.25f); tick();
    check(std::abs(pressure() - 0.25f) < 1e-6f,
          "second-engine restrike accepts its fresh pressure token and rejects the released one");
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0); tick();
    host.setKeyPressureFromUI(fresh, 0.9f); tick();
    bool revoked = true;
    for (int i = 0; i < 8; ++i)
    {
        const auto v = host.getExtensionEngine().getVoiceInfo(i);
        if (v.active && v.note == 60) revoked &= v.pressure != 0.9f;
    }
    check(revoked, "host takeover revokes second-engine UI pressure ownership");
}
} // namespace cs80::tests::ui_completion
