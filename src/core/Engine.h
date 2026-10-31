#pragma once

#include "Dsp.h"
#include "OriginalSubOscillator.h"
#include "ExternalModulationInterpolator.h"
#include "Params.h"
#include "PerformanceControls.h"
#include "PraWahCircuit.h"
#include "BbdChorus.h"
#include "OutputTapCircuit.h"
#include "SourceFactoryTones.h"
#include "KasScanner.h"
#include "KbcContour.h"
#include "ControllerCurves.h"
#include "experimental/ContinuousVoiceCircuit.h"

#include <array>
#include <cstdint>

namespace cs80
{

// Source-performance policy v1: current source models plus explicit software
// performance overlays. Isolation retains priority and its original contract.
// This policy does not select the separate experimental oscillator/backend.
enum class RenderPolicy { legacy, sourcePerformance, isolation };
inline constexpr RenderPolicy renderPolicy(float parity, float sourcePerformance) noexcept
{
    return parity >= 0.5f ? RenderPolicy::isolation
         : sourcePerformance >= 0.5f ? RenderPolicy::sourcePerformance
                                     : RenderPolicy::legacy;
}
inline constexpr bool usesSourceModels(RenderPolicy policy) noexcept
{
    return policy != RenderPolicy::legacy;
}
// PURE CS-80 is the isolation policy: every non-original (modern-layer)
// feature must be inert under it, whatever its own parameters say.
inline constexpr bool isPureCs80(RenderPolicy policy) noexcept
{
    return policy == RenderPolicy::isolation;
}

// One render-thread transaction: effective parameters (including render policy)
// and existing rank-local selector identities, not a second saved bank/authority.
struct ControlFrame
{
    ParamValues effectiveValues;
    std::array<int, 2> hardwareSources { 13, 13 };
    std::uint32_t modelVersion = source_factory::modelVersion;
};

struct RankSettings
{
    bool saw = true;
    bool pulse = false;
    bool hpfOn = true;
    bool lpfOn = true;
    float sine = 0.0f;
    float noise = 0.0f;
    float pulseWidth = 0.5f;
    float pwmSpeed = 0.45f;
    float pwmDepth = 0.0f;
    int feet = 1;
    float hpfCutoff = 25.0f;
    float hpfEffectiveCv = 0.0f; // cached by buildSettings
    float hpfRes = 0.0f;
    float lpfCutoff = 7200.0f;
    float lpfEffectiveCv = 0.0f;
    float lpfRes = 0.12f;
    float fIl = 0.5f;
    float fAl = 0.55f;
    float fA = 0.035f;
    float fD = 0.42f;
    float fR = 0.30f;
    float vcfLevel = 0.78f;
    float vA = 0.008f;
    float vD = 0.32f;
    float vS = 0.78f;
    float vR = 0.28f;
    float vLevel = 0.72f;
    float initBrill = 0.0f;
    float initLevel = 0.0f;
    float afterBrill = 0.0f;
    float afterLevel = 0.0f;
    float pan = 0.0f;
};

struct GlobalSettings
{
    bool parityMode = false;
    RenderPolicy renderPolicy = RenderPolicy::legacy;
    float masterTune = 0.0f;
    float detuneCents = 7.0f;
    float mix = 0.5f;
    float brilliance = 0.0f;
    float resonance = 0.22f;
    float masterSustain = 0.0f;
    float volume = 0.62f;
    float initBend = 0.0f;
    float portamento = 0.0f;
    int glideMode = 0;
    int sustainMode = 0;
    float keyRelease = 0.18f;
    float subSpeed = 4.2f;
    int subWave = 0;
    bool subOriginalMode = true;
    int subOriginalFunction = 0;
    float subOriginalSpeed = 4.2f;
    float subExtLevel = 0.0f;
    float subVco = 0.0f;
    float subVcf = 0.0f;
    float subVca = 0.0f;
    float subAtSpeed = 0.0f;
    float subAtVco = 0.0f;
    float subAtVcf = 0.0f;
    float ringSpeed = 2.0f;
    float ringDepth = 0.0f;
    float ringAttack = 0.08f;
    float ringDecay = 0.45f;
    float ringAmount = 0.0f;
    bool chorusOn = false;
    bool chorusTremolo = false;
    float chorusSpeed = 0.55f;
    float chorusDepth = 0.55f;
    bool stereo = true;
    float voiceDispersion = 0.35f;
    float drift = 0.25f;
    float analogNoise = 0.12f;
    bool mpe = false;
    float pitchBendRange = 2.0f;
    float mpeBendRange = 48.0f;
    float tuningRef = 440.0f;
    float modWheelSub = 0.25f;
    float modWheelVcf = 0.0f;
    float kbdBrillianceLow = 0.0f;
    float kbdBrillianceHigh = 0.0f;
    float kbdLevelLow = 0.0f;
    float kbdLevelHigh = 0.0f;
    // Cached footswitch gates. With no plug or no assignment both stay open,
    // which is the pre-assigner behavior.
    bool sustainGate = true;
    bool glideGate = true;
    bool pedalWah = false;
    bool outputHigh = true;
    float lfoRate[3] { 0.28f, 0.55f, 1.1f };
    float lfoDepth[3] { 0.0f, 0.0f, 0.0f };
    int lfoDest[3] { 0, 0, 0 };
    int fxType[3] { 0, 0, 0 };
    float fxMix[3] { 0.0f, 0.0f, 0.0f };

