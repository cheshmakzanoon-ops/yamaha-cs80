#include "AdvancedPanel.h"

#include "PluginProcessor.h"
#include "core/FactoryPresets.h"

#include <vector>

namespace cs80::ui
{

namespace
{
const juce::StringArray kDestinations { "OFF", "PITCH", "VCF", "VCA", "PWM", "PAN" };
const juce::StringArray kFxTypes { "OFF", "CHORUS", "PHASER", "TAPE DELAY", "PLATE REVERB", "DRIVE" };
const juce::StringArray kSubWaves { "SINE", "TRIANGLE", "SAW", "SQUARE", "S&H", "RANDOM" };
const juce::StringArray kArpModes { "UP", "DOWN", "UP / DOWN", "RANDOM", "PLAYED" };
const juce::StringArray kGlideModes { "OFF", "PORTAMENTO", "GLISSANDO" };

struct TextItem
{
    juce::String text;
    juce::Rectangle<float> bounds;
    float height = 10.0f;
    bool bold = false;
    juce::Colour colour { ivory };
    juce::Justification justification { juce::Justification::centredLeft };
    int lines = 1;
};

struct SectionItem
{
    juce::Rectangle<float> bounds;
    juce::String title;
};

struct ComponentItem
{
    juce::Component* component = nullptr;
    juce::Rectangle<float> bounds;
};

juce::Font displayFont(float height, int style = juce::Font::plain)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(),
                                        height, style));
}
} // namespace

class AdvancedPage final : public juce::Component
{
public:
    AdvancedPage(Cs80AudioProcessor& p, float designWidth, float designHeight)
        : proc(p), designW(designWidth), designH(designHeight)
    {
        setInterceptsMouseClicks(false, true);
    }

    void paint(juce::Graphics& g) override
    {
        const float sx = (float) getWidth() / designW;
        const float sy = (float) getHeight() / designH;
        g.saveState();
        g.addTransform(juce::AffineTransform::scale(sx, sy));

        for (const auto& section : sections)
        {
            juce::ColourGradient fill(juce::Colour(0xff17232d), section.bounds.getX(), section.bounds.getY(),
                                      juce::Colour(0xff0b131a), section.bounds.getX(), section.bounds.getBottom(), false);
            g.setGradientFill(fill);
            g.fillRoundedRectangle(section.bounds, 5.0f);
            g.setColour(lineColour.withAlpha(0.75f));
            g.drawRoundedRectangle(section.bounds, 5.0f, 1.0f);
            g.setColour(amber);
            g.fillRect(section.bounds.getX() + 14.0f, section.bounds.getY() + 15.0f, 3.0f, 13.0f);
            g.setColour(ivory);
            auto titleFont = displayFont( 13.0f, juce::Font::bold);
            titleFont.setExtraKerningFactor(0.09f);
            g.setFont(titleFont);
            g.drawText(section.title.toUpperCase(), (int) section.bounds.getX() + 25,
                       (int) section.bounds.getY() + 10, (int) section.bounds.getWidth() - 40, 22,
                       juce::Justification::centredLeft);
            g.setColour(lineColour.withAlpha(0.55f));
            g.drawHorizontalLine((int) section.bounds.getY() + 37,
                                 section.bounds.getX() + 14.0f, section.bounds.getRight() - 14.0f);
        }

        for (const auto& text : texts)
        {
            auto font = displayFont(
                                   text.height, text.bold ? juce::Font::bold : juce::Font::plain);
            font.setExtraKerningFactor(text.bold ? 0.04f : 0.0f);
            g.setFont(font);
            g.setColour(text.colour);
            g.drawFittedText(text.text, text.bounds.toNearestInt(), text.justification,
                             text.lines, 0.8f);
        }
        g.restoreState();
    }

