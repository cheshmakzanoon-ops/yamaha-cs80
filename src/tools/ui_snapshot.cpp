#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UiComponents.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>
#include <memory>

namespace
{
int failures = 0;

void check(bool condition, const char* what)
{
    if (! condition)
    {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

void validateVisibleControls(juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (! child->isVisible())
            continue;
        // Validate user-facing controls, not internal popup/list viewports
        // which intentionally scroll outside their parent's viewport.
        if (dynamic_cast<cs80::ui::ParamKnob*>(child) != nullptr
            || dynamic_cast<cs80::ui::LedButton*>(child) != nullptr)
        {
            check(! child->getBounds().isEmpty(), "visible panel control has positive bounds");
            check(parent.getLocalBounds().contains(child->getBounds()),
                  "visible panel control is inside parent");
        }
        validateVisibleControls(*child);
    }
}

juce::Button* findButton(juce::Component& parent, const juce::String& text)
{
    for (auto* child : parent.getChildren())
    {
        if (!child->isVisible()) continue;
        if (auto* button = dynamic_cast<juce::Button*>(child))
            if (button->getButtonText() == text) return button;
        if (auto* found = findButton(*child, text)) return found;
    }
    return nullptr;
}

int countHardwareSliders(juce::Component& parent)
{
    int count = 0;
    for (auto* child : parent.getChildren())
    {
        if (!child->isVisible()) continue;
        if (auto* slider = dynamic_cast<juce::Slider*>(child))
            if (slider->getSliderStyle() == juce::Slider::LinearVertical) ++count;
        count += countHardwareSliders(*child);
    }
    return count;
}

void validateKeyboardControls(juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (!child->isVisible()) continue;
        if (auto* knob = dynamic_cast<cs80::ui::ParamKnob*>(child))
        {
            auto& slider = knob->getSlider();
            check(slider.getWantsKeyboardFocus(), "visible parameter supports keyboard focus");
            check(slider.getTitle().isNotEmpty(), "visible parameter has accessible title");
        }
        validateKeyboardControls(*child);
    }
}

void flushGui(int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}
} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::String outDir = "artifacts/ui";
    bool selfTest = false;
    for (int i = 1; i < argc; ++i)
    {
        if (! juce::String(argv[i]).compareIgnoreCase("--out") && i + 1 < argc)
            outDir = argv[++i];
        else if (! juce::String(argv[i]).compareIgnoreCase("--self-test"))
            selfTest = true;
    }

    juce::File dir(juce::File::getCurrentWorkingDirectory().getChildFile(outDir));
    dir.createDirectory();

    Cs80AudioProcessor processor;
    check(processor.getLatencySamples() == 63, "reported latency at construction");
    processor.prepareToPlay(48000.0, 512);
    check(processor.getLatencySamples() == 63, "reported latency after prepare");

    // Parameter and preset integrity. These are the checks a host would rely
    // on before the instrument is even asked to make a sound.
    check(processor.getFactoryPresets().size() >= 200, "factory bank present");
    for (int index = 0; index < cs80::P_numParams; ++index)
    {
        const auto& def = cs80::paramDef(index);
        check(std::abs(processor.getParamValue(index) - processor.getFactoryPresets()[0].values[index])
                  <= std::max(0.0001f, (def.max - def.min) * 0.00001f),
              "initial program name and actual parameter values agree");
    }
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        const auto id = Cs80AudioProcessor::parameterId(i);
        check(processor.getAPVTS().getParameter(id) != nullptr, "parameter exposed");
    }

    // Parameter physical values must use the host's skewed mapping. Linear
    // normalization here formerly turned e.g. an 850Hz preset into ~20Hz.
    for (int index = 0; index < cs80::P_numParams; ++index)
    {
        const auto& def = cs80::paramDef(index);
        for (float fraction : { 0.0f, 0.2f, 0.5f, 0.8f, 1.0f })
        {
            const float physical = def.min + fraction * (def.max - def.min);
            processor.setParamValue(index, physical);
            check(std::abs(processor.getParamValue(index) - physical) <= std::max(0.0001f, (def.max - def.min) * 0.00001f),
                  "physical parameter round trips host skew");
        }
    }

    processor.loadFactoryPreset(22);
    check(processor.getPresetName() == "Replicant Brass", "factory preset load");
    check(processor.getCurrentPresetIndex() == 22, "factory preset index");

    // Host state round-trip.
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    Cs80AudioProcessor restored;
    restored.prepareToPlay(48000.0, 512);
    restored.setStateInformation(state.getData(), (int) state.getSize());
    check(restored.getPresetName() == "Replicant Brass", "state round-trip preset name");
    check(restored.getCurrentPresetIndex() == 22, "state round-trip preset index");
    check(std::abs(restored.getParamValue(cs80::P_mix)
                   - processor.getParamValue(cs80::P_mix)) < 1.0e-5f,
          "state round-trip parameter");

    // Audio path, including MIDI delivery through the JUCE buffer.
    {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> buffer(2, 1024);
        buffer.clear();
        processor.processBlock(buffer, midi);
        check(buffer.getMagnitude(0, 0, buffer.getNumSamples()) > 0.0001f,
              "plugin renders MIDI");
        check(processor.getActiveVoiceCount() >= 2, "plugin allocates voices");
        processor.getEngine().allNotesOff(false);
    }

    // GUI control mailboxes are consumed on the audio thread, including a
    // host block exceeding prepareToPlay's advertised size.
    {
        processor.setRibbonFromUI(0.2f);
        processor.setPitchWheelFromUI(-0.1f);
        processor.setModWheelFromUI(0.5f);
        juce::AudioBuffer<float> large(2, 10003);
        juce::MidiBuffer events;
        events.addEvent(juce::MidiMessage::noteOn(1, 57, (juce::uint8) 100), 4095);
        events.addEvent(juce::MidiMessage::noteOff(1, 57), 8193);
        processor.processBlock(large, events);
        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < large.getNumSamples(); ++i)
                finite = finite && std::isfinite(large.getSample(ch, i));
        check(finite, "oversized host block stays finite");
        check(large.getMagnitude(0, 4095, 4098) > 0.0001f, "oversized block MIDI renders after chunk boundary");
        processor.setRibbonFromUI(0.0f);
        processor.setPitchWheelFromUI(0.0f);
        processor.setModWheelFromUI(0.0f);
        processor.getEngine().allNotesOff(false);
    }

    // Physical programmer memories are independent controls, not destructive
    // copies over the main-panel APVTS. Test actual effective audio settings.
    {
        Cs80AudioProcessor memories;
        memories.prepareToPlay(48000.0, 64);
        const int cutoff = cs80::chParam(0, cs80::CP_lpfCutoff);
        memories.setParamValue(cutoff, 3400.0f);
        memories.setParamValue(cs80::chParam(0, cs80::CP_feet), 2.0f);
        memories.setParamValue(cs80::chParam(0, cs80::CP_pan), 0.2f);
        for (int bank = 0; bank < 4; ++bank)
            memories.setMemoryValue(bank, cs80::CP_lpfCutoff, 500.0f + 300.0f * (float) bank);
        memories.selectHardwareSource(0, 11);
        memories.selectHardwareSource(1, 12);
        juce::AudioBuffer<float> audio(2, 64);
        juce::MidiBuffer noMidi;
        memories.processBlock(audio, noMidi);
        check(std::abs(memories.getEngine().getParams()[cutoff] - 500.0f) < 0.01f, "memory 1 drives rank I");
        check(std::abs(memories.getEngine().getParams()[cs80::chParam(1, cs80::CP_lpfCutoff)] - 1400.0f) < 0.01f, "memory 4 drives rank II");
        check(std::abs(memories.getParamValue(cutoff) - 3400.0f) < 0.01f, "memory recall preserves physical panel");
        check(memories.getEngine().getParams()[cs80::chParam(0, cs80::CP_feet)] == 2.0f, "memory leaves footage live");
        check(std::abs(memories.getEngine().getParams()[cs80::chParam(0, cs80::CP_pan)] - 0.2f) < 0.001f, "memory leaves modern pan live");
        juce::MemoryBlock memoryState;
        memories.getStateInformation(memoryState);
        Cs80AudioProcessor recalled;
        recalled.setStateInformation(memoryState.getData(), (int) memoryState.getSize());
        check(recalled.getHardwareSource(0) == 11 && recalled.getHardwareSource(1) == 12, "hardware source selectors round trip");
        for (int bank = 0; bank < 4; ++bank)
            check(std::abs(recalled.getMemoryValue(bank, cs80::CP_lpfCutoff) - (500.0f + 300.0f * (float) bank)) < 0.01f, "independent programmer memory round trip");
        memories.selectHardwareSource(0, 13);
        memories.processBlock(audio, noMidi);
        check(std::abs(memories.getEngine().getParams()[cutoff] - 3400.0f) < 0.01f, "PANEL restores untouched manual controls");
        memories.selectHardwareSource(0, 2);
        memories.processBlock(audio, noMidi);
        cs80::ParamValues brass;
        cs80::applyHardwareTone(brass, 0, 2);
        check(std::abs(memories.getEngine().getParams()[cutoff] - brass[cutoff]) < 0.01f, "hardware factory selector drives effective voice");
        check(std::abs(memories.getParamValue(cutoff) - 3400.0f) < 0.01f, "factory selection preserves PANEL settings");
        memories.loadFactoryPreset(22);
        check(memories.getHardwareSource(0) == 13 && memories.getHardwareSource(1) == 13, "digital program recall resets selectors to PANEL");
        const float beforeInvalid = memories.getMemoryValue(0, cs80::CP_lpfCutoff);
        memories.setMemoryValue(0, cs80::CP_lpfCutoff, std::numeric_limits<float>::quiet_NaN());
        check(memories.getMemoryValue(0, cs80::CP_lpfCutoff) == beforeInvalid, "programmer rejects nonfinite edits");
    }

    Cs80AudioProcessorEditor editor(processor);
    editor.setSize(1600, 1000);
    editor.setVisible(true);
    flushGui(150);

    auto save = [&](const juce::String& name) {
        validateVisibleControls(editor);
        auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
        check(! image.isNull(), "snapshot image created");
        check(image.getWidth() == editor.getWidth(), "snapshot width");
        check(image.getHeight() == editor.getHeight(), "snapshot height");
        auto file = dir.getChildFile(name + ".png");
        file.deleteFile();
        juce::FileOutputStream stream(file);
        juce::PNGImageFormat png;
        if (stream.openedOk() && png.writeImageToStream(image, stream))
            std::printf("wrote %s\n", file.getFullPathName().toRawUTF8());
        else
        {
            std::printf("FAILED %s\n", file.getFullPathName().toRawUTF8());
            ++failures;
        }
    };

    validateKeyboardControls(editor);
    check(countHardwareSliders(editor) >= 40, "main instrument uses hardware slider banks");
    save("main-1600x1000");

    editor.setSize(1200, 760);
    flushGui(100);
    save("main-1200x760");

    editor.setSize(1120, 700);
    flushGui(100);
    save("main-1120x700");

    editor.setSize(1600, 1000);
    editor.showAdvancedPanel(true);
    check(editor.isAdvancedVisible(), "advanced panel opens");
    for (int page = 0; page < 4; ++page)
    {
        editor.showAdvancedPage(page);
        flushGui(80);
        save("advanced-page" + juce::String(page + 1));
    }

    editor.showPresetBrowser(true);
    check(! editor.isAdvancedVisible(), "browser replaces advanced panel");
    flushGui(100);
    save("browser-1600x1000");
    editor.showPresetBrowser(false);
    // Repeated overlay transitions must restore the instrument surface.
    for (int pass = 0; pass < 3; ++pass)
    {
        editor.showAdvancedPanel(true);
        check(editor.isAdvancedVisible(), "advanced panel reopens");
        editor.showPresetBrowser(true);
        check(! editor.isAdvancedVisible(), "browser hides advanced on repeated transition");
        editor.showPresetBrowser(false);
        editor.showAdvancedPanel(false);
        check(! editor.isAdvancedVisible(), "advanced panel closes");
    }
    flushGui(80);
    save("main-restored-1600x1000");

    if (auto* lid = findButton(editor, "OPEN MEMORY LID"))
    {
        lid->triggerClick();
        flushGui(100);
        check(countHardwareSliders(editor) >= 96, "all four miniature programmer banks exposed");
        save("memory-programmer-1600x1000");
        if (auto* close = findButton(editor, "CLOSE LID"))
        {
            close->triggerClick();
            flushGui(80);
        }
        else check(false, "memory lid has working close control");
        check(findButton(editor, "OPEN MEMORY LID") != nullptr, "memory lid reopens after close");
    }
    else check(false, "physical memory lid can be opened");

    // User preset save/load path. This touches the per-user application data
    // directory, which is created on demand by the processor.
    processor.saveUserPreset("UI Smoke Preset");
    const auto files = processor.getUserPresetFiles();
    bool found = false;
    for (const auto& file : files)
        found = found || file.getFileNameWithoutExtension() == "UI Smoke Preset";
    check(found, "user preset saved");
    if (found)
    {
        processor.loadFactoryPreset(0);
        for (const auto& file : processor.getUserPresetFiles())
            if (file.getFileNameWithoutExtension() == "UI Smoke Preset")
                processor.loadUserPreset(file);
        check(processor.getPresetName() == "UI Smoke Preset", "user preset loaded");
        for (const auto& file : processor.getUserPresetFiles())
            if (file.getFileNameWithoutExtension() == "UI Smoke Preset")
                file.deleteFile();
    }

    if (failures == 0)
        std::printf("UI SMOKE TEST PASSED\n");
    else
        std::printf("UI SMOKE TEST: %d FAILURE(S)\n", failures);

    return selfTest ? (failures == 0 ? 0 : 1) : 0;
}
