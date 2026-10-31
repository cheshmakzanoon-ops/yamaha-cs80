#include "Engine.h"

#include <algorithm>
#include <cmath>

namespace cs80
{

Engine::Engine()
{
    originalSubSmoothing = (float) (-std::expm1(-1.0 / (0.010 * internalRate)));
    wahSmoothing = (float) (-std::expm1(-1.0 / (0.012 * internalRate)));
    outputGainSmoothing = (float) (-std::expm1(-1.0 / (0.005 * internalRate)));
    initialPitchDecay = std::exp(-1.0 / (kNominalInitialPitchDecaySeconds * internalRate));
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

void Engine::reset()
{
    for (auto& v : voices)
    {
        v.active = false;
        v.held = false;
        v.sustained = false;
        v.pedalHeld = false;
        v.note = 60;
        v.age = 0;
        v.initialPitchTransient = 0.0;
        for (auto& r : v.ranks)
        {
            r.osc.reset(0.0f);
            r.hpf.reset();
            r.lpf.reset();
            r.fenv.reset();
            r.aenv.reset();
        }
    }
    heldCount = 0;
    sustainPedals.fill(false);
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
    nextVoice = 0;
    globalBend = 0.0f;
    modWheel = 0.0f;
    expression = 1.0f;
    ribbon = 0.0f;
    smoothedVolume = global.volume;
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
}

void Engine::setParams(const ParamValues& values)
{
    hardwareSources = {13, 13};
    sourceRoutes.fill(source_factory::Route::panel_or_memory);
    sourceActive.fill(false);
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
        const bool sourceModels = usesSourceModels(renderPolicy(frame.effectiveValues[P_parityMode],
                                                                frame.effectiveValues[P_sourcePerformance]));
        if (selector != hardwareSources[rank] || sourceModels != usesSourceModels(global.renderPolicy))
            routes[rank] = source_factory::route(rank, selector, sourceModels);
        if (source_factory::isSourceDriven(routes[rank])
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
    for (int rank = 0; rank < kNumRanks; ++rank)
        sourceActive[rank] = source_factory::isSourceDriven(routes[rank]);
    sourceCutoff = {};
    sourceShared = {};
    applyParams(frame.effectiveValues);
    controlFrameStatus = ControlFrameStatus::accepted;
    return true;
}

void Engine::applyParams(const ParamValues& values)
{
    params = values;
    // Sanitize once at the core boundary before integer casts and DSP laws.
    for (int i = 0; i < P_numParams; ++i)
    {
        const auto& def = paramDef(i);
        const float value = params[i];
        params[i] = std::isfinite(value) ? clampf(value, def.min, def.max) : def.def;
    }
    const auto previousPolicy = global.renderPolicy;
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
                // all eight circuits, not only currently audible voices.
                if (!voice.active && !sourceActive[ch]) continue;
                auto& rank = voice.ranks[ch];
                const auto& settings = rankSettings[ch];
                if (voice.active) rank.aenv.setSustain(settings.vS);
                rank.fenv.setLevels(filterInitialTarget(ch), filterAttackTarget(ch));
                if (voice.active)
                    rank.aenv.setTimes(std::max(0.001f, settings.vA * global.envTimeScale),
                                       std::max(0.001f, settings.vD * global.envTimeScale),
                                       std::max({settings.vR, global.keyRelease, performanceRelease})
                                           * global.envTimeScale);
                rank.fenv.setTimes(std::max(0.001f, settings.fA * global.envTimeScale),
                                   std::max(0.001f, settings.fD * global.envTimeScale),
                                   std::max({settings.fR, global.keyRelease, performanceRelease})
                                       * global.envTimeScale);
            }
        }
    }

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
    global.chorusSpeed = p(P_chorusSpeed);
    global.chorusDepth = p(P_chorusDepth);
    global.stereo = p(P_outputMode) >= 0.5f;
    global.voiceDispersion = p(P_voiceDispersion);
    global.drift = p(P_drift);
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
    global.fxType[0] = (int) std::lround(p(P_fx1Type));
    global.fxType[1] = (int) std::lround(p(P_fx2Type));
    global.fxType[2] = (int) std::lround(p(P_fx3Type));
    global.fxMix[0] = p(P_fx1Mix);
    global.fxMix[1] = p(P_fx2Mix);
    global.fxMix[2] = p(P_fx3Mix);
    global.macro[0] = p(P_macro1);
    global.macro[1] = p(P_macro2);
    global.macro[2] = p(P_macro3);
    global.macro[3] = p(P_macro4);
    global.arpOn = p(P_arpOn) >= 0.5f;
    global.arpMode = std::clamp((int) std::lround(p(P_arpMode)), 0, 4);
    global.arpRate = std::max(0.1f, p(P_arpRate));
    global.arpOctaves = std::clamp((int) std::lround(p(P_arpOctaves)), 1, 4);

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
            global.fxType[i] = 0;
            global.fxMix[i] = 0.0f;
        }
        for (float& macro : global.macro)
            macro = 0.5f;
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
            // frozen common-bus bias. Cold never-triggered Idle remains at zero.
            // Inactive touch is explicitly zero; no idle oscillator/audio-filter
            // clocks or state resets are introduced by this control-only model.
            drives[rank][voice] = {z, v.active ? v.velocity : 0.0f, v.active ? v.pressure : 0.0f};
        }
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