    void resized() override
    {
        const float sx = (float) getWidth() / designW;
        const float sy = (float) getHeight() / designH;
        for (auto& item : components)
            if (item.component != nullptr)
                item.component->setBounds(juce::Rectangle<int>(
                    juce::roundToInt(item.bounds.getX() * sx),
                    juce::roundToInt(item.bounds.getY() * sy),
                    juce::roundToInt(item.bounds.getWidth() * sx),
                    juce::roundToInt(item.bounds.getHeight() * sy)));
    }

    void section(float x, float y, float w, float h, const juce::String& title)
    {
        sections.push_back({ { x, y, w, h }, title });
    }

    void text(const juce::String& value, float x, float y, float w, float h,
              float height, bool bold = false, juce::Colour colour = ivory,
              juce::Justification justification = juce::Justification::topLeft, int lines = 1)
    {
        texts.push_back({ value, { x, y, w, h }, height, bold, colour, justification, lines });
    }

    void rule(float x, float y, float w)
    {
        texts.push_back({ juce::String::repeatedString("-", 1), { x, y, w, 1.0f }, 1.0f, false,
                          lineColour, juce::Justification::centredLeft, 1 });
    }

    ParamKnob* knob(float x, float y, float w, float h, int parameter,
                    const juce::String& label = {})
    {
        auto control = std::make_unique<ParamKnob>(proc, parameter, label);
        auto* raw = control.get();
        owned.push_back(std::move(control));
        addAndMakeVisible(*raw);
        components.push_back({ raw, { x, y, w, h } });
        return raw;
    }

    LedButton* toggle(float x, float y, float w, float h, int parameter,
                      const juce::String& label)
    {
        auto control = std::make_unique<LedButton>(label);
        auto* raw = control.get();
        addAndMakeVisible(*raw);
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.getAPVTS(), Cs80AudioProcessor::parameterId(parameter), *raw));
        owned.push_back(std::move(control));
        components.push_back({ raw, { x, y, w, h } });
        return raw;
    }

    juce::ComboBox* choice(float x, float y, float w, float h, int parameter,
                           const juce::StringArray& items)
    {
        auto control = std::make_unique<juce::ComboBox>();
        control->addItemList(items, 1);
        control->setJustificationType(juce::Justification::centredLeft);
        control->setWantsKeyboardFocus(false);
        auto* raw = control.get();
        addAndMakeVisible(*raw);
        comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            proc.getAPVTS(), Cs80AudioProcessor::parameterId(parameter), *raw));
        owned.push_back(std::move(control));
        components.push_back({ raw, { x, y, w, h } });
        return raw;
    }

private:
    Cs80AudioProcessor& proc;
    float designW;
    float designH;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<ComponentItem> components;
    std::vector<TextItem> texts;
    std::vector<SectionItem> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedPage)
};

