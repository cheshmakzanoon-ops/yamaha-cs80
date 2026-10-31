#pragma once

#include "Dsp.h"
#include "ModernLayer.h"
#include "ModulationLfo.h"
#include "Ig00156Filter.h"
#include "ReconstructedOscillator.h"
#include "Vca.h"
#include "OriginalSubOscillator.h"
#include "ExternalModulationInterpolator.h"
#include "Params.h"
#include "PerformanceControls.h"
#include "PraWahCircuit.h"
#include "BbdChorus.h"
#include "FxSlot.h"
#include "OutputTapCircuit.h"
#include "HeadphoneAmpCircuit.h"
#include "ExpressionOptics.h"
#include "NoiseSource.h"
#include "Oversampling.h"
#include "Smoothing.h"
#include "SourceFactoryTones.h"
#include "MemoryPanels.h"
#include "KasScanner.h"
#include "KbcContour.h"
#include "TouchCircuit.h"
#include "Tuning.h"
#include "MpeZones.h"
#include "ControllerCurves.h"
#include "experimental/ContinuousVoiceCircuit.h"
#include "ToleranceModel.h"

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
    std::array<memory_panels::Positions, 2> programmerPositions {};
    std::array<bool, 2> hasProgrammerPositions {};

    // Explicit production programmer coordinate. Existing aggregate frames
    // without positions retain their historical physical-unit parameter API.
    static ControlFrame programmer(const ParamValues& values, std::array<int, 2> selectors = {13, 13}) noexcept
    {
        ControlFrame frame { values, selectors };
        const bool source = usesSourceModels(renderPolicy(values[P_parityMode], values[P_sourcePerformance]));
        for (int rank = 0; rank < 2; ++rank)
            if (source && selectors[rank] >= 11 && selectors[rank] <= 13)
            {
                frame.programmerPositions[rank] = memory_panels::positionsFromParameters(values, rank);
                frame.hasProgrammerPositions[rank] = true;
            }
        return frame;
    }
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
    int lfoWave[3] { 0, 0, 0 }; // modern layer; 0 = historical sine
    int fxType[3] { 0, 0, 0 };
    float fxMix[3] { 0.0f, 0.0f, 0.0f };
    // Modern FX (ws5): model 0 keeps the classic type above.
    int fxModel[3] { 0, 0, 0 };
    float fxParamA[3] { 0.5f, 0.5f, 0.5f };
    float fxParamB[3] { 0.5f, 0.5f, 0.5f };
    int fxOversampling[3] { 2, 2, 2 };
    bool fxGainComp = false;

    // Performance macros are neutral at 0.5. They are applied after the panel
    // values so a macro never overwrites what the player sees on the panel.
    float macro[4] { 0.5f, 0.5f, 0.5f, 0.5f };
    float envTimeScale = 1.0f;

    bool arpOn = false;
    int arpMode = 0;
    float arpRate = 4.0f;
    int arpOctaves = 1;

    // Plan 2.5: 0 thermal drift (circuit engine), 1 the historical sine ("worn unit").
    int driftModel = 0;

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
    // Rank views refer into this instance's component arrays. Engine instances
    // own their realtime state and must never alias another instance's lanes.
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    void prepare(double hostSampleRate);
    void reset();
    // Legacy entry point retains sanitization and denotes PANEL/no source model.
    void setParams(const ParamValues& values);
    enum class ControlFrameStatus { accepted, invalidSelector, unsupportedModel, nonfiniteParameter, compileFailure, invalidProgrammerPosition };
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
    std::array<double, 26> getSourceBusReadout(int rank) const noexcept
    {
        return rank >= 0 && rank < kNumRanks && sourceActive[rank]
            ? sourceTones[rank].busVolts : std::array<double, 26>{};
    }
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

    // ---- Phase 6 modern layer (inert in PURE CS-80) ----------------------
    // Render thread only. Scala/KBM table used when tuning_mode = 1.
    void setScalaTuning(const tuning::TuningTable& table) noexcept;
    // Table used when tuning_mode = 2, e.g. built from an MTS-ESP master.
    void setMtsTuning(const tuning::TuningTable& table) noexcept;
    // A complete MIDI Tuning Standard SysEx message (F0..F7) updates the
    // tuning_mode 2 table. Allocation-free. Returns true when it applied.
    bool handleSysEx(const std::uint8_t* data, int size) noexcept;
    const tuning::TuningTable& getMtsTuning() const noexcept { return mtsTable; }
    // Pitch (fractional MIDI semitones) a note plays at under the accepted mode.
    float tunedPitch(int midiNote) const noexcept;
    const MpeZoneLayout& getMpeZones() const noexcept { return mpeZones; }
    // Voice mode accepted for this frame (0 in PURE CS-80): 0 eight voices,
    // 1 sixteen (two engines, see PluginProcessor), 2/3/4 unison x2/x4/x8.
    int getVoiceMode() const noexcept { return global.voiceMode; }
    // True while some circuit is neither key-held nor pedal-held.
    bool hasFreeCircuit() const noexcept;
    // Direct physical-pedal proxy, independent of the optional MIDI CC11 mapping.
    void setExpression(float value);

    // Modern modulation layer (IMPROVEMENT_PLAN Phase 6; inert under PURE CS-80). MSEG
    // shapes and matrix routing are session data, not parameters; the host transport
    // drives tempo sync. Render thread: copies only, no allocation or locking.
    void setModernConfig(const modern::ModernConfig& config) noexcept { modern.setConfig(config); }
    const modern::ModernConfig& getModernConfig() const noexcept { return modern.getConfig(); }
    void setTransport(const modern::Transport& transport) noexcept { modern.setTransport(transport); }
    const modern::Transport& getModernTransport() const noexcept { return modern.getTransport(); }
    // Render-thread diagnostics: current offsets and MSEG state.
    const modern::RankOffsets& getModernRankOffsets(int voice, int rank) const noexcept
    { return modern.rankOffsets(voice, rank); }
    const modern::GlobalOffsets& getModernGlobalOffsets() const noexcept { return modern.getGlobalOffsets(); }
    bool isModernLayerActive() const noexcept { return modern.isActive(); }
    float getModernMsegValue(int voice, int index) const noexcept { return modern.msegValue(voice, index); }
    double getModernMsegDisplayPosition(int index) const noexcept { return modern.displayPosition(index); }

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
    // Retained last internal phase of the current factor's FIR: 63 host
    // samples at every factor (2x: (127-1)/2). Query after each control frame.
    // Reported latency: the decimator's 63 host samples at every factor, plus in
    // the circuit engine the reconstructed oscillator's linear-phase antialias
    // kernel, 8 INTERNAL samples (plan step 1.2) = 8/N host samples at factor N
    // (4, 2, 1). Legacy's PolyBLEP oscillator adds none.
    int getOscillatorLatencySamples() const noexcept
    {
        return usesSourceModels(global.renderPolicy)
            ? static_cast<int>(ReconstructedOscillator::latencyInternalSamples()) / std::max(1, oversample) : 0;
    }
    int getLatencySamples() const noexcept
    { return OversamplingDecimator::hostLatencyFor(oversample) + getOscillatorLatencySamples(); }
    double getHostSampleRate() const noexcept { return hostRate; }
    double getInternalSampleRate() const noexcept { return internalRate; }
    // Effective internal factor (2, 4 or 8) from the OVERSAMPLING mode, the
    // render policy, the host rate and the offline flag (see Oversampling.h).
    int getOversamplingFactor() const noexcept { return oversample; }
    // Host offline-render flag (AudioProcessor::isNonRealtime). The circuit
    // engine renders offline at HQ. A factor change re-prepares the
    // rate-dependent state without allocating and stops sounding notes, like a
    // render-policy change. Render thread or setup only.
    void setNonRealtime(bool offline) noexcept;

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
        // Circuit engine TWS nodes (Phase 2.4): smoothed aftertouch a, held
        // initial touch b, pitch transient C. Zero in Legacy.
        float touchA = 0.0f, touchB = 0.0f, touchC = 0.0f;
        uint64_t assignmentSerial = 0; // read-only slot identity for UI telemetry
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
        // Legacy: the Svf memories. Circuit engine: the nodal stages' BP and LP
        // integrator histories, so the same zero/equality probes apply.
        Svf::State hpf, lpf;
        // Circuit engine: complete IG00156 nodal stage states (zero in legacy).
        Ig00156Stage::State hpCircuit, lpCircuit;
    };
    // Exact current state (not last-render telemetry). Render thread only;
    // not safe for concurrent UI polling. No locks, allocations or mutations.
    RankAudioState getRankAudioState(int voice, int rank) const noexcept;
    // Complete circuit-engine oscillator state (charge, reset, coupling
    // memories, pending antialias residuals); same render-thread restriction.
    // Pipeline oracles continue an exact copy of it (free-running voices).
    const ReconstructedOscillator::State& getRankOscillatorState(int voice, int rank) const noexcept
    {
        return voices[(size_t) std::clamp(voice, 0, kNumVoices - 1)].ranks[std::clamp(rank, 0, kNumRanks - 1)].wsc.getState();
    }
    struct RankEnvelopeState { Adsr::State amplitude; FilterEnv::State filter; };
    // Exact current envelope-state proxy; same render-thread-only restriction.
    RankEnvelopeState getRankEnvelopeState(int voice, int rank) const noexcept;
    Ig00151Vca::State getRankVcaState(int voice, int rank) const noexcept
    {
        if (voice < 0 || voice >= kNumVoices || rank < 0 || rank >= kNumRanks)
            return {};
        return voices[(size_t) voice].ranks[rank].vca.getState();
    }
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
    // Render thread only: tap 0 LEFT, 1 RIGHT, 2 GENERAL. All three are always
    // clocked; LISTENING OUTPUT chooses which the host hears (circuit engine).
    // Model units, not measured volts.
    // Nodes describe the last processed internal sample; mode already selects
    // the next interval after a parameter event (no instantaneous node re-solve).
    OutputTapState getOutputTapState(int tap) const noexcept;
    // Last internal (pre-decimator) sample of the selected listening output,
    // before the optional digital mono fold-down. Render thread only.
    struct ListeningSample { float left = 0.0f, right = 0.0f; };
    ListeningSample getListeningSample() const noexcept { return listeningSample; }

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
    // Warm-up of the cold oscillators' coupling memories (10 coupling time
    // constants: 4.5e-5 of the discharged DC step remains).
    static constexpr double kColdWarmUpSeconds = 1.0;

    // Each circuit component has one contiguous 16-lane array. The lightweight
    // Rank views below preserve voice/rank ownership while keeping envelopes,
    // coefficients and diagnostics away from the large oscillator histories.
    // Arithmetic and lane order remain unchanged, including Legacy RNG draws.
    static constexpr std::size_t kRankLanes = kNumVoices * kNumRanks;
    struct RankArrays
    {
        alignas(64) std::array<Oscillator, kRankLanes> osc {};
        alignas(64) std::array<ReconstructedOscillator, kRankLanes> wsc {};
        alignas(64) std::array<Svf, kRankLanes> hpf {};
        alignas(64) std::array<Svf, kRankLanes> lpf {};
        alignas(64) std::array<Ig00156Stage, kRankLanes> hpStage;
        alignas(64) std::array<Ig00156Stage, kRankLanes> lpStage;
        alignas(64) std::array<Ig00151Vca, kRankLanes> vca {};
        alignas(64) std::array<bool, kRankLanes> vcaCold {};
        alignas(64) std::array<FilterEnv, kRankLanes> fenv {};
        alignas(64) std::array<Adsr, kRankLanes> aenv {};
        alignas(64) std::array<float, kRankLanes> trimTune {};
        alignas(64) std::array<float, kRankLanes> trimCutoff {};
        alignas(64) std::array<float, kRankLanes> trimLevel {};
        alignas(64) std::array<float, kRankLanes> trimEnv {};
        alignas(64) std::array<float, kRankLanes> trimPan {};
        alignas(64) std::array<RankInfo, kRankLanes> info {};
        alignas(64) std::array<Exp2Memo, kRankLanes> hpCutoffExp {};
        alignas(64) std::array<Exp2Memo, kRankLanes> lpCutoffExp {};
        alignas(64) std::array<Exp2Memo, kRankLanes> keyVoltsExp {};
        alignas(64) std::array<voice_continuity::IdleRankControls, kRankLanes> idle {};
        alignas(64) std::array<voice_continuity::IdleStageReferences, kRankLanes> idleRefs {};
        alignas(64) std::array<voice_continuity::IdleHistory, kRankLanes> idleHistory {};
        alignas(64) std::array<double, kRankLanes> idleLead {};
        alignas(64) std::array<int, kRankLanes> idleFirstSamples {};
        alignas(64) std::array<tolerance::CardDraws, kRankLanes> tolDraws {};
        alignas(64) std::array<tolerance::Ratios, kRankLanes> tol {};
        alignas(64) std::array<tolerance::ThermalDrift, kRankLanes> thermal {};
        alignas(64) std::array<float, kRankLanes> driftCents {};
        RankArrays()
        {
            for (auto& stage : lpStage) stage = Ig00156Stage(kIg00156LowPassNetwork);
            vcaCold.fill(true);
        }
    };

    struct Rank
    {
        Oscillator& osc;
        ReconstructedOscillator& wsc;
        Svf& hpf;
        Svf& lpf;
        Ig00156Stage& hpStage;
        Ig00156Stage& lpStage;
        Ig00151Vca& vca;
        bool& vcaCold;
        FilterEnv& fenv;
        Adsr& aenv;
        float& trimTune;
        float& trimCutoff;
        float& trimLevel;
        float& trimEnv;
        float& trimPan;
        RankInfo& info;
        Exp2Memo& hpCutoffExp;
        Exp2Memo& lpCutoffExp;
        Exp2Memo& keyVoltsExp;
        voice_continuity::IdleRankControls& idle;
        voice_continuity::IdleStageReferences& idleRefs;
        voice_continuity::IdleHistory& idleHistory;
        double& idleLead;
        int& idleFirstSamples;
        tolerance::CardDraws& tolDraws;
        tolerance::Ratios& tol;
        tolerance::ThermalDrift& thermal;
        float& driftCents;
        Rank(RankArrays& a, std::size_t lane) noexcept
            : osc(a.osc[lane]),
              wsc(a.wsc[lane]),
              hpf(a.hpf[lane]),
              lpf(a.lpf[lane]),
              hpStage(a.hpStage[lane]),
              lpStage(a.lpStage[lane]),
              vca(a.vca[lane]),
              vcaCold(a.vcaCold[lane]),
              fenv(a.fenv[lane]),
              aenv(a.aenv[lane]),
              trimTune(a.trimTune[lane]),
              trimCutoff(a.trimCutoff[lane]),
              trimLevel(a.trimLevel[lane]),
              trimEnv(a.trimEnv[lane]),
              trimPan(a.trimPan[lane]),
              info(a.info[lane]),
              hpCutoffExp(a.hpCutoffExp[lane]),
              lpCutoffExp(a.lpCutoffExp[lane]),
              keyVoltsExp(a.keyVoltsExp[lane]),
              idle(a.idle[lane]),
              idleRefs(a.idleRefs[lane]),
              idleHistory(a.idleHistory[lane]),
              idleLead(a.idleLead[lane]),
              idleFirstSamples(a.idleFirstSamples[lane]),
              tolDraws(a.tolDraws[lane]),
              tol(a.tol[lane]),
              thermal(a.thermal[lane]),
              driftCents(a.driftCents[lane]) {}
    };

    struct Voice
    {
        Voice(RankArrays& a, std::size_t voice) noexcept
            : ranks { Rank(a, voice * kNumRanks), Rank(a, voice * kNumRanks + 1) } {}

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
        // SH7 per-voice K hold (Phase 2.2, circuit engine).
        kas::HeldKeyNode keyNode;
        // KBC contour of the key last assigned to this channel (Phase 2.3):
        // per rank effective cutoff CV (HP, LP) and level gain contributions.
        int kbcKey = 36;
        float kbcCv[kNumRanks][2] {};
        float kbcLevelGain[kNumRanks] {};
        // TWS a/b/C nodes (Phase 2.4, circuit engine) and their present values.
        touch::TouchVoice touch;
        float touchA = 0.0f, touchB = 0.0f, touchC = 0.0f;
        // Phase 6: MPE upper-zone membership and unison detune (semitones).
        bool upperZone = false;
        float unisonOffset = 0.0f;
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
    int effectiveOversampling() const noexcept;
    // Rate-dependent setup for the current factor; allocation-free once
    // prepare() has reserved buffers for maximumOversamplingFactor(hostRate).
    void configureInternalRate();
    bool updateOversampling();
    // Circuit-engine control smoothing (Smoothing.h). The settings fields are
    // retargeted after each buildSettings and advanced once per internal
    // sample; performance controllers are smoothed for the duration of one
    // internal sample and their event-set targets restored afterwards.
    void retargetSmoothedSettings() noexcept;
    void beginSmoothedControls() noexcept;
    void endSmoothedControls() noexcept;
    float filterInitialTarget(int rank) const noexcept;
    float filterAttackTarget(int rank) const noexcept;
    void updateSourceReadouts(float sampleRate) noexcept;
    struct SampleContext
    {
        float sr, invSr;
        std::array<float, 2> pwmSine;
        bool glideActive;
        double glideCoef;
        float panelNoise;
        bool freeRunning, sharedNoise, smoothControls;
    };
    SampleContext tickGlobals(float externalModulation);
    void tickRanks(const SampleContext&, float& mixL, float& mixR);
    void mixBus(const SampleContext&, float& mixL, float& mixR);
    void tickOutput(const SampleContext&, float mixL, float mixR, float& outL, float& outR);
    void processInternalSample(float& outL, float& outR, float externalModulation);
    void startVoice(Voice& voice, int note, float velocity, int midiChannel);
    // Circuit engine: bring a silent free-running circuit exactly up to date
    // before its note-on (ContinuousVoiceCircuit.h catch-up).
    void wakeIdleVoice(Voice& voice) noexcept;
    void prepareWarmColdOscillators() noexcept;
    void refreshToleranceRatios() noexcept;
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
    bool noteMapped(int midiNote) const noexcept;
    void retuneSoundingVoices() noexcept;
    int unisonCount() const noexcept
    { return global.voiceMode == 2 ? 2 : global.voiceMode == 3 ? 4 : global.voiceMode == 4 ? 8 : 1; }
    bool isZoneMaster(const Voice& voice, int midiChannel) const noexcept;

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
    std::array<bool, kNumRanks> programmerActive {};
    std::array<memory_panels::Positions, kNumRanks> programmerPositions {};
    // Immutable per-selection maps; compiled only when an active address changes.
    std::array<source_factory::ToneControls, kNumRanks> sourceTones;
    std::array<source_factory::RankBoundary, kNumRanks> sourceBoundaries;
    std::array<source_factory::CutoffReadout, kNumRanks> sourceCutoff;
    source_factory::SharedReadout sourceShared;
    // Exact memoization of algebraic receiver networks; capacitor clocks still run.
    source_factory::BothRankDrives sourceReadoutDrives {};
    float sourceReadoutResonance = 0.0f;
    bool sourceReadoutCacheValid = false;
    std::array<std::array<float, kNumVoices>, kNumRanks> sourceFilterValues {};
    std::uint64_t sourceCompileCount = 0;
    ControlFrameStatus controlFrameStatus = ControlFrameStatus::accepted;

    RankArrays rankChains;
    std::array<Voice, kNumVoices> voices;
    uint64_t voiceCounter = 0;
    int nextVoice = 0;
    int lastNote = 60;
    float globalBend = 0.0f;
    float modWheel = 0.0f;
    float lastChannelPressure = 0.0f; // modern-layer matrix source only
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
    // Circuit engine cold oscillators after their warm-up (prepare time).
    std::array<ReconstructedOscillator::State, kNumVoices * kNumRanks> warmColdOscillators {};
    // TWS/TRG touch model (Phase 2.4): per-sample coefficients, the TRG5 bus
    // voltage (Newton warm start) and its single-key full-scale normalisation.
    touch::Coefficients touchCoefficients;
    double trg5BusVolts = 0.0, trg5FullScaleVolts = 1.0;
    // VOICE DISPERSION scale the tolerance ratios were last computed for.
    float toleranceScaleApplied = 0.0f;
    // Lever values the KBC contour contributions were last computed from.
    std::array<float, 4> refreshedContourLevers {};
    bool prepared = false;
    bool nonRealtime = false;
    SmootherBank settingsSmoothing;
    struct ControllerSmoothing
    {
        ControlSmoother bend, modWheel, ribbon;
        std::array<ControlSmoother, kNumVoices> pressure, mpeBend, timbre;
        std::array<uint64_t, kNumVoices> age {};
        float bendTarget = 0.0f, modWheelTarget = 0.0f, ribbonTarget = 0.0f;
        // Expression acts at its gain stage through the PRA I2/I3 optical cell
        // (ExpressionOptics.h); the wah cell reads the pedal target itself.
        float expressionGain = 1.0f;
        std::array<float, kNumVoices> pressureTarget {}, mpeBendTarget {}, timbreTarget {};
        bool snapPending = true;
    } controllerSmoothing;
    double controlSmoothing = 1.0;  // one-pole coefficient at the internal rate
    float subSpeedSmoothing = 0.0015f; // legacy 0.0015 as a time constant
    // Circuit-engine noise sources (NoiseSource.h) and listening output: the
    // LEFT/RIGHT jacks, the GENERAL mono jack, or the headphone amplifier fed
    // from the preamp outputs (HeadphoneAmpCircuit.h). Legacy uses none of them.
    CircuitNoiseSources circuitNoise;
    ExpressionOpticalCell expressionOptics;
    HeadphoneAmpCircuit phonesL, phonesR;
    int listeningOutput = 0;
    ListeningSample listeningSample;

    std::array<float, 2> pwmPhase {};
    Lfo subLfo;
    OriginalSubOscillator originalSub;
    ExternalModulationInterpolator externalInput;
    float originalSubSpeedSmoothed = 4.2f;
    float originalSubSmoothing = 0.0f;
    // Modern-layer LFOs: sine is the historical Lfo law; each has its own random stream.
    ModulationLfo modLfos[3] { ModulationLfo(0x6c8e9cf5u), ModulationLfo(0x3b9e5d21u), ModulationLfo(0xa1c4f00du) };
    float subValue = 0.0f;
    float lfoValues[3] {};
    float lfoPitch = 0.0f, lfoFilter = 0.0f, lfoAmp = 0.0f, lfoPwm = 0.0f, lfoPan = 0.0f;
    float subSpeedSmoothed = 4.2f;
    ExpMemo<double> glideExp;
    modern::ModernLayer modern;

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
    OversamplingDecimator decL, decR;
    Rng audioRng { 0x51c80u };
    Rng modRng { 0x9e3779b9u };

    float dcL = 0.0f, dcR = 0.0f;
    float dcInL = 0.0f, dcInR = 0.0f;
    float smoothedVolume = 0.62f;
    Adsr ringAdsr;
    bool ringKeyDown = false;
    int heldCount = 0;
    std::array<bool, 17> sustainPedals {}; // MIDI channels1..16; index0 unused

    // Three serial modern FX slots (src/core/FxSlot.h).
    std::array<fx::FxSlot, 3> fxSlots;
    float driftPhase = 0.0f;
    float voiceDrift[kNumVoices] {};

    std::array<HeldKey, kMaxHeldKeys> heldKeys {};
    // Phase 6 tuning tables, last played pitch (glide start), MPE zones.
    tuning::TuningTable scalaTable, mtsTable;
    float lastPitch = 60.0f;
    MpeZoneLayout mpeZones;
    float upperZoneBend = 0.0f;
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
