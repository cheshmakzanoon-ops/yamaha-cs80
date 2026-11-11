#include "plugin/PluginProcessor.h"
#include "plugin/KeyboardTuningPage.h"
#include <atomic>
#include <bit>
#include <cmath>
#include <iostream>
#include <thread>

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
const juce::String scale = "Inspector twelve-tone\n12\n100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n700.0\n800.0\n900.0\n1000.0\n1100.0\n2/1\n";
const juce::String mapping = "12\n0\n127\n60\n69\n432.0\n12\nx\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n";
void maskedMts(juce::MidiBuffer& midi, bool realtime = true, std::uint8_t cents = 89)
{
    // One-byte scale/octave, channel 2 only, +25 cents on every pitch class.
    const std::uint8_t data[] {std::uint8_t(realtime ? 0x7f : 0x7e),0x7f,8,8,0,0,2,cents,cents,cents,cents,cents,cents,cents,cents,cents,cents,cents,cents};
    midi.addEvent(juce::MidiMessage::createSysExMessage(data, sizeof(data)), 0);
}
}
int main()
{
    cs80::TuningInspection telemetry;
    check(!telemetry.read().ready, "initial snapshot waits for completed block");
    check(telemetry.requested().note == 69 && telemetry.requested().channel == 1, "default selection is A4/channel 1");
    const float precise = std::bit_cast<float>(std::uint32_t(0x428a1234));
    telemetry.publish(127,16,2,true,precise); const auto first = telemetry.read();
    check(std::bit_cast<std::uint32_t>(first.pitch) == 0x428a1234 && first.note == 127 && first.channel == 16,
          "packing retains exact float bits and selection endpoints");
    telemetry.publish(0,1,1,false,-0.125f); const auto second = telemetry.read();
    telemetry.reset(); const auto reset = telemetry.read();
    std::atomic<bool> started {false}, finished {false};
    std::thread writer([&] {
        while (!started.load(std::memory_order_acquire)) std::this_thread::yield();
        for (int i=0; i<50000; ++i) {
            telemetry.publish(127,16,2,true,precise);
            telemetry.publish(0,1,1,false,-0.125f); telemetry.reset();
            telemetry.request(127,16); telemetry.request(0,1);
        }
        finished.store(true,std::memory_order_release);
    });
    bool coherent = true;
    started.store(true,std::memory_order_release);
    do {
        const auto r = telemetry.read(); const auto q = telemetry.requested();
        coherent &= (r == first || r == second || r == reset)
            && ((q.note == 69 && q.channel == 1) || (q.note == 127 && q.channel == 16) || (q.note == 0 && q.channel == 1));
    } while (!finished.load(std::memory_order_acquire));
    writer.join(); check(coherent, "concurrent response and selection polls never tear");

    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-tuning-inspection");
    for (bool circuit : {false,true}) for (bool sixteen : {false,true}) for (int frames : {0,32})
    {
        Cs80AudioProcessor host(fixture.getFile());
        check(!host.getTuningInspection().ready, "processor starts not ready");
        host.setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        host.setParamValue(cs80::P_parityMode,0);
        host.setParamValue(cs80::P_voiceMode,sixteen ? 1 : 0);
        host.setParamValue(cs80::P_tuningMode,0);
        host.prepareToPlay(48000,32);
        juce::AudioBuffer<float> audio(2,frames); juce::MidiBuffer midi;
        auto tick = [&] { host.processBlock(audio,midi); midi.clear(); };
        {
            cs80::ui::modern_layer::KeyboardTuningPage page(host);
            check(page.tuningInspectionText().contains("waiting"), "UI waits before first callback");
            check(page.inspectionNoteBox().getSelectedId() == 70 && page.inspectionChannelBox().getSelectedId() == 1,
                  "UI initializes default note/channel selectors");
        }
        tick(); auto r = host.getTuningInspection();
        check(r.ready && r.note == 69 && r.channel == 1 && r.mode == 0 && r.mapped && r.pitch == 69,
              "completed callback publishes equal temperament, including zero frames");
        check(host.requestTuningInspection(60,16), "valid request accepted");
        check(host.getTuningInspection() == r, "request cannot fabricate response before callback");
        {
            cs80::ui::modern_layer::KeyboardTuningPage page(host);
            check(page.tuningInspectionText().contains("waiting"), "UI hides stale response for a new selection");
            check(page.inspectionNoteBox().getSelectedId() == 61 && page.inspectionChannelBox().getSelectedId() == 16,
                  "reopened page retains coherent requested selection");
            page.inspectionNoteBox().setSelectedId(62, juce::sendNotificationSync);
            check(host.getTuningInspectionRequest().note == 61 && host.getTuningInspectionRequest().channel == 16,
                  "note selector sends UI request without changing channel");
            page.inspectionChannelBox().setSelectedId(2, juce::sendNotificationSync);
            check(host.getTuningInspectionRequest().channel == 2, "channel selector sends UI request");
        }
        host.requestTuningInspection(60,16);
        for (auto bad : {std::pair{-1,1}, {128,1}, {60,0}, {60,17}})
            check(!host.requestTuningInspection(bad.first,bad.second), "invalid request rejected");
        check(host.getTuningInspectionRequest().note == 60 && host.getTuningInspectionRequest().channel == 16,
              "malformed request preserves valid pending selection");
        tick(); r = host.getTuningInspection();
        check(r.note == 60 && r.channel == 16 && r.pitch == 60, "callback echoes coherent requested selection");
        host.setParamValue(cs80::P_masterTune,1); host.setParamValue(cs80::P_detune,1);
        midi.addEvent(juce::MidiMessage::pitchWheel(16,16383),0); tick();
        check(host.getTuningInspection() == r, "master tuning, rank detune and bend do not alter table telemetry");
        check(host.loadScalaTuning(scale,mapping).isEmpty(), "Scala/KBM fixture accepted");
        host.setParamValue(cs80::P_tuningMode,1);
        check(host.getTuningInspection() == r, "Scala and mode requests wait for audio application");
        tick(); r = host.getTuningInspection();
        check(r.mode == 1 && !r.mapped && r.note == 60, "Scala KBM unmapped note is explicitly reported");
        {
            cs80::ui::modern_layer::KeyboardTuningPage page(host);
            check(page.tuningInspectionText().contains("unmapped"), "UI does not present unmapped pitch as playable");
        }
        host.requestTuningInspection(69,1); tick(); r = host.getTuningInspection();
        check(r.mode == 1 && r.mapped && near(r.pitch,69.f + 12.f * std::log2(432.f/440.f)),
              "Scala reference pitch comes from audio-applied fractional table");
        host.clearScalaTuning(); tick();
        check(host.getTuningInspection().mode == 1 && host.getTuningInspection().pitch == 69 && host.getTuningInspection().mapped,
              "Scala clear publishes actual fallback table without pretending mode changed");
        check(host.loadScalaTuning(scale,mapping).isEmpty(), "Scala reload accepted"); tick();
        check(near(host.getTuningInspection().pitch,r.pitch), "Scala reload restores actual tuning");
        host.setParamValue(cs80::P_tuningMode,2); host.requestTuningInspection(60,2); maskedMts(midi); tick();
        r = host.getTuningInspection();
        check(r.mode == 2 && r.mapped && near(r.pitch,60.25f), "MTS addressed channel pitch is published after SysEx");
        // Non-real-time messages describe the table for future onsets, not a
        // promise that already-held oscillators were retuned.
        maskedMts(midi,false,114); tick();
        check(near(host.getTuningInspection().pitch,60.5f), "NRT MTS also reports the current table");
        {
            cs80::ui::modern_layer::KeyboardTuningPage page(host);
            check(page.tuningInspectionText().contains("60.5000") && page.tuningInspectionText().contains("table"),
                  "UI presents precise table pitch rather than sounding frequency");
        }
        maskedMts(midi); tick();
        host.requestTuningInspection(60,1); tick();
        check(host.getTuningInspection().pitch == 60, "MTS unaddressed channel retains independent table");
        host.requestTuningInspection(60,2); host.setParamValue(cs80::P_parityMode,1); tick();
        r = host.getTuningInspection();
        check(r.mode == 0 && r.pitch == 60 && r.mapped, "PURE reports effective equal temperament, not stored MTS mode");
        host.setParamValue(cs80::P_parityMode,0); tick();
        check(host.getTuningInspection().mode == 2 && near(host.getTuningInspection().pitch,60.25f), "leaving PURE restores applied MTS table");
        host.prepareToPlay(48000,32);
        check(!host.getTuningInspection().ready, "prepare invalidates previous observation");
        check(host.getTuningInspectionRequest().note == 60 && host.getTuningInspectionRequest().channel == 2, "prepare retains request");
        tick(); check(host.getTuningInspection().ready && host.getTuningInspection().channel == 2, "next callback answers retained selection");
        host.releaseResources(); check(!host.getTuningInspection().ready, "release invalidates snapshot");
        check(host.getTuningInspectionRequest().channel == 2, "release retains request");
    }
    std::cout << "Tuning inspection: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
