#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

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
    processor.prepareToPlay(48000.0, 512);

    // Parameter and preset integrity. These are the checks a host would rely
    // on before the instrument is even asked to make a sound.
    check(processor.getFactoryPresets().size() >= 200, "factory bank present");
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        const auto id = Cs80AudioProcessor::parameterId(i);
        check(processor.getAPVTS().getParameter(id) != nullptr, "parameter exposed");
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

    Cs80AudioProcessorEditor editor(processor);
    editor.setSize(1600, 1000);
    editor.setVisible(true);
    flushGui(150);

    auto save = [&](const juce::String& name) {
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
