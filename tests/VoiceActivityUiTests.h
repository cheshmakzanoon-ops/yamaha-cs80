#pragma once

// Native editor regression: 16-voice mode shows both engines; switching back
// to eight or PURE mode removes the extension lamps and status together.

#include "UiReadinessTests.h"
#include "plugin/StudioDrawers.h"

namespace cs80::tests
{
template <typename Check, typename Flush>
void testVoiceActivityUi(Check check, Flush flush)
{
    using namespace ui_readiness_detail;
    juce::TemporaryFile root("-cs80-voice-meter-ui");
    Cs80AudioProcessor host(root.getFile().getChildFile("presets"));
    host.prepareToPlay(48000.0, 256);
    host.setParamValue(P_voiceMode, 1.0f);
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer notes;
    for (int i = 0; i < 16; ++i)
        notes.addEvent(juce::MidiMessage::noteOn(1, 40 + i, (juce::uint8) 90), i);
    host.processBlock(audio, notes);
    notes.clear();

    check(host.getVoiceActivitySnapshot().capacity == 16 && host.getVoiceActivitySnapshot().activeCount() == 16,
          "16-voice meter UI fixture starts with all sixteen sounding notes");
    Cs80AudioProcessorEditor editor(host);
    editor.addToDesktop(0);
    editor.setVisible(true);
    auto* lamps = findComponent<juce::Component>(editor, "activity_lamps");
    auto* voiceStatus = findComponent<juce::Label>(editor, "voice_status");
    auto* scope = findComponent<cs80::ui::ScopeDrawer>(editor);
    check(lamps != nullptr && voiceStatus != nullptr && scope != nullptr,
          "voice meter lamps, accessible status and Studio drawer are present");
    if (lamps == nullptr || voiceStatus == nullptr || scope == nullptr) return;
    scope->setVisible(true);
    flush(160); // Timer-driven UI refresh, never an artificial direct callback.
    check(voiceStatus->getText() == "Active voices: 16 of 16"
              && lamps->getDescription().contains("Sixteen voice lamps")
              && scope->getDisplayedVoiceCapacity() == 16 && scope->getDisplayedVoiceCount() == 16,
          "native toolbar and Studio drawer expose both banks with sixteen held voices");

    host.setParamValue(P_voiceMode, 0.0f);
    host.processBlock(audio, notes);
    flush(160);
    check(host.getVoiceActivitySnapshot().capacity == 8
              && voiceStatus->getText().contains("of 8")
              && lamps->getDescription().startsWith("Eight lamps")
              && scope->getDisplayedVoiceCapacity() == 8
              && scope->getDisplayedVoiceCount() <= 8,
          "native toolbar and drawer return to eight voices without a stale second row");

    host.setParamValue(P_voiceMode, 1.0f);
    host.setPure(true);
    host.processBlock(audio, notes);
    flush(160);
    check(host.getVoiceActivitySnapshot().capacity == 8
              && voiceStatus->getText().contains("of 8")
              && scope->getDisplayedVoiceCapacity() == 8,
          "PURE mode keeps eight-voice editor semantics despite a requested sixteen-voice setting");
    scope->setVisible(false);
    host.releaseResources();
}
} // namespace cs80::tests
