#include "Engine.h"
#include "DriftSineExact.h"
#include "RingModulatorCircuit.h"

#include <algorithm>
#include <cmath>

namespace cs80
{

#if defined(CS80_DRIFT_SINE_SOURCE_WITNESS)
thread_local drift_opt::SourceWorkCounters drift_opt::sourceWork {};
#endif

namespace
{
// Every continuous control the circuit engine smooths, in ONE list (field order
// is the SmootherBank slot order). Absent on purpose: switches and choices,
// envelope times and levels (the RC envelopes already move continuously toward
// a new level), time controls (portamento, release, envelope scale) and the
// SUB speeds, which keep their own smoothing.
template <class Visit>
void visitSmoothedSettings(GlobalSettings& g, RankSettings* ranks, int rankCount, Visit&& visit)
{
    for (float* field : { &g.masterTune, &g.detuneCents, &g.mix, &g.brilliance, &g.resonance, &g.volume,
                          &g.initBend, &g.subExtLevel, &g.subVco, &g.subVcf, &g.subVca, &g.subAtVco,
                          &g.subAtVcf, &g.ringSpeed, &g.ringDepth, &g.ringAmount, &g.chorusSpeed,
                          &g.chorusDepth, &g.voiceDispersion, &g.drift, &g.analogNoise, &g.tuningRef,
                          &g.modWheelSub, &g.modWheelVcf, &g.kbdBrillianceLow, &g.kbdBrillianceHigh,
                          &g.kbdLevelLow, &g.kbdLevelHigh })
        visit(*field);
    for (int i = 0; i < 3; ++i)
    {
        visit(g.lfoRate[i]);
        visit(g.lfoDepth[i]);
        visit(g.fxMix[i]);
    }
    for (int rank = 0; rank < rankCount; ++rank)
    {
        auto& r = ranks[rank];
        for (float* field : { &r.sine, &r.noise, &r.pulseWidth, &r.pwmSpeed, &r.pwmDepth, &r.hpfEffectiveCv,
                              &r.hpfRes, &r.lpfEffectiveCv, &r.lpfRes, &r.vcfLevel, &r.vLevel, &r.initBrill,
                              &r.initLevel, &r.afterBrill, &r.afterLevel, &r.pan })
            visit(*field);
    }
}
} // namespace

Engine::Engine()
    : voices { Voice(rankChains, 0), Voice(rankChains, 1), Voice(rankChains, 2), Voice(rankChains, 3), Voice(rankChains, 4), Voice(rankChains, 5), Voice(rankChains, 6), Voice(rankChains, 7) }
{
    originalSubSmoothing = (float) (-std::expm1(-1.0 / (0.010 * internalRate)));
    wahSmoothing = (float) (-std::expm1(-1.0 / (0.012 * internalRate)));
    outputGainSmoothing = (float) (-std::expm1(-1.0 / (0.005 * internalRate)));
    initialPitchDecay = std::exp(-1.0 / (kNominalInitialPitchDecaySeconds * internalRate));
    idleStepSamples = voice_continuity::idleStepSamples(internalRate);
    idleTransientDecay = std::exp(-(double) idleStepSamples / (kNominalInitialPitchDecaySeconds * internalRate));
    touchCoefficients.prepare(internalRate);
    trg5FullScaleVolts = touch::busVolts(std::array<double, 1> { 1.0 }, touch::kFullScaleVolts, 64);
    setParams(params);
}

void Engine::prepare(double hostSampleRate)
{
    // Software resource/numeric guard, not an analog tuning assumption.
    hostRate = std::isfinite(hostSampleRate) && hostSampleRate >= 8000.0
               && hostSampleRate <= 768000.0 ? hostSampleRate : 48000.0;
    // Reserve every rate-sized buffer once for the largest selectable factor,
    // so a later quality or offline change re-prepares without allocating.
    // The final prepare below fully reinitializes them at the actual rate.
    {
        const float reserveRate = (float) (hostRate * (double) maximumOversamplingFactor(hostRate));
        chorus.prepare(reserveRate);
        bbdChorus.prepare(reserveRate);
        for (auto& slot : fxSlots)
            slot.prepare(reserveRate);
    }
    oversample = effectiveOversampling();
    configureInternalRate();
    prepared = true;

    Rng trimRng(0xC5801234u);
    for (int vi = 0; vi < kNumVoices; ++vi)
        for (int ch = 0; ch < kNumRanks; ++ch)
        {
            // Plan 2.5: fixed per-channel component draws (circuit engine).
            auto& r = voices[(size_t) vi].ranks[ch];
            r.tolDraws = tolerance::drawCard(vi * kNumRanks + ch);
            r.tol = {};
        }
    toleranceScaleApplied = 0.0f;
    for (auto& v : voices)
    {
        for (auto& r : v.ranks)
        {
            r.trimTune = trimRng.nextBipolar();
            r.trimCutoff = trimRng.nextBipolar();
            r.trimLevel = trimRng.nextBipolar();
            r.trimEnv = trimRng.nextBipolar();
            r.trimPan = trimRng.nextBipolar();
        }
        v.active = false;
        v.idleControlsPrimed = false;
        v.held = false;
        v.sustained = false;
        v.pedalHeld = false;
    }
    for (int i = 0; i < kNumVoices; ++i)
        voiceDrift[i] = (float) i * 0.73f;
    reset();
    // Resource changes retain the accepted source identity; public setParams
    // intentionally means PANEL and would discard it here.
    applyParams(params);
}

int Engine::effectiveOversampling() const noexcept
{
    return oversamplingFactor(oversamplingMode(params[P_oversampling]),
                              usesSourceModels(global.renderPolicy), nonRealtime, hostRate);
}

void Engine::configureInternalRate()
{
    internalRate = hostRate * (double) oversample;
    initialPitchDecay = std::exp(-1.0 / (kNominalInitialPitchDecaySeconds * internalRate));

    chorus.prepare((float) internalRate);
    bbdChorus.prepare((float) internalRate);
    for (auto& slot : fxSlots)
        slot.prepare((float) internalRate);
    decL.prepare(oversample);
    decR.prepare(oversample);
    externalInput.prepare((float) hostRate, oversample);
    // Engine rate guards imply a valid 16kHz..1.536MHz circuit rate. Profile
    // defaults are documented engineering inferences, not measured calibration.
    praWahL.prepare(internalRate, PraWahCircuit::Profile{}, 1.0);
    praWahR.prepare(internalRate, PraWahCircuit::Profile{}, 1.0);
    outputCircuit.prepare(internalRate, InferredSymmetricOutputProfile{},
                          global.outputHigh ? OutputTapCircuit::Mode::high : OutputTapCircuit::Mode::low);
    // Explicitly provisional10ms rate smoothing, sample-rate independent.
    originalSubSmoothing = (float) (-std::expm1(-1.0 / (0.010 * internalRate)));
    // Explicitly provisional switch/pedal smoothing, sample-rate independent.
    wahSmoothing = (float) (-std::expm1(-1.0 / (0.012 * internalRate)));
    outputGainSmoothing = (float) (-std::expm1(-1.0 / (0.005 * internalRate)));
    // Circuit-engine control smoothing (Smoothing.h), in time, not per sample.
    controlSmoothing = smoothingCoefficient(kControlSmoothingSeconds, internalRate);
    settingsSmoothing.setCoefficient(controlSmoothing);
    subSpeedSmoothing = (float) smoothingCoefficient(legacyPerSampleSeconds(0.0015), internalRate);
    circuitNoise.prepare(internalRate);
    expressionOptics.prepare(internalRate);
    phonesL.prepare(internalRate, HeadphoneAmpCircuit::Profile {});
    phonesR.prepare(internalRate, HeadphoneAmpCircuit::Profile {});
    // Modern layer (ws5), free-running idle path and KAS scan (ws3), and the
    // circuit-engine voice (ws1): every rate-dependent coefficient. The
    // oscillator basis pool is fixed and static, so a render-thread factor
    // change acquires a shared basis without allocating (bounded mutex, the
    // pool is only contended by another instance preparing).
    modern.prepare(internalRate);
    idleStepSamples = voice_continuity::idleStepSamples(internalRate);
    idleTransientDecay = std::exp(-(double) idleStepSamples / (kNominalInitialPitchDecaySeconds * internalRate));
    touchCoefficients.prepare(internalRate);
    scanner.prepare(internalRate);
    for (int vi = 0; vi < kNumVoices; ++vi)
        for (int ch = 0; ch < kNumRanks; ++ch)
        {
            auto& r = voices[(size_t) vi].ranks[ch];
            (void) r.wsc.prepare(internalRate, OscillatorNominalProfile {});
            r.vca.prepare(internalRate);
            r.thermal.prepare(internalRate, vi * kNumRanks + ch);   // plan 2.5; warm-up restarts here
            r.driftCents = 0.0f;
        }
    prepareWarmColdOscillators();
}

// True when the effective factor changed and the rate-dependent state was
// re-prepared; the caller then resets like a render-policy change.
bool Engine::updateOversampling()
{
    if (!prepared)
        return false;
    const int factor = effectiveOversampling();
    if (factor == oversample)
        return false;
    oversample = factor;
    configureInternalRate();
    return true;
}

void Engine::setNonRealtime(bool offline) noexcept
{
    if (offline == nonRealtime)
        return;
    nonRealtime = offline;
    if (updateOversampling())
        reset();
}

void Engine::refreshToleranceRatios() noexcept
{
    // Plan 2.5: identity outside the Source Performance policy (Legacy keeps its
    // own trims; PURE CS-80 masks VOICE DISPERSION to zero anyway).
    const bool circuitVariation = global.renderPolicy == RenderPolicy::sourcePerformance;
    const float scale = circuitVariation ? global.voiceDispersion / tolerance::kDefaultDispersion : 0.0f;
    if (scale == toleranceScaleApplied)
        return;
    for (auto& voice : voices)
        for (auto& rank : voice.ranks)
            rank.tol = tolerance::ratios(rank.tolDraws, scale);
    toleranceScaleApplied = scale;
}

void Engine::prepareWarmColdOscillators() noexcept
{
    // Circuit engine cold state (Phase 2.1, declared): the instrument has been
    // running since power-on, so each cold oscillator's 0.1 s coupling memories
    // are in their running state, not discharged. Zero prehistory instead gave
    // the first chord after a load or policy change a decaying DC step of the
    // saw pin (up to +1.4 dB at the jack on a dense PANEL chord). The same
    // closed-form idle law runs the warm-up; its result is cached per rate.
    Rng coldPhases(kColdPhaseSeed);
    const double hz = OscillatorNominalProfile {}.referenceHzNominal;
    for (int vi = 0; vi < kNumVoices; ++vi)
        for (int ch = 0; ch < kNumRanks; ++ch)
        {
            auto& wsc = voices[(size_t) vi].ranks[ch].wsc;
            wsc.reset(coldPhases.nextUnipolar(), hz);
            (void) wsc.advanceIdle(kColdWarmUpSeconds, hz, WscControls {});
            warmColdOscillators[(size_t) (vi * kNumRanks + ch)] = wsc.getState();
        }
}

void Engine::reset()
{
    sourceReadoutCacheValid = false;
    // Circuit engine: every circuit starts cold but free-running (Phase 2.1).
    const bool freeRunning = freeRunningVoices();
    Rng coldPhases(kColdPhaseSeed);
    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[(size_t) vi];
        v.active = false;
        v.idleControlsPrimed = false;
        v.held = false;
        v.sustained = false;
        v.pedalHeld = false;
        v.note = 60;
        v.age = 0;
        v.initialPitchTransient = 0.0;
        for (auto& r : v.ranks)
        {
            // Circuit engine (Phase 2.1): declared distinct cold phases, so a
            // first chord is not phase-locked. Legacy: zero, as before.
            const float coldPhase = freeRunning ? coldPhases.nextUnipolar() : 0.0f;
            r.osc.reset(coldPhase);
            r.wsc.reset(coldPhase, OscillatorNominalProfile {}.referenceHzNominal);
            if (freeRunning)
                r.wsc.restoreState(warmColdOscillators[(size_t) (vi * kNumRanks + (&r - v.ranks))]);
            r.hpf.reset();
            r.lpf.reset();
            r.hpStage.reset();
            r.lpStage.reset();
            // Idle output is connected too, so the explicit cold reset must
            // clear old coupling charge before any later note can clock it.
            if (freeRunning) r.vca.reset(0.0f);
            r.vcaCold = true;
            r.fenv.reset();
            if (freeRunning) r.fenv.selectInitialWhileIdle();
            r.aenv.reset();
            r.idle = {};
            r.idleRefs = {};
        }
        v.idleSteps = 0;
        v.keyNode.track();
        v.touch.reset();
        v.touchA = v.touchB = v.touchC = 0.0f;
        // This counter is physical elapsed time, not a scheduling offset.
        // The first-step threshold in tickRanks staggers work without inventing time.
        v.idleClock = 0;
        idleNoise[(size_t) vi] = Rng(kIdleNoiseSeed + (std::uint32_t) vi);
        if (freeRunning)
        {
            // Every field the idle path reads is defined for a cold circuit.
            v.note = kColdKeyNote;
            v.midiChannel = 1;
            v.glidePitch = v.targetPitch = (float) kColdKeyNote;
            v.velocity = v.pressure = v.timbre = v.mpeBend = 0.0f;
            v.ribbonRatio = 1.0f;
            v.contour = keyboardContourWeights(kColdKeyNote);
            v.kbcKey = kbc::physicalKey(kColdKeyNote);
        }
    }
    heldCount = 0;
    sustainPedals.fill(false);
    scanner.reset();
    unassignedCount = 0;
    trg5BusVolts = 0.0;
    ringKeyDown = false;
    ringAdsr.reset();
    ringPhase = 0.0f;
    ringEnv = 0.0f;
    ringFrequency = 0.0f;
    pwmPhase = { 0.0f, 0.0f };
    subLfo.reset(0.0f);
    originalSub.reset();
    externalInput.reset();
    originalSubSpeedSmoothed = global.subOriginalSpeed;
    for (auto& l : modLfos)
        l.reset(0.0f);
    chorus.reset();
    bbdChorus.reset();
    for (auto& slot : fxSlots)
        slot.resetAudio();
    decL.reset();
    decR.reset();
    wahL.reset();
    wahR.reset();
    // reset() also restores expression to 1 below: do not start an optical
    // sweep from the previous pedal value or let a bypassed tail survive.
    praWahL.reset(1.0);
    praWahR.reset(1.0);
    wahMix = global.pedalWah ? 1.0f : 0.0f;
    wahRunning = false;
    outputCircuit.reset(global.outputHigh ? OutputTapCircuit::Mode::high : OutputTapCircuit::Mode::low);
    outputNodes = {};
    outputGain = outputLevelGain(global.outputHigh);
    lastNote = 60;
    lastPitch = 60.0f;
    upperZoneBend = 0.0f;
    nextVoice = 0;
    globalBend = 0.0f;
    modWheel = 0.0f;
    lastChannelPressure = 0.0f;
    expression = 1.0f;
    ribbon = 0.0f;
    smoothedVolume = global.volume;
    modern.reset();
    heldKeyCount = 0;
    arpSequenceLength = 0;
    arpIndex = 0;
    arpDirection = 1;
    arpVoice = -1;
    arpCurrentNote = -1;
    arpSourceOrder = 0;
    arpGateOpen = false;
    arpWasOn = false;
    arpClock = 0.0f;
    arpGateTime = 0.0f;
    dcL = dcR = dcInL = dcInR = 0.0f;
    refreshKeyboardContour();
    // Smoothed controls start at their targets again (no glide after a reset).
    settingsSmoothing.requestSnap();
    controllerSmoothing.snapPending = true;
    circuitNoise.reset();
    phonesL.reset();
    phonesR.reset();
    listeningSample = {};
}

