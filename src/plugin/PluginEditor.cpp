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
// Software-rendered key surfaces keep snapshots identical to the interactive
// editor. The key depth, moulded front lip and black-key bevel are drawn here.
class HardwareKeyboard final : public juce::MidiKeyboardComponent
{
public:
    HardwareKeyboard(juce::MidiKeyboardState& state, Orientation orientation)
        : MidiKeyboardComponent(state, orientation) {}

    void drawWhiteNote(int, juce::Graphics& g, juce::Rectangle<float> area,
                       bool down, bool over, juce::Colour, juce::Colour) override
    {
        auto key = area.reduced(0.6f, 0.0f);
        const float front = key.getHeight() * 0.09f;
        const auto colour = down ? juce::Colour(0xffd4bd82) : juce::Colour(0xffece7d7);
        g.setGradientFill(juce::ColourGradient(colour.darker(0.30f), key.getX(), key.getY(),
                                               colour.brighter(0.07f), key.getX(), key.getBottom() - front, false));
        g.fillRect(key);
        g.setColour(colour.darker(0.16f));
        g.fillRect(key.removeFromBottom(front));
        g.setColour(juce::Colours::white.withAlpha(0.62f));
        g.drawHorizontalLine((int) key.getBottom(), key.getX() + 1, key.getRight() - 1);
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.drawVerticalLine((int) area.getRight()-1, area.getY(), area.getBottom());
        if (over && !down) { g.setColour(juce::Colour(0xfff3d691).withAlpha(0.17f)); g.fillRect(area); }
    }