    // Performance macros are neutral at 0.5. They are applied after the panel
    // values so a macro never overwrites what the player sees on the panel.
    float macro[4] { 0.5f, 0.5f, 0.5f, 0.5f };
    float envTimeScale = 1.0f;

    bool arpOn = false;
    int arpMode = 0;
    float arpRate = 4.0f;
    int arpOctaves = 1;

    // ws3 keyboard/MIDI/tuning settings (all neutral at their defaults).
    bool kasScanTiming = false;
    float velocityCurve = 0.0f, pressureCurve = 0.0f, pressureThreshold = 0.0f;
    bool ribbonCcEnabled = false;
    int ribbonCc = 16;
    int tuningMode = 0;
    int voiceMode = 0;
    float unisonDetune = 10.0f;
};

class Engine
{
public:
    Engine();

    void prepare(double hostSampleRate);
    void reset();
    // Legacy entry point retains sanitization and denotes PANEL/no source model.
    void setParams(const ParamValues& values);
    enum class ControlFrameStatus { accepted, invalidSelector, unsupportedModel, nonfiniteParameter, compileFailure };
    // Invalid frames are rejected as a whole, preserving parameters, compiled
    // controls and audio state. Finite core values retain established clamping.
    bool setControlFrame(const ControlFrame& frame);
    ControlFrameStatus getControlFrameStatus() const noexcept { return controlFrameStatus; }
    struct SourceToneInfo
    {
        int selector = 13;
        source_factory::Route route = source_factory::Route::panel_or_memory;
        source_factory::Provenance provenance;
    };
    SourceToneInfo getSourceToneInfo(int rank) const noexcept;
    std::uint64_t getSourceCompileCount() const noexcept { return sourceCompileCount; }
    // Last same-sample receiver traces, not physical measurements. Cleared on a
    // valid source transaction; render-thread diagnostics only, like RankInfo.
    source_factory::CutoffReadout getSourceCutoffReadout(int rank) const noexcept;
    source_factory::SharedReadout getSourceSharedReadout() const noexcept { return sourceShared; }
    const ParamValues& getParams() const noexcept { return params; }

    void noteOn(int midiNote, float velocity, int midiChannel = 1);
    void noteOff(int midiNote, int midiChannel = 1);
    void allNotesOff(bool release = true, int midiChannel = 0);
    // A channel of 0 applies the pressure to every matching note; a positive
    // channel is required for correct per-note behaviour in MPE.
    void polyPressure(int midiNote, float pressure, int midiChannel = 0);
    void channelPressure(int midiChannel, float pressure);
    void pitchBend(int midiChannel, float value); // -1..1
    void controlChange(int controller, float value, int midiChannel = 1);
    void setRibbon(float value);
    // Direct physical-pedal proxy, independent of the optional MIDI CC11 mapping.
    void setExpression(float value);