void Engine::setParams(const ParamValues& values)
{
    hardwareSources = {13, 13};
    sourceRoutes.fill(source_factory::Route::panel_or_memory);
    sourceActive.fill(false);
    programmerActive.fill(false);
    sourceCutoff = {};
    sourceShared = {};
    controlFrameStatus = ControlFrameStatus::accepted;
    applyParams(values);
}

bool Engine::setControlFrame(const ControlFrame& frame)
{
    if (frame.modelVersion != source_factory::modelVersion)
    {
        controlFrameStatus = ControlFrameStatus::unsupportedModel;
        return false;
    }
    for (int selector : frame.hardwareSources)
        if (selector < 0 || selector > 13)
        {
            controlFrameStatus = ControlFrameStatus::invalidSelector;
            return false;
        }
    for (float value : frame.effectiveValues.v)
        if (!std::isfinite(value))
        {
            controlFrameStatus = ControlFrameStatus::nonfiniteParameter;
            return false;
        }

    const bool sourceModels = usesSourceModels(renderPolicy(frame.effectiveValues[P_parityMode],
                                                            frame.effectiveValues[P_sourcePerformance]));
    std::array<bool, kNumRanks> programmer {};
    for (int rank = 0; rank < kNumRanks; ++rank)
    {
        programmer[rank] = sourceModels && frame.hardwareSources[rank] >= 11 && frame.hasProgrammerPositions[rank];
        if (programmer[rank] && !memory_panels::valid(frame.programmerPositions[rank]))
        {
            controlFrameStatus = ControlFrameStatus::invalidProgrammerPosition;
            return false;
        }
    }

    // Validate/compile both ranks before publishing any part of the transaction.
    // Originally incomplete columns use the separate, versioned inference
    // manifest. Their source_inferred identity and original unknown masks remain
    // explicit; PANEL/memory/off routes never reuse a stale source adapter.
    auto tones = sourceTones;
    auto routes = sourceRoutes;
    std::uint64_t compilations = 0;
    for (int rank = 0; rank < kNumRanks; ++rank)
    {
        const int selector = frame.hardwareSources[rank];
        if (selector != hardwareSources[rank] || sourceModels != usesSourceModels(global.renderPolicy))
            routes[rank] = source_factory::route(rank, selector, sourceModels);
        if (programmer[rank])
        {
            if (!programmerActive[rank] || !tones[rank].compiled || tones[rank].source.selector != selector
                || programmerPositions[rank] != frame.programmerPositions[rank])
            {
                if (!memory_panels::compileTone(rank, selector, frame.programmerPositions[rank], tones[rank]))
                {
                    controlFrameStatus = ControlFrameStatus::compileFailure;
                    return false;
                }
                ++compilations;
            }
        }
        else if (source_factory::isSourceDriven(routes[rank])
            && (!tones[rank].compiled || tones[rank].source.selector != selector))
        {
            if (!source_factory::compileTone(rank, selector, tones[rank]))
            {
                controlFrameStatus = ControlFrameStatus::compileFailure;
                return false;
            }
            ++compilations;
        }
    }
    hardwareSources = frame.hardwareSources;
    sourceRoutes = routes;
    sourceTones = tones;
    sourceCompileCount += compilations;
    programmerActive = programmer;
    programmerPositions = frame.programmerPositions;
    for (int rank = 0; rank < kNumRanks; ++rank)
        sourceActive[rank] = programmer[rank] || source_factory::isSourceDriven(routes[rank]);
    sourceCutoff = {};
    sourceShared = {};
    applyParams(frame.effectiveValues);
    controlFrameStatus = ControlFrameStatus::accepted;
    return true;
}

void Engine::applyParams(const ParamValues& values)
{
    sourceReadoutCacheValid = false;
    params = values;
    // Sanitize once at the core boundary before integer casts and DSP laws.
    for (int i = 0; i < P_numParams; ++i)
    {
        const auto& def = paramDef(i);
        const float value = params[i];
        params[i] = std::isfinite(value) ? clampf(value, def.min, def.max) : def.def;
    }
    const auto previousPolicy = global.renderPolicy;
    const int previousTuning = global.tuningMode;
    const bool wasArp = arpWasOn;
    const bool hadSustainGate = global.sustainGate;
    buildSettings();
    // A selector event changes the physical load for the next interval, without
    // resetting capacitor voltage, previous source or inferred readout smoothing.
    outputCircuit.setMode(global.outputHigh ? OutputTapCircuit::Mode::high : OutputTapCircuit::Mode::low);
    // The quality mode, or a policy change that moves it, re-prepares the
    // rate-dependent state into buffers reserved by prepare(): no allocation.
    const bool rateChanged = updateOversampling();
    // Effective policy changes clear notes/controllers and effect histories: no
    // old numerical EG state, MIDI holds or FX may leak across model policies.
    // A retained source-performance switch change under isolation is inert.
    // No allocation occurs on this RT boundary.
    if (previousPolicy != global.renderPolicy || rateChanged)
    {
        reset();
        // Legacy reset left these OnePole histories intact. Clear only for
        // mode transitions to preserve mode-off historical rendering.
        for (auto& slot : fxSlots)
            slot.resetDamping();
    }
    // Circuit engine: the new settings become smoothing targets; the fields keep
    // the running values (exactly the targets until audio has been rendered).
    if (usesSourceModels(global.renderPolicy))
        retargetSmoothedSettings();
    listeningOutput = std::clamp((int) std::lround(params[P_outputJack]), 0, 2);

    // Gate edges act when parameters are applied, not by per-sample polling, so
    // the outcome does not depend on how the host partitions a render.
    if (hadSustainGate && ! global.sustainGate)
        cancelSustainTails();

    // Physical timing ports remain connected while a voice sounds/releases.
    // Refresh in either opt-in source policy: legacy dynamic automation is unchanged.
    // This retains the existing provisional timing/bend/completion law, not a
    // calibrated IC model. Live level selection is a state-continuous engineering
    // inference; see docs/ENVELOPE_TARGET_TRANSITION_REVIEW.md.
    if (usesSourceModels(global.renderPolicy))
    {
        const float performanceRelease = global.sustainGate ? global.masterSustain * 10.0f : 0.0f;
        for (auto& voice : voices)
        {
            for (int ch = 0; ch < kNumRanks; ++ch)
            {
                // Source TP1 remains electrically connected after its VCA has
                // become idle. Its target/timing ports therefore stay live for
                // all eight circuits, not only currently audible voices. With
                // free-running circuits (Phase 2.1) a silent PANEL/MEMORY filter
                // EG is clocked too, so its ports are live as well.
                auto& rank = voice.ranks[ch];
                const auto& settings = rankSettings[ch];
                if (voice.active) rank.aenv.setSustain(settings.vS);
                rank.fenv.setLevels(filterInitialTarget(ch), filterAttackTarget(ch));
                if (voice.active)
                    rank.aenv.setTimes(std::max(0.001f, settings.vA * global.envTimeScale * rank.tol.ampEnvTime),
                                       std::max(0.001f, settings.vD * global.envTimeScale * rank.tol.ampEnvTime),
                                       std::max({settings.vR, global.keyRelease, performanceRelease})
                                           * global.envTimeScale * rank.tol.ampEnvTime);
                rank.fenv.setTimes(std::max(0.001f, settings.fA * global.envTimeScale * rank.tol.filterEnvTime),
                                   std::max(0.001f, settings.fD * global.envTimeScale * rank.tol.filterEnvTime),
                                   std::max({settings.fR, global.keyRelease, performanceRelease})
                                       * global.envTimeScale * rank.tol.filterEnvTime);
            }
        }
    }

    refreshKeyboardContour();
    refreshToleranceRatios();
    if (global.tuningMode != previousTuning)
        retuneSoundingVoices();

    // Toggling the arpeggiator is a performance action, not a parameter that
    // should leave old voices hanging. Enabling it clears the directly played
    // voices; disabling it stops the running pattern.
    if (global.arpOn != wasArp)
    {
        if (global.arpOn)
        {
            for (auto& v : voices)
                releaseVoice(v);
            heldKeyCount = 0;
            arpClock = 1.0e9f;
        }
        else
        {
            stopArpVoice();
            heldKeyCount = 0;
            arpClock = 0.0f;
        }
        arpWasOn = global.arpOn;
    }
}