    void drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> area,
                       bool down, bool over, juce::Colour) override
    {
        g.setColour(juce::Colour(0xff010201)); g.fillRect(area);
        auto cap = area.reduced(area.getWidth() * 0.13f, 0);
        cap.removeFromBottom(area.getHeight() * 0.13f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff30332f), cap.getX(), cap.getY(),
                                               down ? juce::Colour(0xff76684a) : juce::Colour(0xff111410),
                                               cap.getX(), cap.getBottom(), false));
        g.fillRect(cap);
        g.setColour(juce::Colour(0xff63655e));
        g.drawHorizontalLine((int) cap.getBottom(), cap.getX(), cap.getRight());
        g.setColour(juce::Colours::white.withAlpha(over ? 0.2f : 0.08f));
        g.drawVerticalLine((int) cap.getX(), cap.getY(), cap.getBottom());
    }
};
// Magnified view of the four independent analog-memory panels. This does not
// copy a live patch: moving each miniature control edits that bank directly.
class MemoryProgrammer final : public juce::Component
{
public:
    explicit MemoryProgrammer(Cs80AudioProcessor& p) : processor(p)
    {
        setOpaque(true);
        addAndMakeVisible(close);
        setTitle("Memory programmer");
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        close.setButtonText("CLOSE LID");
        close.setTitle("Close memory programmer");
        close.setWantsKeyboardFocus(true);
        close.onClick = [this] { if (onClose) onClose(); };
        for (int bank = 0; bank < 4; ++bank)
        {
            auto button = std::make_unique<juce::TextButton>("MEMORY " + juce::String(bank + 1));
            button->setClickingTogglesState(false);
            button->setWantsKeyboardFocus(true);
            button->setTitle("Select memory " + juce::String(bank + 1));
            button->onClick = [this, bank] { processor.selectHardwareSource(bank % 2, bank < 2 ? 11 : 12); refresh(); };
            button->setTooltip("Listen to this independent memory on channel " + juce::String(bank % 2 + 1));
            addAndMakeVisible(*button);
            bankButtons[(size_t) bank] = std::move(button);
            for (int col = 0; col < 26; ++col)
            {
                const int local = order[(size_t) col];
                const auto& def = cs80::kChannelDefs[local];
                Item item;
                item.bank = bank; item.local = local;
                const bool discrete = local == cs80::CP_saw || local == cs80::CP_pulse;
                if (discrete)
                {
                    item.toggle = std::make_unique<cs80::ui::LedButton>(local == cs80::CP_saw ? "SAW" : "PULSE");
                    auto* toggle = item.toggle.get();
                    toggle->hardwareTab = true;
                    toggle->accent = juce::Colour(0xffa6a697);
                    toggle->setTitle("Memory " + juce::String(bank + 1) + " " + def.name);
                    toggle->setComponentID(Cs80AudioProcessor::memoryParameterId(bank, local));
                    item.buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                        processor.getAPVTS(), Cs80AudioProcessor::memoryParameterId(bank, local), *toggle);
                    addAndMakeVisible(*toggle);
                }
                else
                {
                    item.slider = std::make_unique<cs80::ui::FocusSlider>();
                    auto* slider = item.slider.get();
                    slider->setSliderStyle(juce::Slider::LinearVertical);
                    slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                    slider->setRange(def.min, def.max);
                    slider->setSkewFactor(def.skew);
                    slider->setColour(juce::Slider::thumbColourId, juce::Colour(0xffa6a697));
                    slider->setDoubleClickReturnValue(true, def.def);
                    slider->setTitle("Memory " + juce::String(bank + 1) + " " + def.name);
                    slider->setTooltip(slider->getTitle());
                    slider->setDescription("Independent analog memory control. Use the arrow keys to adjust; select this memory to hear it.");
                    slider->setComponentID(Cs80AudioProcessor::memoryParameterId(bank, local));
                    item.sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                        processor.getAPVTS(), Cs80AudioProcessor::memoryParameterId(bank, local), *slider);
                    slider->onValueChange = [bank, local, slider] {
                        slider->setTooltip(slider->getTitle() + ": "
                            + cs80::ui::formatParamValue(cs80::chParam(bank % 2, local), (float) slider->getValue()));
                    };
                    addAndMakeVisible(*slider);
                }
                items.push_back(std::move(item));
            }
        }
        refresh();
    }

    void refresh()
    {
        for (auto& item : items)
        {
            if (item.slider && ! item.slider->isMouseButtonDown())
            {
                item.slider->setTooltip(item.slider->getTitle() + ": "
                    + cs80::ui::formatParamValue(cs80::chParam(item.bank % 2, item.local), (float) item.slider->getValue()));
            }
            if (item.toggle)
            {
                item.toggle->setTooltip(item.toggle->getTitle() + (item.toggle->getToggleState() ? ": ON" : ": OFF"));
            }
        }
        for (int bank = 0; bank < 4; ++bank)
            bankButtons[(size_t) bank]->setToggleState(
                processor.getHardwareSource(bank % 2) == (bank < 2 ? 11 : 12), juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        g.saveState();
        const float sx = (float) getWidth() / 1472.0f, sy = (float) getHeight() / 554.0f;
        g.addTransform(juce::AffineTransform::scale(sx, sy));
        g.fillAll(juce::Colour(0xff1a1c17));
        g.setColour(juce::Colour(0xffbdb9a5)); g.drawRect(2, 2, 1468, 550, 2);
        g.setFont(displayFont(17, juce::Font::bold));
        g.drawText("MEMORY PROGRAMMER", 24, 16, 450, 24, juce::Justification::centredLeft);
        g.setFont(displayFont(10));
        g.drawText("FOUR INDEPENDENT MINIATURE PANELS  /  MAGNIFIED VIEW", 24, 42, 1000, 17, juce::Justification::centredLeft);
        for (int bank = 0; bank < 4; ++bank)
        {
            const float y = 73.0f + (float) bank * 112.0f;
            g.setColour(juce::Colour(0xff292c23)); g.fillRect(14.0f, y, 1444.0f, 107.0f);
            g.setColour(juce::Colour(0xffc8c7b3));
            g.setFont(displayFont(10, juce::Font::bold));
            g.drawText(bank % 2 == 0 ? "CHANNEL I" : "CHANNEL II", 23, (int)y + 60, 110, 18, juce::Justification::centred);
            for (int col = 0; col < 26; ++col)
            {
                g.setFont(displayFont(8.0f, juce::Font::bold));
                g.drawFittedText(labels[(size_t)col], 145 + col * 50, (int)y + 3, 48, 22,
                                 juce::Justification::centred, 2, 0.7f);
            }
        }
        g.setColour(juce::Colour(0xffc8c7b3)); g.setFont(displayFont(9));
        g.drawText("Miniature controls are independent of the main panel. Select a memory to hear it; PANEL returns to the unchanged front-panel settings.",
                   22, 526, 1420, 16, juce::Justification::centredLeft);
        g.restoreState();
    }

    void resized() override
    {
        const float sx = (float) getWidth() / 1472.0f, sy = (float) getHeight() / 554.0f;
        auto r = [sx,sy](float x,float y,float w,float h) {
            return juce::Rectangle<int>(juce::roundToInt(x*sx),juce::roundToInt(y*sy),
                                       juce::roundToInt(w*sx),juce::roundToInt(h*sy));
        };
        close.setBounds(r(1320,20,125,28));
        for (int bank = 0; bank < 4; ++bank)
        {
            const float y = 73.0f + (float) bank * 112.0f;
            bankButtons[(size_t) bank]->setBounds(r(24,y+24,108,28));
            for (int col = 0; col < 26; ++col)
            {
                auto& item = items[(size_t)(bank*26+col)];
                if (item.slider) item.slider->setBounds(r(145+(float)col*50,y+27,48,73));
                if (item.toggle) item.toggle->setBounds(r(153+(float)col*50,y+38,32,54));
            }
        }
    }
    std::function<void()> onClose;

private:
    struct Item
    {
        int bank=0, local=0;
        std::unique_ptr<cs80::ui::FocusSlider> slider;
        std::unique_ptr<cs80::ui::LedButton> toggle;
        // Destroy attachments before their controls.
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };
    Cs80AudioProcessor& processor;
    juce::TextButton close;
    std::array<std::unique_ptr<juce::TextButton>,4> bankButtons;
    std::vector<Item> items;
    const std::array<int,26> order {{
        cs80::CP_pwmSpeed, cs80::CP_pwmDepth, cs80::CP_pw, cs80::CP_pulse, cs80::CP_saw, cs80::CP_noise,
        cs80::CP_hpfCutoff, cs80::CP_hpfRes, cs80::CP_lpfCutoff, cs80::CP_lpfRes,
        cs80::CP_fIl, cs80::CP_fAl, cs80::CP_fA, cs80::CP_fD, cs80::CP_fR,
        cs80::CP_vcfLevel, cs80::CP_sine, cs80::CP_vA, cs80::CP_vD, cs80::CP_vS, cs80::CP_vR, cs80::CP_vLevel,
        cs80::CP_initBrill, cs80::CP_initLevel, cs80::CP_afterBrill, cs80::CP_afterLevel
    }};
    const std::array<const char*,26> labels {{
        "SPEED","PWM","PW","PULSE","SAW","NOISE","HPF","RES H","LPF","RES L",
        "IL","AL","A","D","R","VCF LEVEL","SINE","A","D","S","R","LEVEL",
        "INITIAL\nBRILL","INITIAL\nLEVEL","AFTER\nBRILL","AFTER\nLEVEL"
    }};
};

} // namespace

