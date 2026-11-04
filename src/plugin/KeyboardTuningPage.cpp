#include "KeyboardTuningPage.h"

#include "ArtCommon.h"
#include "PluginProcessor.h"
#include "UiComponents.h"

#include <cmath>

namespace cs80::ui::modern_layer
{

namespace
{
juce::RangedAudioParameter* parameter(Cs80AudioProcessor& p, int index)
{
    return p.getAPVTS().getParameter(juce::String(cs80::paramId(index)));
}

juce::Font pageFont(float height, bool bold = false)
{
    return cs80::art::displayFont(std::max(height, cs80::art::kLegendFloorPx), bold ? juce::Font::bold : juce::Font::plain);
}

void styleLabel(juce::Label& label, const juce::String& text, float height = 14.0f, juce::Colour colour = ivory)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(pageFont(height));
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setInterceptsMouseClicks(false, false);
}

void styleCombo(juce::ComboBox& box, const juce::String& title)
{
    box.setColour(juce::ComboBox::backgroundColourId, panelDark.brighter(0.08f));
    box.setColour(juce::ComboBox::textColourId, ivory);
    box.setColour(juce::ComboBox::outlineColourId, lineColour);
    box.setColour(juce::ComboBox::arrowColourId, amber);
    box.setTitle(title);
}

void styleSlider(juce::Slider& slider, const juce::String& title)
{
    slider.setColour(juce::Slider::trackColourId, amber.withAlpha(0.85f));
    slider.setColour(juce::Slider::backgroundColourId, panelDark.brighter(0.15f));
    slider.setColour(juce::Slider::thumbColourId, ivory);
    slider.setColour(juce::Slider::textBoxTextColourId, ivory);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, panelDark);
    slider.setColour(juce::Slider::textBoxOutlineColourId, lineColour);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, true, 62, 22);
    slider.setTitle(title);
}

void styleToggle(juce::ToggleButton& button, const juce::String& tooltip)
{
    button.setColour(juce::ToggleButton::textColourId, ivory);
    button.setColour(juce::ToggleButton::tickColourId, amberBright);
    button.setColour(juce::ToggleButton::tickDisabledColourId, ivoryDim);
    button.setTooltip(tooltip);
}

void styleButton(juce::TextButton& button, const juce::String& tooltip)
{
    button.setColour(juce::TextButton::buttonColourId, panelDark.brighter(0.12f));
    button.setColour(juce::TextButton::textColourOffId, ivory);
    button.setTooltip(tooltip);
}

void fillPanel(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(panelDark);
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(lineColour);
    g.drawRoundedRectangle(area.reduced(0.5f), 6.0f, 1.0f);
}
} // namespace