void Engine::buildSettings()
{
    auto p = [this](int i) { return params.v[(size_t) i]; };

    global.parityMode = p(P_parityMode) >= 0.5f;
    global.renderPolicy = renderPolicy(p(P_parityMode), p(P_sourcePerformance));
    global.masterTune = p(P_masterTune);
    global.detuneCents = p(P_detune);
    global.mix = p(P_mix);
    global.brilliance = p(P_brilliance);
    global.resonance = p(P_resonance);
    global.masterSustain = p(P_masterSustain);
    global.volume = p(P_volume);
    global.initBend = p(P_initBend);
    global.portamento = p(P_portamento);
    global.glideMode = (int) std::lround(p(P_glideMode));
    global.sustainMode = (int) std::lround(p(P_sustainMode));
    global.keyRelease = p(P_keyRelease);
    global.subWave = (int) std::lround(p(P_subWave));
    global.subSpeed = p(P_subSpeed);
    global.subOriginalMode = p(P_subOriginalMode) >= 0.5f;
    global.subOriginalFunction = (int) std::lround(p(P_subOriginalFunction));
    global.subOriginalSpeed = p(P_subOriginalSpeed);
    global.subExtLevel = p(P_subExtLevel);
    global.subVco = p(P_subVco);
    global.subVcf = p(P_subVcf);
    global.subVca = p(P_subVca);
    global.subAtSpeed = p(P_subAtSpeed);
    global.subAtVco = p(P_subAtVco);
    global.subAtVcf = p(P_subAtVcf);
    global.ringSpeed = p(P_ringSpeed);
    global.ringDepth = p(P_ringDepth);
    global.ringAttack = p(P_ringAttack);
    global.ringDecay = p(P_ringDecay);
    global.ringAmount = p(P_ringAmount);
    global.chorusOn = p(P_chorusOn) >= 0.5f;
    global.chorusTremolo = p(P_chorusMode) >= 0.5f;
    global.chorusTabs = static_cast<ChorusTabMode>((int) std::lround(p(P_chorusTabs)));
    global.chorusSpeed = p(P_chorusSpeed);
    global.chorusDepth = p(P_chorusDepth);
    global.stereo = p(P_outputMode) >= 0.5f;
    global.voiceDispersion = p(P_voiceDispersion);
    global.drift = p(P_drift);
    global.driftModel = p(P_driftModel) >= 0.5f ? 1 : 0;
    global.analogNoise = p(P_analogNoise);
    global.mpe = p(P_mpeOn) >= 0.5f;
    global.pitchBendRange = p(P_pitchBendRange);
    global.mpeBendRange = p(P_mpeBendRange);
    global.tuningRef = p(P_tuningRef);
    global.modWheelSub = p(P_modWheelSub);
    global.modWheelVcf = p(P_modWheelVcf);
    global.kbdBrillianceLow = p(P_kbdBrillianceLow);
    global.kbdBrillianceHigh = p(P_kbdBrillianceHigh);
    global.kbdLevelLow = p(P_kbdLevelLow);
    global.kbdLevelHigh = p(P_kbdLevelHigh);
    {
        const bool plugged = p(P_footswitchPlugged) >= 0.5f;
        const bool pressed = p(P_footswitchPressed) >= 0.5f;
        global.sustainGate = footswitchGate(p(P_sustainAssign) >= 0.5f, plugged, pressed);
        global.glideGate = footswitchGate(p(P_glideAssign) >= 0.5f, plugged, pressed);
    }
    global.pedalWah = p(P_footMode) >= 0.5f;
    global.outputHigh = p(P_outputLevel) >= 0.5f;
    global.lfoRate[0] = p(P_lfo1Rate);
    global.lfoRate[1] = p(P_lfo2Rate);
    global.lfoRate[2] = p(P_lfo3Rate);
    global.lfoDepth[0] = p(P_lfo1Depth);
    global.lfoDepth[1] = p(P_lfo2Depth);
    global.lfoDepth[2] = p(P_lfo3Depth);
    global.lfoDest[0] = (int) std::lround(p(P_lfo1Dest));
    global.lfoDest[1] = (int) std::lround(p(P_lfo2Dest));
    global.lfoDest[2] = (int) std::lround(p(P_lfo3Dest));
    global.lfoWave[0] = std::clamp((int) std::lround(p(P_lfo1Wave)), 0, kLfoWaveCount - 1);
    global.lfoWave[1] = std::clamp((int) std::lround(p(P_lfo2Wave)), 0, kLfoWaveCount - 1);
    global.lfoWave[2] = std::clamp((int) std::lround(p(P_lfo3Wave)), 0, kLfoWaveCount - 1);
    global.fxType[0] = (int) std::lround(p(P_fx1Type));
    global.fxType[1] = (int) std::lround(p(P_fx2Type));
    global.fxType[2] = (int) std::lround(p(P_fx3Type));
    global.fxMix[0] = p(P_fx1Mix);
    global.fxMix[1] = p(P_fx2Mix);
    global.fxMix[2] = p(P_fx3Mix);
    for (int i = 0; i < 3; ++i)
    {
        const int base = P_fx1Model + i * (P_fx2Model - P_fx1Model);
        global.fxModel[i] = std::clamp((int) std::lround(p(base)), 0, fx::kModelCount - 1);
        global.fxParamA[i] = p(base + 1);
        global.fxParamB[i] = p(base + 2);
        global.fxOversampling[i] = std::clamp((int) std::lround(p(base + 3)), 0, fx::Oversampler::kMaxStages);
    }
    global.fxGainComp = p(P_fxGainCompensation) >= 0.5f;
    global.macro[0] = p(P_macro1);
    global.macro[1] = p(P_macro2);
    global.macro[2] = p(P_macro3);
    global.macro[3] = p(P_macro4);
    global.arpOn = p(P_arpOn) >= 0.5f;
    global.arpMode = std::clamp((int) std::lround(p(P_arpMode)), 0, 4);
    global.arpRate = std::max(0.1f, p(P_arpRate));
    global.arpOctaves = std::clamp((int) std::lround(p(P_arpOctaves)), 1, 4);
    global.kasScanTiming = p(P_kasScanTiming) >= 0.5f;
    global.velocityCurve = p(P_velocityCurve);
    global.pressureCurve = p(P_pressureCurve);
    global.pressureThreshold = p(P_pressureThreshold);
    global.ribbonCcEnabled = p(P_ribbonCcEnable) >= 0.5f;
    global.ribbonCc = std::clamp((int) std::lround(p(P_ribbonCc)), 1, 119);
    global.tuningMode = std::clamp((int) std::lround(p(P_tuningMode)), 0, 2);
    global.voiceMode = std::clamp((int) std::lround(p(P_voiceMode)), 0, 4);
    global.unisonDetune = p(P_unisonDetune);

    // Effective settings only: retain stored automation/patch values unchanged.
    // Zero variation is an uncalibrated nominal fixture, NOT measured tolerances.
    if (global.parityMode)
    {
        global.keyRelease = 0.0f;
        global.subOriginalMode = true;
        global.ringSpeed = std::min(global.ringSpeed, 200.0f);
        global.stereo = true;
        global.voiceDispersion = global.drift = global.analogNoise = 0.0f;
        global.mpe = global.arpOn = false;
        global.pitchBendRange = global.mpeBendRange = 0.0f;
        global.tuningRef = 440.0f; // nominal reference, not an identified unit's tuning
        global.modWheelSub = global.modWheelVcf = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            global.lfoDepth[i] = 0.0f;
            global.lfoDest[i] = 0;
            global.lfoWave[i] = 0;
            global.fxType[i] = 0;
            global.fxMix[i] = 0.0f;
            global.fxModel[i] = 0;
        }
        global.fxGainComp = false;
        for (float& macro : global.macro)
            macro = 0.5f;
        // Modern-layer tuning and voice modes are inert in PURE CS-80. The KAS
        // scan option, controller curves and the ribbon CC stay: they model an
        // original circuit or adapt the controller, not add an instrument feature.
        global.tuningMode = 0;
        global.voiceMode = 0;
    }

    // Modern modulation layer: rates, matrix amounts and response curves. PURE CS-80
    // disables it whatever the parameters say.
    {
        modern::ModernControls controls;
        controls.enabled = ! isPureCs80(global.renderPolicy);
        for (int m = 0; m < modern::kMsegCount; ++m)
            controls.msegRate[(std::size_t) m] = std::exp2(p(P_mseg1Rate + m)); // octaves -> speed
        for (int s = 0; s < modern::kModSlots; ++s)
            controls.amount[(std::size_t) s] = p(P_mod1Amount + s);
        controls.velocityCurve = p(P_modVelocityCurve);
        controls.pressureCurve = p(P_modPressureCurve);
        controls.wheelCurve = p(P_modWheelCurve);
        controls.keyCurve = p(P_modKeyCurve);
        modern.setControls(controls);
    }

    // Macro layer. 0.5 is neutral, so a patch sounds exactly like its panel
    // settings until the player deliberately moves a macro.
    const float brightnessMacro = (global.macro[0] - 0.5f) * 2.0f;
    const float motionMacro = (global.macro[1] - 0.5f) * 2.0f;
    const float widthMacro = (global.macro[2] - 0.5f) * 2.0f;
    const float spaceMacro = (global.macro[3] - 0.5f) * 2.0f;
    global.brilliance = clampf(global.brilliance + brightnessMacro * 0.75f, -3.0f, 3.0f);
    global.envTimeScale = std::pow(2.0f, -motionMacro * 1.1f);
    global.detuneCents = clampf(global.detuneCents + widthMacro * 12.0f, 0.0f, 50.0f);
    global.voiceDispersion = clampf(global.voiceDispersion + widthMacro * 0.22f, 0.0f, 1.0f);
    global.chorusDepth = clampf(global.chorusDepth + widthMacro * 0.25f, 0.0f, 1.0f);
    global.ringAmount = clampf(global.ringAmount + spaceMacro * 0.22f, 0.0f, 1.0f);
    global.subVcf = clampf(global.subVcf + spaceMacro * 0.20f, 0.0f, 1.0f);
    global.subVca = clampf(global.subVca + spaceMacro * 0.12f, 0.0f, 1.0f);
    for (int i = 0; i < 3; ++i)
        global.lfoDepth[i] = clampf(global.lfoDepth[i] + std::max(0.0f, motionMacro) * 0.18f, 0.0f, 1.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        auto rp = [&](int idx) { return params.v[(size_t) chParam(ch, idx)]; };
        auto& r = rankSettings[ch];
        r.saw = rp(CP_saw) >= 0.5f;
        r.pulse = rp(CP_pulse) >= 0.5f;
        r.sine = rp(CP_sine);
        r.noise = rp(CP_noise);
        r.pulseWidth = rp(CP_pw);
        r.pwmSpeed = rp(CP_pwmSpeed);
        r.pwmDepth = rp(CP_pwmDepth);
        r.feet = (int) std::lround(rp(CP_feet));
        r.hpfCutoff = rp(CP_hpfCutoff);
        r.hpfEffectiveCv = filterEffectiveCv(r.hpfCutoff);
        r.hpfRes = rp(CP_hpfRes);
        r.hpfOn = rp(CP_hpfOn) >= 0.5f;
        r.lpfCutoff = rp(CP_lpfCutoff);
        r.lpfEffectiveCv = filterEffectiveCv(r.lpfCutoff);
        r.lpfRes = rp(CP_lpfRes);
        r.lpfOn = rp(CP_lpfOn) >= 0.5f;
        r.fIl = rp(CP_fIl);
        r.fAl = rp(CP_fAl);
        r.fA = rp(CP_fA);
        r.fD = rp(CP_fD);
        r.fR = rp(CP_fR);
        r.vcfLevel = rp(CP_vcfLevel);
        r.vA = rp(CP_vA);
        r.vD = rp(CP_vD);
        r.vS = rp(CP_vS);
        r.vR = rp(CP_vR);
        r.vLevel = rp(CP_vLevel);
        r.initBrill = rp(CP_initBrill);
        r.initLevel = rp(CP_initLevel);
        r.afterBrill = rp(CP_afterBrill);
        r.afterLevel = rp(CP_afterLevel);
        r.pan = rp(CP_pan);
        if (global.parityMode)
        {
            r.hpfOn = r.lpfOn = true;
            r.pan = 0.0f; // neutral fixture; physical output routing remains W8
        }
        if (sourceActive[ch])
        {
            const auto& tone = sourceTones[ch];
            r.saw = tone.wsc.sawEnabled;
            r.pulse = tone.wsc.pulseEnabled;
            // Named panel-duty bridge only. This increment does not integrate
            // the separate oscillator model or pretend PW is the WSC pin law.
            r.pulseWidth = static_cast<float>(tone.dutyProxy);
            r.pwmSpeed = static_cast<float>(tone.pwmRateHz);
            r.pwmDepth = static_cast<float>(tone.pwmDepth);
            r.noise = static_cast<float>(tone.noiseGain);
            r.sine = static_cast<float>(tone.sinePathGain);
            r.vcfLevel = static_cast<float>(tone.filterPathGain);
            r.fA = static_cast<float>(tone.filterEnvelope.attackSeconds);
            r.fD = static_cast<float>(tone.filterEnvelope.decaySeconds);
            r.fR = static_cast<float>(tone.filterEnvelope.releaseSeconds);
            r.vA = static_cast<float>(tone.amplitudeEnvelope.attackSeconds);
            r.vD = static_cast<float>(tone.amplitudeEnvelope.decaySeconds);
            r.vR = static_cast<float>(tone.amplitudeEnvelope.releaseSeconds);
            r.vS = static_cast<float>(tone.amplitudeEnvelope.sustainRatio);
            sourceBoundaries[ch] = tone.boundary;
        }
        else
        {
            // Preserve the other rank's eight Q and LC receiver ports in a
            // mixed frame. These nominal stiff sources do not change its audio.
            source_factory::bridgeBoundary(r.hpfRes, r.lpfRes, r.vLevel,
                                           r.initLevel, r.afterLevel, sourceBoundaries[ch]);
        }
    }
}

float Engine::filterInitialTarget(int rank) const noexcept
{
    return sourceActive[rank]
        ? static_cast<float>(source_factory::envelopeStateFromVolts(sourceTones[rank].filterEnvelope.initialVolts))
        : filterInitialOffset(rankSettings[rank].fIl);
}

float Engine::filterAttackTarget(int rank) const noexcept
{
    return sourceActive[rank]
        ? static_cast<float>(source_factory::envelopeStateFromVolts(sourceTones[rank].filterEnvelope.attackVolts))
        : filterAttackOffset(rankSettings[rank].fAl);
}

void Engine::updateSourceReadouts(float sampleRate) noexcept
{
    source_factory::BothRankDrives drives {};
    for (int rank = 0; rank < kNumRanks; ++rank)
        for (int voice = 0; voice < kNumVoices; ++voice)
        {
            auto& v = voices[static_cast<std::size_t>(voice)];
            auto& fenv = v.ranks[rank].fenv;
            // The common FH/FL sources see all eight receivers at the SAME
            // interval. Tick each source-selected envelope here exactly once,
            // before any voice is rendered; never copy a one-voice solution.
            if (sourceActive[rank] && !v.active && !v.held && !v.pedalHeld)
            {
                // A prior legacy hard panic may have retired audio without an
                // EG gate event. Reconcile at the first control interval, NOT
                // during source selection, so zero-frame continuity survives.
                // Never restart an already-releasing/completed control tail.
                const auto stage = fenv.getState().stage;
                if (stage == FilterEnv::Stage::Attack || stage == FilterEnv::Stage::Decay
                    || stage == FilterEnv::Stage::Sustain)
                    fenv.noteOff();
            }
            const float z = sourceActive[rank]
                ? fenv.tick(sampleRate, true, true) : fenv.getValue();
            sourceFilterValues[rank][voice] = z;
            // Continuous control-state inference: a source-selected filter EG
            // continues to relax after VCA retirement, avoiding a permanently
            // frozen common-bus bias. Cold released Idle also follows IL.
            // Phase 2.4: the TRG banks see every channel's held TWS b and
            // smoothed a, silent channels included (b holds its last strike; a
            // relaxes once the key is up). Formerly inactive touch was zero.
            drives[rank][voice] = {z, v.touchB, v.touchA};
        }
    // These networks are memoryless algebra, not time integration. After a
    // control tail settles, identical inputs imply identical readouts. Keep
    // every EG/TWS clock above running; invalidate on any parameter/selection
    // transaction and reset. Equality is exact (no threshold or decimation).
    bool unchanged = sourceReadoutCacheValid && global.resonance == sourceReadoutResonance;
    for (int rank = 0; unchanged && rank < kNumRanks; ++rank)
        for (int voice = 0; unchanged && voice < kNumVoices; ++voice)
        {
            const auto& a = drives[rank][voice];
            const auto& b = sourceReadoutDrives[rank][voice];
            unchanged = a.filterEnvelopeZ == b.filterEnvelopeZ
                     && a.velocity == b.velocity && a.pressure == b.pressure;
        }
    if (unchanged) return;
    sourceReadoutDrives = drives;
    sourceReadoutResonance = global.resonance;
    sourceReadoutCacheValid = true;
    for (int rank = 0; rank < kNumRanks; ++rank)
        if (sourceActive[rank])
            source_factory::evaluateCutoff(sourceTones[rank], drives[rank], sourceCutoff[rank]);
    // One 32-Q/16-LC network. Shared RES/AM/KB use the documented nominal
    // stiff boundaries; no independent per-voice or per-rank master copies.
    source_factory::evaluateShared(sourceBoundaries, global.resonance, drives, sourceShared);
}

int Engine::chooseVoice()
{
    // Yamaha owner's manual p43: cyclic assignment of eight voice circuits;
    // an additional held key is ignored, not an oldest-held voice steal.
    for (int offset = 0; offset < kNumVoices; ++offset)
    {
        const int index = (nextVoice + offset) % kNumVoices;
        const auto& voice = voices[(size_t) index];
        if (! voice.held && ! voice.pedalHeld)
        {
            nextVoice = (index + 1) % kNumVoices;
            return index;
        }
    }
    return -1;
}

void Engine::rememberHeldKey(int note, float velocity, int midiChannel)
{
    for (int i = 0; i < heldKeyCount; ++i)
    {
        if (heldKeys[(size_t) i].note == note
            && heldKeys[(size_t) i].midiChannel == midiChannel)
        {
            heldKeys[(size_t) i].velocity = velocity;
            rebuildArpSequence();
            return;
        }
    }

    if (heldKeyCount >= kMaxHeldKeys)
    {
        for (int i = 1; i < heldKeyCount; ++i)
            heldKeys[(size_t) (i - 1)] = heldKeys[(size_t) i];
        --heldKeyCount;
    }

    const bool firstKey = heldKeyCount == 0;
    auto& key = heldKeys[(size_t) heldKeyCount++];
    key.note = note;
    key.velocity = clampf(velocity, 0.0f, 1.0f);
    key.midiChannel = midiChannel;
    key.order = ++keyOrder;
    rebuildArpSequence();

    // Start the pattern on the next sample rather than after a full clock
    // period. This makes the first note of a chord feel immediate.
    if (firstKey)
    {
        arpClock = 1.0f / std::max(0.25f, global.arpRate);
        arpIndex = 0;
    }
}

void Engine::forgetHeldKey(int note, int midiChannel)
{
    for (int i = 0; i < heldKeyCount; ++i)
    {
        if (heldKeys[(size_t) i].note == note
            && heldKeys[(size_t) i].midiChannel == midiChannel)
        {
            if (arpSourceOrder == heldKeys[(size_t) i].order)
                stopArpVoice();
            for (int j = i + 1; j < heldKeyCount; ++j)
                heldKeys[(size_t) (j - 1)] = heldKeys[(size_t) j];
            --heldKeyCount;
            rebuildArpSequence();
            if (heldKeyCount == 0)
            {
                stopArpVoice();
                arpClock = 0.0f;
            }
            return;
        }
    }
}

void Engine::rebuildArpSequence()
{
    arpSequenceLength = 0;
    arpIndex = 0;
    if (heldKeyCount == 0)
        return;

    std::array<HeldKey, kMaxHeldKeys> base {};
    const int baseCount = heldKeyCount;
    for (int i = 0; i < baseCount; ++i)
        base[(size_t) i] = heldKeys[(size_t) i];
    if (global.arpMode != 4)
    {
        // Keep source channel/velocity/order while sorting pitches. Identical
        // pitches on different MIDI channels are distinct held keys.
        for (int i = 1; i < baseCount; ++i)
        {
            const auto value = base[(size_t) i];
            int j = i - 1;
            while (j >= 0 && base[(size_t) j].note > value.note)
            {
                base[(size_t) (j + 1)] = base[(size_t) j];
                --j;
            }
            base[(size_t) (j + 1)] = value;
        }
    }

    const int octaves = std::clamp(global.arpOctaves, 1, 4);
    for (int oct = 0; oct < octaves; ++oct)
        for (int i = 0; i < baseCount; ++i)
        {
            auto step = base[(size_t) i];
            step.note += oct * 12;
            if (step.note >= 0 && step.note <= 127 && arpSequenceLength < (int) arpSequence.size())
                arpSequence[(size_t) arpSequenceLength++] = step;
        }

    if (global.arpMode == 1)
        std::reverse(arpSequence.begin(), arpSequence.begin() + arpSequenceLength);
    else if (global.arpMode == 2 && arpSequenceLength > 2)
    {
        const int ascending = arpSequenceLength;
        for (int i = ascending - 2; i > 0; --i)
            arpSequence[(size_t) arpSequenceLength++] = arpSequence[(size_t) i];
    }
}

