#pragma once

#include "Dsp.h"
#include "OriginalSubOscillator.h"
#include "ExternalModulationInterpolator.h"
#include "Params.h"
#include "PerformanceControls.h"

#include <array>
#include <cstdint>

namespace cs80
{

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
};

class Engine
{
public:
    Engine();

    void prepare(double hostSampleRate);
    void reset();
    void setParams(const ParamValues& values);
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

    // Optional separate host-rate mono modulation; never mixed directly into output.
    // Input/output storage must not overlap (plugin captures aliased host input).
    // Null means disconnected: reconstruction history is cleared on transition.
    void render(float* interleavedStereo, int numFrames, const float* externalMono = nullptr);

    int activeVoices() const noexcept;
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
    };
    // Offline/render-thread diagnostics for response tests and plotted traces.
    RankInfo getRankInfo(int voice, int rank) const;

private:
    static constexpr int kNumVoices = 8;
    static constexpr int kNumRanks = 2;
    static constexpr int kMaxHeldKeys = 32;
    // Nominal ideal C-path pole only; not measured complete touch circuitry.
    static constexpr double kNominalInitialPitchDecaySeconds = 0.050;

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
    };

    struct HeldKey
    {
        int note = 60;
        int midiChannel = 1;
        float velocity = 0.8f;
        uint64_t order = 0;
    };

    void buildSettings();
    void processInternalSample(float& outL, float& outR, float externalModulation);
    void startVoice(Voice& voice, int note, float velocity, int midiChannel);
    void releaseVoice(Voice& voice);
    void releaseKey(Voice& voice);
    // Footswitch gate fell: shorten already-released tails to their non-performance
    // release without restarting the envelope stage, retriggering or holding a note.
    void cancelSustainTails();
    bool isSustainPedalDown(int midiChannel) const noexcept;
    Voice* findVoiceForNote(int note, int midiChannel);
    int chooseVoice();

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

    // PRA EXP-WAH branch and GENERAL output trim. Smoothing prevents switch clicks;
    // the default EXP/HIGH state never touches these filters or the gain (exact 1).
    Svf wahL, wahR;
    float wahMix = 0.0f;
    float wahFrequency = kProvisionalWahLowHz;
    bool wahRunning = false;
    float wahSmoothing = 0.0f;
    float outputGain = 1.0f;
    float outputGainSmoothing = 0.0f;

    float ringPhase = 0.0f;
    float ringEnv = 0.0f;
    float ringFrequency = 0.0f;
    bool ringGate = false;
    bool anyVoiceWasActive = false;

    Chorus chorus;
    Reverb reverb;
    DelayLine fxL, fxR;
    OnePole fxDampL, fxDampR;
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
    Chorus fxChorus;
    Phaser phaser;
    float driftPhase = 0.0f;
    float voiceDrift[kNumVoices] {};

    std::array<HeldKey, kMaxHeldKeys> heldKeys {};
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