KeyboardTuningPage::KeyboardTuningPage(Cs80AudioProcessor& p) : processor(p)
{
    setTitle("Keyboard and tuning");
    styleLabel(tuningTitle, "TUNING", 15.0f, amber);
    styleLabel(keyboardTitle, "KEYBOARD AND VOICES", 15.0f, amber);
    tuningTitle.setFont(pageFont(15.0f, true));
    keyboardTitle.setFont(pageFont(15.0f, true));

    styleLabel(tuningModeLabel, "Tuning source");
    styleCombo(tuningMode, "Tuning source");
    tuningMode.addItemList({ "Equal temperament (12-TET)", "Scala / KBM file", "MIDI Tuning Standard / MTS-ESP" }, 1);
    tuningMode.setTooltip("Modern layer: PURE CS-80 always plays equal temperament.");
    if (auto* q = parameter(p, cs80::P_tuningMode))
        tuningModeAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*q, tuningMode);
    bindParameterContextMenu(tuningMode, processor, Cs80AudioProcessor::parameterId(cs80::P_tuningMode), cs80::P_tuningMode);

    styleLabel(voiceModeLabel, "Voice mode");
    styleCombo(voiceMode, "Voice mode");
    voiceMode.addItemList({ "8 voices (original)", "16 voices (two instruments)", "Unison x2", "Unison x4", "Unison x8" }, 1);
    voiceMode.setTooltip("Labelled extensions beyond the eight voice cards; PURE CS-80 always plays eight single voices.");
    if (auto* q = parameter(p, cs80::P_voiceMode))
        voiceModeAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*q, voiceMode);
    bindParameterContextMenu(voiceMode, processor, Cs80AudioProcessor::parameterId(cs80::P_voiceMode), cs80::P_voiceMode);

    styleButton(loadScl, "Load a Scala scale (.scl). It is kept with the session, not with presets.");
    styleButton(loadKbm, "Load a Scala keyboard mapping (.kbm) for the loaded scale; without one, middle C is 261.63 Hz.");
    styleButton(clear, "Forget the loaded Scala scale and mapping.");
    loadScl.onClick = [this] { chooseFile(false); };
    loadKbm.onClick = [this] { chooseFile(true); };
    clear.onClick = [this] { clearTuning(); };

    styleLabel(description, {}, 13.5f);
    styleLabel(status, {}, 13.0f, ivoryDim);
    description.setJustificationType(juce::Justification::topLeft);
    status.setJustificationType(juce::Justification::topLeft);

    styleToggle(scanTiming, "Declared KAS scanner model: key events act at 1 ms scan instants, lowest key first. "
                            "Adds at most 1 ms of note latency. Circuit engine only.");
    styleToggle(ribbonCcEnable, "Lets a MIDI controller play the ribbon: 64 centre, 0 and 127 the ends. Off by default.");
    if (auto* q = parameter(p, cs80::P_kasScanTiming))
        scanTimingAttachment = std::make_unique<juce::ButtonParameterAttachment>(*q, scanTiming);
    if (auto* q = parameter(p, cs80::P_ribbonCcEnable))
        ribbonCcEnableAttachment = std::make_unique<juce::ButtonParameterAttachment>(*q, ribbonCcEnable);
    bindParameterContextMenu(scanTiming, processor,
                             Cs80AudioProcessor::parameterId(cs80::P_kasScanTiming), cs80::P_kasScanTiming);
    bindParameterContextMenu(ribbonCcEnable, processor,
                             Cs80AudioProcessor::parameterId(cs80::P_ribbonCcEnable), cs80::P_ribbonCcEnable);

    const struct { int id; const char* name; const char* tip; int decimals; const char* suffix; } defs[] = {
        { cs80::P_unisonDetune, "Unison detune", "Spread of the stacked unison voices.", 0, " ct" },
        { cs80::P_velocityCurve, "Velocity curve", "Interface layer: -1 hard, 0 identity, +1 soft. Calibrates your controller, not the instrument.", 2, "" },
        { cs80::P_pressureCurve, "Aftertouch curve", "Interface layer: -1 hard, 0 identity, +1 soft.", 2, "" },
        { cs80::P_pressureThreshold, "Aftertouch threshold", "Interface layer: removes a controller's resting pressure and rescales the rest.", 2, "" },
        { cs80::P_ribbonCc, "Ribbon CC number", "Controller number that plays the ribbon when ribbon CC is on (16 by default).", 0, "" },
    };
    for (size_t i = 0; i < rows.size(); ++i)
    {
        auto& row = rows[i];
        const auto& d = defs[i];
        styleLabel(row.label, d.name);
        styleSlider(row.slider, d.name);
        row.slider.setTooltip(d.tip);
        if (auto* q = parameter(p, d.id))
            row.attachment = std::make_unique<juce::SliderParameterAttachment>(*q, row.slider);
        const int decimals = d.decimals;
        const juce::String suffix = d.suffix;
        row.slider.textFromValueFunction = [decimals, suffix](double v) { return juce::String(v, decimals) + suffix; };
        row.slider.updateText();
        addAndMakeVisible(row.label);
        addAndMakeVisible(row.slider);
    }

    styleLabel(note, "PURE CS-80 forces equal temperament and eight voices. The curves, the ribbon CC and the KAS scan "
                     "stay available there: they adapt a MIDI controller or model an original circuit.", 12.5f, ivoryDim);
    note.setJustificationType(juce::Justification::topLeft);

    for (auto* c : std::initializer_list<juce::Component*> { &tuningTitle, &keyboardTitle, &tuningModeLabel, &voiceModeLabel,
                                                             &tuningMode, &voiceMode, &loadScl, &loadKbm, &clear, &description,
                                                             &status, &scanTiming, &ribbonCcEnable, &note })
        addAndMakeVisible(*c);
    refreshStatus();
    startTimerHz(4);
}

KeyboardTuningPage::~KeyboardTuningPage()
{
    stopTimer();
}

juce::String KeyboardTuningPage::loadScalaText(const juce::String& scl, const juce::String& kbm)
{
    const auto error = processor.loadScalaTuning(scl, kbm);
    if (error.isEmpty())
    {
        sclText = scl;
        kbmText = kbm;
    }
    lastError = error;
    refreshStatus();
    return error;
}

void KeyboardTuningPage::clearTuning()
{
    processor.clearScalaTuning();
    sclText = kbmText = lastError = {};
    refreshStatus();
}