void Engine::startVoice(Voice& v, int note, float velocity, int midiChannel)
{
    // Bounded continuity only: inactive circuits still use the legacy reset.
    // This does not clock idle circuits or model their unknown held key voltage.
    const bool preserveAudioState = usesSourceModels(global.renderPolicy) && v.active
                                    && !v.held && !v.pedalHeld;
    // chooseVoice only returns an unheld circuit.
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
    v.targetPitch = (float) note;
    v.contour = keyboardContourWeights(note);
    v.glidePitch = global.glideGate && global.portamento > 0.0001f ? (float) lastNote : (float) note;
    // TWS C precedes the TRG4 PB control. Keep its nominal onset state alive
    // even when PB depth is zero. Reset-to-velocity is still a MIDI/retrigger
    // approximation, not a complete model of the b sample/hold and clamp.
    v.initialPitchTransient = -(double) v.velocity;
    v.ribbonRatio = ribbonFrequencyRatio(ribbon + params[P_ribbon]);
    v.age = ++voiceCounter;
    lastNote = note;
    ++heldCount;

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
            r.hpf.reset();
            r.lpf.reset();
        }
        // Filter envelope: a reused sounding tail restarts its attack from the
        // capacitor's present level (source policies, IMPROVEMENT_PLAN 1.3);
        // legacy keeps the historical reset to IL.
        if (!preserveAudioState)
            r.fenv.reset();
        // Amplitude envelope: a reused sounding tail restarts its attack from the level it has
        // reached instead of from zero (source policies only). Zeroing it removed the whole tail in
        // one sample, a step of up to 0.022 full scale across eight sampled factory programs (0.007
        // now), a tick under a passage. Inferred from the same reasoning as the oscillator and filter
        // memories above (no traced reset path); the real envelope capacitor behaviour is not
        // measured. Legacy keeps the reset.
        if (!preserveAudioState)
            r.aenv.reset();
        r.fenv.noteOn(filterInitialTarget(ch),
                      filterAttackTarget(ch),
                      std::max(0.001f, rs.fA * global.envTimeScale),
                      std::max(0.001f, rs.fD * global.envTimeScale),
                      std::max(0.001f, rs.fR * global.envTimeScale),
                      0.0f,
                      preserveAudioState);
        r.aenv.noteOn(std::max(0.001f, rs.vA * global.envTimeScale),
                      std::max(0.001f, rs.vD * global.envTimeScale),
                      rs.vS,
                      std::max(0.001f, rs.vR * global.envTimeScale));
    }

    if (! ringKeyDown)
    {
        ringKeyDown = true;
        ringPhase = 0.0f;
        ringEnv = 0.0f;
        ringAdsr.reset();
        ringAdsr.noteOn(global.ringAttack, global.ringDecay, 0.0f, 0.35f);
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
                          * global.envTimeScale);
        r.fenv.setRelease(std::max({rs.fR, global.keyRelease, performanceRelease})
                          * global.envTimeScale);
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
                    rank.fenv.reset();
                }
                v.active = v.sustained = v.pedalHeld = false;
            }
    }

    if (global.arpOn)
    {
        rememberHeldKey(midiNote, velocity, midiChannel);
        return;
    }

    const int idx = chooseVoice();
    if (idx < 0)
        return; // all circuits occupied; leave held/ring state untouched
    startVoice(voices[(size_t) idx], midiNote, velocity, midiChannel);
}