struct Cs80AudioProcessorEditor::Impl final : public juce::Component
{
    explicit Impl(Cs80AudioProcessor& p)
        : processor(p),
          keyboard(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard),
          pitchWheel("PITCH", true),
          modWheel("MOD", false),
          advanced(p),
          browser(p),
          memoryProgrammer(p)
    {
        setOpaque(true);
        setLookAndFeel(&lookAndFeel);
        setWantsKeyboardFocus(true);
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        setTitle("CS-80 instrument panel");

        buildTopBar();
        buildMasterSection();
        buildChannels();
        buildToneSection();
        buildPerformance();
        buildKeyboard();

        addAndMakeVisible(advanced);
        advanced.setVisible(false);
        advanced.setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        addAndMakeVisible(browser);
        browser.setVisible(false);
        browser.setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
        addAndMakeVisible(memoryProgrammer);
        memoryProgrammer.setVisible(false);
        memoryProgrammer.onClose = [this] {
            memoryProgrammer.setVisible(false);
            if (getPeer() != nullptr) memoryLid.grabKeyboardFocus();
        };

        advanced.onClose = [this] { showAdvanced(false); };
        browser.onClose = [this] { showBrowser(false); };
        browser.onPresetLoaded = [this]
        {
            syncPresetDisplay();
            syncToneMarks();
            repaint();
        };

        // Host state/program changes may arrive on non-message threads.
        // The existing message-thread timer polls metadata and selectors;
        // never install a processor callback that captures this editor.
        syncPresetDisplay();
        syncToneMarks();
    }