void Engine::triggerArpNote()
{
    if (arpSequenceLength <= 0)
        return;

    stopArpVoice();

    int index = 0;
    if (global.arpMode == 3)
        index = (int) (modRng.nextUInt() % (uint32_t) arpSequenceLength);
    else
    {
        if (arpIndex >= arpSequenceLength)
            arpIndex = 0;
        index = arpIndex;
        arpIndex = (arpIndex + 1) % arpSequenceLength;
    }

    const auto& step = arpSequence[(size_t) index];
    const int voice = chooseVoice();
    if (voice < 0)
        return;
    startVoice(voices[(size_t) voice], step.note, step.velocity, step.midiChannel);
    arpVoice = voice;
    arpCurrentNote = step.note;
    arpSourceOrder = step.order;
}

void Engine::stopArpVoice()
{
    if (arpVoice >= 0 && arpVoice < kNumVoices)
        releaseVoice(voices[(size_t) arpVoice]);
    arpVoice = -1;
    arpCurrentNote = -1;
    arpSourceOrder = 0;
    arpGateOpen = false;
}

void Engine::updateArpeggiator(float invSampleRate)
{
    if (! global.arpOn)
    {
        if (arpGateOpen || arpVoice >= 0)
            stopArpVoice();
        return;
    }

    if (heldKeyCount == 0)
    {
        if (arpGateOpen || arpVoice >= 0)
            stopArpVoice();
        return;
    }

    const float interval = 1.0f / std::max(0.25f, global.arpRate);
    arpClock += invSampleRate;

    if (arpGateOpen)
    {
        arpGateTime -= invSampleRate;
        if (arpGateTime <= 0.0f)
            stopArpVoice();
    }

    if (arpClock >= interval)
    {
        arpClock -= interval;
        if (arpClock >= interval)
            arpClock = std::fmod(arpClock, interval);
        triggerArpNote();
        arpGateOpen = arpVoice >= 0;
        arpGateTime = interval * (global.arpMode == 3 ? 0.45f : 0.58f);
    }
}

void Engine::wakeIdleVoice(Voice& v) noexcept
{
    voice_continuity::wakeCircuit(v.ranks, kNumRanks, (float) internalRate, idleStepSamples,
                                  v.idleClock, v.idleSteps,
                                  idleNoise[(size_t) (&v - voices.data())]);
    v.idleClock = 0;
    v.idleSteps = 0;
}

void Engine::refreshKeyboardContour() noexcept
{
    refreshedContourLevers = { global.kbdBrillianceLow, global.kbdBrillianceHigh,
                               global.kbdLevelLow, global.kbdLevelHigh };
    // KBC1/KBC2 hold one contour voltage per voice channel for the key last
    // assigned to it (0.033 uF held nodes, refreshed by the multiplexer, so a
    // lever move reaches held notes). All eight load the shared FH/FL/LC buses.
    const bool circuit = usesSourceModels(global.renderPolicy);
    std::array<double, kNumVoices> kc {}, kb {};
    double kcSum = 0.0, kbSum = 0.0;
    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[(size_t) vi];
        if (circuit)
        {
            kc[(size_t) vi] = kbc::outputVolts(v.kbcKey, global.kbdBrillianceLow, global.kbdBrillianceHigh);
            kb[(size_t) vi] = kbc::outputVolts(v.kbcKey, global.kbdLevelLow, global.kbdLevelHigh);
        }
        kcSum += kc[(size_t) vi];
        kbSum += kb[(size_t) vi];
    }
    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[(size_t) vi];
        for (int ch = 0; ch < kNumRanks; ++ch)
        {
            if (! circuit)
            {
                v.kbcCv[ch][0] = v.kbcCv[ch][1] = 0.0f;
                v.kbcLevelGain[ch] = 0.0f;
                continue;
            }
            const double own = kc[(size_t) vi], level = kb[(size_t) vi];
            if (sourceActive[ch])
            {
                // Source-compiled receiver map, including the shared-bus loading
                // that every channel's KC branch adds (SourceFactoryTones.h).
                for (int side = 0; side < 2; ++side)
                {
                    const auto& m = sourceTones[ch].cutoff[(size_t) side];
                    v.kbcCv[ch][side] = static_cast<float>(m.effectiveCvPerPinVolt
                        * (m.pinPerTouch * own + m.pinPerLoadedBus * m.busPerTouchSum * kcSum));
                }
                const auto& src = sourceBoundaries[ch].level;
                constexpr double g = 1.0 / 560000.0;
                const double bus = src.ohms * g * source_factory::levelPinPerTouch * kbSum
                    / (1.0 + source_factory::voiceCount * src.ohms * g * (1.0 - source_factory::levelPinPerLoadedBus));
                v.kbcLevelGain[ch] = static_cast<float>((source_factory::levelPinPerLoadedBus * bus
                    + source_factory::levelPinPerTouch * level) / source_factory::levelUnitVolts);
            }
            else
            {
                v.kbcCv[ch][0] = static_cast<float>(own * kbc::kKcHpCvPerVolt);
                v.kbcCv[ch][1] = static_cast<float>(own * kbc::kKcLpCvPerVolt);
                v.kbcLevelGain[ch] = static_cast<float>(level * kbc::kKbLevelPerVolt);
            }
        }
    }
}

void Engine::startVoice(Voice& v, int note, float velocity, int midiChannel)
{
    // Circuit engine (Phase 2.1): every circuit free-runs, so assignment never
    // resets oscillator phase or filter memories; a silent circuit is first
    // caught up to the present. Legacy keeps the historical reset.
    // chooseVoice only returns an unheld circuit.
    const bool wasActive = v.active;
    const bool preserveAudioState = freeRunningVoices() && !v.held && !v.pedalHeld;
    if (preserveAudioState && ! wasActive)
        wakeIdleVoice(v);
    // IL is a selected drive, not an assignment-time capacitor reset. Keep
    // charge even for a first strike before the released circuit settles.
    const bool preserveEnvelope = preserveAudioState;
    v.idleClock = 0;
    v.idleSteps = 0;
    v.keyNode.track(); // TR low: K acquires the new key
    v.active = true;
    v.held = true;
    v.sustained = false;
    v.pedalHeld = false;
    v.note = note;
    v.midiChannel = midiChannel;
    v.velocity = clampf(velocity, 0.0f, 1.0f);
    v.pressure = 0.0f;
    v.timbre = 0.0f;
    v.mpeBend = 0.0f;
    // Phase 6 tuning: exactly (float) note in 12-TET (tuning_mode 0).
    v.targetPitch = tunedPitch(note);
    v.upperZone = global.mpe && mpeZones.inUpperZone(midiChannel);
    v.unisonOffset = 0.0f;
    v.contour = keyboardContourWeights(note);
    v.kbcKey = kbc::physicalKey(note);
    v.glidePitch = global.glideGate && global.portamento > 0.0001f ? lastPitch : v.targetPitch;
    // TWS C precedes the TRG4 PB control. Keep its nominal onset state alive
    // even when PB depth is zero. Reset-to-velocity is still a MIDI/retrigger
    // approximation, not a complete model of the b sample/hold and clamp.
    v.initialPitchTransient = -(double) v.velocity;
    // Circuit engine: TR opens the TWS b acquisition window (Phase 2.4).
    v.touch.trigger(v.velocity);
    v.ribbonRatio = ribbonFrequencyRatio(ribbon + params[P_ribbon]);
    v.age = ++voiceCounter;
    lastNote = note;
    lastPitch = v.targetPitch;
    ++heldCount;
    if (usesSourceModels(global.renderPolicy))
        refreshKeyboardContour();

    for (int ch = 0; ch < 2; ++ch)
    {
        auto& r = v.ranks[ch];
        const auto& rs = rankSettings[ch];
        // Consume the legacy draw even when retaining phase: unrelated noise
        // must not move in the shared RNG stream. Default-off order is unchanged.
        const float legacyPhase = audioRng.nextUnipolar();
        if (!preserveAudioState)
        {
            r.osc.reset(legacyPhase);
            // Same cold-start convention for the circuit engine's oscillator:
            // the legacy draw as its phase, zero audio prehistory.
            r.wsc.reset(legacyPhase, OscillatorNominalProfile {}.referenceHzNominal);
            r.hpf.reset();
            r.lpf.reset();
            r.hpStage.reset();
            r.lpStage.reset();
            r.vcaCold = true;
        }
        // Filter envelope: a reused circuit restarts its attack from the
        // capacitor's present level (source policies, IMPROVEMENT_PLAN 1.3;
        // silent circuits too since Phase 2.1); legacy keeps the reset to IL.
        if (!preserveEnvelope)
            r.fenv.reset();
        // Amplitude envelope: a reused sounding tail restarts its attack from the level it has
        // reached instead of from zero (source policies only). Zeroing it removed the whole tail in
        // one sample, a step of up to 0.022 full scale across eight sampled factory programs (0.007
        // now), a tick under a passage. Inferred from the same reasoning as the oscillator and filter
        // memories above (no traced reset path); the real envelope capacitor behaviour is not
        // measured. Legacy keeps the reset. A silent circuit's VCA is closed:
        // reset it (this also clears a stage left behind by a hard panic).
        if (!preserveAudioState || !wasActive)
            r.aenv.reset();
        r.fenv.noteOn(filterInitialTarget(ch),
                      filterAttackTarget(ch),
                      std::max(0.001f, rs.fA * global.envTimeScale * r.tol.filterEnvTime),
                      std::max(0.001f, rs.fD * global.envTimeScale * r.tol.filterEnvTime),
                      std::max(0.001f, rs.fR * global.envTimeScale * r.tol.filterEnvTime),
                      0.0f,
                      preserveEnvelope);
        r.aenv.noteOn(std::max(0.001f, rs.vA * global.envTimeScale * r.tol.ampEnvTime),
                      std::max(0.001f, rs.vD * global.envTimeScale * r.tol.ampEnvTime),
                      rs.vS,
                      std::max(0.001f, rs.vR * global.envTimeScale * r.tol.ampEnvTime),
                      usesSourceModels(global.renderPolicy));
    }

    if (! ringKeyDown)
    {
        ringKeyDown = true;
        // PRA53 TR selects the IG00159 envelope stage, not the IG00150
        // carrier phase or stored capacitor charge. Source inference and
        // preregistered exact-state gates: RING_EVENT_CONTINUITY_PROPOSAL.md.
        // Legacy keeps its historical phase/envelope reset bit for bit.
        const bool retainRingState = usesSourceModels(global.renderPolicy);
        if (!retainRingState)
        {
            ringPhase = 0.0f;
            ringEnv = 0.0f;
            ringAdsr.reset();
        }
        ringAdsr.noteOn(global.ringAttack, global.ringDecay, 0.0f, 0.35f, retainRingState);
    }
}

void Engine::releaseVoice(Voice& v)
{
    if (! v.active)
        return;
    if (v.held)
    {
        v.held = false;
        --heldCount;
    }
    v.sustained = false;
    v.pedalHeld = false;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& r = v.ranks[ch];
        const auto& rs = rankSettings[ch];
        // Hardware SUSTAIN is a release-time performance control, not a filter
        // sustain-level offset. Retain longer per-rank/key-release settings as
        // software extensions; the exact potentiometer law remains unmeasured.
        // An assigned, plugged and open footswitch disables only this term.
        const float performanceRelease = global.sustainGate ? global.masterSustain * 10.0f : 0.0f;
        r.aenv.setRelease(std::max({rs.vR, global.keyRelease, performanceRelease})
                          * global.envTimeScale * r.tol.ampEnvTime);
        r.fenv.setRelease(std::max({rs.fR, global.keyRelease, performanceRelease})
                          * global.envTimeScale * r.tol.filterEnvTime);
        r.aenv.noteOff();
        r.fenv.noteOff();
    }
    bool anyHeld = false;
    for (const auto& voice : voices)
        anyHeld = anyHeld || voice.held || voice.pedalHeld;
    if (! anyHeld)
    {
        heldCount = 0;
        ringKeyDown = false;
        ringAdsr.noteOff();
    }
}

void Engine::noteOn(int midiNote, float velocity, int midiChannel)
{
    if (midiNote < 0 || midiNote > 127)
        return;
    // Interface-layer controller curve (Phase 2.4); exact identity at 0.
    if (global.velocityCurve != 0.0f)
        velocity = applyVelocityCurve(velocity, global.velocityCurve);
    // Opt-in KAS scan timing: act at the next scan instant, in scan order.
    if (scanTimingActive() && scanner.push(midiNote, midiChannel, velocity, true))
        return;
    applyNoteOn(midiNote, velocity, midiChannel);
}

void Engine::applyNoteOn(int midiNote, float velocity, int midiChannel)
{
    if (global.sustainMode == 1)
    {
        for (auto& v : voices)
            if (v.sustained)
            {
                // Sustain II terminates previous released notes when a new
                // note/chord starts; it does not restart their release timer.
                for (auto& rank : v.ranks)
                {
                    rank.aenv.reset();
                    // TR was already released. Its separate filter capacitor
                    // remains connected; do not replace charge or restart time.
                    if (!freeRunningVoices()) rank.fenv.reset();
                }
                v.active = v.sustained = v.pedalHeld = false;
            }
    }

    if (global.arpOn)
    {
        rememberHeldKey(midiNote, velocity, midiChannel);
        return;
    }

    // Phase 6: a note a microtuning leaves unmapped does not sound.
    if (global.tuningMode != 0 && ! noteMapped(midiNote))
        return;
    // Phase 6 unison extension: N circuits per key, detuned symmetrically.
    if (const int unison = unisonCount(); unison > 1)
    {
        for (int k = 0; k < unison; ++k)
        {
            const int idx = chooseVoice();
            if (idx < 0)
                break;
            startVoice(voices[(size_t) idx], midiNote, velocity, midiChannel);
            voices[(size_t) idx].unisonOffset = global.unisonDetune / 100.0f
                * ((float) k / (float) (unison - 1) - 0.5f);
        }
        return;
    }

    const int idx = chooseVoice();
    if (idx < 0)
    {
        // All circuits occupied; leave held/ring state untouched. Circuit
        // engine: the KAS keeps seeing this held key (Phase 2.2).
        if (usesSourceModels(global.renderPolicy))
            rememberUnassignedKey(midiNote, velocity, midiChannel);
        return;
    }
    startVoice(voices[(size_t) idx], midiNote, velocity, midiChannel);
}

void Engine::rememberUnassignedKey(int note, float velocity, int midiChannel)
{
    for (int i = 0; i < unassignedCount; ++i)
        if (unassignedKeys[(size_t) i].note == note && unassignedKeys[(size_t) i].midiChannel == midiChannel)
            return;
    if (unassignedCount >= kMaxHeldKeys)
        return;
    auto& key = unassignedKeys[(size_t) unassignedCount++];
    key.note = note;
    key.midiChannel = midiChannel;
    key.velocity = velocity;
    key.order = ++keyOrder;
}

