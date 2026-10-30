#pragma once

#include "UiReadinessTests.h"

#include <chrono>
#include <cmath>

namespace cs80::tests
{
// Native editor wiring only; deterministic decay/hold/clock edge cases live in
// OutputMeterDisplayTests.cpp. Never inject NaNs into the instrument DSP.
template <typename Check, typename Flush>
void testOutputMeterUi(Check check, Flush flush)
{
    using namespace ui_readiness_detail;
    juce::TemporaryFile root("-cs80-meter-ui");
    Cs80AudioProcessor host(root.getFile().getChildFile("presets"));
    ParamValues values;
    values[P_analogNoise] = values[P_drift] = values[P_voiceDispersion] = 0.0f;
    host.applyParameterValues(values);
    host.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer notes;
    notes.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    host.processBlock(audio, notes);
    notes.clear();
    for (int block = 0; block < 4; ++block) host.processBlock(audio, notes);

    RefreshProbe unchanged(host);
    Cs80AudioProcessorEditor editor(host);
    editor.addToDesktop(0);
    editor.setVisible(true);
    auto* meter = findComponent<juce::Label>(editor, "output_sample_peak");
    auto* panic = findComponent<juce::Button>(editor, "panic_notes");
    auto* sub = findComponent<juce::Label>(editor, "sub_mode_status");
    check(meter != nullptr && panic != nullptr && sub != nullptr,
          "output meter and adjacent PANIC/SUB controls have stable editor IDs");
    if (meter == nullptr || panic == nullptr || sub == nullptr) return;
    check(meter->isShowing() && !meter->getBounds().isEmpty()
              && !meter->getBounds().intersects(panic->getBounds())
              && !meter->getBounds().intersects(sub->getBounds()),
          "sample-peak readout is visible without overlapping PANIC or SUB status");
    const auto floorText = meter->getText();
    flush(180);
    check(meter->getText() == floorText && floorText.contains("SAMPLE PEAK")
              && floorText.contains("<= -120 dBFS") && floorText.contains("READ-ONLY"),
          "opening a meter clears accumulated old audio history and shows an explicit silence floor");
    const auto tip = meter->getTooltip();
    check(tip.contains("not true peak or loudness") && tip.contains("does not prove hard clipping")
              && tip.contains("never limits, normalizes or changes audio")
              && tip.contains("NON-FINITE") && tip.contains("NaN or infinity"),
          "meter hover help distinguishes sample peak, over-range events and nonfinite samples from limiting");

    float actualPeak = 0.0f;
    for (int block = 0; block < 8; ++block)
    {
        host.processBlock(audio, notes);
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                actualPeak = std::max(actualPeak, std::abs(audio.getSample(channel, sample)));
    }
    check(std::isfinite(actualPeak) && actualPeak > 0.000001f,
          "meter UI fixture produces finite output above the display floor");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (meter->getText() == floorText && std::chrono::steady_clock::now() < deadline) flush(20);
    check(meter->getText() != floorText && meter->getText().contains("SAMPLE PEAK")
              && meter->getText().contains("dBFS") && !meter->getText().contains("NON-FINITE"),
          "existing message-thread timer presents newly produced output telemetry");
    if (actualPeak > 1.0f)
        check(meter->getText().contains("OVER-RANGE"), "actual above-full-scale sample has a textual over-range indicator");
    check(unchanged.unchanged(),
          "opening, reading and refreshing the output meter changes no APVTS values, metadata or host gestures");
    host.releaseResources();
}
} // namespace cs80::tests