AdvancedPanel::AdvancedPanel(Cs80AudioProcessor& p)
    : processor(p)
{
    setVisible(false);

    for (int i = 0; i < 4; ++i)
    {
        pages[(size_t) i] = std::make_unique<AdvancedPage>(processor, 1500.0f, 774.0f);
        addAndMakeVisible(*pages[(size_t) i]);
    }

    const char* names[] = { "MODULATION", "SUB OSC / RING", "FX / MACROS", "ARP / PERFORMANCE" };
    for (int i = 0; i < 4; ++i)
    {
        auto tab = std::make_unique<juce::TextButton>(names[i]);
        tab->setClickingTogglesState(true);
        tab->setRadioGroupId(0x80 + 7);
        tab->setWantsKeyboardFocus(false);
        tab->onClick = [this, i] { showPage(i); };
        addAndMakeVisible(*tab);
        tabs[(size_t) i] = std::move(tab);
    }

    closeButton.setWantsKeyboardFocus(false);
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };
    addAndMakeVisible(closeButton);

    // --- Modulation --------------------------------------------------------
    {
        auto& page = *pages[0];
        const float xs[3] = { 20.0f, 515.0f, 1010.0f };
        const int rateParams[3] = { cs80::P_lfo1Rate, cs80::P_lfo2Rate, cs80::P_lfo3Rate };
        const int depthParams[3] = { cs80::P_lfo1Depth, cs80::P_lfo2Depth, cs80::P_lfo3Depth };
        const int destParams[3] = { cs80::P_lfo1Dest, cs80::P_lfo2Dest, cs80::P_lfo3Dest };
        for (int i = 0; i < 3; ++i)
        {
            page.section(xs[i], 20.0f, 470.0f, 250.0f, "LFO " + juce::String(i + 1));
            page.knob(xs[i] + 24.0f, 76.0f, 106.0f, 94.0f, rateParams[i], "RATE");
            page.knob(xs[i] + 140.0f, 76.0f, 106.0f, 94.0f, depthParams[i], "DEPTH");
            page.text("DESTINATION", xs[i] + 270.0f, 62.0f, 180.0f, 16.0f, 9.0f, true, ivoryDim);
            page.choice(xs[i] + 270.0f, 82.0f, 180.0f, 30.0f, destParams[i], kDestinations);
            page.text(i == 0 ? "Free-running global LFO. Route it to pitch, filter,\namplitude, PWM or pan."
                             : (i == 1 ? "Second LFO, independent rate and destination.\nUse it for slow filter drift."
                                       : "Third LFO. A fast setting gives the CS-80\nits characteristic shimmer."),
                      xs[i] + 24.0f, 184.0f, 420.0f, 44.0f, 10.5f, false, ivoryDim,
                      juce::Justification::topLeft, 2);
        }

        page.section(20.0f, 290.0f, 730.0f, 440.0f, "Modulation routing / expression");
        page.knob(48.0f, 350.0f, 112.0f, 92.0f, cs80::P_modWheelSub, "WHEEL > SUB");
        page.knob(178.0f, 350.0f, 112.0f, 92.0f, cs80::P_modWheelVcf, "WHEEL > VCF");
        page.knob(308.0f, 350.0f, 112.0f, 92.0f, cs80::P_initBend, "INITIAL BEND");
        page.knob(438.0f, 350.0f, 112.0f, 92.0f, cs80::P_portamento, "GLIDE");
        page.text("GLIDE MODE", 48.0f, 466.0f, 170.0f, 16.0f, 9.0f, true, ivoryDim);
        page.choice(48.0f, 486.0f, 180.0f, 30.0f, cs80::P_glideMode, kGlideModes);
        page.toggle(254.0f, 488.0f, 145.0f, 28.0f, cs80::P_mpeOn, "MPE");
        page.toggle(410.0f, 488.0f, 170.0f, 28.0f, cs80::P_sustainMode, "SUSTAIN II");
        page.toggle(592.0f, 488.0f, 135.0f, 28.0f, cs80::P_outputMode, "STEREO");
        page.text("The CS-80's pressure and velocity are separate performance dimensions.\n"
                  "Initial touch sets the attack brightness; aftertouch bends the filter while held.\n"
                  "MPE maps per-note pitch bend and pressure when the host sends it.",
                  48.0f, 544.0f, 670.0f, 70.0f, 10.8f, false, ivoryDim,
                  juce::Justification::topLeft, 3);

        page.section(770.0f, 290.0f, 710.0f, 440.0f, "Pitch / tuning / expression");
        page.knob(800.0f, 350.0f, 112.0f, 92.0f, cs80::P_pitchBendRange, "WHEEL BEND");
        page.knob(930.0f, 350.0f, 112.0f, 92.0f, cs80::P_mpeBendRange, "MPE BEND");
        page.knob(1060.0f, 350.0f, 112.0f, 92.0f, cs80::P_tuningRef, "A4 TUNING");
        page.knob(1190.0f, 350.0f, 112.0f, 92.0f, cs80::P_keyRelease, "KEY RELEASE");
        page.knob(1320.0f, 350.0f, 112.0f, 92.0f, cs80::P_masterTune, "MASTER TUNE");
        page.text("PLAY RANGE", 800.0f, 464.0f, 160.0f, 16.0f, 9.0f, true, ivoryDim);
        page.text("88 keys / MIDI 0-127", 800.0f, 482.0f, 220.0f, 18.0f, 10.5f, false, ivory);
        page.text("MICROTUNING", 800.0f, 514.0f, 160.0f, 16.0f, 9.0f, true, ivoryDim);
        page.text("The engine is tuned to the selected A4 reference. Use a MIDI\n"
                  "tuning standard or an MTS-capable host for alternate temperaments.",
                  800.0f, 532.0f, 640.0f, 42.0f, 10.5f, false, ivoryDim);
        page.text("The pitch lever is spring-loaded; MPE member channels use their own bend\n"
                  "range. The ribbon is a separate continuous controller and returns to centre.",
                  800.0f, 610.0f, 640.0f, 42.0f, 10.5f, false, ivoryDim);
    }

    // --- Sub oscillator, ring modulator, chorus, analog voice ---------------
    {
        auto& page = *pages[1];
        page.section(20.0f, 20.0f, 730.0f, 330.0f, "Sub oscillator");
        page.text("WAVEFORM", 48.0f, 66.0f, 170.0f, 16.0f, 9.0f, true, ivoryDim);
        page.choice(48.0f, 86.0f, 180.0f, 30.0f, cs80::P_subWave, kSubWaves);
        page.knob(260.0f, 72.0f, 120.0f, 96.0f, cs80::P_subSpeed, "SPEED");
        page.knob(48.0f, 178.0f, 94.0f, 72.0f, cs80::P_subVco, "VCO");
        page.knob(158.0f, 178.0f, 94.0f, 72.0f, cs80::P_subVcf, "VCF");
        page.knob(268.0f, 178.0f, 94.0f, 72.0f, cs80::P_subVca, "VCA");
        page.knob(400.0f, 178.0f, 94.0f, 72.0f, cs80::P_subAtSpeed, "AT SPEED");
        page.knob(510.0f, 178.0f, 94.0f, 72.0f, cs80::P_subAtVco, "AT VCO");
        page.text("The CS-80's sub-oscillator is a global modulation source, not a sub-bass\n"
                  "oscillator. The VCO/VCF/VCA amounts sweep the voice and its own rate under\n"
                  "pressure. At low speeds it is a slow tremolo; at audio rates it adds sidebands.",
                  48.0f, 258.0f, 670.0f, 60.0f, 10.8f, false, ivoryDim,
                  juce::Justification::topLeft, 3);

        page.section(770.0f, 20.0f, 710.0f, 330.0f, "Ring modulator");
        page.knob(800.0f, 82.0f, 118.0f, 100.0f, cs80::P_ringSpeed, "SPEED");
        page.knob(928.0f, 82.0f, 118.0f, 100.0f, cs80::P_ringDepth, "DEPTH");
        page.knob(1056.0f, 82.0f, 118.0f, 100.0f, cs80::P_ringAttack, "ATTACK");
        page.knob(1184.0f, 82.0f, 118.0f, 100.0f, cs80::P_ringDecay, "DECAY");
        page.knob(1312.0f, 82.0f, 118.0f, 100.0f, cs80::P_ringAmount, "AMOUNT");
        page.text("A global ring modulator feeds the output alongside the voices. The envelope\n"
                  "is retriggered by the first key of a phrase. Depth and amount are independent:\n"
                  "depth controls the modulator level, amount controls the dry/wet balance.",
                  800.0f, 212.0f, 650.0f, 62.0f, 10.8f, false, ivoryDim,
                  juce::Justification::topLeft, 3);

        page.section(20.0f, 372.0f, 730.0f, 378.0f, "Chorus / tremolo");
        page.knob(48.0f, 434.0f, 126.0f, 104.0f, cs80::P_chorusSpeed, "SPEED");
        page.knob(186.0f, 434.0f, 126.0f, 104.0f, cs80::P_chorusDepth, "DEPTH");
        page.toggle(342.0f, 450.0f, 136.0f, 28.0f, cs80::P_chorusOn, "CHORUS");
        page.toggle(492.0f, 450.0f, 150.0f, 28.0f, cs80::P_chorusMode, "TREMOLO");
        page.text("The original uses a pair of BBD delay lines with complementary modulation.\n"
                  "This model keeps the band-limited, slightly noisy character of the BBD\n"
                  "rather than a clean digital chorus. Tremolo taps the same source.",
                  48.0f, 570.0f, 660.0f, 70.0f, 10.8f, false, ivoryDim,
                  juce::Justification::topLeft, 3);

        page.section(770.0f, 372.0f, 710.0f, 378.0f, "Analog voice");
        page.knob(800.0f, 432.0f, 126.0f, 104.0f, cs80::P_voiceDispersion, "DISPERSION");
        page.knob(938.0f, 432.0f, 126.0f, 104.0f, cs80::P_drift, "DRIFT");
        page.knob(1076.0f, 432.0f, 126.0f, 104.0f, cs80::P_analogNoise, "NOISE");
        page.text("Dispersion spreads the eight voice cards in tuning, cutoff and level.\n"
                  "Drift slowly moves each card; noise adds the floor of a 1977 signal path.\n"
                  "Set them to zero for a calibrated, repeatable render.",
                  800.0f, 554.0f, 650.0f, 60.0f, 10.8f, false, ivoryDim,
                  juce::Justification::topLeft, 3);
    }

    // --- FX and macros ------------------------------------------------------
    {
        auto& page = *pages[2];
        const float xs[3] = { 20.0f, 510.0f, 1000.0f };
        const int typeParams[3] = { cs80::P_fx1Type, cs80::P_fx2Type, cs80::P_fx3Type };
        const int mixParams[3] = { cs80::P_fx1Mix, cs80::P_fx2Mix, cs80::P_fx3Mix };
        for (int i = 0; i < 3; ++i)
        {
            page.section(xs[i], 20.0f, 480.0f, 300.0f, "FX " + juce::String(i + 1));
            page.text("TYPE", xs[i] + 28.0f, 70.0f, 160.0f, 16.0f, 9.0f, true, ivoryDim);
            page.choice(xs[i] + 28.0f, 90.0f, 250.0f, 30.0f, typeParams[i], kFxTypes);
            page.knob(xs[i] + 298.0f, 72.0f, 140.0f, 112.0f, mixParams[i], "MIX");
            page.text("Three insert slots, processed in series after the voice mixer.\n"
                      "Tape Delay includes modulation and a filtered feedback path.\n"
                      "Plate Reverb is a dense, bright algorithmic plate.",
                      xs[i] + 28.0f, 204.0f, 424.0f, 66.0f, 10.6f, false, ivoryDim,
                      juce::Justification::topLeft, 3);
        }

        page.section(20.0f, 342.0f, 730.0f, 408.0f, "Performance macros");
        page.knob(52.0f, 406.0f, 136.0f, 112.0f, cs80::P_macro1, "BRIGHTNESS");
        page.knob(196.0f, 406.0f, 136.0f, 112.0f, cs80::P_macro2, "MOTION");
        page.knob(340.0f, 406.0f, 136.0f, 112.0f, cs80::P_macro3, "WIDTH");
        page.knob(484.0f, 406.0f, 136.0f, 112.0f, cs80::P_macro4, "SPACE");
        page.text("Macros are patch-level offsets. At centre they are neutral; turning them\n"
                  "up pushes brilliance, envelope motion, detune/chorus width and the sub/ring\n"
                  "depth respectively. They never replace the underlying panel settings.",
                  52.0f, 552.0f, 660.0f, 68.0f, 10.8f, false, ivoryDim,
                  juce::Justification::topLeft, 3);

        page.section(770.0f, 342.0f, 710.0f, 408.0f, "Output stage");
        page.knob(800.0f, 406.0f, 128.0f, 112.0f, cs80::P_volume, "VOLUME");
        page.knob(938.0f, 406.0f, 128.0f, 112.0f, cs80::P_mix, "MIX I/II");
        page.toggle(1090.0f, 420.0f, 150.0f, 28.0f, cs80::P_outputMode, "STEREO");
        page.toggle(1254.0f, 420.0f, 150.0f, 28.0f, cs80::P_mpeOn, "MPE");
        page.text("Stereo mode pans the two ranks and opens the chorus to its full width.\n"
                  "Mono sums the output after the effects so it folds down without phase loss.",
                  1090.0f, 466.0f, 310.0f, 56.0f, 10.5f, false, ivoryDim,
                  juce::Justification::topLeft, 3);
        page.text("All effects are bypassed at 0% mix. The engine is oversampled 2x internally\n"
                  "and the output stage is DC-blocked and soft-limited.",
                  800.0f, 584.0f, 650.0f, 42.0f, 10.5f, false, ivoryDim);
    }

    // --- Arpeggiator and performance ---------------------------------------
    {
        auto& page = *pages[3];
        page.section(20.0f, 20.0f, 730.0f, 330.0f, "Arpeggiator");
        page.toggle(48.0f, 70.0f, 140.0f, 28.0f, cs80::P_arpOn, "ARP ON");
        page.text("MODE", 48.0f, 120.0f, 160.0f, 16.0f, 9.0f, true, ivoryDim);
        page.choice(48.0f, 140.0f, 220.0f, 30.0f, cs80::P_arpMode, kArpModes);
        page.knob(300.0f, 88.0f, 125.0f, 104.0f, cs80::P_arpRate, "RATE");
        page.knob(438.0f, 88.0f, 125.0f, 104.0f, cs80::P_arpOctaves, "OCTAVES");
        page.text("The arpeggiator holds the chord and retriggers the CS-80 voice\n"
                  "envelopes at the selected clock. Up/Down, random and played-order modes\n"
                  "cover the patterns used on the classic records.",
                  48.0f, 224.0f, 660.0f, 66.0f, 10.8f, false, ivoryDim,
                  juce::Justification::topLeft, 3);

        page.section(770.0f, 20.0f, 710.0f, 330.0f, "Performance");
        page.knob(800.0f, 86.0f, 120.0f, 102.0f, cs80::P_portamento, "GLIDE");
        page.knob(928.0f, 86.0f, 120.0f, 102.0f, cs80::P_keyRelease, "RELEASE");
        page.knob(1056.0f, 86.0f, 120.0f, 102.0f, cs80::P_initBend, "INIT BEND");
        page.text("GLIDE MODE", 1200.0f, 78.0f, 200.0f, 16.0f, 9.0f, true, ivoryDim);
        page.choice(1200.0f, 98.0f, 210.0f, 30.0f, cs80::P_glideMode, kGlideModes);
        page.toggle(1200.0f, 150.0f, 210.0f, 28.0f, cs80::P_sustainMode, "SUSTAIN II");
        page.text("Portamento glides between successive notes. Glissando quantises\n"
                  "the glide to semitones. Sustain I releases normally; Sustain II latches\n"
                  "released notes until the next key press.",
                  800.0f, 220.0f, 650.0f, 62.0f, 10.8f, false, ivoryDim,
                  juce::Justification::topLeft, 3);

        page.section(20.0f, 372.0f, 1460.0f, 378.0f, "About this instrument");
        page.text("CS-80 REFERENCE", 52.0f, 416.0f, 400.0f, 26.0f, 19.0f, true, ivory);
        page.text("A circuit-informed recreation of the Yamaha CS-80 (1977): eight voices, each with two\n"
                  "independent ranks, for sixteen oscillator/filter/VCA chains. The signal path follows the\n"
                  "original's high-pass into low-pass topology, linear VCA and global sub-oscillator.\n\n"
                  "The primary panel exposes the controls a player touches; this page exposes the modern\n"
                  "layer (LFOs, MPE, effects, dispersion) without cluttering the instrument. All controls are\n"
                  "host-automatable and saved in the plugin state. The preset bank includes the 22 hardware\n"
                  "tone-selector programs plus 200 performance patches.",
                  52.0f, 450.0f, 900.0f, 190.0f, 12.0f, false, ivoryDim,
                  juce::Justification::topLeft, 8);
        page.text("ENGINE  -  2x oversampled  -  8 voices  -  16 ranks  -  2x host rate\n"
                  "FORMAT  -  VST3 + standalone  -  44.1-192 kHz  -  MPE + poly pressure\n"
                  "BUILD   -  Reference Instruments  -  circuit study no. 1977-CS80",
                  1000.0f, 450.0f, 440.0f, 90.0f, 11.0f, false, amber,
                  juce::Justification::topLeft, 4);
        page.text("Touch the ribbon above the keyboard to bend pitch. Drag the pitch lever (PITCH)\n"
                  "or modulation wheel (MOD) at the left of the keyboard. Double-click any knob to reset it.",
                  52.0f, 646.0f, 900.0f, 50.0f, 10.8f, false, ivoryDim,
                  juce::Justification::topLeft, 3);
    }

    showPage(0);
}