bool Engine::forgetUnassignedKey(int note, int midiChannel)
{
    for (int i = 0; i < unassignedCount; ++i)
        if (unassignedKeys[(size_t) i].note == note && unassignedKeys[(size_t) i].midiChannel == midiChannel)
        {
            for (int j = i + 1; j < unassignedCount; ++j)
                unassignedKeys[(size_t) (j - 1)] = unassignedKeys[(size_t) j];
            --unassignedCount;
            return true;
        }
    return false;
}

void Engine::reconsiderUnassignedKeys()
{
    // Declared KAS model (KasScanner.h): a held key without a circuit is seen
    // again by the next scan and takes the first free circuit, lowest key
    // first. It was not struck now, so its initial touch is zero.
    while (unassignedCount > 0 && usesSourceModels(global.renderPolicy))
    {
        const int idx = chooseVoice();
        if (idx < 0)
            return;
        int best = 0;
        for (int i = 1; i < unassignedCount; ++i)
            if (unassignedKeys[(size_t) i].note < unassignedKeys[(size_t) best].note)
                best = i;
        const auto key = unassignedKeys[(size_t) best];
        forgetUnassignedKey(key.note, key.midiChannel);
        startVoice(voices[(size_t) idx], key.note, 0.0f, key.midiChannel);
    }
}

bool Engine::isSustainPedalDown(int midiChannel) const noexcept
{
    if (midiChannel < 1 || midiChannel > 16)
        return false;
    if (! global.mpe)
        return sustainPedals[(size_t) midiChannel];
    // MPE: a zone's master pedal holds its members (upper zone: channel 16).
    return sustainPedals[(size_t) midiChannel]
        || (mpeZones.inUpperZone(midiChannel) ? sustainPedals[16] : sustainPedals[1]);
}

void Engine::releaseKey(Voice& v)
{
    if (! v.held)
        return;
    // Explicit MIDI extension: pedal hold takes priority in both panel
    // sustain modes; this is not a model of the physical CS-80 footswitch.
    if (isSustainPedalDown(v.midiChannel))
    {
        v.held = false;
        v.pedalHeld = true;
        heldCount = std::max(0, heldCount - 1);
    }
    else if (global.sustainMode == 1 && global.sustainGate)
    {
        releaseVoice(v);
        v.sustained = true;
    }
    else
        releaseVoice(v);
}

void Engine::cancelSustainTails()
{
    for (auto& v : voices)
    {
        // Held and pedal-held notes are not tails; MIDI CC64 bookkeeping is a
        // separate software convention and is deliberately left alone.
        if (! v.active || v.held || v.pedalHeld)
            continue;
        v.sustained = false;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& r = v.ranks[ch];
            const auto& rs = rankSettings[ch];
            // setRelease keeps stage, value and elapsed stage time, so the
            // bent fast-release window is not restarted.
            r.aenv.setRelease(std::max(rs.vR, global.keyRelease) * global.envTimeScale * r.tol.ampEnvTime);
            r.fenv.setRelease(std::max(rs.fR, global.keyRelease) * global.envTimeScale * r.tol.filterEnvTime);
        }
    }
}

void Engine::noteOff(int midiNote, int midiChannel)
{
    if (scanTimingActive() && scanner.push(midiNote, midiChannel, 0.0f, false))
        return;
    applyNoteOff(midiNote, midiChannel);
}

void Engine::applyNoteOff(int midiNote, int midiChannel)
{
    if (global.arpOn)
    {
        forgetHeldKey(midiNote, midiChannel);
        return;
    }
    // A released key that never found a circuit simply stops being seen.
    if (unassignedCount > 0 && forgetUnassignedKey(midiNote, midiChannel))
        return;
    if (auto* voice = findVoiceForNote(midiNote, midiChannel))
    {
        releaseKey(*voice);
        // Unison: the key's other circuits release with it.
        if (unisonCount() > 1)
            while (auto* other = findVoiceForNote(midiNote, midiChannel))
                releaseKey(*other);
        if (unassignedCount > 0)
            reconsiderUnassignedKeys();
    }
}

Engine::Voice* Engine::findVoiceForNote(int note, int midiChannel)
{
    // MIDI note identity includes channel even with expressive MPE disabled.
    // Never redirect an unmatched note-off to a different member/channel.
    Voice* newest = nullptr;
    for (auto& v : voices)
        if (v.active && v.held && v.note == note && v.midiChannel == midiChannel)
            if (newest == nullptr || v.age > newest->age)
                newest = &v;
    return newest;
}

void Engine::allNotesOff(bool release, int midiChannel)
{
    if (midiChannel < 0 || midiChannel > 16)
        return;
    const bool allChannels = midiChannel == 0;
    // Channel0 is the legacy application panic: clear every pedal and key.
    // Channel-mode MIDI messages do not reset controller state.
    if (allChannels)
        sustainPedals.fill(false);
    scanner.discard(midiChannel);
    {
        int kept = 0;
        for (int i = 0; i < unassignedCount; ++i)
            if (! allChannels && unassignedKeys[(size_t) i].midiChannel != midiChannel)
                unassignedKeys[(size_t) kept++] = unassignedKeys[(size_t) i];
        unassignedCount = kept;
    }

    int kept = 0;
    for (int i = 0; i < heldKeyCount; ++i)
        if (! allChannels && heldKeys[(size_t) i].midiChannel != midiChannel)
            heldKeys[(size_t) kept++] = heldKeys[(size_t) i];
    const bool keysChanged = kept != heldKeyCount;
    heldKeyCount = kept;
    if (keysChanged || allChannels)
        rebuildArpSequence();
    if (heldKeyCount == 0)
        arpClock = 0.0f;

    if (arpVoice >= 0 && arpVoice < kNumVoices
        && (allChannels || voices[(size_t) arpVoice].midiChannel == midiChannel))
    {
        arpVoice = -1;
        arpCurrentNote = -1;
        arpSourceOrder = 0;
        arpGateOpen = false;
    }

    for (auto& voice : voices)
    {
        if (! allChannels && voice.midiChannel != midiChannel)
            continue;
        if (! release)
        {
            // Explicit inferred MIDI-panic gate policy: hard-mute audio as
            // before, but source TP1 controls must be released from their
            // retained value rather than held forever in a pre-panic stage.
            // Legacy retains its historical hard-panic policy.
            for (int rank = 0; rank < kNumRanks; ++rank)
                if (sourceActive[rank])
                {
                    auto& fenv = voice.ranks[rank].fenv;
                    const auto stage = fenv.getState().stage;
                    if (stage == FilterEnv::Stage::Attack || stage == FilterEnv::Stage::Decay
                        || stage == FilterEnv::Stage::Sustain)
                        fenv.noteOff();
                }
            // Explicit MIDI All Sound Off is a hard panic, unlike ordinary
            // envelope retirement. It intentionally clears the coupling tail.
            if (freeRunningVoices())
                for (auto& rank : voice.ranks)
                {
                    rank.vca.reset(0.0f);
                    rank.vcaCold = true;
                }
            voice.active = voice.held = voice.sustained = voice.pedalHeld = false;
        }
        else if (allChannels)
        {
            voice.pedalHeld = false;
            releaseVoice(voice);
        }
        else
            releaseKey(voice); // CC123 follows note-off, including pedal hold
    }
    heldCount = 0;
    for (const auto& voice : voices)
        if (voice.active && (voice.held || voice.pedalHeld))
            ++heldCount;
    if (heldCount == 0)
    {
        ringKeyDown = false;
        ringAdsr.noteOff();
    }
}

void Engine::polyPressure(int midiNote, float pressure, int midiChannel)
{
    pressure = clampf(pressure, 0.0f, 1.0f);
    // Interface-layer controller curve (Phase 2.4); exact identity at default.
    if (global.pressureCurve != 0.0f || global.pressureThreshold > 0.0f)
        pressure = applyPressureCurve(pressure, global.pressureCurve, global.pressureThreshold);
    for (auto& v : voices)
        if (v.active && v.note == midiNote
            && (! global.mpe || midiChannel <= 0 || v.midiChannel == midiChannel))
        {
            v.pressure = pressure;
        }
}

void Engine::channelPressure(int midiChannel, float pressure)
{
    pressure = clampf(pressure, 0.0f, 1.0f);
    if (global.pressureCurve != 0.0f || global.pressureThreshold > 0.0f)
        pressure = applyPressureCurve(pressure, global.pressureCurve, global.pressureThreshold);
    lastChannelPressure = pressure;
    for (auto& v : voices)
        if (v.active && (isZoneMaster(v, midiChannel) || v.midiChannel == midiChannel))
            v.pressure = pressure;
}

void Engine::pitchBend(int midiChannel, float value)
{
    if (global.parityMode)
        return;
    value = clampf(value, -1.0f, 1.0f);
    if (global.mpe)
    {
        // MPE zones (Phase 6): a master bends its zone, a member its notes.
        // Ranges negotiated by RPN 0 replace the parameters.
        const auto role = mpeZones.role(midiChannel);
        if (role == MpeZoneLayout::Role::lowerMaster || (role == MpeZoneLayout::Role::none && midiChannel == 1))
        {
            const float range = mpeZones.masterRange(false);
            globalBend = value * (range >= 0.0f ? range : global.pitchBendRange);
            return;
        }
        if (role == MpeZoneLayout::Role::upperMaster)
        {
            const float range = mpeZones.masterRange(true);
            upperZoneBend = value * (range >= 0.0f ? range : global.pitchBendRange);
            return;
        }
        const float negotiated = mpeZones.memberRange(role == MpeZoneLayout::Role::upperMember);
        const float range = negotiated >= 0.0f ? negotiated : global.mpeBendRange;
        for (auto& v : voices)
            if (v.active && v.midiChannel == midiChannel)
                v.mpeBend = value * range;
    }
    else
    {
        globalBend = value * global.pitchBendRange;
    }
}

void Engine::controlChange(int controller, float value, int midiChannel)
{
    if (controller == 120 || controller == 123)
    {
        allNotesOff(controller == 123, midiChannel);
        return;
    }
    // Ribbon from a MIDI CC (interface layer, off by default): 64 is centre.
    if (global.ribbonCcEnabled && controller == global.ribbonCc && std::isfinite(value))
    {
        const int cc = (int) std::lround(clampf(value, 0.0f, 1.0f) * 127.0f);
        setRibbon(cc >= 64 ? (float) (cc - 64) / 63.0f : (float) (cc - 64) / 64.0f);
    }
    // MPE zone negotiation (Phase 6): RPN 0 / RPN 6 while MPE is on.
    if (global.mpe && std::isfinite(value) && (controller == 101 || controller == 100 || controller == 6 || controller == 38))
    {
        mpeZones.controlChange(midiChannel, controller, (int) std::lround(clampf(value, 0.0f, 1.0f) * 127.0f));
        return;
    }
    if (global.parityMode && (controller == 1 || controller == 11
                              || controller == 64 || controller == 74))
        return;
    // clampf passes NaN through; a non-finite controller value is not an event.
    if (! std::isfinite(value))
        return;
    value = clampf(value, 0.0f, 1.0f);
    if (controller == 1)
        modWheel = value;
    else if (controller == 11)
        expression = value;
    else if (controller == 64)
    {
        if (midiChannel < 1 || midiChannel > 16)
            return;
        const bool down = value >= 0.5f;
        if (down == sustainPedals[(size_t) midiChannel])
            return;
        sustainPedals[(size_t) midiChannel] = down;
        if (! down)
        {
            for (auto& voice : voices)
                if (voice.pedalHeld && ! isSustainPedalDown(voice.midiChannel))
                {
                    voice.pedalHeld = false;
                    if (! voice.held && ! voice.sustained)
                        releaseVoice(voice);
                }
            heldCount = 0;
            for (const auto& voice : voices)
                if (voice.held || voice.pedalHeld)
                    ++heldCount;
            if (heldCount == 0)
            {
                ringKeyDown = false;
                ringAdsr.noteOff();
            }
            if (unassignedCount > 0)
                reconsiderUnassignedKeys();
        }
    }
    else if (controller == 74)
    {
        for (auto& v : voices)
            if (v.active && (isZoneMaster(v, midiChannel) || v.midiChannel == midiChannel))
                v.timbre = value;
    }
}

void Engine::setExpression(float value)
{
    if (std::isfinite(value))
        expression = clampf(value, 0.0f, 1.0f);
}

void Engine::setRibbon(float value)
{
    ribbon = clampf(value, -1.0f, 1.0f);
}

float Engine::tunedPitch(int midiNote) const noexcept
{
    const int n = std::clamp(midiNote, 0, 127);
    if (global.tuningMode == 1) return scalaTable.pitch[(size_t) n];
    if (global.tuningMode == 2) return mtsTable.pitch[(size_t) n];
    return (float) midiNote;
}

bool Engine::noteMapped(int midiNote) const noexcept
{
    const int n = std::clamp(midiNote, 0, 127);
    if (global.tuningMode == 1) return scalaTable.mapped[(size_t) n];
    if (global.tuningMode == 2) return mtsTable.mapped[(size_t) n];
    return true;
}

void Engine::retuneSoundingVoices() noexcept
{
    // A retuning reaches sounding notes (as MTS-ESP asks); the glide law
    // carries them to the new target.
    for (auto& v : voices)
        if (v.active)
            v.targetPitch = tunedPitch(v.note);
}

void Engine::setScalaTuning(const tuning::TuningTable& table) noexcept
{
    scalaTable = table;
    if (global.tuningMode == 1) retuneSoundingVoices();
}

void Engine::setMtsTuning(const tuning::TuningTable& table) noexcept
{
    mtsTable = table;
    if (global.tuningMode == 2) retuneSoundingVoices();
}

bool Engine::handleSysEx(const std::uint8_t* data, int size) noexcept
{
    if (! tuning::applyMtsSysEx(data, size, mtsTable))
        return false;
    if (global.tuningMode == 2) retuneSoundingVoices();
    return true;
}

bool Engine::hasFreeCircuit() const noexcept
{
    for (const auto& v : voices)
        if (! v.held && ! v.pedalHeld)
            return true;
    return false;
}

bool Engine::isZoneMaster(const Voice& v, int midiChannel) const noexcept
{
    // Non-MPE and the lower zone: channel 1 is the master (historical rule).
    if (! v.upperZone) return midiChannel == 1;
    return midiChannel == 16 && mpeZones.role(16) == MpeZoneLayout::Role::upperMaster;
}

void Engine::retargetSmoothedSettings() noexcept
{
    int slot = 0;
    visitSmoothedSettings(global, rankSettings, kNumRanks,
                          [&](float& field) { field = settingsSmoothing.retarget(slot++, field); });
}

