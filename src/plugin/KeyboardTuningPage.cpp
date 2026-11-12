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

    styleLabel(voiceModeLabel, "Voice mode");
    styleCombo(voiceMode, "Voice mode");
    voiceMode.addItemList({ "8 voices (original)", "16 voices (two instruments)", "Unison x2", "Unison x4", "Unison x8" }, 1);
    voiceMode.setTooltip("Labelled extensions beyond the eight voice cards; PURE CS-80 always plays eight single voices.");
    if (auto* q = parameter(p, cs80::P_voiceMode))
        voiceModeAttachment = std::make_unique<juce::ComboBoxParameterAttachment>(*q, voiceMode);

    styleButton(loadScl, "Load a Scala scale (.scl). It is kept with the session, not with presets.");
    styleButton(loadKbm, "Load a Scala keyboard mapping (.kbm) for the loaded scale; without one, middle C is 261.63 Hz.");
    styleButton(clear, "Forget the loaded Scala scale and mapping.");
    loadScl.onClick = [this] { chooseFile(false); };
    loadKbm.onClick = [this] { chooseFile(true); };
    clear.onClick = [this] { clearTuning(); };

    styleLabel(description, {}, 13.5f);
    styleLabel(status, {}, 13.0f, ivoryDim);
    status.setInterceptsMouseClicks(true, false); // TooltipWindow must hit this label.
    styleCombo(inspectionNote, "Inspect MIDI note");
    styleCombo(inspectionChannel, "Inspect MIDI channel");
    for (int n = 0; n < 128; ++n) inspectionNote.addItem("MIDI note " + juce::String(n), n + 1);
    for (int ch = 1; ch <= 16; ++ch) inspectionChannel.addItem("Channel " + juce::String(ch), ch);
    const auto requested = processor.getTuningInspectionRequest();
    inspectionNote.setSelectedId(requested.note + 1, juce::dontSendNotification);
    inspectionChannel.setSelectedId(requested.channel, juce::dontSendNotification);
    const auto inspect = [this] {
        processor.requestTuningInspection(inspectionNote.getSelectedId() - 1, inspectionChannel.getSelectedId());
        refreshStatus();
    };
    inspectionNote.onChange = inspect;
    inspectionChannel.onChange = inspect;
    styleLabel(inspectionStatus, {}, 13.0f, ivoryDim);
    inspectionStatus.setJustificationType(juce::Justification::topLeft);
    inspectionStatus.setTitle("Applied tuning table lookup");
    inspectionStatus.setInterceptsMouseClicks(true, false);
    inspectionStatus.setTooltip("Read-only current tuning-table lookup for MIDI channels 1-16, published at the end of an audio callback. Before master tuning, rank offsets, pitch bend and modulation. Non-real-time tuning can change this table without retuning held voices. Unmapped keys do not admit new notes. A selected Scala or MTS slot can still contain the default equal-tempered table; the mode label does not prove a file/master was loaded. This is not a measurement of sounding audio.");
    description.setJustificationType(juce::Justification::topLeft);
    status.setJustificationType(juce::Justification::topLeft);
    styleLabel(mpeStatus, {}, 13.0f, ivoryDim);
    mpeStatus.setJustificationType(juce::Justification::topLeft);
    mpeStatus.setTitle("Applied MPE zones and bend ranges");
    mpeStatus.setInterceptsMouseClicks(true, false);
    mpeStatus.setTooltip("Read-only state from the last completed audio callback, refreshed four times per second. Incoming MCM/RPN messages can override the stored bend knobs. Ranges are displayed rounded to 0.01 semitone; DSP values are unchanged. MPE is masked in PURE CS-80. Changes wait for an audio callback.");

    styleToggle(scanTiming, "Declared KAS scanner model: key events act at 1 ms scan instants, lowest key first. "
                            "Adds at most 1 ms of note latency. Circuit engine only.");
    styleToggle(ribbonCcEnable, "Lets a MIDI controller play the ribbon: 64 centre, 0 and 127 the ends. Off by default.");
    if (auto* q = parameter(p, cs80::P_kasScanTiming))
        scanTimingAttachment = std::make_unique<juce::ButtonParameterAttachment>(*q, scanTiming);
    if (auto* q = parameter(p, cs80::P_ribbonCcEnable))
        ribbonCcEnableAttachment = std::make_unique<juce::ButtonParameterAttachment>(*q, ribbonCcEnable);

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
                                                             &status, &mpeStatus, &inspectionNote, &inspectionChannel, &inspectionStatus, &scanTiming, &ribbonCcEnable, &note })
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
    bool changed = false;
    const auto error = processor.loadScalaTuning(scl, kbm, &changed);
    lastError = error;
    refreshStatus();
    // Host listeners may save state or destroy this page synchronously. Keep
    // notification outside the Scala lock and do not touch the page afterward.
    if (changed)
        processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
    return error;
}

juce::String KeyboardTuningPage::loadKeyboardMappingText(const juce::String& kbm, std::optional<unsigned> expectedVersion)
{
    bool changed = false;
    const auto error = processor.loadScalaKeyboardMapping(kbm, expectedVersion, &changed);
    lastError = error;
    refreshStatus();
    if (changed)
        processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
    return error;
}

