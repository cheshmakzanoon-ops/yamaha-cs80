#include "PluginEditor.h"

#include "AdvancedPanel.h"
#include "PresetBrowser.h"
#include "UiComponents.h"
#include "core/FactoryPresets.h"

#include <array>
#include <cmath>
#include <vector>

namespace
{
struct Placement
{
    juce::Component* component = nullptr;
    juce::Rectangle<float> bounds;
};

constexpr float kDesignWidth = 1600.0f;
constexpr float kDesignHeight = 1000.0f;

juce::Font displayFont(float height, int style = juce::Font::plain)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(),
                                        height, style));
}
} // namespace

struct Cs80AudioProcessorEditor::Impl final : public juce::Component
{
    explicit Impl(Cs80AudioProcessor& p)
        : processor(p),
          keyboard(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard),
          pitchWheel("PITCH", true),
          modWheel("MOD", false),
          advanced(p),
          browser(p)
    {
        setOpaque(true);
        setLookAndFeel(&lookAndFeel);
        setWantsKeyboardFocus(true);

        buildTopBar();
        buildMasterSection();
        buildChannels();
        buildToneSection();
        buildPerformance();
        buildKeyboard();

        addAndMakeVisible(advanced);
        advanced.setVisible(false);
        addAndMakeVisible(browser);
        browser.setVisible(false);

        advanced.onClose = [this] { showAdvanced(false); };
        browser.onClose = [this] { showBrowser(false); };
        browser.onPresetLoaded = [this]
        {
            syncPresetDisplay();
            syncToneMarks();
            repaint();
        };

        processor.onPresetChanged = [this]
        {
            syncPresetDisplay();
            syncToneMarks();
            repaint();
        };

        syncPresetDisplay();
        syncToneMarks();
    }

    ~Impl() override
    {
        processor.onPresetChanged = nullptr;
        setLookAndFeel(nullptr);
    }

    // -----------------------------------------------------------------------
    // Construction helpers
    // -----------------------------------------------------------------------
    void place(juce::Component& component, float x, float y, float w, float h)
    {
        placements.push_back({ &component, { x, y, w, h } });
    }

    cs80::ui::ParamKnob& addKnob(int parameter, float x, float y, float w, float h,
                                 const juce::String& name = {})
    {
        auto knob = std::make_unique<cs80::ui::ParamKnob>(processor, parameter, name);
        auto* raw = knob.get();
        addAndMakeVisible(*raw);
        place(*raw, x, y, w, h);
        knobs.push_back(std::move(knob));
        return *raw;
    }