void Engine::beginSmoothedControls() noexcept
{
    if (settingsSmoothing.snapIsPending())
    {
        int slot = 0;
        visitSmoothedSettings(global, rankSettings, kNumRanks,
                              [&](float& field) { field = settingsSmoothing.snapToTarget(slot++); });
        settingsSmoothing.endSnap();
    }
    else if (!settingsSmoothing.settled())
    {
        int slot = 0;
        settingsSmoothing.beginAdvance();
        visitSmoothedSettings(global, rankSettings, kNumRanks,
                              [&](float& field) { field = settingsSmoothing.advance(slot++); });
        settingsSmoothing.endAdvance();
    }

    // The four contour levers are smoothed fields: a lever move reaches the
    // KBC held nodes (refreshed by the multiplexer, Phase 2.3) as they glide.
    if (global.kbdBrillianceLow != refreshedContourLevers[0] || global.kbdBrillianceHigh != refreshedContourLevers[1]
        || global.kbdLevelLow != refreshedContourLevers[2] || global.kbdLevelHigh != refreshedContourLevers[3])
        refreshKeyboardContour();

    // Controllers: keep the event-set targets, use smoothed values for this
    // sample only. A settled smoother returns its target bit for bit.
    auto& c = controllerSmoothing;
    const double k = controlSmoothing;
    const float ribbonTotal = ribbon + params[P_ribbon];
    c.bendTarget = globalBend;
    c.modWheelTarget = modWheel;
    c.ribbonTarget = ribbon;
    if (c.snapPending)
    {
        c.bend.snap(globalBend);
        c.modWheel.snap(modWheel);
        expressionOptics.reset(expression);
        c.ribbon.snap(ribbonTotal);
    }
    globalBend = (float) c.bend.advance(globalBend, k);
    modWheel = (float) c.modWheel.advance(modWheel, k);
    // The EXP volume path is an optical cell (ws4 hand-off, ExpressionOptics.h):
    // its declared log-R law is this controller's smoothing.
    expressionOptics.setPedal(expression);
    c.expressionGain = (float) expressionOptics.tick();
    const double ribbonSmoothed = c.ribbon.advance(ribbonTotal, k);
    if (!c.ribbon.atTarget(ribbonTotal))
        ribbon = (float) (ribbonSmoothed - params[P_ribbon]);
    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[(size_t) vi];
        c.pressureTarget[(size_t) vi] = v.pressure;
        c.mpeBendTarget[(size_t) vi] = v.mpeBend;
        c.timbreTarget[(size_t) vi] = v.timbre;
        // A new note starts from its own touch, bend and timbre, not a glide
        // from the previous note on this circuit.
        if (c.snapPending || c.age[(size_t) vi] != v.age)
        {
            c.age[(size_t) vi] = v.age;
            c.pressure[(size_t) vi].snap(v.pressure);
            c.mpeBend[(size_t) vi].snap(v.mpeBend);
            c.timbre[(size_t) vi].snap(v.timbre);
        }
        v.pressure = (float) c.pressure[(size_t) vi].advance(v.pressure, k);
        v.mpeBend = (float) c.mpeBend[(size_t) vi].advance(v.mpeBend, k);
        v.timbre = (float) c.timbre[(size_t) vi].advance(v.timbre, k);
    }
    c.snapPending = false;
}

void Engine::endSmoothedControls() noexcept
{
    auto& c = controllerSmoothing;
    globalBend = c.bendTarget;
    modWheel = c.modWheelTarget;
    ribbon = c.ribbonTarget;
    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[(size_t) vi];
        v.pressure = c.pressureTarget[(size_t) vi];
        v.mpeBend = c.mpeBendTarget[(size_t) vi];
        v.timbre = c.timbreTarget[(size_t) vi];
    }
}

Engine::SampleContext Engine::tickGlobals(float externalModulation)
{
    const float sr = (float) internalRate;
    const float invSr = 1.0f / sr;

    // KAS scan instants (Phase 2.2, opt-in). Switching the option off flushes
    // anything still queued at once.
    if (scanTimingActive() || scanner.pending() > 0)
    {
        auto dispatch = [this](const kas::Scanner::Event& e) {
            if (e.on) applyNoteOn(e.note, e.velocity, e.channel);
            else applyNoteOff(e.note, e.channel);
        };
        if (! scanTimingActive())
            while (! scanner.tick(dispatch)) {}
        else
            scanner.tick(dispatch);
    }

    updateArpeggiator(invSr);
    // Circuit engine: smoothed settings and controllers for this sample.
    const bool smoothControls = usesSourceModels(global.renderPolicy);
    if (smoothControls)
        beginSmoothedControls();

    // Plan 2.5 (circuit engine): tolerance ratios for the present VOICE DISPERSION
    // (recomputed only when it moves) and every channel's thermal drift, silent or
    // not, so the drift's time base never depends on activity. Legacy and PURE
    // CS-80 keep identity ratios (scale 0) and do not clock the drift.
    {
        refreshToleranceRatios();
        if (global.renderPolicy == RenderPolicy::sourcePerformance)
            for (auto& voice : voices)
                for (auto& rank : voice.ranks)
                    rank.driftCents = rank.thermal.tick();
    }

    // Global modulation oscillators.
    for (int i = 0; i < 3; ++i)
        lfoValues[i] = usesSourceModels(global.renderPolicy)
            ? modLfos[i].tick(global.lfoRate[i], sr, global.lfoWave[i],
                              global.lfoDepth[i] != 0.0f || modern.isActive())
            : modLfos[i].tick(global.lfoRate[i], sr, global.lfoWave[i]);

    lfoPitch = lfoFilter = lfoAmp = 0.0f;
    lfoPwm = lfoPan = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float v = lfoValues[i] * global.lfoDepth[i];
        switch (global.lfoDest[i])
        {
            case 1: lfoPitch += v * 2.0f; break;
            case 2: lfoFilter += v * 1.6f; break;
            case 3: lfoAmp += v * 0.25f; break;
            case 4: lfoPan += v * 0.7f; break;
            case 5: lfoPwm += v * 0.18f; break;
            default: break;
        }
    }

    pwmPhase[0] += rankSettings[0].pwmSpeed * invSr;
    pwmPhase[1] += rankSettings[1].pwmSpeed * invSr;
    pwmPhase[0] -= std::floor(pwmPhase[0]);
    pwmPhase[1] -= std::floor(pwmPhase[1]);
    // PWM is one shared sine per rank; evaluate it once, not once per voice.
    const std::array<float, 2> pwmSine = { std::sin(kTwoPi * pwmPhase[0]), std::sin(kTwoPi * pwmPhase[1]) };

    // TRG5 positive diode-OR -> SP. Legacy: normalized ideal maximum of held
    // voices, not event memory (physical release removes it even if a pedal or
    // release tail sounds; the arpeggiator uses its generated held gates).
    // Circuit engine: the diode bus below. Either way SP enters the SUB speed
    // converter exactly once, scaled there by AT SPEED (VC4): the IG00150
    // exponential converter (ws4) adds octaves for it; the bus only shapes it.
    float sharedPressure = 0.0f;
    if (usesSourceModels(global.renderPolicy))
    {
        // Circuit engine (Phase 2.4): every channel's TWS nodes advance each
        // sample. The key's force drives a only while it is held (a released
        // or pedal-held key exerts none); b holds the initial touch; C is the
        // initial pitch transient. TRG5 joins every a through a 1S1555 onto one
        // 47k bus, normalised to one full-pressure key (TouchCircuit.h).
        std::array<double, kNumVoices> a {};
        double top = 0.0;
        for (int vi = 0; vi < kNumVoices; ++vi)
        {
            auto& voice = voices[(size_t) vi];
            const auto t = voice.touch.tick(touchCoefficients, voice.active && voice.held ? voice.pressure : 0.0);
            voice.touchA = (float) t.a;
            voice.touchB = (float) t.b;
            voice.touchC = (float) t.c;
            voice.initialPitchTransient = t.c;
            a[(size_t) vi] = t.a;
            top = std::max(top, t.a);
        }
        trg5BusVolts = top > 0.0 ? touch::busVolts(a, trg5BusVolts, 2) : 0.0;
        sharedPressure = (float) std::clamp(trg5BusVolts / trg5FullScaleVolts, 0.0, 1.0);
    }
    else
    for (const auto& voice : voices)
        if (voice.active && voice.held)
            sharedPressure = std::max(sharedPressure, voice.pressure);

    // Modern layer (Phase 6): MSEGs and matrix evaluated once per sample. Identity offsets
    // (0, or ratios of exactly 1) when nothing is routed or under PURE CS-80.
    modern.beginSample({ modWheel, ribbon + params[P_ribbon], lastChannelPressure, sharedPressure, expression,
                         global.pitchBendRange > 0.0f ? globalBend / global.pitchBendRange : 0.0f,
                         subValue, ringEnv, { lfoValues[0], lfoValues[1], lfoValues[2] } });
    const auto& modernGlobal = modern.getGlobalOffsets();

    if (global.subOriginalMode && usesSourceModels(global.renderPolicy))
    {
        // Circuit engine: IG00150 exponential converter (touch adds octaves) smoothed on
        // the control node, band-limited resets. ws4-circuits.md Step 1.
        const double rate = originalSub.circuitRate(subTargetRateMemo(
            global.subOriginalSpeed, global.subAtSpeed, sharedPressure) * (double) modernGlobal.subSpeedRatio,
            originalSubSmoothing);
        originalSubSpeedSmoothed = (float) rate;
        subValue = originalSub.tickCircuit(rate, sr, global.subOriginalFunction,
                                           -global.subExtLevel * externalModulation);
    }
    else if (global.subOriginalMode)
    {
        const float speed = OriginalSubOscillator::targetRate(
            global.subOriginalSpeed, global.subAtSpeed, sharedPressure) * modernGlobal.subSpeedRatio;
        originalSubSpeedSmoothed += (speed - originalSubSpeedSmoothed) * originalSubSmoothing;
        // EJ/SUB input chain inverts. Peak1 digital input at fullLEVEL maps
        // to nominal peak1 modulation, NOT a claim of volts-per-full-scale.
        subValue = originalSub.tick(originalSubSpeedSmoothed, sr, global.subOriginalFunction,
                                    -global.subExtLevel * externalModulation);
    }
    else
    {
        // Preserve the shipped waveform mapping, smoothing and RNG stream.
        float speed = global.subSpeed;
        speed *= 1.0f + global.subAtSpeed * sharedPressure * 2.0f;
        speed *= modernGlobal.subSpeedRatio;
        // Legacy keeps its per-sample constant; the circuit engine its time constant.
        subSpeedSmoothed += (speed - subSpeedSmoothed)
                            * (usesSourceModels(global.renderPolicy) ? subSpeedSmoothing : 0.0015f);
        subValue = subLfo.tick(subSpeedSmoothed, sr, global.subWave, modRng);
    }
    subValue *= modernGlobal.subDepthGain;
    driftPhase += 0.031f * invSr;
    driftPhase -= std::floor(driftPhase);


    // Double accumulator avoids long-glide float rounding stalling before target.
    // An open assigned footswitch suppresses new glides and snaps running ones to target.
    const bool glideActive = global.glideGate && global.portamento > 0.0001f;
    const double glideCoef = glideActive
                                ? glideExp(-1.0 / ((double) global.portamento * sr))
                                : 0.0;

    if (sourceActive[0] || sourceActive[1])
        updateSourceReadouts(sr);

    // Circuit engine: ONE shared panel-noise generator sample for every voice
    // (SUB29 fan-out, NoiseSource.h). Legacy draws per rank below, as before.
    const bool sharedNoise = usesSourceModels(global.renderPolicy);
    const float panelNoise = sharedNoise ? circuitNoise.panel() : 0.0f;

    const bool freeRunning = freeRunningVoices();
    return { sr, invSr, pwmSine, glideActive, glideCoef, panelNoise,
             freeRunning, sharedNoise, smoothControls };
}