    // Optional separate host-rate mono modulation; never mixed directly into output.
    // Input/output storage must not overlap (plugin captures aliased host input).
    // Null means disconnected: reconstruction history is cleared on transition.
    void render(float* interleavedStereo, int numFrames, const float* externalMono = nullptr);

    int activeVoices() const noexcept;
    bool isParityMode() const noexcept { return global.parityMode; }
    // Render-thread diagnostic. UI should derive the requested policy from its
    // APVTS snapshot instead of reading this audio-owned accepted state.
    RenderPolicy getRenderPolicy() const noexcept { return global.renderPolicy; }
    // Offline diagnostics; read on the rendering thread, not concurrently.
    float getRingFrequency() const noexcept { return ringFrequency; }
    float getRingEnvelope() const noexcept { return ringEnv; }
    float getSubOscillatorValue() const noexcept { return subValue; }
    float getSubOscillatorRate() const noexcept
    { return global.subOriginalMode ? originalSubSpeedSmoothed : subSpeedSmoothed; }
    // Odd retained phase of a 255-tap FIR: (127-1)/2 =63 host samples.
    int getLatencySamples() const noexcept { return Decimator::kHostLatency; }
    double getHostSampleRate() const noexcept { return hostRate; }
    double getInternalSampleRate() const noexcept { return internalRate; }

    // The voice telemetry is used by tests and the UI voice meter.
    struct VoiceInfo
    {
        bool active = false;
        int note = -1;
        int channel = 1;
        float velocity = 0.0f;
        float pressure = 0.0f;
        float bend = 0.0f;
        float ribbonRatio = 1.0f;
        bool held = false, pedalHeld = false;
    };
    VoiceInfo getVoiceInfo(int index) const;
    struct RankInfo
    {
        float hpfHz = 0.0f, lpfHz = 0.0f;
        float filterEnvelope = 0.0f, ampEnvelope = 0.0f;
        float oscillatorHz = 0.0f;
        float keyVolts = 0.0f, hpfQ = 0.0f, lpfQ = 0.0f;
        bool hpfEnabled = false, lpfEnabled = false;
    };
    // Offline/render-thread diagnostics for response tests and plotted traces.
    RankInfo getRankInfo(int voice, int rank) const;
    struct RankAudioState
    {
        float phase = 0.0f;
        Svf::State hpf, lpf;
    };
    // Exact current state (not last-render telemetry). Render thread only;
    // not safe for concurrent UI polling. No locks, allocations or mutations.
    RankAudioState getRankAudioState(int voice, int rank) const noexcept;
    struct RankEnvelopeState { Adsr::State amplitude; FilterEnv::State filter; };
    // Exact current envelope-state proxy; same render-thread-only restriction.
    RankEnvelopeState getRankEnvelopeState(int voice, int rank) const noexcept;
    struct PraWahState
    {
        PraWahCircuit::Capacitors capacitors;
        double sampleRate = 0.0, opticalOhms = 0.0, targetOhms = 0.0;
        float wetMix = 0.0f;
    };
    // Exact reconstruction circuit state; channel 0/1, render thread only.
    // These are model variables, not measured instrument voltages/resistances.
    PraWahState getPraWahState(int channel) const noexcept;
    struct OutputTapState
    {
        OutputTapCircuit::State state;
        OutputTapCircuit::Nodes nodes;
        double sampleRate = 0.0;
        OutputTapCircuit::Mode mode = OutputTapCircuit::Mode::high;
    };
    // Render thread only: tap 0 LEFT, 1 RIGHT, 2 GENERAL. GENERAL is a separately
    // clocked diagnostic, not a host listening mode. Model units, not measured volts.
    // Nodes describe the last processed internal sample; mode already selects
    // the next interval after a parameter event (no instantaneous node re-solve).
    OutputTapState getOutputTapState(int tap) const noexcept;

private:
    static constexpr int kNumVoices = 8;
    static constexpr int kNumRanks = 2;
    static constexpr int kMaxHeldKeys = 32;
    // Nominal ideal C-path pole only; not measured complete touch circuitry.
    static constexpr double kNominalInitialPitchDecaySeconds = 0.050;
    // Circuit engine cold state (Phase 2.1, declared inference): KAS6 power-on
    // clear memorizes key code C4#, read here through the engine's MIDI
    // convention; cold oscillator phases come from their own fixed-seed stream
    // so a first chord does not start with every oscillator in phase.
    static constexpr int kColdKeyNote = 61;
    static constexpr std::uint32_t kColdPhaseSeed = 0xC5800A11u;
    static constexpr std::uint32_t kIdleNoiseSeed = 0xC58011D1u;

