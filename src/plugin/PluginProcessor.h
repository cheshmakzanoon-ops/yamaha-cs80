#pragma once

#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "core/Params.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>
#include <vector>

class Cs80AudioProcessorEditor;

class Cs80AudioProcessor : public juce::AudioProcessor
{
public:
    Cs80AudioProcessor();
    ~Cs80AudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CS-80 Reference"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getAPVTS() const noexcept { return apvts; }
    cs80::Engine& getEngine() noexcept { return engine; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    const std::vector<cs80::FactoryPreset>& getFactoryPresets() const { return cs80::factoryPresets(); }
    int getCurrentPresetIndex() const noexcept { return currentProgram; }
    int getActiveVoiceCount() const noexcept { return engine.activeVoices(); }
    juce::String getPresetName() const { return presetName; }
    void setPresetName(const juce::String& name);
    void loadFactoryPreset(int index);
    void loadUserPreset(const juce::File& file);
    void saveUserPreset(const juce::String& name);
    juce::Array<juce::File> getUserPresetFiles() const;

    float getParamValue(int index) const;
    void setParamValue(int index, float physicalValue);
    cs80::ParamValues currentValues() const;
    void applyParameterValues(const cs80::ParamValues& values);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String parameterId(int index) { return juce::String(cs80::paramId(index)); }

    std::function<void()> onPresetChanged;

private:
    void readParameters();
    void handleMidi(const juce::MidiMessage& message);
    void applyValues(const cs80::ParamValues& values);
    juce::File userPresetDirectory() const;

    cs80::Engine engine;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    cs80::ParamValues cached;
    std::vector<float> interleaved;
    juce::String presetName { "Init Patch" };
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Cs80AudioProcessor)
};