bool Engine::isSustainPedalDown(int midiChannel) const noexcept
{
    if (midiChannel < 1 || midiChannel > 16)
        return false;
    return sustainPedals[(size_t) midiChannel] || (global.mpe && sustainPedals[1]);
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
            r.aenv.setRelease(std::max(rs.vR, global.keyRelease) * global.envTimeScale);
            r.fenv.setRelease(std::max(rs.fR, global.keyRelease) * global.envTimeScale);
        }
    }
}

void Engine::noteOff(int midiNote, int midiChannel)
{
    if (global.arpOn)
    {
        forgetHeldKey(midiNote, midiChannel);
        return;
    }
    if (auto* voice = findVoiceForNote(midiNote, midiChannel))
        releaseKey(*voice);
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
            // Legacy/PANEL paths and Sustain-II reset semantics are unchanged.
            for (int rank = 0; rank < kNumRanks; ++rank)
                if (sourceActive[rank])
                {
                    auto& fenv = voice.ranks[rank].fenv;
                    const auto stage = fenv.getState().stage;
                    if (stage == FilterEnv::Stage::Attack || stage == FilterEnv::Stage::Decay
                        || stage == FilterEnv::Stage::Sustain)
                        fenv.noteOff();
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
    for (auto& v : voices)
        if (v.active && (midiChannel == 1 || v.midiChannel == midiChannel))
            v.pressure = pressure;
}

void Engine::pitchBend(int midiChannel, float value)
{
    if (global.parityMode)
        return;
    value = clampf(value, -1.0f, 1.0f);
    if (global.mpe && midiChannel != 1)
    {
        for (auto& v : voices)
            if (v.active && v.midiChannel == midiChannel)
                v.mpeBend = value * global.mpeBendRange;
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
        }
    }
    else if (controller == 74)
    {
        for (auto& v : voices)
            if (v.active && (midiChannel == 1 || v.midiChannel == midiChannel))
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

void Engine::processInternalSample(float& outL, float& outR, float externalModulation)
{
    const float sr = (float) internalRate;
    const float invSr = 1.0f / sr;

    updateArpeggiator(invSr);

    // Global modulation oscillators.
    for (int i = 0; i < 3; ++i)
        lfoValues[i] = modLfos[i].tick(global.lfoRate[i], sr, 0, modRng);

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
    const float pwmSine[2] = { std::sin(kTwoPi * pwmPhase[0]), std::sin(kTwoPi * pwmPhase[1]) };

    // TRG5 positive diode-OR: normalized ideal maximum, not event memory.
    // Physical release removes this proxy even if a pedal/release tail sounds.
    // Arpeggiator extension uses its generated held voice gates.
    float sharedPressure = 0.0f;
    for (const auto& voice : voices)
        if (voice.active && voice.held)
            sharedPressure = std::max(sharedPressure, voice.pressure);
    if (global.subOriginalMode)
    {
        const float speed = OriginalSubOscillator::targetRate(
            global.subOriginalSpeed, global.subAtSpeed, sharedPressure);
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
        subSpeedSmoothed += (speed - subSpeedSmoothed) * 0.0015f;
        subValue = subLfo.tick(subSpeedSmoothed, sr, global.subWave, modRng);
    }
    driftPhase += 0.031f * invSr;
    driftPhase -= std::floor(driftPhase);

    float mixL = 0.0f;
    float mixR = 0.0f;
    int stillHeld = 0;

    // Double accumulator avoids long-glide float rounding stalling before target.
    // An open assigned footswitch suppresses new glides and snaps running ones to target.
    const bool glideActive = global.glideGate && global.portamento > 0.0001f;
    const double glideCoef = glideActive
                                ? glideExp(-1.0 / ((double) global.portamento * sr))
                                : 0.0;

    if (sourceActive[0] || sourceActive[1])
        updateSourceReadouts(sr);

    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[(size_t) vi];
        if (! v.active)
            continue;

        if (v.held || v.pedalHeld)
            ++stillHeld;

        if (glideActive)
            v.glidePitch = v.targetPitch + (v.glidePitch - v.targetPitch) * glideCoef;
        else
            v.glidePitch = v.targetPitch;

        // Conditional TWS C positive-step response: 1 uF * (100k || 100k).
        // Coefficient is computed at prepare, not in this sample loop. Do not
        // freeze a permanent detuning residual; snap only a negligible state
        // to zero well before double-precision denormals are approached.
        v.initialPitchTransient *= initialPitchDecay;
        if (std::abs(v.initialPitchTransient) < 1.0e-15)
            v.initialPitchTransient = 0.0;

        // Sustain I holds each released voice's last ribbon pitch. Sustain II
        // keeps release tails coupled to subsequent ribbon gestures.
        if (v.held || v.pedalHeld || (global.sustainMode == 1 && global.sustainGate))
            v.ribbonRatio = ribbonFrequencyRatio(ribbon + params[P_ribbon]);

        const float glideOffset = v.glidePitch - (float) v.note;
        const float dispersion = global.voiceDispersion;
        const float driftVoice = global.drift * std::sin(kTwoPi * (driftPhase + voiceDrift[vi])) * 0.9f;

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& r = v.ranks[ch];
            auto& rs = rankSettings[ch];

            float pitch = (float) v.note
                          + global.masterTune
                          + (ch == 1 ? global.detuneCents / 100.0f : 0.0f)
                          + globalBend
                          + v.mpeBend
                          + glideOffset
                          + (float) (global.initBend * v.initialPitchTransient)
                          + lfoPitch
                          + subValue * (global.subVco + global.subAtVco * v.pressure) * 1.5f
                          + subValue * modWheel * global.modWheelSub * 2.0f
                          + r.trimTune * dispersion * (kProvisionalVoiceTuneSpreadCents / 100.0f)
                          + driftVoice;

            if (global.glideMode == 2 && global.glideGate)
                pitch = std::round(pitch);

            // Transposition is a separate nominal harmonic ratio, not rounded
            // equal-tempered fifths and not part of the shared filter key CV.
            const float freq = midiToHz(pitch, global.tuningRef) * v.ribbonRatio
                               * kFeetRatios[std::clamp(rs.feet, 0, 5)];
            const float pwm = pwmSine[ch];
            const float pw = clampf(rs.pulseWidth + rs.pwmDepth * 0.35f * pwm + lfoPwm, 0.05f, 0.95f);

            const auto osc = r.osc.tick(freq, sr, pw, rs.saw, rs.pulse, 0.0f, audioRng);

            float x = osc.saw + osc.pulse;
            x += audioRng.nextBipolar() * rs.noise * 0.8f;
            x *= 0.9f;

            // Shared key-voltage domain precedes VCO footage/vibrato. Master
            // pitch and key portamento move it; ribbon routing is unverified.
            const float keyVolts = filterKeyVolts(v.glidePitch + global.masterTune, r.keyVoltsExp);
            const float brillOct = global.brilliance
                                   + (sourceActive[ch] ? 0.0f : rs.initBrill * v.velocity * 2.0f)
                                   + (sourceActive[ch] ? 0.0f : rs.afterBrill * v.pressure * 2.0f)
                                   + v.timbre * 0.8f
                                   + modWheel * global.modWheelVcf * 1.5f
                                   // Owner manual p39: pressure adds SUB VCF depth,
                                   // not a static cutoff offset. The 1.5-octave scale
                                   // is provisional, not a measured circuit gain.
                                   + subValue * (global.subVcf + global.subAtVcf * v.pressure) * 1.5f
                                   + lfoFilter
                                   // Keyboard contour: zero (exactly) when both levers are centred.
                                   + keyboardBrillianceOctaves(global.kbdBrillianceLow,
                                                               global.kbdBrillianceHigh, v.contour);

            const float fenv = sourceActive[ch] ? sourceFilterValues[ch][vi]
                : r.fenv.tick(sr, true, usesSourceModels(global.renderPolicy));
            const float envLp = fenv;
            const float envHp = fenv * kProvisionalHpfEnvelopeGain;

            const float cutoffLp = sourceActive[ch]
                ? nominalFilterCutoff(keyVolts, static_cast<float>(sourceCutoff[ch].effectiveCv[vi][1]) + brillOct, r.lpCutoffExp)
                : nominalFilterCutoff(keyVolts, rs.lpfEffectiveCv + brillOct + envLp, r.lpCutoffExp);
            const float cutoffHp = sourceActive[ch]
                ? nominalFilterCutoff(keyVolts, static_cast<float>(sourceCutoff[ch].effectiveCv[vi][0]) + brillOct, r.hpCutoffExp)
                : nominalFilterCutoff(keyVolts, rs.hpfEffectiveCv + brillOct + envHp, r.hpCutoffExp);
            const float qLp = sourceActive[ch] ? static_cast<float>(sourceShared.q[ch][vi][1])
                : provisionalFilterQ(rs.lpfRes * 0.8f + global.resonance * 0.6f);
            const float qHp = sourceActive[ch] ? static_cast<float>(sourceShared.q[ch][vi][0])
                : provisionalFilterQ(rs.hpfRes * 0.8f + global.resonance * 0.6f);

            float filtered = x;
            // Circuit engine: IG00156 stages with in-loop OTA saturation and the
            // source-derived 4.7 MOhm feedforward floor. Legacy: unchanged Svf.
            const bool circuitFilter = usesSourceModels(global.renderPolicy);
            const float cutoffFloor = circuitFilter ? kIg00156FeedforwardHz : 0.0f;
            if (rs.hpfOn)
            {
                r.hpf.set(cutoffHp + cutoffFloor, qHp, sr);
                filtered = circuitFilter ? r.hpf.processCircuit(filtered).hp : r.hpf.process(filtered).hp;
            }
            if (rs.lpfOn)
            {
                r.lpf.set(cutoffLp + cutoffFloor, qLp, sr);
                filtered = circuitFilter ? r.lpf.processCircuit(filtered).lp : r.lpf.process(filtered).lp;
            }

            const float sine = osc.sine * rs.sine;
            // Source LC/LI is linear in its explicit unit scale. Retain global
            // effective transports once; floor negative total gain at zero,
            // but do NOT apply the legacy 2.6 cap or per-tone normalization.
            const float levelCv = sourceActive[ch]
                ? std::max(0.0f, static_cast<float>(sourceShared.levelGain[ch][vi])
                             + subValue * global.subVca * 0.45f + lfoAmp
                             + keyboardLevelOffset(global.kbdLevelLow, global.kbdLevelHigh, v.contour))
                : clampf(rs.vLevel
                                             + rs.initLevel * v.velocity
                                             + rs.afterLevel * v.pressure
                                             + subValue * global.subVca * 0.45f
                                             + lfoAmp
                                             + keyboardLevelOffset(global.kbdLevelLow,
                                                                   global.kbdLevelHigh, v.contour),
                                         0.0f, 2.6f);
            const float amp = r.aenv.tick(sr, true, usesSourceModels(global.renderPolicy));
            r.info = { cutoffHp, cutoffLp, fenv, amp, freq, keyVolts, qHp, qLp, rs.hpfOn, rs.lpfOn };
            const float chGain = ch == 0 ? (1.0f - global.mix) : global.mix;
            // Velocity acts through the selected initial-touch destinations.
            // An unconditional velocity gain made disabled touch controls audible.
            float voiceOut = (filtered * rs.vcfLevel + sine)
                             * levelCv * amp * chGain * 0.17f;

            float pan = clampf(rs.pan + r.trimPan * dispersion * 0.8f + lfoPan, -1.0f, 1.0f);
            mixL += voiceOut * (0.5f - 0.5f * pan);
            mixR += voiceOut * (0.5f + 0.5f * pan);
        }

        if (v.ranks[0].aenv.isIdle() && v.ranks[1].aenv.isIdle())
        {
            v.active = false;
            v.held = false;
            v.sustained = false;
        }
    }

    heldCount = stillHeld;

    // Ring AD controls oscillator FREQUENCY. Keep it clocking while bypassed
    // so enabling MODULATION does not resurrect a frozen envelope.
    ringEnv = ringAdsr.tick(sr, false, usesSourceModels(global.renderPolicy));
    ringFrequency = clampf(ringModulationFrequency(global.ringSpeed,
                                                   global.ringDepth, ringEnv),
                           0.0f, sr * 0.4f);
    ringPhase += ringFrequency * invSr;
    ringPhase -= std::floor(ringPhase);
    if (global.ringAmount > 0.001f)
    {
        const float carrier = std::sin(kTwoPi * ringPhase);
        const float amount = clampf(global.ringAmount, 0.0f, 1.0f);
        mixL = lerp(mixL, mixL * carrier, amount);
        mixR = lerp(mixR, mixR * carrier, amount);
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

    mixL += audioRng.nextBipolar() * global.analogNoise * 0.0008f;
    mixR += audioRng.nextBipolar() * global.analogNoise * 0.0008f;

    // Legacy: provisional generic delay-line chorus/tremolo. Source policies: OE2 BBD
    // delay path (docs/BBD_CHORUS_MODEL.md); its inferred constants are not calibrated.
    const auto ch = usesSourceModels(global.renderPolicy)
        ? bbdChorus.process(mixL, mixR, global.chorusSpeed, global.chorusDepth,
                            global.chorusOn, global.chorusTremolo)
        : chorus.process(mixL, mixR, global.chorusSpeed, global.chorusDepth,
                         global.chorusOn, global.chorusTremolo);
    mixL = ch.l;
    mixR = ch.r;

    // Three modern FX slots, processed in series after the provisional CS-80
    // chorus. The type numbers are fixed by the UI: 1 chorus, 2 phaser,
    // 3 tape delay, 4 plate reverb, 5 drive.
    for (int slot = 0; slot < 3; ++slot)
    {
        auto& fx = fxSlots[(std::size_t) slot];
        const int type = global.fxType[slot];
        const float amount = global.fxMix[slot];
        const int live = (amount <= 0.001f || type <= 0) ? 0 : type;
        if (live != fx.liveType)
        {
            // A slot that goes quiet, or changes effect, must not keep the old effect's audio:
            // it would replay it, late, the next time the slot is switched on.
            fx.clear(fx.liveType);
            fx.liveType = live;
        }
        if (live == 0)
            continue;

        switch (live)
        {
            case 1: // modern stereo chorus
            {
                const auto m = fx.chorus.process(mixL, mixR, 0.62f, 0.62f, true, false);
                mixL = lerp(mixL, m.l, amount * 0.62f);
                mixR = lerp(mixR, m.r, amount * 0.62f);
                break;
            }
            case 2: // six-stage phaser
            {
                const auto p = fx.phaser.process(mixL, mixR, 0.32f, 0.72f);
                mixL = lerp(mixL, p.l, amount * 0.72f);
                mixR = lerp(mixR, p.r, amount * 0.72f);
                break;
            }
            case 3: // tape delay with cross-feedback and wow
            {
                const float wow = std::sin(kTwoPi * driftPhase) * 0.0007f;
                const float d = (0.34f + wow) * sr;
                const float wl = fx.dampL.process(fx.delayL.read(d));
                const float wr = fx.dampR.process(fx.delayR.read(d));
                fx.delayL.write(mixL + wr * 0.36f);
                fx.delayR.write(mixR + wl * 0.36f);
                mixL = lerp(mixL, wl, amount * 0.58f);
                mixR = lerp(mixR, wr, amount * 0.58f);
                break;
            }
            case 4: // stereo plate reverb
            {
                const float wl = fx.reverb.process(mixL, 0);
                const float wr = fx.reverb.process(mixR, 1);
                mixL = lerp(mixL, wl, amount * 0.62f);
                mixR = lerp(mixR, wr, amount * 0.62f);
                break;
            }
            case 5: // OTA-style drive
            {
                mixL = lerp(mixL, softClip(mixL * 3.2f) * 0.68f, amount);
                mixR = lerp(mixR, softClip(mixR * 3.2f) * 0.68f, amount);
                break;
            }
            default:
                break;
        }
    }

    // 10 Hz DC blocker. It removes the offset from asymmetric oscillator
    // resets without eating the low end of the instrument.
    const float dcCoeff = std::exp(-kTwoPi * 10.0f * invSr);
    const float dcOutL = mixL - dcInL + dcCoeff * dcL;
    const float dcOutR = mixR - dcInR + dcCoeff * dcR;
    dcInL = mixL;
    dcInR = mixR;
    dcL = dcOutL;
    dcR = dcOutR;

    smoothedVolume += (global.volume - smoothedVolume) * 0.003f;
    const float gain = smoothedVolume * 4.0f * expression;
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
        outputNodes = outputCircuit.process(circuitOutputDriver(outL * kCircuitOutputReference),
                                            circuitOutputDriver(outR * kCircuitOutputReference));
        outL = (float) outputNodes.left.audibleJackVolts;
        outR = (float) outputNodes.right.audibleJackVolts;
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
    return { v.active, v.note, v.midiChannel, v.velocity, v.pressure, v.mpeBend, v.ribbonRatio, v.held, v.pedalHeld };
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
    return { r.osc.getPhase(), r.hpf.getState(), r.lpf.getState() };
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