    struct Rank
    {
        Oscillator osc;
        Svf hpf;
        Svf lpf;
        FilterEnv fenv;
        Adsr aenv;
        float trimTune = 0.0f;
        float trimCutoff = 0.0f;
        float trimLevel = 0.0f;
        float trimEnv = 0.0f;
        float trimPan = 0.0f;
        RankInfo info;
        Exp2Memo hpCutoffExp, lpCutoffExp, keyVoltsExp;
        // Circuit engine (Phase 2.1): controls of the last processed interval,
        // which drive the idle path and the wake catch-up of a silent circuit.
        voice_continuity::IdleRankControls idle;
        voice_continuity::IdleStageReferences idleRefs;
        voice_continuity::IdleHistory idleHistory;
    };

    struct Voice
    {
        bool active = false;
        bool held = false;
        bool sustained = false;
        bool pedalHeld = false;
        int note = 60;
        int midiChannel = 1;
        float velocity = 0.8f;
        float pressure = 0.0f;
        float timbre = 0.0f;
        float mpeBend = 0.0f;
        double glidePitch = 60.0;
        float targetPitch = 60.0f;
        double initialPitchTransient = 0.0;
        float ribbonRatio = 1.0f;
        uint64_t age = 0;
        KeyboardContourWeights contour;
        Rank ranks[kNumRanks];
        // Circuit engine free-running state (Phase 2.1). idleClock counts the
        // internal samples since the last idle step (or since retirement);
        // idleSteps counts the idle steps since retirement.
        int idleClock = 0;
        int idleSteps = 0;
        // The filter EG holds a reset (not a completed-release) state: cold
        // after reset/prepare, or cleared by Sustain II termination.
        bool envelopeReset = true;
        // SH7 per-voice K hold (Phase 2.2, circuit engine).
        kas::HeldKeyNode keyNode;
        // KBC contour of the key last assigned to this channel (Phase 2.3):
        // per rank effective cutoff CV (HP, LP) and level gain contributions.
        int kbcKey = 36;
        float kbcCv[kNumRanks][2] {};
        float kbcLevelGain[kNumRanks] {};
    };

    struct HeldKey
    {
        int note = 60;
        int midiChannel = 1;
        float velocity = 0.8f;
        uint64_t order = 0;
    };

    void applyParams(const ParamValues& values);
    void buildSettings();
    float filterInitialTarget(int rank) const noexcept;
    float filterAttackTarget(int rank) const noexcept;
    void updateSourceReadouts(float sampleRate) noexcept;
    void processInternalSample(float& outL, float& outR, float externalModulation);
    void startVoice(Voice& voice, int note, float velocity, int midiChannel);
    // Circuit engine: bring a silent free-running circuit exactly up to date
    // before its note-on (ContinuousVoiceCircuit.h catch-up).
    void wakeIdleVoice(Voice& voice) noexcept;
    // Circuit engine: recompute every channel's KBC contour contributions.
    void refreshKeyboardContour() noexcept;
    bool freeRunningVoices() const noexcept { return usesSourceModels(global.renderPolicy); }
    void releaseVoice(Voice& voice);
    void releaseKey(Voice& voice);
    // Footswitch gate fell: shorten already-released tails to their non-performance
    // release without restarting the envelope stage, retriggering or holding a note.
    void cancelSustainTails();
    bool isSustainPedalDown(int midiChannel) const noexcept;
    Voice* findVoiceForNote(int note, int midiChannel);
    int chooseVoice();
    // Note events after the optional KAS scan quantisation (Phase 2.2).
    void applyNoteOn(int midiNote, float velocity, int midiChannel);
    void applyNoteOff(int midiNote, int midiChannel);
    bool scanTimingActive() const noexcept
    { return global.kasScanTiming && usesSourceModels(global.renderPolicy) && ! global.arpOn; }
    // Circuit engine: a still-held key that found no free circuit is assigned
    // when one frees (declared KAS model, KasScanner.h).
    void rememberUnassignedKey(int note, float velocity, int midiChannel);
    bool forgetUnassignedKey(int note, int midiChannel);
    void reconsiderUnassignedKeys();