void KeyboardTuningPage::clearTuning()
{
    const bool changed = processor.clearScalaTuning();
    lastError = {};
    refreshStatus();
    if (changed)
        processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged(true));
}

void KeyboardTuningPage::chooseFile(bool keyboardMapping)
{
    if (keyboardMapping && ! processor.hasScalaTuning())
    {
        lastError = "Load a .scl scale before its .kbm mapping.";
        refreshStatus();
        return;
    }
    const auto sourceRevision = processor.scalaTuningRevision();
    chooser = std::make_unique<juce::FileChooser>(keyboardMapping ? "Load a Scala keyboard mapping" : "Load a Scala scale",
                                                  juce::File(), keyboardMapping ? "*.kbm" : "*.scl");
    juce::Component::SafePointer<KeyboardTuningPage> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safe, keyboardMapping, sourceRevision](const juce::FileChooser& fc)
                         {
                             if (safe == nullptr)
                                 return;
                             const auto file = fc.getResult();
                             if (! file.existsAsFile())
                                 return;
                             const auto text = file.loadFileAsString();
                             if (keyboardMapping)
                                 safe->loadKeyboardMappingText(text, sourceRevision);
                             else
                                 safe->loadScalaText(text, {});
                         });
}

void KeyboardTuningPage::refreshStatus()
{
    const auto loaded = processor.getScalaDescription();
    const bool hasScale = processor.hasScalaTuning();
    const auto text = ! hasScale ? juce::String("Scala: no scale loaded")
        : loaded.isNotEmpty() ? "Scala: " + loaded : juce::String("Scala: unnamed scale");
    if (description.getText() != text)
        description.setText(text, juce::dontSendNotification);
    juce::String mts;
    if (! Cs80AudioProcessor::isMtsEspAvailable())
        mts = "MTS-ESP: the client library is not compiled into this build; MIDI Tuning Standard SysEx still works.";
    else
        mts = processor.isMtsEspConnected() ? "MTS-ESP: master connected." : "MTS-ESP: no master connected.";
    const auto masterScale = processor.getMtsEspScaleName();
    if (masterScale.isNotEmpty()) mts += " Scale: " + masterScale;
    const auto line = lastError.isNotEmpty() ? lastError + "\n" + mts : mts;
    if (status.getText() != line)
        status.setText(line, juce::dontSendNotification);
    status.setTooltip(line);
    const auto applied = processor.getMpeStatus();
    juce::String mpe;
    if (! applied.ready) mpe = "MPE: waiting for an audio callback.";
    else if (! applied.enabled) mpe = "MPE: off in the last audio callback (including PURE masking).";
    else
    {
        const auto zone = [](bool upper, int members, float managerRange, float memberRange)
        {
            const juce::String name = upper ? "Upper" : "Lower";
            if (members == 0) return name + ": zone disabled";
            const int manager = upper ? 16 : 1;
            const int first = upper ? 16 - members : 2;
            const int last = upper ? 15 : 1 + members;
            return name + ": manager " + juce::String(manager) + ", members " + juce::String(first)
                + (first == last ? juce::String() : "-" + juce::String(last))
                + "\nBend: manager " + juce::String(managerRange, 2) + " st, members "
                + juce::String(memberRange, 2) + " st";
        };
        mpe = "MPE: last completed audio callback\n"
            + zone(false, applied.lowerMembers, applied.lowerManagerRange, applied.lowerMemberRange) + "\n"
            + zone(true, applied.upperMembers, applied.upperManagerRange, applied.upperMemberRange);
    }
    if (mpeStatus.getText() != mpe) mpeStatus.setText(mpe, juce::dontSendNotification);
    const auto request = processor.getTuningInspectionRequest();
    inspectionNote.setSelectedId(request.note + 1, juce::dontSendNotification);
    inspectionChannel.setSelectedId(request.channel, juce::dontSendNotification);
    const auto lookup = processor.getTuningInspection();
    juce::String inspected;
    if (!lookup.ready || lookup.note != request.note || lookup.channel != request.channel)
        inspected = "Table lookup: waiting for an audio callback.";
    else
    {
        const juce::String source = lookup.mode == 1 ? "Scala/KBM" : lookup.mode == 2 ? "MTS/MTS-ESP" : "12-TET";
        inspected = source + " table, note " + juce::String(lookup.note) + ", channel " + juce::String(lookup.channel) + ": ";
        if (!lookup.mapped) inspected += "unmapped";
        else if (!std::isfinite(lookup.pitch)) inspected += "pitch unavailable";
        else inspected += "MIDI pitch " + juce::String(lookup.pitch, 4);
        inspected += "\nBefore master/rank offsets; not sounding-note pitch.";
    }
    if (inspectionStatus.getText() != inspected) inspectionStatus.setText(inspected, juce::dontSendNotification);
    loadKbm.setEnabled(hasScale);
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
        a.removeFromTop(8);
        mpeStatus.setBounds(a.removeFromTop(96));
        auto inspectRow = a.removeFromTop(30);
        inspectionNote.setBounds(inspectRow.removeFromLeft(inspectRow.getWidth() / 2).reduced(0, 2));
        inspectRow.removeFromLeft(6);
        inspectionChannel.setBounds(inspectRow.reduced(0, 2));
        inspectionStatus.setBounds(a.removeFromTop(58));
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