void KeyboardTuningPage::chooseFile(bool keyboardMapping)
{
    if (keyboardMapping && sclText.isEmpty())
    {
        lastError = "Load a .scl scale before its .kbm mapping.";
        refreshStatus();
        return;
    }
    chooser = std::make_unique<juce::FileChooser>(keyboardMapping ? "Load a Scala keyboard mapping" : "Load a Scala scale",
                                                  juce::File(), keyboardMapping ? "*.kbm" : "*.scl");
    juce::Component::SafePointer<KeyboardTuningPage> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safe, keyboardMapping](const juce::FileChooser& fc)
                         {
                             if (safe == nullptr)
                                 return;
                             const auto file = fc.getResult();
                             if (! file.existsAsFile())
                                 return;
                             const auto text = file.loadFileAsString();
                             if (keyboardMapping)
                                 safe->loadScalaText(safe->sclText, text);
                             else
                                 safe->loadScalaText(text, {});
                         });
}

void KeyboardTuningPage::refreshStatus()
{
    const auto loaded = processor.getScalaDescription();
    const auto text = loaded.isNotEmpty() ? "Scala: " + loaded : juce::String("Scala: no scale loaded");
    if (description.getText() != text)
        description.setText(text, juce::dontSendNotification);
    juce::String mts;
    if (! Cs80AudioProcessor::isMtsEspAvailable())
        mts = "MTS-ESP: the client library is not compiled into this build; MIDI Tuning Standard SysEx still works.";
    else
        mts = processor.isMtsEspConnected() ? "MTS-ESP: master connected." : "MTS-ESP: no master connected.";
    const auto line = lastError.isNotEmpty() ? lastError + "\n" + mts : mts;
    if (status.getText() != line)
        status.setText(line, juce::dontSendNotification);
    loadKbm.setEnabled(sclText.isNotEmpty() || loaded.isNotEmpty());
}

void KeyboardTuningPage::timerCallback()
{
    if (isShowing())
        refreshStatus();
}

void KeyboardTuningPage::paint(juce::Graphics& g)
{
    fillPanel(g, tuningPanel.toFloat());
    fillPanel(g, keyboardPanel.toFloat());
}

void KeyboardTuningPage::resized()
{
    auto area = getLocalBounds().reduced(8);
    note.setBounds(area.removeFromBottom(40).reduced(4, 2));
    area.removeFromBottom(6);
    tuningPanel = area.removeFromLeft(area.getWidth() * 9 / 20);
    area.removeFromLeft(10);
    keyboardPanel = area;

    const int rowHeight = 30;
    {
        auto a = tuningPanel.reduced(12, 8);
        tuningTitle.setBounds(a.removeFromTop(24));
        a.removeFromTop(6);
        auto modeRow = a.removeFromTop(rowHeight);
        tuningModeLabel.setBounds(modeRow.removeFromLeft(juce::jmin(130, modeRow.getWidth() / 3)));
        tuningMode.setBounds(modeRow);
        a.removeFromTop(10);
        auto buttons = a.removeFromTop(rowHeight);
        const int w = (buttons.getWidth() - 16) / 3;
        loadScl.setBounds(buttons.removeFromLeft(w));
        buttons.removeFromLeft(8);
        loadKbm.setBounds(buttons.removeFromLeft(w));
        buttons.removeFromLeft(8);
        clear.setBounds(buttons);
        a.removeFromTop(10);
        description.setBounds(a.removeFromTop(48));
        status.setBounds(a.removeFromTop(64));
    }
    {
        auto a = keyboardPanel.reduced(12, 8);
        keyboardTitle.setBounds(a.removeFromTop(24));
        a.removeFromTop(6);
        auto modeRow = a.removeFromTop(rowHeight);
        voiceModeLabel.setBounds(modeRow.removeFromLeft(juce::jmin(150, modeRow.getWidth() / 3)));
        voiceMode.setBounds(modeRow);
        a.removeFromTop(6);
        const int labelWidth = juce::jmin(170, a.getWidth() / 3);
        auto placeRow = [&](Row& row)
        {
            auto r = a.removeFromTop(rowHeight);
            row.label.setBounds(r.removeFromLeft(labelWidth));
            row.slider.setBounds(r);
            a.removeFromTop(4);
        };
        placeRow(rows[0]);
        scanTiming.setBounds(a.removeFromTop(rowHeight));
        a.removeFromTop(4);
        placeRow(rows[1]);
        placeRow(rows[2]);
        placeRow(rows[3]);
        ribbonCcEnable.setBounds(a.removeFromTop(rowHeight));
        a.removeFromTop(4);
        placeRow(rows[4]);
    }
}

} // namespace cs80::ui::modern_layer