    // Arpeggiator. It is clocked at the internal rate so a rate change is
    // heard immediately and the pattern does not drift with block size.
    void updateArpeggiator(float invSampleRate);
    void rebuildArpSequence();
    void triggerArpNote();
    void stopArpVoice();
    void rememberHeldKey(int note, float velocity, int midiChannel);
    void forgetHeldKey(int note, int midiChannel);

    ParamValues params;
    GlobalSettings global;
    RankSettings rankSettings[kNumRanks];
    std::array<int, kNumRanks> hardwareSources { 13, 13 };
    std::array<source_factory::Route, kNumRanks> sourceRoutes {
        source_factory::Route::panel_or_memory, source_factory::Route::panel_or_memory };
    std::array<bool, kNumRanks> sourceActive {};
    // Immutable per-selection maps; compiled only when an active address changes.
    std::array<source_factory::ToneControls, kNumRanks> sourceTones;
    std::array<source_factory::RankBoundary, kNumRanks> sourceBoundaries;
    std::array<source_factory::CutoffReadout, kNumRanks> sourceCutoff;
    source_factory::SharedReadout sourceShared;
    std::array<std::array<float, kNumVoices>, kNumRanks> sourceFilterValues {};
    std::uint64_t sourceCompileCount = 0;
    ControlFrameStatus controlFrameStatus = ControlFrameStatus::accepted;

    std::array<Voice, kNumVoices> voices;
    uint64_t voiceCounter = 0;
    int nextVoice = 0;
    int lastNote = 60;
    float globalBend = 0.0f;
    float modWheel = 0.0f;
    float expression = 1.0f;
    float ribbon = 0.0f;

    double hostRate = 48000.0;
    double internalRate = 96000.0;
    double initialPitchDecay = 0.0; // initialized in constructor and prepare
    int oversample = 2;
    // Circuit engine idle path: internal samples per idle step and the
    // matching initial-pitch-transient decay over one step.
    int idleStepSamples = 32;
    double idleTransientDecay = 0.0;
    // Per-voice idle-only noise for the wake replay; never the shared audioRng.
    std::array<Rng, kNumVoices> idleNoise;

    std::array<float, 2> pwmPhase {};
    Lfo subLfo;
    OriginalSubOscillator originalSub;
    ExternalModulationInterpolator externalInput;
    float originalSubSpeedSmoothed = 4.2f;
    float originalSubSmoothing = 0.0f;
    Lfo modLfos[3];
    float subValue = 0.0f;
    float lfoValues[3] {};
    float lfoPitch = 0.0f, lfoFilter = 0.0f, lfoAmp = 0.0f, lfoPwm = 0.0f, lfoPan = 0.0f;
    float subSpeedSmoothed = 4.2f;
    ExpMemo<double> glideExp;