AdvancedPanel::~AdvancedPanel() = default;

void AdvancedPanel::showPage(int index)
{
    activePage = juce::jlimit(0, 3, index);
    for (int i = 0; i < 4; ++i)
    {
        pages[(size_t) i]->setVisible(i == activePage);
        tabs[(size_t) i]->setToggleState(i == activePage, juce::dontSendNotification);
    }
    repaint();
}

void AdvancedPanel::paint(juce::Graphics& g)
{
    const float sx = (float) getWidth() / 1552.0f;
    const float sy = (float) getHeight() / 892.0f;
    g.saveState();
    g.addTransform(juce::AffineTransform::scale(sx, sy));

    juce::ColourGradient backdrop(juce::Colour(0xff090f15), 0.0f, 0.0f,
                                   juce::Colour(0xff04080b), 0.0f, 892.0f, false);
    g.setGradientFill(backdrop);
    g.fillRoundedRectangle(0.0f, 0.0f, 1552.0f, 892.0f, 7.0f);
    g.setColour(amber.withAlpha(0.55f));
    g.drawRoundedRectangle(0.5f, 0.5f, 1551.0f, 891.0f, 7.0f, 1.2f);

    g.setColour(juce::Colour(0xff0d161d));
    g.fillRect(0.0f, 0.0f, 1552.0f, 68.0f);
    g.setColour(lineColour.withAlpha(0.75f));
    g.drawHorizontalLine(68, 0.0f, 1552.0f);

    auto titleFont = displayFont( 20.0f, juce::Font::bold);
    titleFont.setExtraKerningFactor(0.12f);
    g.setFont(titleFont);
    g.setColour(ivory);
    g.drawText("ADVANCED CONTROL", 26, 13, 280, 26, juce::Justification::centredLeft);
    g.setColour(amber);
    g.setFont(displayFont( 9.0f, juce::Font::bold));
    g.drawText("MODERN LAYER / ALL PARAMETERS HOST-AUTOMATABLE", 27, 39, 430, 16,
               juce::Justification::centredLeft);

    g.setColour(ivoryDim);
    g.setFont(displayFont( 10.0f, juce::Font::plain));
    g.drawText("PAGE " + juce::String(activePage + 1) + " / 4", 1420, 24, 110, 20,
               juce::Justification::centredRight);

    g.restoreState();
}

void AdvancedPanel::resized()
{
    const float sx = (float) getWidth() / 1552.0f;
    const float sy = (float) getHeight() / 892.0f;
    auto r = [sx, sy](float x, float y, float w, float h) {
        return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };

    for (int i = 0; i < 4; ++i)
    {
        tabs[(size_t) i]->setBounds(r(300.0f + (float) i * 156.0f, 18.0f, 150.0f, 32.0f));
        pages[(size_t) i]->setBounds(r(26.0f, 92.0f, 1500.0f, 774.0f));
    }
    closeButton.setBounds(r(1442.0f, 18.0f, 86.0f, 32.0f));
}

} // namespace cs80::ui
