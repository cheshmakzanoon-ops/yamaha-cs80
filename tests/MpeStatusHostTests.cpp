#include "plugin/PluginProcessor.h"
#include "plugin/KeyboardTuningPage.h"
#include <atomic>
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
void cc(juce::MidiBuffer& midi, int channel, int controller, int value)
{ midi.addEvent(juce::MidiMessage::controllerEvent(channel, controller, value), 0); }
void rpn(juce::MidiBuffer& midi, int channel, int parameter, int value)
{ cc(midi, channel, 101, 0); cc(midi, channel, 100, parameter); cc(midi, channel, 6, value); }
bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
}

int main()
{
    // Concurrent publication tests the telemetry value only, never a processor
    // or its mutable engine. Every observation must be one whole publication.
    cs80::MpeStatus telemetry;
    check(telemetry.read() == cs80::MpeStatusReading{}, "initial telemetry is not ready");
    telemetry.publish(true, 7, 7, 12.345f, 128.27f, 0, 1.234f);
    const auto first = telemetry.read();
    check(near(first.lowerManagerRange, 12.35f) && near(first.lowerMemberRange, 128.27f)
          && near(first.upperMemberRange, 1.23f), "cent rounding and maximum MIDI RPN range are display-only");
    telemetry.publish(true, 0, 15, 2, 48, 9.25f, 24.75f);
    const auto second = telemetry.read();
    telemetry.publish(false, 15, 0, 2, 48, 2, 48);
    const auto disabled = telemetry.read();
    telemetry.reset();
    const auto reset = telemetry.read();
    std::atomic<bool> started { false }, finished { false };
    std::thread writer([&] {
        while (!started.load(std::memory_order_acquire)) std::this_thread::yield();
        for (int n = 0; n < 50000; ++n)
        {
            telemetry.publish(true, 7, 7, 12.345f, 128.27f, 0, 1.234f);
            telemetry.publish(true, 0, 15, 2, 48, 9.25f, 24.75f);
            telemetry.publish(false, 0, 0, 0, 0, 0, 0);
            telemetry.reset();
        }
        finished.store(true, std::memory_order_release);
    });
    bool coherent = true;
    started.store(true, std::memory_order_release);
    do {
        const auto value = telemetry.read();
        coherent &= value == first || value == second || value == disabled || value == reset;
    } while (!finished.load(std::memory_order_acquire));
    writer.join();
    check(coherent, "concurrent polls see only coherent complete atomic publications");

    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-mpe-status");
    for (bool circuit : {false, true}) for (bool sixteen : {false, true}) for (int frames : {0, 32})
    {
        Cs80AudioProcessor host(fixture.getFile());
        check(!host.getMpeStatus().ready, "processor initially waits for callback");
        host.setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        host.setParamValue(cs80::P_parityMode, 0);
        host.setParamValue(cs80::P_voiceMode, sixteen ? 1 : 0);
        host.setParamValue(cs80::P_mpeOn, 0);
        host.setParamValue(cs80::P_pitchBendRange, 12);
        host.setParamValue(cs80::P_mpeBendRange, 7);
        host.prepareToPlay(48000, 32);
        cs80::ui::modern_layer::KeyboardTuningPage page(host);
        check(page.mpeStatusText().contains("waiting"), "page shows waiting before first callback");
        juce::AudioBuffer<float> audio(2, frames);
        juce::MidiBuffer midi;
        auto tick = [&] { host.processBlock(audio, midi); midi.clear(); };
        tick();
        check(host.getMpeStatus() == disabled, "completed callback publishes disabled");
        host.setParamValue(cs80::P_mpeOn, 1);
        check(host.getMpeStatus() == disabled, "parameter request alone does not fabricate applied enable");
        tick();
        auto status = host.getMpeStatus();
        check(status.ready && status.enabled && status.lowerMembers == 15 && status.upperMembers == 0,
              "actual default lower zone in both engines and voice capacities");
        check(status.lowerManagerRange == 12 && status.lowerMemberRange == 7
              && status.upperManagerRange == 12 && status.upperMemberRange == 7,
              "manual fallback reads actual engine global ranges");
        host.setParamValue(cs80::P_pitchBendRange, 10);
        host.setParamValue(cs80::P_mpeBendRange, 9);
        tick(); status = host.getMpeStatus();
        check(status.lowerManagerRange == 10 && status.lowerMemberRange == 9, "unnegotiated fallback tracks applied parameters");
        rpn(midi, 1, 6, 7); rpn(midi, 16, 6, 7); tick(); status = host.getMpeStatus();
        check(status.lowerMembers == 7 && status.upperMembers == 7 && status.lowerManagerRange == 2
              && status.lowerMemberRange == 48 && status.upperManagerRange == 2 && status.upperMemberRange == 48,
              "both MCM zones publish protocol defaults even in zero-frame callbacks");
        rpn(midi, 1, 0, 3); cc(midi, 1, 38, 25);
        rpn(midi, 2, 0, 23); cc(midi, 2, 38, 50);
        rpn(midi, 16, 0, 11); cc(midi, 16, 38, 75);
        rpn(midi, 15, 0, 127); cc(midi, 15, 38, 127);
        tick(); status = host.getMpeStatus();
        check(near(status.lowerManagerRange, 3.25f) && near(status.lowerMemberRange, 23.5f)
              && near(status.upperManagerRange, 11.75f) && near(status.upperMemberRange, 128.27f),
              "four independent fractional RPN ranges including maximum survive packing");
        page.clearTuning();
        const auto label = page.mpeStatusText();
        check(label.contains("Lower: manager 1, members 2-8") && label.contains("Upper: manager 16, members 9-15")
              && label.contains("3.25 st") && label.contains("128.27 st"), "page formats applied channel spans and cent ranges");
        host.setParamValue(cs80::P_pitchBendRange, 5);
        host.setParamValue(cs80::P_mpeBendRange, 6); tick();
        check(host.getMpeStatus() == status, "parameter edits cannot override negotiated ranges in telemetry");
        rpn(midi, 16, 6, 12); tick(); status = host.getMpeStatus();
        check(status.lowerMembers == 2 && status.upperMembers == 12 && near(status.lowerManagerRange, 3.25f)
              && status.upperManagerRange == 2 && status.upperMemberRange == 48, "overlap shrink retains opposite zone ranges");
        rpn(midi, 1, 6, 0); tick();
        check(host.getMpeStatus().lowerMembers == 0 && host.getMpeStatus().upperMembers == 12, "lower zone disable is observable");
        rpn(midi, 16, 6, 0); tick();
        check(host.getMpeStatus().enabled && host.getMpeStatus().lowerMembers == 0 && host.getMpeStatus().upperMembers == 0,
              "both disabled zones are distinct from disabled MPE");
        host.setParamValue(cs80::P_parityMode, 1); tick();
        check(host.getMpeStatus() == disabled, "PURE masks stored enable request");
        page.clearTuning(); check(page.mpeStatusText().contains("MPE: off"), "page reports applied PURE masking");
        host.prepareToPlay(48000, 32);
        check(!host.getMpeStatus().ready, "prepare resets completed-block observation");
        tick(); check(host.getMpeStatus().ready, "next callback restores readiness");
        host.releaseResources(); check(!host.getMpeStatus().ready, "release cannot leave stale MPE status");
    }
    std::cout << "MPE status: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
