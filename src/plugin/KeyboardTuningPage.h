#pragma once

// Studio "Keyboard" tab: tuning sources and the ws3 keyboard/voice settings
// (IMPROVEMENT_PLAN Phase 2.2, 2.4 and 6). A thin view over the processor's
// tuning API (loadScalaTuning, clearScalaTuning, getScalaDescription,
// isMtsEspAvailable, isMtsEspConnected) and the nine ws3 host parameters:
// kas_scan_timing, velocity_curve, pressure_curve, pressure_threshold,
// ribbon_cc_enable, ribbon_cc, tuning_mode, voice_mode and unison_detune.
// It owns no state of its own: the Scala/KBM texts live in the session
// (WS3_TUNING) and every control is a parameter attachment.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

class Cs80AudioProcessor;

namespace cs80::ui::modern_layer
{

class KeyboardTuningPage final : public juce::Component, private juce::Timer
{
public:
    explicit KeyboardTuningPage(Cs80AudioProcessor& processor);
    ~KeyboardTuningPage() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    // Test hooks: the same paths the file buttons take, without a file chooser.
    juce::String loadScalaText(const juce::String& scl, const juce::String& kbm = {});
    void clearTuning();
    juce::String statusText() const { return status.getText(); }
    juce::String tuningText() const { return description.getText(); }
    juce::ComboBox& tuningModeBox() noexcept { return tuningMode; }
    juce::ComboBox& voiceModeBox() noexcept { return voiceMode; }

private:
    void timerCallback() override;
    void refreshStatus();
    void chooseFile(bool keyboardMapping);

    struct Row
    {
        juce::Label label;
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    Cs80AudioProcessor& processor;
    juce::Label tuningTitle, keyboardTitle, tuningModeLabel, voiceModeLabel, description, status, note;
    juce::ComboBox tuningMode, voiceMode;
    std::unique_ptr<juce::ComboBoxParameterAttachment> tuningModeAttachment, voiceModeAttachment;
    juce::TextButton loadScl { "LOAD .SCL" }, loadKbm { "LOAD .KBM" }, clear { "CLEAR" };
    juce::ToggleButton scanTiming { "KAS scan timing (1 ms scan instants)" };
    juce::ToggleButton ribbonCcEnable { "Ribbon from MIDI CC" };
    std::unique_ptr<juce::ButtonParameterAttachment> scanTimingAttachment, ribbonCcEnableAttachment;
    std::array<Row, 5> rows; // unison detune, velocity curve, pressure curve, pressure threshold, ribbon CC number
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String sclText, kbmText, lastError;
    juce::Rectangle<int> tuningPanel, keyboardPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardTuningPage)
};

} // namespace cs80::ui::modern_layer