void Engine::tickRanks(const SampleContext& context, float& mixL, float& mixR)
{
    const auto& [sr, invSr, pwmSine, glideActive, glideCoef, panelNoise,
                 freeRunning, sharedNoise, smoothControls] = context;
    int stillHeld = 0;
    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[(size_t) vi];
        // Circuit engine (Phase 2.1): a silent circuit keeps running. It is
        // clocked once per idle step rather than every sample, and caught up
        // exactly when it is next assigned (ContinuousVoiceCircuit.h).
        // A cold circuit has no previous controls to interpolate. Run exactly
        // its first physical sample to seed them; never invent a zero-Hz past.
        const bool coldBootstrap = freeRunning && !v.active && !v.idleControlsPrimed;
        int idleSamples = 0;
        bool modernUpdatedForTail = false;
        if (! v.active && !coldBootstrap)
        {
            const int firstStep = idleStepSamples - vi * idleStepSamples / kNumVoices;
            const int due = v.idleSteps == 0 ? firstStep : idleStepSamples;
            // Keep a removed pan route current until its cached offset clears.
            // Released controller inputs stay zero after the first idle deadline,
            // even when the smoother restores their event-set targets afterward.
            if (freeRunning && modern.isActive()
                && (modern.hasPanDestinations()
                    || modern.rankOffsets(vi, 0).pan != 0.0f
                    || modern.rankOffsets(vi, 1).pan != 0.0f)
                && (v.ranks[0].vca.hasCouplingTail() || v.ranks[1].vca.hasCouplingTail()))
            {
                // Tail pan needs this sample's modulation. Predict the original
                // idle deadline without changing its clock or ownership cadence.
                const bool scheduled = v.idleClock + 1 >= due;
                const bool releasedControls = scheduled || v.idleSteps > 0;
                modern.processVoice(vi,
                    { v.age, v.held || v.pedalHeld, v.note, v.velocity,
                      releasedControls ? 0.0f : v.pressure, releasedControls ? 0.0f : v.timbre, v.contour,
                      { v.ranks[0].info.filterEnvelope, v.ranks[1].info.filterEnvelope },
                      { v.ranks[0].info.ampEnvelope, v.ranks[1].info.ampEnvelope } }, scheduled);
                modernUpdatedForTail = true;
            }
            // The amplitude gate may be closed while its downstream VCA
            // coupling capacitor still drives the mix. Keep that signed RC
            // discharge connected at audio rate; only the oscillator/filter
            // use the decimated idle schedule. No fade or changed pole.
            if (freeRunning)
                for (int ch = 0; ch < kNumRanks; ++ch)
                {
                    auto& r = v.ranks[ch];
                    if (!r.vca.hasCouplingTail())
                        continue;
                    const float closed = r.vca.processClosed();
                    const float chGain = ch == 0 ? (1.0f - global.mix) : global.mix;
                    if (chGain == 0.0f)
                        continue;
                    const auto& rs = rankSettings[ch];
                    const auto& mod = modern.rankOffsets(vi, ch);
                    const float pan = clampf(rs.pan + r.trimPan * global.voiceDispersion * 0.8f
                                             + lfoPan + mod.pan, -1.0f, 1.0f);
                    float panLeft, panRight;
                    constantPowerPan(pan, panLeft, panRight);
                    const float voiceOut = closed * chGain * 0.17f;
                    mixL += voiceOut * panLeft;
                    mixR += voiceOut * panRight;
                }
            if (! freeRunning || ++v.idleClock < due)
                continue;
            idleSamples = v.idleClock;
            v.idleClock = 0;
            if (++v.idleSteps >= voice_continuity::kIdleStepWrap)
                v.idleSteps -= voice_continuity::kIdleStepWrap
                               - voice_continuity::kHistoryStride * voice_continuity::kHistoryLength;
            // Released key: no pressure, MIDI member expression or held EG
            // gate (also after a hard panic). The TWS b hold (velocity) stays.
            v.pressure = v.timbre = v.mpeBend = 0.0f;
            for (int ch = 0; ch < kNumRanks; ++ch)
            {
                auto& fenv = v.ranks[ch].fenv;
                const auto stage = fenv.getState().stage;
                if (! sourceActive[ch] && (stage == FilterEnv::Stage::Attack
                    || stage == FilterEnv::Stage::Decay || stage == FilterEnv::Stage::Sustain))
                    fenv.noteOff();
            }
        }
        const bool idle = idleSamples > 0;
        const float rate = idle ? sr / (float) idleSamples : sr;

        if (v.held || v.pedalHeld)
            ++stillHeld;

        // SH7/SH8 K hold (Phase 2.2, circuit engine): NAND(TR, SUS). TR is high
        // once the key (or, as a MIDI extension, the CC64 pedal) lets go; SUS is
        // high in SUSTAIN I. Holding freezes the key voltage, glide included,
        // and lets it droop (KasScanner.h). Legacy never holds: ratio 1.
        float keyRatio = 1.0f;
        if (freeRunning)
        {
            if (kas::shHolds(! (v.held || v.pedalHeld), global.sustainMode == 0))
            {
                if (! v.keyNode.holding)
                    v.keyNode.hold(filterKeyVolts((float) v.glidePitch));
                else
                    v.keyNode.advance((double) (idle ? idleSamples : 1) / internalRate);
                keyRatio = (float) v.keyNode.ratio();
            }
            else
                v.keyNode.track();
        }

        // A held K node freezes the glide (portamento acts upstream on KI).
        if (idle)
        {
            if (! v.keyNode.holding)
            {
                if (glideActive)
                    v.glidePitch = v.targetPitch + (v.glidePitch - v.targetPitch)
                                   * std::exp(-(double) idleSamples / ((double) global.portamento * sr));
                else
                    v.glidePitch = v.targetPitch;
            }

        }
        else
        {
        if (! v.keyNode.holding)
        {
        if (glideActive)
            v.glidePitch = v.targetPitch + (v.glidePitch - v.targetPitch) * glideCoef;
        else
            v.glidePitch = v.targetPitch;
        }

        // Conditional TWS C positive-step response: 1 uF * (100k || 100k).
        // Coefficient is computed at prepare, not in this sample loop. Do not
        // freeze a permanent detuning residual; snap only a negligible state
        // to zero well before double-precision denormals are approached.
        if (! freeRunning) // circuit engine: C comes from the TWS model (Phase 2.4)
            v.initialPitchTransient *= initialPitchDecay;
        }
        if (std::abs(v.initialPitchTransient) < 1.0e-15)
            v.initialPitchTransient = 0.0;

        // Sustain I holds each released voice's last ribbon pitch. Sustain II
        // keeps release tails coupled to subsequent ribbon gestures.
        if (v.held || v.pedalHeld || (global.sustainMode == 1 && global.sustainGate))
            v.ribbonRatio = ribbonFrequencyRatio(ribbon + params[P_ribbon]);

        const float glideOffset = v.glidePitch - (float) v.note;
        // Touch drives: Legacy uses the MIDI values; the circuit engine the TWS
        // held initial touch b and the smoothed aftertouch a (Phase 2.4).
        const float touchVelocity = freeRunning ? v.touchB : v.velocity;
        const float touchPressure = freeRunning ? v.touchA : v.pressure;
        const float dispersion = global.voiceDispersion;
        const float driftVoice = drift_opt::sine(global.drift, driftPhase, voiceDrift[vi]);
        // Envelope sources are the previous sample's values of each rank.
        if (! modernUpdatedForTail)
            modern.processVoice(vi, { v.age, v.held || v.pedalHeld, v.note, v.velocity, v.pressure, v.timbre, v.contour,
                                      { v.ranks[0].info.filterEnvelope, v.ranks[1].info.filterEnvelope },
                                      { v.ranks[0].info.ampEnvelope, v.ranks[1].info.ampEnvelope } });

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& r = v.ranks[ch];
            auto& rs = rankSettings[ch];
            const auto& mod = modern.rankOffsets(vi, ch);
            // Tune trim and drift. Legacy: uniform trims and the historical sine,
            // unchanged. Circuit engine (plan 2.5): the declared alignment
            // residual and the thermal 1/f drift, or the sine as the "worn unit".
            const bool circuitVariation = global.renderPolicy == RenderPolicy::sourcePerformance;
            const float tuneTrim = circuitVariation ? r.tol.tuneSemitones
                                                    : r.trimTune * dispersion * (kProvisionalVoiceTuneSpreadCents / 100.0f);
            const float driftTerm = circuitVariation && global.driftModel == 0
                                        ? global.drift / tolerance::kDefaultDrift * r.driftCents * 0.01f
                                        : driftVoice;

            float pitch = (float) v.note
                          + global.masterTune
                          + (ch == 1 ? global.detuneCents / 100.0f : 0.0f)
                          + (v.upperZone ? upperZoneBend : globalBend)
                          + v.mpeBend
                          + glideOffset
                          + (float) (global.initBend * v.initialPitchTransient)
                          + lfoPitch
                          + subValue * (global.subVco + global.subAtVco * touchPressure) * 1.5f
                          + subValue * modWheel * global.modWheelSub * 2.0f
                          + tuneTrim
                          + driftTerm
                          + v.unisonOffset
                          + mod.pitch;

            if (global.glideMode == 2 && global.glideGate)
                pitch = std::round(pitch);

            // Transposition is a separate nominal harmonic ratio, not rounded
            // equal-tempered fifths and not part of the shared filter key CV.
            const float pitchHz = usesSourceModels(global.renderPolicy)
                ? r.pitchHz(pitch, global.tuningRef) : midiToHz(pitch, global.tuningRef);
            const float freq = pitchHz * v.ribbonRatio
                               * kFeetRatios[std::clamp(rs.feet, 0, 5)] * keyRatio;
            const float pwm = pwmSine[ch];
            const float pw = clampf(rs.pulseWidth + rs.pwmDepth * 0.35f * pwm + lfoPwm + mod.pulseWidth, 0.05f, 0.95f);

            // Circuit engine: WSC pin volts from one charge/reset trajectory
            // through the fixed inferred adapter; 8 internal samples of
            // linear-phase antialias delay. Legacy: PolyBLEP, unchanged.
            // An idle step (Phase 2.1) advances the oscillator below, without
            // a per-sample tick and without drawing from the shared noise stream.
            float sawPulse = 0.0f, sineWave = 0.0f;
            float x = 0.0f;
            if (! idle)
            {
            if (usesSourceModels(global.renderPolicy))
            {
                WscControls wscControls;
                wscControls.pulseThresholdVolts = ReconstructedOscillator::panelThresholdVoltsInferred(pw);
                wscControls.sawEnabled = rs.saw;
                wscControls.pulseEnabled = rs.pulse;
                const auto pins = r.wsc.tick(freq, wscControls);
                sawPulse = voice_continuity::sawPulseUnits(pins);
                sineWave = static_cast<float>(pins.sinePin2 * kWscEngineUnitsPerVolt);
            }
            else
            {
                const auto osc = r.osc.tick(freq, sr, pw, rs.saw, rs.pulse, 0.0f, audioRng);
                sawPulse = osc.saw + osc.pulse;
                sineWave = osc.sine;
            }

            x = sawPulse;
            // The shared RNG draw is consumed in both engines so oscillator start
            // phases keep their sequence; the circuit engine hears the SUB29 sample.
            const float rankNoise = coldBootstrap ? 0.0f : audioRng.nextBipolar();
            x += (sharedNoise ? panelNoise : rankNoise) * rs.noise * 0.8f;
            x *= 0.9f;
            }

            // Shared key-voltage domain precedes VCO footage/vibrato. Master
            // pitch and key portamento move it; ribbon routing is unverified.
            const float keyVolts = filterKeyVolts(v.glidePitch + global.masterTune, r.keyVoltsExp) * keyRatio;
            const float brillOct = global.brilliance
                                   + (sourceActive[ch] ? 0.0f : rs.initBrill * touchVelocity * 2.0f)
                                   + (sourceActive[ch] ? 0.0f : rs.afterBrill * touchPressure * 2.0f)
                                   + v.timbre * 0.8f
                                   + modWheel * global.modWheelVcf * 1.5f
                                   // Owner manual p39: pressure adds SUB VCF depth,
                                   // not a static cutoff offset. The 1.5-octave scale
                                   // is provisional, not a measured circuit gain.
                                   + subValue * (global.subVcf + global.subAtVcf * touchPressure) * 1.5f
                                   + lfoFilter
                                   // Keyboard contour: zero (exactly) when both levers are centred.
                                   // Legacy: frozen per-key weights. Circuit engine: the KBC
                                   // network, added per filter side below (Phase 2.3).
                                   + (freeRunning ? 0.0f : keyboardBrillianceOctaves(global.kbdBrillianceLow,
                                                               global.kbdBrillianceHigh, v.contour));
            const float contourHp = v.kbcCv[ch][0], contourLp = v.kbcCv[ch][1];

            const float fenv = sourceActive[ch] ? sourceFilterValues[ch][vi]
                : r.fenv.tick(rate, true, usesSourceModels(global.renderPolicy));
            const float envLp = fenv;
            const float envHp = fenv * kProvisionalHpfEnvelopeGain;

            // Plan 2.5: the channel's component ratios (exactly 1 in Legacy and PURE CS-80).
            const float cutoffLp = r.tol.lpCutoff * (sourceActive[ch]
                ? nominalFilterCutoff(keyVolts, static_cast<float>(sourceCutoff[ch].effectiveCv[vi][1]) + brillOct + contourLp + mod.lpCutoff, r.lpCutoffExp)
                : nominalFilterCutoff(keyVolts, rs.lpfEffectiveCv + brillOct + envLp + contourLp + mod.lpCutoff, r.lpCutoffExp));
            const float cutoffHp = r.tol.hpCutoff * (sourceActive[ch]
                ? nominalFilterCutoff(keyVolts, static_cast<float>(sourceCutoff[ch].effectiveCv[vi][0]) + brillOct + contourHp + mod.hpCutoff, r.hpCutoffExp)
                : nominalFilterCutoff(keyVolts, rs.hpfEffectiveCv + brillOct + envHp + contourHp + mod.hpCutoff, r.hpCutoffExp));
            const float qLp = r.tol.lpQ * (sourceActive[ch] ? static_cast<float>(sourceShared.q[ch][vi][1])
                : provisionalFilterQ(rs.lpfRes * 0.8f + global.resonance * 0.6f));
            const float qHp = r.tol.hpQ * (sourceActive[ch] ? static_cast<float>(sourceShared.q[ch][vi][0])
                : provisionalFilterQ(rs.hpfRes * 0.8f + global.resonance * 0.6f));

            // Circuit engine: IG00156 nodal stages (input networks, 180 pF
            // branch, 1 uF couplings, in-loop OTA saturation; the 4.7 MOhm
            // feedforward is internal), free-running (Phase 2.1). Legacy:
            // unchanged Svf.
            const float hpQ = modern::modulatedQ(qHp, mod.hpQ), lpQ = modern::modulatedQ(qLp, mod.lpQ);
            if (freeRunning)
            {
                const voice_continuity::IdleRankControls controls { freq, pw, rs.noise, rs.saw, rs.pulse,
                    cutoffHp, hpQ, cutoffLp, lpQ, rs.hpfOn, rs.lpfOn };
                if (idle)
                {
                    // Closed-form oscillator advance and one decimated step
                    // of both stages. VCA coupling was already clocked and
                    // mixed at the inactive entry; never discharge it twice.
                    voice_continuity::idleStepRank(r, controls, sr, idleSamples, v.idleSteps);
                    r.info = { cutoffHp, cutoffLp, fenv, 0.0f, freq, keyVolts, qHp, qLp, rs.hpfOn, rs.lpfOn };
                    continue;
                }
                r.idle = controls;
            }
            float filtered = x;
            if (freeRunning)
            {
                // Same stage clocking as the idle path (re-referencing a stage
                // whose histories were last clocked at the idle step rate).
                filtered = voice_continuity::clockStages(r.hpStage, r.lpStage, r.idleRefs, filtered, r.idle, sr);
            }
            else
            {
                if (rs.hpfOn)
                {
                    r.hpf.set(cutoffHp, hpQ, sr);
                    filtered = r.hpf.process(filtered).hp;
                }
                if (rs.lpfOn)
                {
                    r.lpf.set(cutoffLp, lpQ, sr);
                    filtered = r.lpf.process(filtered).lp;
                }
            }

            if (coldBootstrap)
            {
                // Seed oscillator/filter time and controls only. The VCA keeps
                // its existing closed idle treatment and cold LI convention.
                r.vca.relaxIdle(1.0 / sr);
                r.info = { cutoffHp, cutoffLp, fenv, 0.0f, freq, keyVolts, qHp, qLp, rs.hpfOn, rs.lpfOn };
                continue;
            }

            const float sine = sineWave * rs.sine;
            // Legacy: SUB VCA modulation adds to the level CV. Circuit engine:
            // it drives the IG00151 exponential input (Vca.h) instead, so it is
            // left out of the linear LC sum.
            const bool circuitVca = usesSourceModels(global.renderPolicy);
            const float subLevel = circuitVca ? 0.0f : subValue * global.subVca * 0.45f;
            // Source LC/LI is linear in its explicit unit scale. Retain global
            // effective transports once; floor negative total gain at zero,
            // but do NOT apply the legacy 2.6 cap or per-tone normalization.
            // Keyboard level contour: Legacy frozen weights; circuit engine the
            // KBC2 -> KB -> 270k -> LC branch (Phase 2.3).
            const float contourLevel = freeRunning ? v.kbcLevelGain[ch]
                : keyboardLevelOffset(global.kbdLevelLow, global.kbdLevelHigh, v.contour);
            const float levelCv = r.tol.level * (sourceActive[ch]
                ? std::max(0.0f, static_cast<float>(sourceShared.levelGain[ch][vi])
                             + subLevel + lfoAmp
                             + contourLevel + mod.level)
                : clampf(rs.vLevel
                                             + rs.initLevel * touchVelocity
                                             + rs.afterLevel * touchPressure
                                             + subLevel
                                             + lfoAmp
                                             + contourLevel
                                             + mod.level,
                                         0.0f, 2.6f));
            const float amp = r.aenv.tick(sr, true, usesSourceModels(global.renderPolicy));
            r.info = { cutoffHp, cutoffLp, fenv, amp, freq, keyVolts, qHp, qLp, rs.hpfOn, rs.lpfOn };
            const float chGain = ch == 0 ? (1.0f - global.mix) : global.mix;
            // Velocity acts through the selected initial-touch destinations.
            // An unconditional velocity gain made disabled touch controls audible.
            const float vcaInput = filtered * rs.vcfLevel + sine;
            float pan = clampf(rs.pan + r.trimPan * dispersion * 0.8f + lfoPan + mod.pan, -1.0f, 1.0f);
            float voiceOut, panLeft, panRight;
            if (circuitVca)
            {
                // IG00151: LC on the linear input through its 10 nF node, SUB on
                // the exponential input; constant-power rank pan.
                if (r.vcaCold)
                {
                    r.vca.reset(levelCv);
                    r.vcaCold = false;
                }
                voiceOut = r.vca.process(vcaInput, levelCv, subValue * global.subVca * kIg00151SubOctaves, amp)
                           * chGain * 0.17f;
                constantPowerPan(pan, panLeft, panRight);
            }
            else
            {
                voiceOut = vcaInput * levelCv * amp * chGain * 0.17f;
                panLeft = 0.5f - 0.5f * pan;
                panRight = 0.5f + 0.5f * pan;
            }
            mixL += voiceOut * panLeft;
            mixR += voiceOut * panRight;
        }

        v.idleControlsPrimed = true;
        if (v.ranks[0].aenv.isIdle() && v.ranks[1].aenv.isIdle())
        {
            v.active = false;
            v.held = false;
            v.sustained = false;
        }
    }

    heldCount = stillHeld;
}