    ~Impl() override
    {
        setLookAndFeel(nullptr);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key != juce::KeyPress::escapeKey) return false;
        if (memoryProgrammer.isVisible())
        {
            if (memoryProgrammer.onClose) memoryProgrammer.onClose();
            return true;
        }
        if (browserVisible) { showBrowser(false); return true; }
        if (advancedVisible) { showAdvanced(false); return true; }
        return false;
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
        raw->getSlider().getProperties().set("hardwareRotary", true);
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
        const auto channelName = parameter >= cs80::P_numGlobals && parameter < cs80::P_legacyNumParams
            ? juce::String(parameter < cs80::P_ch2Start ? "Channel I " : "Channel II ") : juce::String();
        raw->setTitle(channelName + juce::String(cs80::paramDef(parameter).name));
        raw->setTooltip(raw->getTitle() + (raw->getToggleState() ? ": ON" : ": OFF"));
        place(*raw, x, y, w, h);
        leds.push_back(std::move(button));
        return *raw;
    }

    void buildTopBar()
    {
        for (auto* button : { &prevButton, &nextButton, &browseButton, &saveButton,
                              &initButton, &advancedButton })
        {
            button->setWantsKeyboardFocus(true);
            addAndMakeVisible(*button);
        }

        presetLabel.setJustificationType(juce::Justification::centred);
        presetLabel.setColour(juce::Label::textColourId, cs80::ui::amberBright);
        presetLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        presetLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(presetLabel);

        prevButton.setTitle("Previous preset");
        nextButton.setTitle("Next preset");
        browseButton.setTitle("Open preset library");
        saveButton.setTitle("Save a user preset");
        initButton.setTitle("Load the initial patch");
        advancedButton.setTitle("Show advanced controls");
        prevButton.onClick = [this] { stepPreset(-1); };
        nextButton.onClick = [this] { stepPreset(1); };
        browseButton.onClick = [this]
        {
            showBrowser(true);
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

    cs80::ui::ParamKnob& addFader(int parameter, float x, float y, float w, float h,
                                  const juce::String& label, juce::Colour colour)
    {
        auto& control = addKnob(parameter, x, y, w, h, label);
        control.setHardwareSlider(colour);
        return control;
    }

    static juce::Colour whiteCap() { return juce::Colour(0xffe7e2cf); }
    static juce::Colour greenCap() { return juce::Colour(0xff28564b); }
    static juce::Colour redCap() { return juce::Colour(0xffc94743); }
    static juce::Colour yellowCap() { return juce::Colour(0xffe7c348); }
    static juce::Colour blackCap() { return juce::Colour(0xff363831); }
    static juce::Colour greyCap() { return juce::Colour(0xff8b8a80); }

    void buildMasterSection()
    {
        // The lower strip follows Yamaha's PN2 panel: ring, sub oscillator,
        // feet, tone selectors, mix, brilliance and performance response.
        addKnob(cs80::P_masterTune, 49, 548, 62, 82, "PITCH");
        addFader(cs80::P_detune, 114, 535, 39, 116, "DETUNE", whiteCap());
        addFader(cs80::P_ringAttack, 161, 535, 38, 116, "ATTACK", blackCap());
        addFader(cs80::P_ringDecay, 201, 535, 38, 116, "DECAY", blackCap());
        addFader(cs80::P_ringDepth, 241, 535, 38, 116, "DEPTH", greyCap());
        addFader(cs80::P_ringSpeed, 281, 535, 38, 116, "SPEED", whiteCap());
        addFader(cs80::P_ringAmount, 321, 535, 38, 116, "MOD", greyCap());
        addFader(cs80::P_subWave, 373, 535, 42, 116, "FUNCTION", blackCap());
        addFader(cs80::P_subSpeed, 417, 535, 38, 116, "SPEED", whiteCap());
        addFader(cs80::P_subVco, 457, 535, 38, 116, "VCO", whiteCap());
        addFader(cs80::P_subVcf, 497, 535, 38, 116, "VCF", greenCap());
        addFader(cs80::P_subVca, 537, 535, 38, 116, "VCA", greyCap());
        addFader(cs80::chParam(0, cs80::CP_feet), 582, 535, 36, 116, "FEET I", greyCap());
        addFader(cs80::chParam(1, cs80::CP_feet), 620, 535, 36, 116, "FEET II", greyCap());
        addFader(cs80::P_mix, 1210, 535, 38, 116, "MIX", redCap());
        addFader(cs80::P_brilliance, 1252, 535, 38, 116, "BRILLIANCE", greenCap());
        addFader(cs80::P_resonance, 1294, 535, 38, 116, "RESONANCE", redCap());
        addFader(cs80::P_initBend, 1336, 535, 38, 116, "INIT BEND", whiteCap());
        addFader(cs80::P_subAtSpeed, 1378, 535, 38, 116, "AT SPEED", whiteCap());
        addFader(cs80::P_subAtVco, 1420, 535, 38, 116, "AT VCO", whiteCap());
        addFader(cs80::P_subAtVcf, 1462, 535, 38, 116, "AT VCF", greenCap());
        addKnob(cs80::P_volume, 1504, 551, 49, 82, "VOLUME");
    }

    void buildChannel(int channel, float y)
    {
        auto p = [channel](int local) { return cs80::chParam(channel, local); };
        float x = 458.0f;
        auto fader = [&](int param, const char* name, juce::Colour colour)
        {
            addFader(p(param), x, y + 36, 38, 136, name, colour);
            x += 40.0f;
        };
        fader(cs80::CP_pwmSpeed, "SPEED", whiteCap());
        fader(cs80::CP_pwmDepth, "PWM", greenCap());
        fader(cs80::CP_pw, "PW", greenCap());
        auto& pulse = addLed(p(cs80::CP_pulse), "PULSE", x, y + 86, 36, 54);
        pulse.hardwareTab = true; pulse.accent = greyCap(); x += 40;
        auto& saw = addLed(p(cs80::CP_saw), "SAW", x, y + 86, 36, 54);
        saw.hardwareTab = true; saw.accent = greyCap(); x += 40;
        fader(cs80::CP_noise, "NOISE", greenCap());
        x += 9;
        fader(cs80::CP_hpfCutoff, "HPF", greenCap());
        fader(cs80::CP_hpfRes, "RES H", redCap());
        fader(cs80::CP_lpfCutoff, "LPF", greenCap());
        fader(cs80::CP_lpfRes, "RES L", redCap());
        fader(cs80::CP_fIl, "IL", blackCap());
        fader(cs80::CP_fAl, "AL", blackCap());
        fader(cs80::CP_fA, "A", blackCap());
        fader(cs80::CP_fD, "D", blackCap());
        fader(cs80::CP_fR, "R", yellowCap());
        x += 9;
        fader(cs80::CP_vcfLevel, "VCF LEVEL", greyCap());
        fader(cs80::CP_sine, "SINE", greyCap());
        fader(cs80::CP_vA, "A", blackCap());
        fader(cs80::CP_vD, "D", blackCap());
        fader(cs80::CP_vS, "S", blackCap());
        fader(cs80::CP_vR, "R", yellowCap());
        fader(cs80::CP_vLevel, "LEVEL", greyCap());
        x += 9;
        fader(cs80::CP_initBrill, "BRILLIANCE", greenCap());
        fader(cs80::CP_initLevel, "LEVEL", greyCap());
        fader(cs80::CP_afterBrill, "BRILLIANCE", greenCap());
        fader(cs80::CP_afterLevel, "LEVEL", greyCap());
    }

    void buildChannels()
    {
        buildChannel(0, 112.0f);
        buildChannel(1, 305.0f);
        // Explicit software-only voice settings stay under a separate lid.
        addAndMakeVisible(voiceOptions);
        voiceOptions.setButtonText("VOICE SETTINGS");
        voiceOptions.setTitle("Show software voice settings");
        voiceOptions.setWantsKeyboardFocus(true);
        voiceOptions.onClick = [this] {
            optionsVisible = ! optionsVisible;
            for (auto* component : optionComponents) component->setVisible(optionsVisible);
            repaint();
        };
        place(voiceOptions, 94, 447, 278, 25);
        addAndMakeVisible(memoryLid);
        memoryLid.setButtonText("OPEN MEMORY LID");
        memoryLid.setTitle("Open memory programmer");
        memoryLid.setWantsKeyboardFocus(true);
        memoryLid.onClick = [this] {
            showAdvanced(false);
            showBrowser(false);
            memoryProgrammer.refresh();
            memoryProgrammer.setVisible(true);
            memoryProgrammer.toFront(true);
        };
        place(memoryLid, 94, 410, 278, 27);
        for (int channel = 0; channel < 2; ++channel)
        {
            const float y = 296.0f + (float) channel * 68.0f;
            auto p = [channel](int local) { return cs80::chParam(channel, local); };
            auto& pan = addKnob(p(cs80::CP_pan), 100, y, 68, 60, "PAN");
            auto& level = addKnob(p(cs80::CP_vLevel), 173, y, 68, 60, "LEVEL");
            auto& hp = addLed(p(cs80::CP_hpfOn), "HPF", 247, y + 8, 62, 21);
            auto& lp = addLed(p(cs80::CP_lpfOn), "LPF", 247, y + 33, 62, 21);
            optionComponents.insert(optionComponents.end(), { &pan, &level, &hp, &lp });
        }
        for (auto* component : optionComponents) component->setVisible(false);
    }

    void buildToneSection()
    {
        const std::array<juce::Colour, 14> colours = {
            yellowCap(), yellowCap(), redCap(), whiteCap(), yellowCap(),
            yellowCap(), yellowCap(), whiteCap(), greenCap(), greenCap(), greenCap(),
            whiteCap(), whiteCap(), whiteCap()
        };
        for (int channel = 0; channel < 2; ++channel)
        {
            for (int tone = 0; tone < 14; ++tone)
            {
                auto button = std::make_unique<juce::TextButton>(
                    (tone < 11 ? juce::String(cs80::hardwareToneName(tone, channel)).toUpperCase()
                     : tone == 13 ? juce::String("PANEL")
                     : juce::String("MEMORY ") + juce::String(channel + (tone == 11 ? 1 : 3))));
                const auto fullName = button->getButtonText();
                button->setTitle(fullName);
                button->setDescription((channel == 0 ? juce::String("Channel I") : juce::String("Channel II"))
                    + ": select this voice source with Space or Return. PANEL preserves the main-panel settings.");
                button->setTooltip(fullName + (channel == 0 ? " / CHANNEL I" : " / CHANNEL II"));
                button->setButtonText(fullName.replace("HARPSICHORD ", "HARPSI\nCHORD ")
                                              .replace("CLAVICHORD ", "CLAVI\nCHORD ")
                                              .replace("STRING ", "STRING\n")
                                              .replace("BRASS ", "BRASS\n")
                                              .replace("GUITAR ", "GUITAR\n")
                                              .replace("FUNKY ", "FUNKY\n")
                                              .replace("ORGAN ", "ORGAN\n")
                                              .replace("MEMORY ", "MEMORY\n"));
                button->setClickingTogglesState(true);
                button->setRadioGroupId(0x100 + channel);
                button->setWantsKeyboardFocus(true);
                button->getProperties().set("hardwareTab", true);
                button->setColour(juce::TextButton::buttonColourId, colours[(size_t) tone]);
                button->onClick = [this, channel, tone] { applyHardwareTone(channel, tone); };
                addAndMakeVisible(*button);
                place(*button, 675.0f + (float) tone * 37.0f,
                       556.0f + (float) channel * 45.0f, 34.0f, 41.0f);
                toneButtons[(size_t) channel][(size_t) tone] = std::move(button);
            }
        }
    }

    void buildPerformance()
    {
        addFader(cs80::P_masterSustain, 57, 780, 45, 122, "SUSTAIN", yellowCap());
        addFader(cs80::P_portamento, 106, 780, 45, 122, "PORTA", whiteCap());
        addKnob(cs80::P_chorusSpeed, 160, 782, 55, 64, "SPEED");
        addKnob(cs80::P_chorusDepth, 220, 782, 55, 64, "DEPTH");
        auto& sus = addLed(cs80::P_sustainMode, "SUSTAIN II", 57, 912, 49, 46);
        sus.hardwareTab = true; sus.accent = whiteCap();
        auto& chorus = addLed(cs80::P_chorusOn, "CHORUS", 168, 868, 48, 60);
        chorus.hardwareTab = true; chorus.accent = yellowCap();
        auto& tremolo = addLed(cs80::P_chorusMode, "TREMOLO", 222, 868, 48, 60);
        tremolo.hardwareTab = true; tremolo.accent = whiteCap();
        addFader(cs80::P_glideMode, 110, 905, 46, 60, "GLIDE", greyCap());
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
            processor.setRibbonFromUI(value);
        };
        addAndMakeVisible(ribbon);

        pitchWheel.onValueChange = [this](float value)
        {
            processor.setPitchWheelFromUI(value);
        };
        addAndMakeVisible(pitchWheel);
        pitchWheel.setVisible(false);
        optionComponents.push_back(&pitchWheel);

        modWheel.onValueChange = [this](float value)
        {
            processor.setModWheelFromUI(value);
        };
        addAndMakeVisible(modWheel);
        modWheel.setVisible(false);
        optionComponents.push_back(&modWheel);
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

    void applyHardwareTone(int channel, int source)
    {
        processor.selectHardwareSource(channel, source);
        syncToneMarks();
    }

    void syncToneMarks()
    {
        for (int channel = 0; channel < 2; ++channel)
            for (int source = 0; source < 14; ++source)
                toneButtons[(size_t) channel][(size_t) source]->setToggleState(
                    processor.getHardwareSource(channel) == source, juce::dontSendNotification);
    }

    void syncPresetDisplay()
    {
        const auto name = processor.getPresetName();
        if (presetLabel.getText() != name)
            presetLabel.setText(name, juce::dontSendNotification);
    }

    void showAdvanced(bool shouldShow)
    {
        const bool wasVisible = advancedVisible;
        if (shouldShow) memoryProgrammer.setVisible(false);
        advancedVisible = shouldShow;
        advanced.setVisible(shouldShow);
        advancedButton.setToggleState(shouldShow, juce::dontSendNotification);
        if (shouldShow)
        {
            browser.setVisible(false);
            browserVisible = false;
            advanced.toFront(true);
        }
        else if (wasVisible && getPeer() != nullptr)
            advancedButton.grabKeyboardFocus();
        repaint();
    }

    void showBrowser(bool shouldShow)
    {
        const bool wasVisible = browserVisible;
        if (shouldShow) memoryProgrammer.setVisible(false);
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
            if (wasVisible && getPeer() != nullptr) browseButton.grabKeyboardFocus();
        }
        repaint();
    }

    void tick()
    {
        syncPresetDisplay();
        syncToneMarks();
        if (memoryProgrammer.isVisible()) memoryProgrammer.refresh();
        // The voice allocator is the only animated element; redrawing the
        // whole panel at 8 Hz also keeps preset metadata current.
        repaint();
    }

    // -----------------------------------------------------------------------
    // Painting
    // -----------------------------------------------------------------------
    void panelText(juce::Graphics& g, const juce::String& text, float x, float y,
                   float w, float h, float font = 10.0f,
                   juce::Justification align = juce::Justification::centred)
    {
        g.setColour(juce::Colour(0xffe1dfd3));
        g.setFont(displayFont(font, juce::Font::bold));
        g.drawFittedText(text, juce::Rectangle<float>(x, y, w, h).toNearestInt(), align, 2, 0.7f);
    }

    void group(juce::Graphics& g, const char* title, float x, float y, float w)
    {
        g.setColour(juce::Colour(0xffb6b4a7));
        g.drawLine(x, y + 13, x, y + 19, 0.8f);
        g.drawLine(x + w, y + 13, x + w, y + 19, 0.8f);
        g.drawLine(x, y + 13, x + w, y + 13, 0.8f);
        g.setColour(juce::Colour(0xff20201e));
        g.fillRect(x + w * 0.2f, y + 5, w * 0.6f, 16.0f);
        panelText(g, title, x, y + 3, w, 17, 10);
    }

    void paintMemoryLid(juce::Graphics& g)
    {
        auto r = juce::Rectangle<float>(65, 122, 352, 361);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff252522), 65, 122,
                                               juce::Colour(0xff151614), 417, 483, false));
        g.fillRect(r);
        g.setColour(juce::Colour(0xffb7b5aa));
        g.drawRect(r.reduced(3), 1.0f);
        panelText(g, "SYNTHESIZER  CS-80", 85, 140, 310, 25, 19);
        panelText(g, "VOICE SIGNAL FLOW", 85, 177, 310, 20, 10);
        // Original vector diagram based on the voice topology, not a bitmap
        // from a competing instrument. This lid is not a fake memory bank.
        const char* boxes[] = { "VCO", "HPF", "LPF", "VCA" };
        for (int i = 0; i < 4; ++i)
        {
            const float x = 91.0f + (float) i * 77.0f;
            g.setColour(juce::Colour(0xffb7b5aa));
            g.drawRect(x, 220.0f, 57.0f, 32.0f, 1.0f);
            panelText(g, boxes[i], x, 220, 57, 32, 11);
            if (i < 3) { g.drawLine(x + 57, 236, x + 77, 236, 1); }
        }
        g.setColour(juce::Colour(0xffb7b5aa));
        g.drawLine(117, 252, 117, 275, 1); g.drawLine(117, 275, 350, 275, 1);
        g.drawLine(350, 275, 350, 252, 1);
        panelText(g, "SINE", 155, 257, 100, 18, 8);
        if (optionsVisible)
        {
            g.setColour(juce::Colour(0xff131613)); g.fillRect(83, 286, 314, 151);
            panelText(g, "I", 83, 306, 15, 25, 11);
            panelText(g, "II", 83, 374, 15, 25, 11);
        }
        else
        {
            for (int row = 0; row < 2; ++row)
            {
                float y = 305.0f + (float) row * 59.0f;
                g.setColour(juce::Colour(0xffb7b5aa));
                g.drawRect(126.0f, y, 100.0f, 28.0f, 1.0f);
                g.drawRect(267.0f, y, 100.0f, 28.0f, 1.0f);
                panelText(g, row == 0 ? "VCF ENVELOPE" : "INITIAL TOUCH", 126, y, 100, 28, 8);
                panelText(g, row == 0 ? "VCA ENVELOPE" : "AFTER TOUCH", 267, y, 100, 28, 8);
                g.drawLine(226, y + 14, 267, y + 14, 1);
            }

        }
    }

    void paint(juce::Graphics& g) override
    {
        g.saveState();
        g.addTransform(juce::AffineTransform::scale((float) getWidth() / kDesignWidth,
                                                    (float) getHeight() / kDesignHeight));
        g.fillAll(juce::Colour(0xff10100f));
        // Broad walnut end cheeks and black folded-steel front panel.
        for (float x : { 8.0f, 1558.0f })
        {
            auto cheek = juce::Rectangle<float>(x, 70, 34, 914);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff704731), x, 70,
                                                   juce::Colour(0xff271910), x + 34, 984, false));
            g.fillRoundedRectangle(cheek, 5);
            for (int i = 0; i < 11; ++i)
            {
                g.setColour(juce::Colour(0xffb78457).withAlpha(0.10f + (float)(i % 3) * 0.03f));
                juce::Path grain;
                grain.startNewSubPath(x + 3 + (float)i * 2.6f, 78);
                grain.cubicTo(x + 7 + (float)i * 2.1f, 290, x + (float)i * 2.7f, 650,
                               x + 3 + (float)i * 2.6f, 975);
                g.strokePath(grain, juce::PathStrokeType(0.8f));
            }
        }
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff252623), 800, 78,
                                               juce::Colour(0xff111310), 800, 754, false));
        g.fillRect(42, 78, 1516, 680);
        g.setColour(juce::Colour(0xffbdb9a9));
        g.drawRect(48, 103, 1504, 393, 1);
        g.drawRect(48, 506, 1504, 157, 1);
        for (int x = 60; x < 1540; x += 7)
        {
            g.setColour(juce::Colour(0xff080908));
            g.fillRect(x, 82, 3, 13);
        }
        paintMemoryLid(g);
        for (int channel = 0; channel < 2; ++channel)
        {
            const float y = channel == 0 ? 112.0f : 305.0f;
            panelText(g, channel == 0 ? "I" : "II", 426, y + 67, 27, 35, 23);
            group(g, "VCO", 459, y, 235);
            group(g, "VCF", 706, y, 354);
            group(g, "VCA", 1075, y, 272);
            group(g, "INITIAL", 1365, y, 76);
            group(g, "AFTER", 1445, y, 76);
        }
        g.setColour(juce::Colour(0xff77796f));
        g.drawHorizontalLine(299, 443, 1539);
        group(g, "RING MODULATOR", 158, 508, 200);
        group(g, "SUB OSCILLATOR", 370, 508, 205);
        group(g, "TONE SELECTOR", 675, 520, 514);
        panelText(g, "I", 657, 565, 16, 25, 12);
        panelText(g, "II", 657, 609, 16, 25, 12);
        group(g, "GLOBAL / TOUCH RESPONSE", 1210, 508, 333);

        // Logo rail and recessed ribbon sit between the synthesizer and keybed.
        g.setColour(juce::Colour(0xff080a08)); g.fillRect(48, 672, 1504, 74);
        g.setColour(juce::Colour(0xffb1afa1)); g.drawRect(48, 672, 1504, 74, 1);
        panelText(g, "YAMAHA", 675, 678, 250, 27, 24);
        panelText(g, "CS-80", 1360, 681, 164, 25, 22, juce::Justification::centredRight);
        panelText(g, "INDEPENDENT SOFTWARE RECREATION", 68, 685, 350, 18, 8, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff080907)); g.fillRect(44, 759, 1512, 222);
        g.setColour(juce::Colour(0xffada995)); g.drawRect(49, 771, 238, 197, 1);
        panelText(g, "PERFORMANCE", 62, 757, 211, 19, 10);
        panelText(g, "CHORUS / TREMOLO", 157, 944, 120, 17, 8);
        // Rails and screws use highlights rather than generic card borders.
        g.setColour(juce::Colour(0xff626257)); g.fillRect(43, 977, 1514, 4);
        for (float x : { 55.0f, 1545.0f })
            for (float y : { 110.0f, 489.0f, 656.0f, 970.0f })
            {
                g.setColour(juce::Colour(0xff0a0a09)); g.fillEllipse(x-4,y-3,9,9);
                g.setColour(juce::Colour(0xff898a80)); g.fillEllipse(x-3,y-3,6,6);
                g.setColour(juce::Colour(0xff272821)); g.drawLine(x-2,y,x+2,y,1);
            }
        // Software toolbar intentionally outside the hardware face.
        g.setColour(juce::Colour(0xff090c0d)); g.fillRect(0, 0, 1600, 64);
        panelText(g, "CS-80  /  REFERENCE", 28, 16, 320, 30, 17, juce::Justification::centredLeft);
        panelText(g, "8 VOICES  /  DUAL CHANNEL", 1280, 18, 290, 22, 10);
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

        ribbon.setBounds(r(320.0f, 713.0f, 960.0f, 23.0f));
        pitchWheel.setBounds(r(320.0f, 299.0f, 30.0f, 126.0f));
        modWheel.setBounds(r(358.0f, 299.0f, 30.0f, 126.0f));
        keyboard.setBounds(r(300.0f, 780.0f, 1236.0f, 186.0f));
        if (keyboard.getWidth() > 0)
            keyboard.setKeyWidth((float) keyboard.getWidth() / 36.0f);

        advanced.setBounds(r(24.0f, 88.0f, 1552.0f, 892.0f));
        browser.setBounds(r(200.0f, 120.0f, 1200.0f, 760.0f));
        memoryProgrammer.setBounds(r(48.0f, 104.0f, 1504.0f, 560.0f));
    }

    Cs80AudioProcessor& processor;
    cs80::ui::LookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

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
    std::array<std::array<std::unique_ptr<juce::TextButton>, 14>, 2> toneButtons;
    std::vector<Placement> placements;
    juce::TextButton voiceOptions;
    juce::TextButton memoryLid;
    std::vector<juce::Component*> optionComponents;
    bool optionsVisible = false;

    HardwareKeyboard keyboard;
    cs80::ui::RibbonStrip ribbon;
    cs80::ui::PerformanceWheel pitchWheel;
    cs80::ui::PerformanceWheel modWheel;

    cs80::ui::AdvancedPanel advanced;
    cs80::ui::PresetBrowser browser;
    MemoryProgrammer memoryProgrammer;

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