    cs80::ui::LedButton& addLed(int parameter, const juce::String& label,
                                float x, float y, float w, float h)
    {
        auto button = std::make_unique<cs80::ui::LedButton>(label);
        auto* raw = button.get();
        addAndMakeVisible(*raw);
        buttonAttachments.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processor.getAPVTS(), Cs80AudioProcessor::parameterId(parameter), *raw));
        place(*raw, x, y, w, h);
        leds.push_back(std::move(button));
        return *raw;
    }

    void buildTopBar()
    {
        for (auto* button : { &prevButton, &nextButton, &browseButton, &saveButton,
                              &initButton, &advancedButton })
        {
            button->setWantsKeyboardFocus(false);
            addAndMakeVisible(*button);
        }

        presetLabel.setJustificationType(juce::Justification::centred);
        presetLabel.setColour(juce::Label::textColourId, cs80::ui::amberBright);
        presetLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        presetLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(presetLabel);

        prevButton.onClick = [this] { stepPreset(-1); };
        nextButton.onClick = [this] { stepPreset(1); };
        browseButton.onClick = [this]
        {
            showBrowser(true);
            browser.focusSaveField();
        };
        saveButton.onClick = [this]
        {
            showBrowser(true);
            browser.focusSaveField();
        };
        initButton.onClick = [this]
        {
            const auto& bank = processor.getFactoryPresets();
            int index = 0;
            for (int i = 0; i < (int) bank.size(); ++i)
                if (juce::String(bank[(size_t) i].name).equalsIgnoreCase("Init Patch"))
                {
                    index = i;
                    break;
                }
            processor.loadFactoryPreset(index);
            syncToneMarks();
        };
        advancedButton.setClickingTogglesState(true);
        advancedButton.onClick = [this] { showAdvanced(! advancedVisible); };
    }

    void buildMasterSection()
    {
        // The master strip is 194 px wide. Short, engraved labels keep the
        // two columns readable even at the 1120 px minimum editor size.
        addKnob(cs80::P_masterTune, 32, 130, 82, 62, "TUNE");
        addKnob(cs80::P_detune, 120, 130, 84, 62, "DETUNE");
        addKnob(cs80::P_mix, 32, 196, 82, 62, "MIX I / II");
        addKnob(cs80::P_brilliance, 120, 196, 84, 62, "BRILL");
        addKnob(cs80::P_resonance, 32, 262, 82, 62, "RES");
        addKnob(cs80::P_masterSustain, 120, 262, 84, 62, "SUSTAIN");
        addKnob(cs80::P_volume, 32, 328, 82, 62, "VOLUME");
        addKnob(cs80::P_initBend, 120, 328, 84, 62, "BEND");

        addKnob(cs80::P_chorusSpeed, 32, 420, 82, 60, "CHORUS RATE");
        addKnob(cs80::P_chorusDepth, 120, 420, 84, 60, "CHORUS DEPTH");
        addLed(cs80::P_chorusOn, "CHORUS", 30, 500, 82, 24);
        addLed(cs80::P_chorusMode, "TREMOLO", 120, 500, 84, 24);
        addLed(cs80::P_outputMode, "STEREO", 30, 534, 82, 24);
        addLed(cs80::P_mpeOn, "MPE", 120, 534, 84, 24);
    }

    void buildChannel(int channel, float y)
    {
        auto p = [channel](int local) { return cs80::chParam(channel, local); };
        const float x = 228.0f;

        // VCO ---------------------------------------------------------------
        auto& saw = addLed(p(cs80::CP_saw), "SAW", x + 18, y + 42, 72, 22);
        saw.accent = cs80::ui::amber;
        addLed(p(cs80::CP_pulse), "PULSE", x + 96, y + 42, 72, 22);
        addKnob(p(cs80::CP_sine), 232, y + 72, 74, 60, "SINE");
        addKnob(p(cs80::CP_noise), 308, y + 72, 74, 60, "NOISE");
        addKnob(p(cs80::CP_pw), 384, y + 72, 74, 60, "PULSE WIDTH");
        addKnob(p(cs80::CP_pwmSpeed), 232, y + 138, 74, 60, "PWM RATE");
        addKnob(p(cs80::CP_pwmDepth), 308, y + 138, 74, 60, "PWM DEPTH");
        addKnob(p(cs80::CP_feet), 384, y + 138, 74, 60, "FEET");

        // VCF ---------------------------------------------------------------
        addLed(p(cs80::CP_hpfOn), "HPF ON", 492, y + 42, 76, 22);
        addLed(p(cs80::CP_lpfOn), "LPF ON", 576, y + 42, 76, 22);
        addKnob(p(cs80::CP_hpfCutoff), 490, y + 72, 66, 60, "HPF");
        addKnob(p(cs80::CP_hpfRes), 560, y + 72, 66, 60, "HPF RES");
        addKnob(p(cs80::CP_lpfCutoff), 630, y + 72, 66, 60, "LPF");
        addKnob(p(cs80::CP_lpfRes), 700, y + 72, 66, 60, "LPF RES");
        addKnob(p(cs80::CP_fIl), 488, y + 138, 54, 60, "IL");
        addKnob(p(cs80::CP_fAl), 544, y + 138, 54, 60, "AL");
        addKnob(p(cs80::CP_fA), 600, y + 138, 54, 60, "A");
        addKnob(p(cs80::CP_fD), 656, y + 138, 54, 60, "D");
        addKnob(p(cs80::CP_fR), 712, y + 138, 54, 60, "R");

        // VCA ---------------------------------------------------------------
        addKnob(p(cs80::CP_vA), 796, y + 70, 54, 58, "A");
        addKnob(p(cs80::CP_vD), 853, y + 70, 54, 58, "D");
        addKnob(p(cs80::CP_vS), 910, y + 70, 54, 58, "S");
        addKnob(p(cs80::CP_vR), 796, y + 132, 54, 58, "R");
        addKnob(p(cs80::CP_vcfLevel), 853, y + 132, 54, 58, "VCF LVL");
        addKnob(p(cs80::CP_vLevel), 910, y + 132, 54, 58, "LEVEL");
        addKnob(p(cs80::CP_pan), 853, y + 194, 54, 54, "PAN");

        // Touch -------------------------------------------------------------
        addKnob(p(cs80::CP_initBrill), 982, y + 74, 100, 62, "INIT BRILL");
        addKnob(p(cs80::CP_initLevel), 1100, y + 74, 100, 62, "INIT LEVEL");
        addKnob(p(cs80::CP_afterBrill), 982, y + 144, 100, 62, "AFTER BRILL");
        addKnob(p(cs80::CP_afterLevel), 1100, y + 144, 100, 62, "AFTER LEVEL");
    }

    void buildChannels()
    {
        buildChannel(0, 88.0f);
        buildChannel(1, 358.0f);
    }

    void buildToneSection()
    {
        const float startX = 1238.0f;
        for (int channel = 0; channel < 2; ++channel)
        {
            for (int tone = 0; tone < 11; ++tone)
            {
                const int column = tone / 6;
                const int row = tone % 6;
                auto button = std::make_unique<juce::TextButton>(
                    juce::String(tone + 1).paddedLeft('0', 2) + "  "
                    + juce::String(cs80::hardwareToneName(tone)).toUpperCase());
                button->setClickingTogglesState(true);
                button->setRadioGroupId(0x100 + channel);
                button->setWantsKeyboardFocus(false);
                button->onClick = [this, channel, tone] { applyHardwareTone(channel, tone); };
                addAndMakeVisible(*button);
                const float y = 88.0f + (channel == 0 ? 42.0f : 202.0f) + (float) row * 24.0f;
                const float x = startX + (float) column * 164.0f;
                place(*button, x, y, 156.0f, 21.0f);
                toneButtons[(size_t) channel][(size_t) tone] = std::move(button);
            }
        }

        addKnob(cs80::P_macro1, 1240, 453, 76, 62, "BRIGHT");
        addKnob(cs80::P_macro2, 1320, 453, 76, 62, "MOTION");
        addKnob(cs80::P_macro3, 1400, 453, 76, 62, "WIDTH");
        addKnob(cs80::P_macro4, 1480, 453, 76, 62, "SPACE");
    }

    void buildPerformance()
    {
        addKnob(cs80::P_portamento, 160, 658, 88, 50, "GLIDE");
        addKnob(cs80::P_keyRelease, 252, 658, 88, 50, "RELEASE");
        addKnob(cs80::P_modWheelSub, 344, 658, 88, 50, "MOD > SUB");
        addKnob(cs80::P_modWheelVcf, 436, 658, 88, 50, "MOD > VCF");
        addKnob(cs80::P_pitchBendRange, 528, 658, 88, 50, "BEND RANGE");
        addKnob(cs80::P_tuningRef, 620, 658, 88, 50, "A4 TUNE");
        addKnob(cs80::P_arpRate, 712, 658, 88, 50, "ARP RATE");
        addKnob(cs80::P_fx1Mix, 804, 658, 88, 50, "FX 1 MIX");

        addLed(cs80::P_sustainMode, "SUSTAIN II", 910, 664, 116, 24);
        addLed(cs80::P_arpOn, "ARP", 1034, 664, 86, 24);
        addLed(cs80::P_chorusOn, "CHORUS", 1128, 664, 104, 24);
        addLed(cs80::P_chorusMode, "TREMOLO", 1240, 664, 104, 24);
        addLed(cs80::P_outputMode, "STEREO", 1352, 664, 96, 24);
    }

    void buildKeyboard()
    {
        keyboard.setAvailableRange(24, 84);
        keyboard.setLowestVisibleKey(24);
        keyboard.setScrollButtonsVisible(false);
        keyboard.setOctaveForMiddleC(4);
        keyboard.setVelocity(0.82f, true);
        keyboard.setMidiChannel(1);
        keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffd8d2c3));
        keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff0d1217));
        keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff6f685b));
        keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, cs80::ui::amber.withAlpha(0.22f));
        keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, cs80::ui::amber.withAlpha(0.46f));
        keyboard.setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha(0.55f));
        keyboard.setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, juce::Colour(0xff111a21));
        keyboard.setWantsKeyboardFocus(false);
        addAndMakeVisible(keyboard);

        ribbon.onValueChange = [this](float value)
        {
            processor.getEngine().setRibbon(value);
        };
        addAndMakeVisible(ribbon);

        pitchWheel.onValueChange = [this](float value)
        {
            processor.getEngine().pitchBend(1, value);
        };
        addAndMakeVisible(pitchWheel);

        modWheel.onValueChange = [this](float value)
        {
            processor.getEngine().controlChange(1, value, 1);
        };
        addAndMakeVisible(modWheel);
    }

    // -----------------------------------------------------------------------
    // Behaviour
    // -----------------------------------------------------------------------
    void stepPreset(int delta)
    {
        const auto& bank = processor.getFactoryPresets();
        if (bank.empty())
            return;
        const int count = (int) bank.size();
        int index = processor.getCurrentPresetIndex();
        // If the current state came from a user preset or an edited patch,
        // stepping starts from the closest factory program.
        index = juce::jlimit(0, count - 1, index);
        index = (index + delta + count) % count;
        processor.loadFactoryPreset(index);
        syncToneMarks();
    }

    void applyHardwareTone(int channel, int tone)
    {
        auto values = processor.currentValues();
        cs80::applyHardwareTone(values, channel, tone);
        processor.applyParameterValues(values);
        processor.setPresetName(juce::String(channel == 0 ? "Tone I: " : "Tone II: ")
                                + cs80::hardwareToneName(tone));
        syncToneMarks();
    }

    void clearToneMarks()
    {
        for (auto& bank : toneButtons)
            for (auto& button : bank)
                button->setToggleState(false, juce::dontSendNotification);
    }

    void syncToneMarks()
    {
        const auto name = processor.getPresetName();
        clearToneMarks();
        int channel = -1;
        if (name.startsWithIgnoreCase("Tone I: "))
            channel = 0;
        else if (name.startsWithIgnoreCase("Tone II: "))
            channel = 1;
        if (channel < 0)
            return;

        const auto toneName = name.fromFirstOccurrenceOf(":", false, false).trim();
        for (int tone = 0; tone < 11; ++tone)
            if (toneName.equalsIgnoreCase(cs80::hardwareToneName(tone)))
                toneButtons[(size_t) channel][(size_t) tone]->setToggleState(
                    true, juce::dontSendNotification);
    }

    void syncPresetDisplay()
    {
        const auto name = processor.getPresetName();
        if (presetLabel.getText() != name)
            presetLabel.setText(name, juce::dontSendNotification);
    }

    void showAdvanced(bool shouldShow)
    {
        advancedVisible = shouldShow;
        advanced.setVisible(shouldShow);
        advancedButton.setToggleState(shouldShow, juce::dontSendNotification);
        if (shouldShow)
        {
            browser.setVisible(false);
            browserVisible = false;
            advanced.toFront(true);
        }
        repaint();
    }

    void showBrowser(bool shouldShow)
    {
        browserVisible = shouldShow;
        if (shouldShow)
        {
            browser.refresh();
            browser.setVisible(true);
            browser.toFront(true);
            advanced.setVisible(false);
            advancedVisible = false;
            advancedButton.setToggleState(false, juce::dontSendNotification);
            browser.selectCurrentPreset();
        }
        else
        {
            browser.setVisible(false);
        }
        repaint();
    }

    void tick()
    {
        syncPresetDisplay();
        // The voice allocator is the only animated element; redrawing the
        // whole panel at 8 Hz also keeps preset metadata current.
        repaint();
    }

    // -----------------------------------------------------------------------
    // Painting
    // -----------------------------------------------------------------------
    void paintChannelDecorations(juce::Graphics& g, int channel, float y)
    {
        const bool first = channel == 0;
        auto panel = juce::Rectangle<float>(228.0f, y, 988.0f, first ? 262.0f : 280.0f);

        juce::ColourGradient gradient(juce::Colour(0xff15212b), panel.getX(), panel.getY(),
                                      juce::Colour(0xff0b1218), panel.getX(), panel.getBottom(), false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(panel, 5.0f);
        g.setColour(juce::Colour(0xff31414d));
        g.drawRoundedRectangle(panel, 5.0f, 1.0f);

        // top identification strip
        g.setColour(juce::Colour(0xff0a1117));
        g.fillRect(panel.getX() + 1.0f, panel.getY() + 1.0f, panel.getWidth() - 2.0f, 27.0f);
        g.setColour(cs80::ui::amber);
        g.fillRect(panel.getX() + 10.0f, panel.getY() + 9.0f, 3.0f, 11.0f);

        auto channelFont = displayFont( 13.0f,
                                      juce::Font::bold);
        channelFont.setExtraKerningFactor(0.14f);
        g.setFont(channelFont);
        g.setColour(cs80::ui::ivory);
        g.drawText(first ? "CHANNEL I" : "CHANNEL II",
                   (int) panel.getX() + 22, (int) panel.getY() + 5, 220, 20,
                   juce::Justification::centredLeft);

        g.setColour(cs80::ui::ivoryDim.withAlpha(0.75f));
        g.setFont(displayFont( 9.0f, juce::Font::bold));
        g.drawText(first ? "RANK A / RANK B  -  VCO - VCF - VCA"
                         : "RANK A / RANK B  -  DUAL VOICE CARD",
                   (int) panel.getX() + 252, (int) panel.getY() + 8, 480, 16,
                   juce::Justification::centredLeft);

        // section dividers and headings
        for (float divider : { 482.0f, 788.0f, 972.0f })
        {
            g.setColour(cs80::ui::lineColour.withAlpha(0.45f));
            g.drawVerticalLine((int) divider, panel.getY() + 34.0f, panel.getBottom() - 12.0f);
        }

        const struct { float x; const char* label; } sections[] = {
            { 232.0f, "VCO" }, { 490.0f, "VCF" }, { 794.0f, "VCA / AMP" },
            { 978.0f, "TOUCH RESPONSE" }
        };
        auto sectionFont = displayFont( 8.5f,
                                      juce::Font::bold);
        sectionFont.setExtraKerningFactor(0.11f);
        g.setFont(sectionFont);
        for (const auto& section : sections)
        {
            g.setColour(cs80::ui::amber.withAlpha(0.92f));
            g.drawText(section.label, (int) section.x, (int) panel.getY() + 31,
                       140, 12, juce::Justification::centredLeft);
        }

        g.setColour(cs80::ui::ivoryDim.withAlpha(0.50f));
        g.setFont(displayFont( 7.5f, juce::Font::plain));
        g.drawText("PRESSURE / VELOCITY", 982, (int) panel.getY() + 57, 200, 10,
                   juce::Justification::centredLeft);
        g.drawText("SINE PATH BYPASSES THE FILTER", 232, (int) panel.getY() + 220, 220, 10,
                   juce::Justification::centredLeft);
    }

    void paintMasterDecorations(juce::Graphics& g)
    {
        auto panel = juce::Rectangle<float>(24.0f, 88.0f, 194.0f, 550.0f);
        juce::ColourGradient gradient(juce::Colour(0xff14202a), panel.getX(), panel.getY(),
                                      juce::Colour(0xff0a1117), panel.getX(), panel.getBottom(), false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(panel, 5.0f);
        g.setColour(juce::Colour(0xff31414d));
        g.drawRoundedRectangle(panel, 5.0f, 1.0f);

        auto headingFont = displayFont( 12.0f,
                                      juce::Font::bold);
        headingFont.setExtraKerningFactor(0.12f);
        g.setFont(headingFont);
        g.setColour(cs80::ui::ivory);
        g.drawText("MASTER", 38, 98, 140, 18, juce::Justification::centredLeft);
        g.setColour(cs80::ui::amber);
        g.fillRect(30, 102, 3, 9);

        g.setColour(cs80::ui::ivoryDim.withAlpha(0.72f));
        g.setFont(displayFont( 8.0f, juce::Font::plain));
        g.drawText("GLOBAL VOICE CONDITIONING", 38, 114, 170, 12, juce::Justification::centredLeft);

        g.setColour(cs80::ui::lineColour.withAlpha(0.65f));
        g.drawHorizontalLine(398, 34.0f, 208.0f);
        g.setFont(displayFont( 8.0f, juce::Font::bold));
        g.setColour(cs80::ui::amber.withAlpha(0.9f));
        g.drawText("CHORUS / TREMOLO", 36, 403, 172, 12, juce::Justification::centredLeft);

        g.setColour(cs80::ui::lineColour.withAlpha(0.65f));
        g.drawHorizontalLine(486, 34.0f, 208.0f);
        g.setColour(cs80::ui::ivoryDim.withAlpha(0.45f));
        g.setFont(displayFont( 7.5f, juce::Font::plain));
        g.drawFittedText("BBD CHORUS - GLOBAL SUB OSC - RING MOD\n8 VOICES - 16 RANKS - 2x OVERSAMPLED",
                         34, 570, 172, 56, juce::Justification::topLeft, 2);
    }

    void paintToneDecorations(juce::Graphics& g)
    {
        auto panel = juce::Rectangle<float>(1228.0f, 88.0f, 350.0f, 550.0f);
        juce::ColourGradient gradient(juce::Colour(0xff16222b), panel.getX(), panel.getY(),
                                      juce::Colour(0xff0a1016), panel.getX(), panel.getBottom(), false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(panel, 5.0f);
        g.setColour(juce::Colour(0xff31414d));
        g.drawRoundedRectangle(panel, 5.0f, 1.0f);

        auto headingFont = displayFont( 12.0f,
                                      juce::Font::bold);
        headingFont.setExtraKerningFactor(0.13f);
        g.setFont(headingFont);
        g.setColour(cs80::ui::ivory);
        g.drawText("TONE SELECTOR", 1240, 98, 220, 18, juce::Justification::centredLeft);
        g.setColour(cs80::ui::amber);
        g.fillRect(1232, 102, 3, 9);

        g.setColour(cs80::ui::ivoryDim.withAlpha(0.78f));
        g.setFont(displayFont( 8.0f, juce::Font::bold));
        g.drawText("CHANNEL I", 1240, 116, 140, 12, juce::Justification::centredLeft);
        g.drawText("CHANNEL II", 1240, 276, 140, 12, juce::Justification::centredLeft);
        g.setColour(cs80::ui::lineColour.withAlpha(0.6f));
        g.drawHorizontalLine(430, 1236.0f, 1570.0f);
        g.setColour(cs80::ui::amber);
        g.setFont(displayFont( 8.0f, juce::Font::bold));
        g.drawText("PERFORMANCE MACROS", 1240, 438, 250, 12, juce::Justification::centredLeft);

        // Current patch metadata.
        juce::String category { "LIVE PANEL" };
        juce::String description { "Hardware tone selections and panel edits are live. "
                                   "Open the library to load one of the 200 performance patches." };
        const auto currentName = processor.getPresetName();
        for (const auto& preset : processor.getFactoryPresets())
        {
            if (currentName == juce::String(preset.name))
            {
                category = juce::String(preset.category).toUpperCase();
                description = juce::String(preset.description);
                break;
            }
        }

        g.setColour(cs80::ui::lineColour.withAlpha(0.65f));
        g.drawHorizontalLine(524, 1236.0f, 1570.0f);
        g.setColour(cs80::ui::amber);
        g.setFont(displayFont( 7.5f, juce::Font::bold));
        g.drawText(category, 1240, 532, 320, 12, juce::Justification::centredLeft);
        g.setColour(cs80::ui::ivoryDim.withAlpha(0.78f));
        g.setFont(displayFont( 8.2f, juce::Font::plain));
        g.drawFittedText(description, 1240, 548, 326, 80, juce::Justification::topLeft, 5, 0.82f);
    }

    void paintPerformanceDecorations(juce::Graphics& g)
    {
        auto panel = juce::Rectangle<float>(24.0f, 652.0f, 1554.0f, 62.0f);
        juce::ColourGradient gradient(juce::Colour(0xff121d25), panel.getX(), panel.getY(),
                                      juce::Colour(0xff080f14), panel.getX(), panel.getBottom(), false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(panel, 5.0f);
        g.setColour(juce::Colour(0xff2d3c47));
        g.drawRoundedRectangle(panel, 5.0f, 1.0f);

        auto headingFont = displayFont( 10.0f,
                                      juce::Font::bold);
        headingFont.setExtraKerningFactor(0.12f);
        g.setFont(headingFont);
        g.setColour(cs80::ui::ivory);
        g.drawText("PERFORMANCE", 34, 660, 118, 16, juce::Justification::centredLeft);
        g.setColour(cs80::ui::amber);
        g.setFont(displayFont( 7.0f, juce::Font::plain));
        g.drawFittedText("RIBBON - LEVER\nPRESSURE - GLIDE", 34, 678, 112, 28,
                         juce::Justification::topLeft, 2);
    }

    void paintTopBar(juce::Graphics& g)
    {
        g.setColour(juce::Colour(0xff080d12));
        g.fillRect(18.0f, 18.0f, 1564.0f, 58.0f);
        g.setColour(juce::Colour(0xff22313c));
        g.drawHorizontalLine(76, 18.0f, 1582.0f);

        auto brandFont = displayFont( 31.0f,
                                    juce::Font::bold);
        brandFont.setExtraKerningFactor(0.06f);
        g.setFont(brandFont);
        g.setColour(cs80::ui::ivory);
        g.drawText("CS-80", 38, 21, 132, 34, juce::Justification::centredLeft);

        auto brandSmall = displayFont( 8.0f,
                                     juce::Font::bold);
        brandSmall.setExtraKerningFactor(0.17f);
        g.setFont(brandSmall);
        g.setColour(cs80::ui::amber);
        g.drawText("REFERENCE INSTRUMENTS", 160, 27, 240, 12, juce::Justification::centredLeft);
        g.setColour(cs80::ui::ivoryDim.withAlpha(0.72f));
        g.setFont(displayFont( 7.2f, juce::Font::plain));
        g.drawText("DUAL-CHANNEL POLYPHONIC SYNTHESIZER", 160, 42, 260, 10,
                   juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xff0a1117));
        g.fillRoundedRectangle(456.0f, 25.0f, 338.0f, 32.0f, 3.0f);
        g.setColour(cs80::ui::lineColour.withAlpha(0.9f));
        g.drawRoundedRectangle(456.0f, 25.0f, 338.0f, 32.0f, 3.0f, 0.8f);
        g.setColour(cs80::ui::amber.withAlpha(0.7f));
        g.setFont(displayFont( 6.5f, juce::Font::bold));
        g.drawText("PROGRAM", 463, 27, 60, 8, juce::Justification::centredLeft);

        // Voice allocation LEDs.
        const int active = processor.getActiveVoiceCount();
        g.setColour(cs80::ui::ivoryDim.withAlpha(0.72f));
        g.setFont(displayFont( 7.0f, juce::Font::bold));
        g.drawText("VOICE ALLOCATION  " + juce::String(active) + " / 8",
                   1270, 24, 220, 10, juce::Justification::centredLeft);
        for (int i = 0; i < 8; ++i)
        {
            const bool on = processor.getEngine().getVoiceInfo(i).active;
            auto centre = juce::Point<float>(1276.0f + (float) i * 21.0f, 50.0f);
            if (on)
            {
                g.setColour(cs80::ui::amber.withAlpha(0.20f));
                g.fillEllipse(centre.x - 7.0f, centre.y - 7.0f, 14.0f, 14.0f);
            }
            g.setColour(on ? cs80::ui::amberBright : juce::Colour(0xff25323c));
            g.fillEllipse(centre.x - 3.0f, centre.y - 3.0f, 6.0f, 6.0f);
        }
        g.setColour(cs80::ui::ivoryDim.withAlpha(0.50f));
        g.setFont(displayFont( 7.0f, juce::Font::plain));
        g.drawText("16 RANKS  -  2x OVERSAMPLED  -  MPE", 1270, 59, 260, 9,
                   juce::Justification::centredLeft);
    }

    void paint(juce::Graphics& g) override
    {
        const float sx = (float) getWidth() / kDesignWidth;
        const float sy = (float) getHeight() / kDesignHeight;
        g.saveState();
        g.addTransform(juce::AffineTransform::scale(sx, sy));

        g.fillAll(cs80::ui::charcoal);

        // Wooden end cheeks.
        for (auto cheek : { juce::Rectangle<float>(0.0f, 10.0f, 20.0f, 980.0f),
                            juce::Rectangle<float>(1580.0f, 10.0f, 20.0f, 980.0f) })
        {
            juce::ColourGradient wood(juce::Colour(0xff6b4329), cheek.getX(), cheek.getY(),
                                      juce::Colour(0xff201009), cheek.getX(), cheek.getBottom(), false);
            g.setGradientFill(wood);
            g.fillRect(cheek);
            g.setColour(juce::Colour(0xffb47a48).withAlpha(0.32f));
            for (int i = 0; i < 9; ++i)
                g.drawVerticalLine((int) cheek.getX() + 4 + i * 2, cheek.getY() + 8, cheek.getBottom() - 8);
        }

        // Main chassis.
        juce::ColourGradient body(juce::Colour(0xff17242e), 800.0f, 18.0f,
                                  juce::Colour(0xff091016), 800.0f, 988.0f, false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(18.0f, 18.0f, 1564.0f, 970.0f, 8.0f);
        g.setColour(juce::Colour(0xff40515d));
        g.drawRoundedRectangle(18.0f, 18.0f, 1564.0f, 970.0f, 8.0f, 1.3f);

        // Subtle brushed-metal grain.
        g.setColour(juce::Colours::white.withAlpha(0.011f));
        for (int y = 80; y < 980; y += 4)
            g.drawHorizontalLine(y, 24.0f, 1576.0f);

        paintTopBar(g);
        paintMasterDecorations(g);
        paintChannelDecorations(g, 0, 88.0f);
        paintChannelDecorations(g, 1, 358.0f);
        paintToneDecorations(g);
        paintPerformanceDecorations(g);

        // Ribbon rail.
        g.setColour(juce::Colour(0xff0a1015));
        g.fillRoundedRectangle(24.0f, 722.0f, 1552.0f, 40.0f, 5.0f);
        g.setColour(juce::Colour(0xff2c3b46));
        g.drawRoundedRectangle(24.0f, 722.0f, 1552.0f, 40.0f, 5.0f, 1.0f);

        // Keyboard bed.
        g.setColour(juce::Colour(0xff05090c));
        g.fillRoundedRectangle(24.0f, 770.0f, 1552.0f, 208.0f, 5.0f);
        g.setColour(juce::Colour(0xff273641));
        g.drawRoundedRectangle(24.0f, 770.0f, 1552.0f, 208.0f, 5.0f, 1.0f);
        g.setColour(juce::Colour(0xff080d11));
        g.fillRect(24.0f, 770.0f, 1552.0f, 8.0f);
        g.setColour(cs80::ui::ivoryDim.withAlpha(0.55f));
        g.setFont(displayFont( 7.5f, juce::Font::bold));
        g.drawText("61-KEY VELOCITY / POLYPHONIC AFTERTOUCH KEYBED", 146, 962, 470, 12,
                   juce::Justification::centredLeft);

        // Screws.
        for (auto point : { juce::Point<float>(30.0f, 30.0f), juce::Point<float>(1570.0f, 30.0f),
                            juce::Point<float>(30.0f, 976.0f), juce::Point<float>(1570.0f, 976.0f) })
        {
            g.setColour(juce::Colour(0xff53606b));
            g.fillEllipse(point.x - 4.0f, point.y - 4.0f, 8.0f, 8.0f);
            g.setColour(juce::Colour(0xff111920));
            g.drawLine(point.x - 2.0f, point.y, point.x + 2.0f, point.y, 1.0f);
        }

        g.restoreState();
    }

    void resized() override
    {
        const float sx = (float) getWidth() / kDesignWidth;
        const float sy = (float) getHeight() / kDesignHeight;
        auto r = [sx, sy](float x, float y, float w, float h) {
            return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                                        juce::roundToInt(w * sx), juce::roundToInt(h * sy));
        };

        for (const auto& item : placements)
            item.component->setBounds(r(item.bounds.getX(), item.bounds.getY(),
                                        item.bounds.getWidth(), item.bounds.getHeight()));

        prevButton.setBounds(r(416.0f, 25.0f, 32.0f, 32.0f));
        presetLabel.setBounds(r(456.0f, 25.0f, 338.0f, 32.0f));
        presetLabel.setFont(displayFont(
                                       13.5f * sy, juce::Font::bold));
        nextButton.setBounds(r(800.0f, 25.0f, 32.0f, 32.0f));
        browseButton.setBounds(r(842.0f, 25.0f, 92.0f, 32.0f));
        saveButton.setBounds(r(942.0f, 25.0f, 80.0f, 32.0f));
        initButton.setBounds(r(1030.0f, 25.0f, 64.0f, 32.0f));
        advancedButton.setBounds(r(1102.0f, 25.0f, 154.0f, 32.0f));

        ribbon.setBounds(r(24.0f, 722.0f, 1552.0f, 40.0f));
        pitchWheel.setBounds(r(28.0f, 786.0f, 46.0f, 174.0f));
        modWheel.setBounds(r(82.0f, 786.0f, 46.0f, 174.0f));
        keyboard.setBounds(r(142.0f, 780.0f, 1432.0f, 186.0f));
        if (keyboard.getWidth() > 0)
            keyboard.setKeyWidth((float) keyboard.getWidth() / 36.0f);

        advanced.setBounds(r(24.0f, 88.0f, 1552.0f, 892.0f));
        browser.setBounds(r(200.0f, 120.0f, 1200.0f, 760.0f));
    }

    Cs80AudioProcessor& processor;
    cs80::ui::LookAndFeel lookAndFeel;

    juce::TextButton prevButton { "<" };
    juce::TextButton nextButton { ">" };
    juce::TextButton browseButton { "LIBRARY" };
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton initButton { "INIT" };
    juce::TextButton advancedButton { "ADVANCED" };
    juce::Label presetLabel;

    std::vector<std::unique_ptr<cs80::ui::ParamKnob>> knobs;
    std::vector<std::unique_ptr<cs80::ui::LedButton>> leds;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::array<std::array<std::unique_ptr<juce::TextButton>, 11>, 2> toneButtons;
    std::vector<Placement> placements;

    juce::MidiKeyboardComponent keyboard;
    cs80::ui::RibbonStrip ribbon;
    cs80::ui::PerformanceWheel pitchWheel;
    cs80::ui::PerformanceWheel modWheel;

    cs80::ui::AdvancedPanel advanced;
    cs80::ui::PresetBrowser browser;

    bool advancedVisible = false;
    bool browserVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Impl)
};

Cs80AudioProcessorEditor::Cs80AudioProcessorEditor(Cs80AudioProcessor& p)
    : AudioProcessorEditor(&p)
{
    setOpaque(true);
    impl = std::make_unique<Impl>(p);
    addAndMakeVisible(*impl);

    setSize(1600, 1000);
    setResizable(true, true);
    setResizeLimits(1120, 700, 2400, 1500);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(kDesignWidth / kDesignHeight);

    startTimerHz(8);
}

Cs80AudioProcessorEditor::~Cs80AudioProcessorEditor()
{
    stopTimer();
    static_cast<Cs80AudioProcessor&>(processor).onPresetChanged = nullptr;
    impl.reset();
}

void Cs80AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(cs80::ui::charcoal);
}

void Cs80AudioProcessorEditor::resized()
{
    if (impl != nullptr)
        impl->setBounds(getLocalBounds());
}

void Cs80AudioProcessorEditor::timerCallback()
{
    if (impl != nullptr)
        impl->tick();
}

void Cs80AudioProcessorEditor::showAdvancedPanel(bool shouldShow)
{
    if (impl != nullptr)
        impl->showAdvanced(shouldShow);
}

void Cs80AudioProcessorEditor::showAdvancedPage(int pageIndex)
{
    if (impl != nullptr)
        impl->advanced.showPage(pageIndex);
}

void Cs80AudioProcessorEditor::showPresetBrowser(bool shouldShow)
{
    if (impl != nullptr)
        impl->showBrowser(shouldShow);
}

bool Cs80AudioProcessorEditor::isAdvancedVisible() const
{
    return impl != nullptr && impl->advancedVisible;
}