void Engine::mixBus(const SampleContext& context, float& mixL, float& mixR)
{
    const float sr = context.sr, invSr = context.invSr;
    const bool sharedNoise = context.sharedNoise;
    const auto& modernGlobal = modern.getGlobalOffsets();


    // Ring AD controls oscillator FREQUENCY. Keep it clocking while bypassed
    // so enabling MODULATION does not resurrect a frozen envelope.
    ringEnv = ringAdsr.tick(sr, false, usesSourceModels(global.renderPolicy));
    // Circuit engine: ADJ p19-anchored exponential converter and offset multiplier
    // (RingModulatorCircuit.h, ws4-circuits.md Step 2). Legacy: provisional law, ideal multiply.
    ringFrequency = clampf(usesSourceModels(global.renderPolicy)
                               ? ringFrequencyMemo(global.ringSpeed, global.ringDepth, ringEnv)
                               : ringModulationFrequency(global.ringSpeed, global.ringDepth, ringEnv),
                           0.0f, sr * 0.4f);
    ringPhase += ringFrequency * invSr;
    ringPhase -= std::floor(ringPhase);
    if (global.ringAmount > 0.001f)
    {
        const float carrier = std::sin(kTwoPi * ringPhase);
        const float amount = clampf(global.ringAmount, 0.0f, 1.0f);
        if (usesSourceModels(global.renderPolicy))
        {
            mixL = RingModulatorCircuit::blend(mixL, carrier, amount);
            mixR = RingModulatorCircuit::blend(mixR, carrier, amount);
        }
        else
        {
            mixL = lerp(mixL, mixL * carrier, amount);
            mixR = lerp(mixR, mixR * carrier, amount);
        }
    }

    // PRA EXP-WAH: documented ring -> PRA -> OE1/OE2 -> expression order
    // (docs/OE1_SIGNAL_ROUTING_EVIDENCE.md). The 12ms switching crossfade is
    // an inferred de-click policy, not a recovered FET transient law.
    {
        const float wahTarget = global.pedalWah ? 1.0f : 0.0f;
        if (wahMix != wahTarget)
        {
            wahMix += (wahTarget - wahMix) * wahSmoothing;
            if (std::abs(wahMix - wahTarget) < 1.0e-4f)
                wahMix = wahTarget;
        }
        if (usesSourceModels(global.renderPolicy))
        {
            praWahL.setPedal(expression);
            praWahR.setPedal(expression);
            // Always clock all four physical capacitor states, including when
            // EXP selects dry. Dry X includes input coupling/source loading;
            // wet Y already includes VR2 gain: no SVF or extra normalization.
            const auto left = praWahL.process(mixL);
            const auto right = praWahR.process(mixR);
            mixL = lerp((float) left.x, (float) left.y, wahMix);
            mixR = lerp((float) right.x, (float) right.y, wahMix);
        }
        else if (wahMix > 0.0f)
        {
            const float pedalHz = wahCentreHz(expression);
            // Start on the pedal position rather than sweeping up from a stale value.
            wahFrequency = wahRunning ? wahFrequency + (pedalHz - wahFrequency) * wahSmoothing : pedalHz;
            wahRunning = true;
            wahL.set(wahFrequency, kProvisionalWahQ, sr);
            wahR.set(wahFrequency, kProvisionalWahQ, sr);
            // Svf's band-pass peaks at Q; normalise to unity before make-up gain.
            constexpr float bandGain = kProvisionalWahMakeup / kProvisionalWahQ;
            mixL = lerp(mixL, wahL.process(mixL).bp * bandGain, wahMix);
            mixR = lerp(mixR, wahR.process(mixR).bp * bandGain, wahMix);
        }
        else if (wahRunning)
        {
            wahL.reset();
            wahR.reset();
            wahRunning = false;
        }
    }

    // Shared RNG draws consumed in both engines (start-phase sequence kept).
    float hissL = audioRng.nextBipolar();
    float hissR = audioRng.nextBipolar();
    // Circuit engine: declared mix-bus hiss PSD, one independent source per channel.
    if (sharedNoise)
        circuitNoise.hiss(hissL, hissR);
    mixL += hissL * global.analogNoise * 0.0008f;
    mixR += hissR * global.analogNoise * 0.0008f;

    // Legacy: provisional generic delay-line chorus/tremolo. Source policies: OE2 BBD
    // delay path (docs/BBD_CHORUS_MODEL.md); its inferred constants are not calibrated.
    const auto tabs = chorusSwitches(global.chorusOn, global.chorusTremolo, global.chorusTabs);
    const auto ch = usesSourceModels(global.renderPolicy)
        ? bbdChorus.processIndependent(mixL, mixR, global.chorusSpeed, global.chorusDepth,
                                       tabs.chorus, tabs.tremolo)
        : chorus.process(mixL, mixR, global.chorusSpeed, global.chorusDepth,
                         global.chorusOn, global.chorusTremolo);
    mixL = ch.l;
    mixR = ch.r;

    // Three modern FX slots, processed in series after the provisional CS-80
    // chorus. The type numbers are fixed by the UI: 1 chorus, 2 phaser,
    // 3 tape delay, 4 plate reverb, 5 drive.
    for (int slot = 0; slot < 3; ++slot)
    {
        // A modern model (ws5) replaces the classic type of its slot; model 0 is classic.
        fx::SlotControls controls;
        controls.type = global.fxType[slot];
        controls.model = global.fxModel[slot];
        controls.mix = clampf(global.fxMix[slot] + modernGlobal.fxMix[(std::size_t) slot], 0.0f, 1.0f);
        controls.a = global.fxParamA[slot];
        controls.b = global.fxParamB[slot];
        controls.oversampling = global.fxOversampling[slot];
        controls.gainComp = global.fxGainComp;
        const auto out = fxSlots[(std::size_t) slot].process(controls, mixL, mixR, driftPhase, sr);
        mixL = out.l;
        mixR = out.r;
    }

}

void Engine::tickOutput(const SampleContext& context, float mixL, float mixR, float& outL, float& outR)
{
    const float invSr = context.invSr;
    const bool smoothControls = context.smoothControls;
    // 10 Hz DC blocker. It removes the offset from asymmetric oscillator
    // resets without eating the low end of the instrument.
    const float dcCoeff = smoothControls ? dcCoefficientExp(-kTwoPi * 10.0f * invSr)
                                         : std::exp(-kTwoPi * 10.0f * invSr);
    const float dcOutL = mixL - dcInL + dcCoeff * dcL;
    const float dcOutR = mixR - dcInR + dcCoeff * dcR;
    dcInL = mixL;
    dcInR = mixR;
    dcL = dcOutL;
    dcR = dcOutR;

    // Legacy keeps its historical per-sample constant. The circuit engine's
    // volume is already smoothed with the other controls, in time.
    smoothedVolume = usesSourceModels(global.renderPolicy)
                         ? global.volume : smoothedVolume + (global.volume - smoothedVolume) * 0.003f;
    const float gain = smoothedVolume * 4.0f
                       * (smoothControls ? controllerSmoothing.expressionGain : expression);
    // Source policies remove the arbitrary output conditioner, not the
    // provisional filter saturation law. No calibrated output/headroom claim.
    outL = usesSourceModels(global.renderPolicy) ? mixL * gain : softClip(dcOutL * gain);
    outR = usesSourceModels(global.renderPolicy) ? mixR * gain : softClip(dcOutR * gain);

    if (usesSourceModels(global.renderPolicy))
    {
        // Existing float post-volume/expression values define normalized ideal
        // LEFT/RIGHT preamp-source units, not calibrated LI/RI voltages. GENERAL
        // is formed before separate coupling/loading and always clocked, even
        // when no voice sounds. Only LEFT/RIGHT reach the host in source policies.
        // Phase 1.5 gain staging: fixed digital reference, then the inferred
        // driver headroom, then the passive tap/coupling network.
        const double driveL = circuitOutputDriver(outL * kCircuitOutputReference);
        const double driveR = circuitOutputDriver(outR * kCircuitOutputReference);
        outputNodes = outputCircuit.process(driveL, driveR);
        // HL/HR leave the preamps before the coupling capacitors and HIGH/LOW
        // switch. The headphone stage is always clocked, like the three jacks.
        const double phonesLeft = phonesL.process(driveL), phonesRight = phonesR.process(driveR);
        // LISTENING OUTPUT (Phase 3): which physical output the host hears.
        switch (listeningOutput)
        {
            case 1:
                outL = outR = (float) outputNodes.general.audibleJackVolts;
                break;
            case 2:
                outL = (float) phonesLeft;
                outR = (float) phonesRight;
                break;
            default:
                outL = (float) outputNodes.left.audibleJackVolts;
                outR = (float) outputNodes.right.audibleJackVolts;
                break;
        }
        listeningSample = { outL, outR };
    }
    else
    {
        // Historical scalar HIGH/LOW policy; arithmetic and order unchanged.
        // Do not apply this extra attenuation to the reconstructed output graph.
        const float outputTarget = outputLevelGain(global.outputHigh);
        if (outputGain != outputTarget)
        {
            outputGain += (outputTarget - outputGain) * outputGainSmoothing;
            if (std::abs(outputGain - outputTarget) < 1.0e-6f)
                outputGain = outputTarget;
        }
        outL *= outputGain;
        outR *= outputGain;
    }

    if (! global.stereo)
    {
        const float m = (outL + outR) * 0.5f;
        outL = outR = m;
    }
}

void Engine::processInternalSample(float& outL, float& outR, float externalModulation)
{
    const auto context = tickGlobals(externalModulation);
    float mixL = 0.0f, mixR = 0.0f;
    tickRanks(context, mixL, mixR);
    mixBus(context, mixL, mixR);
    tickOutput(context, mixL, mixR, outL, outR);
    if (context.smoothControls)
        endSmoothedControls();
}

void Engine::render(float* interleavedStereo, int numFrames, const float* externalMono)
{
    if (interleavedStereo == nullptr || numFrames <= 0)
        return;

    externalInput.setConnected(externalMono != nullptr);
    for (int i = 0; i < numFrames; ++i)
    {
        const auto external = externalInput.tick(externalMono != nullptr ? externalMono[i] : 0.0f);
        float lastL = 0.0f, lastR = 0.0f;
        const int retainedPhase = decL.getRetainedPhase();
        for (int os = 0; os < oversample; ++os)
        {
            float l = 0.0f, r = 0.0f;
            processInternalSample(l, r, external[(size_t) os]);
            decL.push(l);
            decR.push(r);
            // Evaluate only the retained last phase, using symmetric tap pairs.
            if (os == retainedPhase)
            {
                lastL = decL.output();
                lastR = decR.output();
            }
        }
        interleavedStereo[i * 2] = lastL;
        interleavedStereo[i * 2 + 1] = lastR;
    }
}

int Engine::activeVoices() const noexcept
{
    int n = 0;
    for (const auto& v : voices)
        if (v.active)
            ++n;
    return n;
}

Engine::SourceToneInfo Engine::getSourceToneInfo(int rank) const noexcept
{
    if (rank < 0 || rank >= kNumRanks)
    {
        SourceToneInfo result;
        result.selector = -1;
        result.route = source_factory::Route::invalid;
        return result;
    }
    SourceToneInfo result;
    result.selector = hardwareSources[rank];
    result.route = sourceRoutes[rank];
    if (sourceActive[rank]) result.provenance = sourceTones[rank].provenance;
    else if (const auto* address = source_factory::address(rank, result.selector))
    {
        result.provenance.unknownCells = source_factory::unknownMask(*address);
        result.provenance.knownCells = source_factory::allOutputsMask & ~result.provenance.unknownCells;
    }
    return result;
}

source_factory::CutoffReadout Engine::getSourceCutoffReadout(int rank) const noexcept
{
    return rank >= 0 && rank < kNumRanks ? sourceCutoff[rank] : source_factory::CutoffReadout{};
}

Engine::VoiceInfo Engine::getVoiceInfo(int index) const
{
    if (index < 0 || index >= kNumVoices)
        return {};
    const auto& v = voices[(size_t) index];
    return { v.active, v.note, v.midiChannel, v.velocity, v.pressure, v.mpeBend, v.ribbonRatio, v.held, v.pedalHeld,
             v.touchA, v.touchB, v.touchC, v.age };
}

Engine::RankInfo Engine::getRankInfo(int voice, int rank) const
{
    if (voice < 0 || voice >= kNumVoices || rank < 0 || rank >= kNumRanks)
        return {};
    return voices[(size_t) voice].ranks[rank].info;
}

Engine::RankAudioState Engine::getRankAudioState(int voice, int rank) const noexcept
{
    if (voice < 0 || voice >= kNumVoices || rank < 0 || rank >= kNumRanks)
        return {};
    const auto& r = voices[(size_t) voice].ranks[rank];
    if (!usesSourceModels(global.renderPolicy))
        return { r.osc.getPhase(), r.hpf.getState(), r.lpf.getState(), {}, {} };
    const auto hp = r.hpStage.getState(), lp = r.lpStage.getState();
    return { static_cast<float>(r.wsc.getPhase()), { (float) hp.bp, (float) hp.lp },
             { (float) lp.bp, (float) lp.lp }, hp, lp };
}

Engine::RankEnvelopeState Engine::getRankEnvelopeState(int voice, int rank) const noexcept
{
    if (voice < 0 || voice >= kNumVoices || rank < 0 || rank >= kNumRanks)
        return {};
    const auto& r = voices[(size_t) voice].ranks[rank];
    return { r.aenv.getState(), r.fenv.getState() };
}

Engine::PraWahState Engine::getPraWahState(int channel) const noexcept
{
    if (channel < 0 || channel > 1)
        return {};
    const auto& circuit = channel == 0 ? praWahL : praWahR;
    return { circuit.capacitorState(), circuit.sampleRate(), circuit.opticalResistance(),
             circuit.targetOpticalResistance(), wahMix };
}

Engine::OutputTapState Engine::getOutputTapState(int tap) const noexcept
{
    if (tap < 0 || tap > 2)
        return {};
    const auto& circuit = tap == 0 ? outputCircuit.leftCircuit()
                          : tap == 1 ? outputCircuit.rightCircuit() : outputCircuit.generalCircuit();
    const auto& nodes = tap == 0 ? outputNodes.left : tap == 1 ? outputNodes.right : outputNodes.general;
    return { circuit.getState(), nodes, circuit.sampleRate(), circuit.getMode() };
}

} // namespace cs80
