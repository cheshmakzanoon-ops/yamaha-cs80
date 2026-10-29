#pragma once

#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "core/Params.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>
#include <atomic>
#include <array>
#include <limits>
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
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    bool isExternalInputEnabled() const noexcept
    { return externalInputEnabled.load(std::memory_order_relaxed); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CS-80 Reference"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram.load(std::memory_order_relaxed); }
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
    int getCurrentPresetIndex() const noexcept { return currentProgram.load(std::memory_order_relaxed); }
    int getActiveVoiceCount() const noexcept { return activeVoiceCount.load(std::memory_order_relaxed); }
    bool isVoiceActive(int i) const noexcept
    {
        return i >= 0 && i < 8 && (activeVoiceMask.load(std::memory_order_relaxed) & (1u << i)) != 0;
    }
    // Single-value mailboxes: only the audio callback mutates the DSP engine.
    void setRibbonFromUI(float v) noexcept { pendingRibbon.store(v, std::memory_order_relaxed); }
    void setPitchWheelFromUI(float v) noexcept { pendingPitch.store(v, std::memory_order_relaxed); }
    void setModWheelFromUI(float v) noexcept { pendingMod.store(v, std::memory_order_relaxed); }
    struct PresetMetadata
    {
        juce::String name;
        int factoryIndex = 0;
    };
    PresetMetadata getPresetMetadata() const;
    juce::String getPresetName() const { return getPresetMetadata().name; }
    void setPresetName(const juce::String& name);
    void loadFactoryPreset(int index);
    void loadUserPreset(const juce::File& file);
    void saveUserPreset(const juce::String& name);
    juce::Array<juce::File> getUserPresetFiles() const;

    // Physical selector: 0..10 factory, 11 memory A, 12 memory B, 13 PANEL.
    int getHardwareSource(int channel) const noexcept;
    void selectHardwareSource(int channel, int source);
    float getMemoryValue(int bank, int local) const noexcept;
    void setMemoryValue(int bank, int local, float value) noexcept;

    static constexpr int memoryControlCount = 26;
    static constexpr int hostParameterCount = cs80::P_numParams + 2 + 4 * memoryControlCount;
    static bool isMemoryControl(int local) noexcept;
    static juce::String hardwareSourceParameterId(int channel);
    static juce::String memoryParameterId(int bank, int local);

    float getParamValue(int index) const;
    void setParamValue(int index, float physicalValue);
    cs80::ParamValues currentValues() const;
    void applyParameterValues(const cs80::ParamValues& values);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String parameterId(int index) { return juce::String(cs80::paramId(index)); }

private:
    void setPresetMetadata(const juce::String& name, int index);
    void readParameters();
    juce::ValueTree normaliseRestoredState(const juce::ValueTree& state) const;
    void resetHardwareSources() noexcept;
    void handleMidi(const juce::MidiMessage& message);
    void applyValues(const cs80::ParamValues& values);
    juce::File userPresetDirectory() const;

    cs80::Engine engine;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    cs80::ParamValues cached;
    std::array<std::atomic<float>*, 2> hardwareSources {};
    std::array<std::array<std::atomic<float>*, cs80::CP_count>, 4> hardwareMemories {};
    std::array<std::array<std::array<float, cs80::CP_count>, 11>, 2> hardwareFactoryValues {};
    std::array<std::atomic<float>*, cs80::P_numParams> parameterValues {};
    static constexpr float noPendingControl = std::numeric_limits<float>::quiet_NaN();
    std::atomic<float> pendingRibbon { noPendingControl }, pendingPitch { noPendingControl }, pendingMod { noPendingControl };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<unsigned> activeVoiceMask { 0 };
    std::vector<float> interleaved;
    std::vector<float> externalScratch;
    std::atomic<bool> externalInputEnabled { false };
    // Not accessed by processBlock. State/program APIs already perform non-RT
    // work; this short metadata lock does not make those APIs realtime-safe.
    mutable juce::CriticalSection presetMetadataLock;
    juce::String presetName { "Init Patch" };
    std::atomic<int> currentProgram { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Cs80AudioProcessor)
};
