#pragma once

#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "core/Params.h"
#include "KeyboardEventQueue.h"
#include "KeyboardGestureCapture.h"
#include "KeyboardPressureOwnership.h"
#include "ModernLayerState.h"
#include "MtsEspClient.h"
#include "core/MpeZones.h"
#include "core/Tuning.h"
#include "OutputMeter.h"
#include "VoiceMonitor.h"
#include "MidiLearn.h"
#include "UndoHistory.h"
#include "HostAutomationBridge.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>
#include <atomic>
#include <array>
#include <cmath>
#include <limits>
#include <functional>
#include <vector>

class Cs80AudioProcessorEditor;
namespace cs80::tests { struct PresetSaveAccess; struct AutomationStateAccess; }

class Cs80AudioProcessor : public juce::AudioProcessor, private juce::MidiKeyboardState::Listener,
                           public cs80::HostAutomationClient
{
public:
    explicit Cs80AudioProcessor(const juce::File& presetDirectory = {});
    ~Cs80AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool usesHostAutomationTiming() const noexcept override;
    int sampleAccurateParameterIndex(const juce::AudioProcessorParameter&) const noexcept override;
    double sampleAccurateParameterValue(size_t lane) const noexcept override;
    void beginHostAutomationBlock(int frameCount) noexcept override;
    bool attachHostAutomationQueue(juce::AudioProcessorParameter&, cs80::HostAutomationQueue) noexcept override;
    void endHostAutomationBlock() noexcept override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    bool isExternalInputEnabled() const noexcept
    { return externalInputEnabled.load(std::memory_order_relaxed); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CS-80 Reference"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    // Host discovery describes capability, not the current MPE/isolation switch.
    // Enable MPE in both plugin and host: channel 1 is master, 2..16 members,
    // with manually matched bend ranges (no RPN zone/range negotiation).
    bool supportsMPE() const override { return true; }
    bool isMidiEffect() const override { return false; }
    // No universal finite silence deadline: playable-mode analog noise runs
    // without voices, and bypassed FX retain histories. Even a noise-free dry
    // note can release for ~61 s (20 s * 2^1.1 * ln(5000)/6) before FX decay.
    // Keep this capability-level policy stable across patches/modes; infinity
    // maps to VST3's kInfiniteTail, not an invented practical noise-floor cutoff.
    // Hosts may therefore keep processing idle instances; offline exports need
    // an explicit end/selection rather than an automatic finite-tail endpoint.
    double getTailLengthSeconds() const override { return std::numeric_limits<double>::infinity(); }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram.load(std::memory_order_relaxed); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    cs80::UndoableParameterState& getAPVTS() noexcept { return apvts; }
    // Modern modulation layer session data (MSEG shapes, matrix routing; ws5-modern).
    cs80::ModernLayerState& getModernLayer() noexcept { return modernLayer; }
    const cs80::UndoableParameterState& getAPVTS() const noexcept { return apvts; }
    cs80::Engine& getEngine() noexcept { return engine; }
    // Non-audio producers only. Never feed host MIDI into this state.
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    // Message-thread maintenance; clears lifecycle epochs and discards JUCE indirect MIDI.
    void serviceKeyboardUI();
    // Non-audio request only. The next block hard-stops existing notes on all
    // channels, clears sustain/arp and invalidates old UI-key ownership. New
    // host MIDI in that block remains playable. FX histories/noise are retained.
    void requestPanicFromUI() noexcept { pendingPanic.store(true, std::memory_order_release); }
    bool isHostKeyDown(int note) const noexcept
    {
        return note >= 0 && note < 128
            && hostKeyMasks[(size_t) note].load(std::memory_order_relaxed) != 0;
    }

    // Host program list: Legacy Library 0..221 (shipped indexes unchanged), then
    // the curated circuit-engine library (cs80::programLibrary()).
    const std::vector<cs80::FactoryPreset>& getFactoryPresets() const { return cs80::programLibrary(); }
    int getCurrentPresetIndex() const noexcept { return currentProgram.load(std::memory_order_relaxed); }
    int getActiveVoiceCount() const noexcept { return activeVoiceCount.load(std::memory_order_relaxed); }
    int getVoiceCapacity() const noexcept { return voiceCapacity.load(std::memory_order_relaxed); }
    void setVoiceMonitorEnabled(bool enabled) noexcept { voiceMonitorEnabled.store(enabled, std::memory_order_relaxed); }
    bool readVoiceMonitor(cs80::VoiceMonitorFrame& frame) const noexcept { return voiceMonitor.read(frame); }
    // Running count of MIDI events (notes, CC, bend, pressure, anything) that processBlock was handed by the host.
    // Presentation only: the UI compares it between polls to light the MIDI activity lamp.
    unsigned getHostMidiEventCount() const noexcept { return hostMidiEvents.load(std::memory_order_relaxed); }
    // One UI consumer; peak/events can describe adjacent polling intervals.
    // Read-only audio telemetry, never a parameter or an output safety limiter.
    cs80::OutputMeterReading consumeOutputMeter() noexcept { return outputMeter.consume(); }
    bool isVoiceActive(int i) const noexcept
    {
        return i >= 0 && i < 16 && (activeVoiceMask.load(std::memory_order_relaxed) & (1u << i)) != 0;
    }
    // Single-value mailboxes: only the audio callback mutates the DSP engine.
    void setRibbonFromUI(float v) noexcept { pendingRibbon.store(v, std::memory_order_relaxed); }
    void setPitchWheelFromUI(float v) noexcept { pendingPitch.store(v, std::memory_order_relaxed); }
    void setModWheelFromUI(float v) noexcept { pendingMod.store(v, std::memory_order_relaxed); }
    // On-screen foot controller: direct physical-expression API, independent of MIDI CC11.
    void setExpressionFromUI(float v) noexcept
    {
        if (! std::isfinite(v)) return;
        v = juce::jlimit(0.0f, 1.0f, v);
        expressionValue.store(v, std::memory_order_relaxed);
        pendingExpression.store(v, std::memory_order_relaxed);
    }
    // Last foot-controller position from the on-screen wheel or non-isolated MIDI CC11.
    // The processor re-applies it after prepareToPlay and isolation transitions; the editor
    // reads it back; otherwise wheel and sound would disagree after a transport restart
    // or after reopening the window.
    float getExpressionValue() const noexcept { return expressionValue.load(std::memory_order_relaxed); }
    using KeyPressureToken = cs80::KeyPressureToken;
    // Non-audio only. Wrap the real synchronous JUCE onset, not a later pitch
    // lookup. Nested scopes restore their predecessor even if dispatch throws.
    template <class Dispatch>
    KeyPressureToken captureKeyboardGesture(int channel, int note, Dispatch&& dispatch)
    {
        cs80::KeyboardGestureCapture capture(this, channel, note);
        dispatch();
        return capture.result();
    }
    // Tagged pressure shares bounded FIFO/timestamp order with its actual onset.
    // Producer locking is permitted here; the audio consumer never locks.
    bool setKeyPressureFromUI(KeyPressureToken token, float pressure)
    { return keyboardEvents.pushPressure(token, pressure, juce::Time::getMillisecondCounter()); }
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
    enum class PresetSaveStatus { saved, alreadyExists, error };
    enum class PresetOverwrite { no, yes };
    struct PresetSaveResult
    {
        PresetSaveStatus status = PresetSaveStatus::error;
        juce::File file;
        juce::String message;
        bool wasSaved() const noexcept { return status == PresetSaveStatus::saved; }
    };
    // Saving never overwrites an existing file without an explicit affirmative policy.
    // Neither the sound nor the displayed preset metadata changes on failure.
    [[nodiscard]] PresetSaveResult saveUserPreset(const juce::String& name,
                                                  PresetOverwrite = PresetOverwrite::no);
    juce::Array<juce::File> getUserPresetFiles() const;

    // Physical selector: 0..10 factory, 11 memory A, 12 memory B, 13 PANEL.
    int getHardwareSource(int channel) const noexcept;
    void selectHardwareSource(int channel, int source);
    float getMemoryValue(int bank, int local) const noexcept;
    void setMemoryValue(int bank, int local, float value) noexcept;

    // Saved-state schema. A state without the attribute is schema 1, written
    // before this marker existed. Missing (or malformed) values restore with the
    // semantics of the schema that wrote the state, never a later default:
    //   1  pre-plan: ENGINE Legacy (SOURCE PERFORMANCE off), original SUB off.
    //   2  plan 0.3 marker; fresh instances still started on Legacy.
    //   3  plan 1.7 engine default flip: fresh instances start on the circuit
    //      engine, so a schema-3 state without ENGINE means Circuit.
    // See docs/IMPROVEMENT_PLAN.md 0.3 and 1.7.
    static constexpr int stateSchemaVersion = 3;
    static constexpr const char* stateSchemaKey = "stateSchemaVersion";
    static constexpr int memoryControlCount = 26;
    static constexpr int hostParameterCount = cs80::P_numParams + 2 + 4 * memoryControlCount;
    static bool isMemoryControl(int local) noexcept;
    // Jack/pedal/selector/output-switch state survives preset recall.
    static bool isPhysicalSetupParameter(int index) noexcept;
    static juce::String hardwareSourceParameterId(int channel);
    static juce::String memoryParameterId(int bank, int local);

    float getParamValue(int index) const;
    void setParamValue(int index, float physicalValue);
    cs80::ParamValues currentValues() const;
    void applyParameterValues(const cs80::ParamValues& values);

    // User-facing engine concepts (plan 1.7) over the unchanged host IDs.
    // ENGINE is source_performance: on = Circuit, off = Legacy (compat), which
    // stays only so old sessions and the legacy factory bank sound unchanged.
    // PURE CS-80 is parity_mode, the isolation contract: original features
    // only, rendered by the circuit engine whatever ENGINE says.
    enum class EngineMode { circuit, legacy };
    void setEngineMode(EngineMode mode);
    EngineMode getEngineMode() const;          // the ENGINE switch as stored
    EngineMode getEffectiveEngineMode() const; // what renders: PURE implies Circuit
    void setPure(bool pure);
    bool isPure() const;
    // New-preset creation (the INIT action's hook): the factory "Init Patch"
    // program with the original SUB panel on the circuit engine, like a fresh
    // instance. The legacy bank program itself is unchanged.
    void loadInitPatch();

    // ---- ws3-keyboard Phase 6 processor APIs (modern layer) ----------------
    // Non-audio threads. Parses a Scala scale and an optional keyboard mapping
    // (empty = linear, middle C 60, A4 440 Hz) and publishes the table to the
    // audio thread, where tuning_mode 1 uses it. Returns an empty string on
    // success, otherwise the reason (the previous tuning is kept). The texts
    // are saved with the session, not with presets.
    juce::String loadScalaTuning(const juce::String& scl, const juce::String& kbm = {});
    void clearScalaTuning();
    juce::String getScalaDescription() const;
    // MTS-ESP (tuning_mode 2): client compiled in, and a master connected.
    static bool isMtsEspAvailable() noexcept { return cs80::MtsEspClient::available(); }
    bool isMtsEspConnected() const noexcept { return mtsEspConnected.load(std::memory_order_relaxed); }
    // Sixteen-voice mode runs a second, identical eight-voice instrument.
    // Render thread diagnostics only, like getEngine().
    const cs80::Engine& getExtensionEngine() const noexcept { return *extensionEngine; }

    // [ws6-library] A/B compare and randomise (PluginProcessorLibrary.cpp).
    // Message thread. A snapshot is the whole sound: panel values, both tone
    // selectors, the four memory banks and the preset name/index. Physical
    // setup (jack, pedal, selector, output switch) and isolation stay live.
    // abToggle() stores the sounding patch in the active slot and recalls the
    // other one; an empty slot starts as a copy, so the first toggle is silent.
    void abToggle();
    // Copy the sounding patch into slot 0 (A) or 1 (B). From A, slot 1 is "A->B".
    void abCopyCurrentTo(int slot);
    int getAbSlot() const noexcept;
    bool isAbSlotStored(int slot) const;
    // Original CS-80 panel controls only, inside safe sub-ranges of their
    // hardware ranges (src/core/Randomise.h); seedable and deterministic.
    void randomiseWithinOriginalRanges(juce::uint32 seed);

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String parameterId(int index) { return juce::String(cs80::paramId(index)); }

    // ---- [ws8-studio] MIDI learn and editor state -------------------------------------------
    // Both live in the plugin state (MIDI_LEARN and UI_STATE children, each with a version), never
    // in host parameters or presets. The map is per session: restoring a state replaces it, and
    // loading a preset leaves it alone. Mappings are neither learned nor applied in PURE CS-80.
    cs80::MidiLearnMap& getMidiLearn() noexcept { return midiLearn; }
    // Host parameter index of a core parameter (extensions are appended after the 228 shipped ones).
    int hostParameterIndex(int coreIndex) const;
    struct EditorState
    {
        std::atomic<bool> tourSeen { false };
        std::atomic<bool> qwertyEnabled { true };
        std::atomic<bool> circuitNotes { false };
        std::atomic<bool> controllersShown { false };
    };
    EditorState& getEditorState() noexcept { return editorState; }
    static constexpr int editorStateVersion = 1;

    // ---- [ws8-studio] Undo history ------------------------------------------------------------
    // The APVTS is built with this UndoManager and records one transaction per user gesture; host
    // automation, MIDI learn, preset loads and state restores are never recorded, and loading a sound
    // starts a fresh history (UndoHistory.h). Message thread.
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    bool undo();
    bool redo();
    // Wrap a programmatic edit the player should undo in one step (A/B switch, randomise, copy rank).
    using ScopedUserEdit = cs80::UndoableParameterState::ScopedUserEdit;
    cs80::UndoableParameterState& getUndoableState() noexcept { return apvts; }

private:
    friend struct cs80::tests::PresetSaveAccess;
    friend struct cs80::tests::AutomationStateAccess;
    // [ws6-library] program change, bank select, A/B state (PluginProcessorLibrary.cpp).
    struct LibraryState;
    std::unique_ptr<LibraryState> library;
    void initialiseLibrary();
    // Audio thread. Returns true when the recalled program changed the render
    // policy (the caller then discards the pre-transition UI batch).
    bool handleProgramChange(int midiChannel, int program);
    void handleBankSelect(int controller, int value, int midiChannel) noexcept;
    void writeLibraryState(juce::ValueTree& state) const;
    void readLibraryState(const juce::ValueTree& state);
    void captureAbSnapshot(int slot);
    void recallAbSnapshot(int slot);
    void setPresetMetadata(const juce::String& name, int index);
    void applyFreshInstancePatch();
    void setParameterGesture(int index, float physicalValue);
    void readParameters();
    juce::ValueTree normaliseRestoredState(const juce::ValueTree& state) const;
    void resetHardwareSources() noexcept;
    void handleMidi(const juce::MidiMessage& message);
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void resetKeyboardHandoff() noexcept;
    void observeHostMidi(const juce::MidiMessage& message) noexcept;
    // Key events go to the first instrument until it is full, then (sixteen-
    // voice mode only) to the second; everything else reaches both.
    void routeNoteOn(int note, float velocity, int channel);
    void routeNoteOff(int note, int channel);
    template <class F> void forEachEngine(F&& f) { f(engine); if (extensionActive) f(*extensionEngine); }
    juce::String publishScala(const juce::String& scl, const juce::String& kbm);
    void applyValues(const cs80::ParamValues& values);
    // [ws8-studio] MIDI learn on the audio thread; studio state in and out of the saved tree.
    void applyMidiLearn(const juce::MidiBuffer& midi) noexcept;
    bool midiLearnConsumes(int cc) const noexcept { return midiLearnLive && midiLearn.parameterFor(cc) >= 0; }
    void writeStudioState(juce::ValueTree& state) const;
    void readStudioState(juce::ValueTree& state);
    juce::File userPresetDirectory() const;
    using PresetWriter = std::function<juce::Result(const juce::XmlElement&, const juce::File&)>;
    PresetSaveResult writeUserPreset(const juce::String&, PresetOverwrite, const PresetWriter&);
    // An empty dependency uses the normal application-data folder. Isolated
    // storage tests can supply a fresh temporary directory instead.
    juce::File presetDirectoryOverride;

    cs80::Engine engine;
    juce::UndoManager undoManager;            // [ws8-studio] constructed before the APVTS that uses it
    cs80::UndoableParameterState apvts;
    cs80::ModernLayerState modernLayer { apvts }; // [ws5-modern] child of the APVTS state; after apvts
    juce::MidiKeyboardState keyboardState;
    juce::MidiBuffer keyboardDiscardBuffer; // Message thread only.
    cs80::KeyboardEventQueue<> keyboardEvents;
    std::array<unsigned, 128> uiHeldMasks {}; // Audio-owned source recovery tracking.
    cs80::SixteenVoicePressureOwnership keyPressureOwnership;
    bool pressureArpWasOn = false; // Audio-owned effective arp edge tracking.
    std::array<std::atomic<unsigned>, 128> hostKeyMasks {};
    cs80::ParamValues cached;
    // Captured once with the effective programmer controls; never serialized as
    // a second authority beside the existing APVTS selector parameters.
    std::array<int, 2> cachedHardwareSources { 13, 13 };
    std::array<std::atomic<float>*, 2> hardwareSources {};
    std::array<std::array<std::atomic<float>*, cs80::CP_count>, 4> hardwareMemories {};
    std::array<std::array<std::array<float, cs80::CP_count>, 11>, 2> hardwareFactoryValues {};
    std::array<std::atomic<float>*, cs80::P_numParams> parameterValues {};
    std::array<juce::RangedAudioParameter*, cs80::kSampleAccurateParameters.size()> automationParameters {};
    std::array<cs80::HostAutomationCurve, cs80::kSampleAccurateParameters.size()> automationCurves {};
    std::array<double, cs80::kSampleAccurateParameters.size()> automationCurrent {};
    std::array<float, cs80::kSampleAccurateParameters.size()> automationParameterReadback {};
    bool hostAutomationBlock = false;
    int hostAutomationFrames = 0, hostAutomationPosition = 0;
    void resetHostAutomationState() noexcept;
    void refreshHostAutomationParameter(size_t lane) noexcept;
    void prepareHostAutomation() noexcept;
    void syncHostAutomationParameters() noexcept;
    void applyHostAutomationAt(int position, bool publish) noexcept;
    int hostAutomationSpan(int position, int remaining) const noexcept;
    static constexpr float noPendingControl = std::numeric_limits<float>::quiet_NaN();
    static_assert(std::atomic<bool>::is_always_lock_free, "Panic handoff must be lock-free");
    std::atomic<bool> pendingPanic { false };
    std::atomic<float> pendingRibbon { noPendingControl }, pendingPitch { noPendingControl }, pendingMod { noPendingControl },
        pendingExpression { noPendingControl };
    std::atomic<float> expressionValue { 1.0f };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> voiceCapacity { 8 };
    std::atomic<unsigned> hostMidiEvents { 0 };
    std::atomic<unsigned> activeVoiceMask { 0 };
    cs80::OutputMeter outputMeter;
    cs80::VoiceMonitor voiceMonitor;
    std::atomic<bool> voiceMonitorEnabled { false };
    std::uint64_t voiceMonitorSerial = 0; // audio-owned
    // ws3-keyboard: CC88 prefixes, second instrument, tuning hand-over.
    cs80::HighResolutionVelocity highResolutionVelocity; // audio thread
    std::unique_ptr<cs80::Engine> extensionEngine;
    std::vector<float> extensionInterleaved;
    bool extensionActive = false; // audio thread
    juce::SpinLock scalaLock;      // writers; the audio thread only try-locks
    cs80::tuning::TuningTable scalaSlot;
    std::atomic<unsigned> scalaVersion { 0 };
    unsigned scalaAppliedVersion = 0; // audio thread
    juce::String scalaText, kbmText, scalaDescription; // guarded by scalaLock
    cs80::MtsEspClient mtsEsp;
    cs80::tuning::TuningTable mtsEspTable; // audio thread
    std::atomic<bool> mtsEspConnected { false };
    std::vector<float> interleaved;
    std::vector<float> externalScratch;
    std::atomic<bool> externalInputEnabled { false };
    // Not accessed by processBlock. State/program APIs already perform non-RT
    // work; this short metadata lock does not make those APIs realtime-safe.
    mutable juce::CriticalSection presetMetadataLock;
    juce::String presetName { "Init Patch" };
    std::atomic<int> currentProgram { 0 };

    // [ws8-studio]
    cs80::MidiLearnMap midiLearn;
    bool midiLearnLive = false;   // audio thread: mappings apply in this block (not PURE CS-80)
    EditorState editorState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Cs80AudioProcessor)
};