    // Isolation uses the reviewed capacitor graph with an explicitly inferred
    // optical/loading/trim profile. Both branches run even in EXP (dry X).
    PraWahCircuit praWahL, praWahR;
    // Legacy EXP-WAH branch and GENERAL output trim. Preserve its arithmetic,
    // transparent EXP bypass and reset policy for default-off compatibility.
    Svf wahL, wahR;
    float wahMix = 0.0f;
    float wahFrequency = kProvisionalWahLowHz;
    bool wahRunning = false;
    float wahSmoothing = 0.0f;
    // Isolation output: explicit inferred symmetric driver/load profile and
    // three independent physical coupling states, including unheard GENERAL.
    OriginalOutputCircuit outputCircuit;
    OriginalOutputCircuit::Outputs outputNodes;
    float outputGain = 1.0f;
    float outputGainSmoothing = 0.0f;

    float ringPhase = 0.0f;
    float ringEnv = 0.0f;
    float ringFrequency = 0.0f;
    bool ringGate = false;
    bool anyVoiceWasActive = false;

    Chorus chorus;
    BbdChorus bbdChorus;
    Decimator decL, decR;
    Rng audioRng { 0x51c80u };
    Rng modRng { 0x9e3779b9u };

    float dcL = 0.0f, dcR = 0.0f;
    float dcInL = 0.0f, dcInR = 0.0f;
    float smoothedVolume = 0.62f;
    Adsr ringAdsr;
    bool ringKeyDown = false;
    int heldCount = 0;
    std::array<bool, 17> sustainPedals {}; // MIDI channels1..16; index0 unused

    // Modern FX slots. Each slot owns one instance of every effect, so two slots set to the
    // same type run two independent effects instead of sharing (and double-advancing) one set
    // of delay lines, and a slot that is switched off or changes type can drop the stale audio
    // of its previous effect instead of replaying it when it is next switched on.
    struct FxSlot
    {
        Chorus chorus;
        Phaser phaser;
        Reverb reverb;
        DelayLine delayL, delayR;
        OnePole dampL, dampR;
        int liveType = 0; // effect whose state may hold audio; 0 = none

        void prepare(float rate)
        {
            chorus.prepare(rate);
            phaser.prepare(rate);
            reverb.prepare(rate);
            delayL.prepare(0.8f, rate);
            delayR.prepare(0.8f, rate);
            dampL.set(9000.0f, rate);
            dampR.set(9000.0f, rate);
            dampL.reset();
            dampR.reset();
            liveType = 0;
        }
        // Delay-line and filter history only; the tape-delay damping OnePoles are left alone so
        // that Engine::reset keeps its historical arithmetic (see applyParams).
        void resetAudio() noexcept
        {
            chorus.reset();
            phaser.reset();
            reverb.reset();
            delayL.reset();
            delayR.reset();
            liveType = 0;
        }
        void resetDamping() noexcept { dampL.reset(); dampR.reset(); }
        // Drop the audio held by one effect type (1 chorus, 2 phaser, 3 tape, 4 plate, 5 drive).
        void clear(int type) noexcept
        {
            switch (type)
            {
                case 1: chorus.reset(); break;
                case 2: phaser.reset(); break;
                case 3: delayL.reset(); delayR.reset(); resetDamping(); break;
                case 4: reverb.reset(); break;
                default: break;
            }
        }
    };
    std::array<FxSlot, 3> fxSlots;
    float driftPhase = 0.0f;
    float voiceDrift[kNumVoices] {};

    std::array<HeldKey, kMaxHeldKeys> heldKeys {};
    // KAS scan model and still-held keys without a circuit (Phase 2.2).
    kas::Scanner scanner;
    std::array<HeldKey, kMaxHeldKeys> unassignedKeys {};
    int unassignedCount = 0;
    int heldKeyCount = 0;
    uint64_t keyOrder = 0;
    // Maximum ascending sequence is 32 keys x 4 octaves. Up/down adds the
    // reverse traversal minus its two endpoints, so reserve twice that.
    std::array<HeldKey, kMaxHeldKeys * 8> arpSequence {};
    int arpSequenceLength = 0;
    int arpIndex = 0;
    int arpDirection = 1;
    int arpVoice = -1;
    int arpCurrentNote = -1;
    uint64_t arpSourceOrder = 0;
    bool arpGateOpen = false;
    bool arpWasOn = false;
    float arpClock = 0.0f;
    float arpGateTime = 0.0f;
};

} // namespace cs80
