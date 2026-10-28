#include "Engine.h"

#include <algorithm>
#include <cmath>

namespace cs80
{

Engine::Engine()
{
    setParams(params);
}

void Engine::prepare(double hostSampleRate)
{
    // Software resource/numeric guard, not an analog tuning assumption.
    hostRate = std::isfinite(hostSampleRate) && hostSampleRate >= 8000.0
               && hostSampleRate <= 768000.0 ? hostSampleRate : 48000.0;
    oversample = 2;
    internalRate = hostRate * (double) oversample;

    chorus.prepare((float) internalRate);
    fxChorus.prepare((float) internalRate);
    phaser.prepare((float) internalRate);
    reverb.prepare((float) internalRate);
    fxL.prepare(0.8f, (float) internalRate);
    fxR.prepare(0.8f, (float) internalRate);
    fxDampL.set(9000.0f, (float) internalRate);
    fxDampR.set(9000.0f, (float) internalRate);
    decL.prepare((float) internalRate);
    decR.prepare((float) internalRate);

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
    setParams(params);
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
    for (auto& l : modLfos)
        l.reset(0.0f);
    chorus.reset();
    fxChorus.reset();
    phaser.reset();
    reverb.reset();
    fxL.reset();
    fxR.reset();
    decL.reset();
    decR.reset();
    lastNote = 60;
    nextVoice = 0;
    globalBend = 0.0f;
    modWheel = 0.0f;
    expression = 1.0f;
    ribbon = 0.0f;
    lastPressure = 0.0f;
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
    params = values;
    // Sanitize once at the core boundary before integer casts and DSP laws.
    for (int i = 0; i < P_numParams; ++i)
    {
        const auto& def = paramDef(i);
        const float value = params[i];
        params[i] = std::isfinite(value) ? clampf(value, def.min, def.max) : def.def;
    }
    const bool wasArp = arpWasOn;
    buildSettings();

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
    global.subVco = p(P_subVco);
    global.subVcf = p(P_subVcf);
    global.subVca = p(P_subVca);
    global.subAtSpeed = p(P_subAtSpeed);
    global.subAtVco = p(P_subAtVco);
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
    }
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
    v.glidePitch = global.portamento > 0.0001f ? (float) lastNote : (float) note;
    v.initBendValue = -global.initBend * v.velocity * 1.0f;
    v.ribbonRatio = ribbonFrequencyRatio(ribbon + params[P_ribbon]);
    v.age = ++voiceCounter;
    lastNote = note;
    ++heldCount;

    for (int ch = 0; ch < 2; ++ch)
    {
        auto& r = v.ranks[ch];
        const auto& rs = rankSettings[ch];
        r.osc.reset(audioRng.nextUnipolar());
        r.hpf.reset();
        r.lpf.reset();
        r.fenv.reset();
        r.aenv.reset();
        r.fenv.noteOn(filterInitialOffset(rs.fIl),
                      filterAttackOffset(rs.fAl),
                      std::max(0.001f, rs.fA * global.envTimeScale),
                      std::max(0.001f, rs.fD * global.envTimeScale),
                      std::max(0.001f, rs.fR * global.envTimeScale),
                      0.0f);
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
        const float performanceRelease = global.masterSustain * 10.0f;
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
    else if (global.sustainMode == 1)
    {
        releaseVoice(v);
        v.sustained = true;
    }
    else
        releaseVoice(v);
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
            voice.active = voice.held = voice.sustained = voice.pedalHeld = false;
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
    bool matched = false;
    for (auto& v : voices)
        if (v.active && v.note == midiNote
            && (! global.mpe || midiChannel <= 0 || v.midiChannel == midiChannel))
        {
            v.pressure = pressure;
            matched = true;
        }
    if (matched)
        lastPressure = pressure;
}

void Engine::channelPressure(int midiChannel, float pressure)
{
    pressure = clampf(pressure, 0.0f, 1.0f);
    lastPressure = pressure;
    for (auto& v : voices)
        if (v.active && (midiChannel == 1 || v.midiChannel == midiChannel))
            v.pressure = pressure;
}

void Engine::pitchBend(int midiChannel, float value)
{
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

void Engine::setRibbon(float value)
{
    ribbon = clampf(value, -1.0f, 1.0f);
}

void Engine::processInternalSample(float& outL, float& outR)
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

    float speed = global.subSpeed;
    speed *= 1.0f + global.subAtSpeed * lastPressure * 2.0f;
    subSpeedSmoothed += (speed - subSpeedSmoothed) * 0.0015f;
    subValue = subLfo.tick(subSpeedSmoothed, sr, global.subWave, modRng);
    driftPhase += 0.031f * invSr;
    driftPhase -= std::floor(driftPhase);

    float mixL = 0.0f;
    float mixR = 0.0f;
    int stillActive = 0;
    int stillHeld = 0;

    // Double accumulator avoids long-glide float rounding stalling before target.
    const double glideCoef = global.portamento > 0.0001f
                                ? std::exp(-1.0 / ((double) global.portamento * sr))
                                : 0.0;

    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[(size_t) vi];
        if (! v.active)
            continue;

        if (v.held || v.pedalHeld)
            ++stillHeld;

        if (global.portamento > 0.0001f)
            v.glidePitch = v.targetPitch + (v.glidePitch - v.targetPitch) * glideCoef;
        else
            v.glidePitch = v.targetPitch;

        if (std::abs(v.initBendValue) > 0.0001f)
            v.initBendValue *= std::exp(-1.0f / (0.075f * sr));

        // Sustain I holds each released voice's last ribbon pitch. Sustain II
        // keeps release tails coupled to subsequent ribbon gestures.
        if (v.held || v.pedalHeld || global.sustainMode == 1)
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
                          + v.initBendValue
                          + lfoPitch
                          + subValue * (global.subVco + global.subAtVco * v.pressure) * 1.5f
                          + subValue * modWheel * global.modWheelSub * 2.0f
                          + r.trimTune * dispersion * 7.0f
                          + driftVoice;

            if (global.glideMode == 2)
                pitch = std::round(pitch);

            // Transposition is a separate nominal harmonic ratio, not rounded
            // equal-tempered fifths and not part of the shared filter key CV.
            const float freq = midiToHz(pitch, global.tuningRef) * v.ribbonRatio
                               * kFeetRatios[std::clamp(rs.feet, 0, 5)];
            const float pwm = std::sin(kTwoPi * pwmPhase[ch]);
            const float pw = clampf(rs.pulseWidth + rs.pwmDepth * 0.35f * pwm + lfoPwm, 0.05f, 0.95f);

            const auto osc = r.osc.tick(freq, sr, pw, rs.saw, rs.pulse, 0.0f, audioRng);

            float x = osc.saw + osc.pulse;
            x += audioRng.nextBipolar() * rs.noise * 0.8f;
            x *= 0.9f;

            // Shared key-voltage domain precedes VCO footage/vibrato. Master
            // pitch and key portamento move it; ribbon routing is unverified.
            const float keyVolts = filterKeyVolts(v.glidePitch + global.masterTune);
            const float brillOct = global.brilliance
                                   + rs.initBrill * v.velocity * 2.0f
                                   + rs.afterBrill * v.pressure * 2.0f
                                   + v.timbre * 0.8f
                                   + modWheel * global.modWheelVcf * 1.5f
                                   + subValue * global.subVcf * 1.5f
                                   + lfoFilter;

            const float fenv = r.fenv.tick(sr, true);
            const float envLp = fenv;
            const float envHp = fenv * kProvisionalHpfEnvelopeGain;

            const float cutoffLp = nominalFilterCutoff(keyVolts, rs.lpfEffectiveCv + brillOct + envLp);
            const float cutoffHp = nominalFilterCutoff(keyVolts, rs.hpfEffectiveCv + brillOct + envHp);
            const float qLp = provisionalFilterQ(rs.lpfRes * 0.8f + global.resonance * 0.6f);
            const float qHp = provisionalFilterQ(rs.hpfRes * 0.8f + global.resonance * 0.6f);

            float filtered = x;
            if (rs.hpfOn)
            {
                r.hpf.set(cutoffHp, qHp, sr);
                filtered = r.hpf.process(filtered).hp;
            }
            if (rs.lpfOn)
            {
                r.lpf.set(cutoffLp, qLp, sr);
                filtered = r.lpf.process(filtered).lp;
            }

            const float sine = osc.sine * rs.sine;
            const float levelCv = clampf(rs.vLevel
                                             + rs.initLevel * v.velocity
                                             + rs.afterLevel * v.pressure
                                             + subValue * global.subVca * 0.45f
                                             + lfoAmp,
                                         0.0f, 2.6f);
            const float amp = r.aenv.tick(sr, true);
            r.info = { cutoffHp, cutoffLp, fenv, amp, freq, keyVolts, qHp, qLp };
            const float chGain = ch == 0 ? (1.0f - global.mix) : global.mix;
            const float velocityGain = 0.38f + 0.62f * v.velocity;
            float voiceOut = (filtered * rs.vcfLevel + sine)
                             * levelCv * amp * chGain * velocityGain * 0.17f;

            float pan = clampf(rs.pan + r.trimPan * dispersion * 0.8f + lfoPan, -1.0f, 1.0f);
            mixL += voiceOut * (0.5f - 0.5f * pan);
            mixR += voiceOut * (0.5f + 0.5f * pan);

            if (! r.aenv.isIdle())
                ++stillActive;
        }

        if (v.ranks[0].aenv.isIdle() && v.ranks[1].aenv.isIdle())
        {
            v.active = false;
            v.held = false;
            v.sustained = false;
        }
    }

    stillActive = 0;
    for (auto& v : voices)
        if (v.active)
            ++stillActive;
    heldCount = stillHeld;

    // Ring AD controls oscillator FREQUENCY. Keep it clocking while bypassed
    // so enabling MODULATION does not resurrect a frozen envelope.
    ringEnv = ringAdsr.tick(sr, false);
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

    mixL += audioRng.nextBipolar() * global.analogNoise * 0.0008f;
    mixR += audioRng.nextBipolar() * global.analogNoise * 0.0008f;

    // BBD-style stereo chorus/tremolo.
    const auto ch = chorus.process(mixL, mixR, global.chorusSpeed, global.chorusDepth,
                                   global.chorusOn, global.chorusTremolo);
    mixL = ch.l;
    mixR = ch.r;

    // Three modern FX slots, processed in series after the authentic CS-80
    // chorus. The type numbers are fixed by the UI: 1 chorus, 2 phaser,
    // 3 tape delay, 4 plate reverb, 5 drive.
    for (int slot = 0; slot < 3; ++slot)
    {
        const int type = global.fxType[slot];
        const float amount = global.fxMix[slot];
        if (amount <= 0.001f || type <= 0)
            continue;

        switch (type)
        {
            case 1: // modern stereo chorus
            {
                const auto m = fxChorus.process(mixL, mixR, 0.62f, 0.62f, true, false);
                mixL = lerp(mixL, m.l, amount * 0.62f);
                mixR = lerp(mixR, m.r, amount * 0.62f);
                break;
            }
            case 2: // six-stage phaser
            {
                const auto p = phaser.process(mixL, mixR, 0.32f, 0.72f);
                mixL = lerp(mixL, p.l, amount * 0.72f);
                mixR = lerp(mixR, p.r, amount * 0.72f);
                break;
            }
            case 3: // tape delay with cross-feedback and wow
            {
                const float wow = std::sin(kTwoPi * driftPhase) * 0.0007f;
                const float d = (0.34f + wow) * sr;
                const float wl = fxDampL.process(fxL.read(d));
                const float wr = fxDampR.process(fxR.read(d));
                fxL.write(mixL + wr * 0.36f);
                fxR.write(mixR + wl * 0.36f);
                mixL = lerp(mixL, wl, amount * 0.58f);
                mixR = lerp(mixR, wr, amount * 0.58f);
                break;
            }
            case 4: // stereo plate reverb
            {
                const float wl = reverb.process(mixL, 0);
                const float wr = reverb.process(mixR, 1);
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
    outL = softClip(dcOutL * gain);
    outR = softClip(dcOutR * gain);

    if (! global.stereo)
    {
        const float m = (outL + outR) * 0.5f;
        outL = outR = m;
    }
    (void) stillActive;
}

void Engine::render(float* interleavedStereo, int numFrames)
{
    if (interleavedStereo == nullptr || numFrames <= 0)
        return;

    for (int i = 0; i < numFrames; ++i)
    {
        float lastL = 0.0f, lastR = 0.0f;
        for (int os = 0; os < oversample; ++os)
        {
            float l = 0.0f, r = 0.0f;
            processInternalSample(l, r);
            decL.push(l);
            decR.push(r);
            // Evaluate only the retained odd phase, using symmetric tap pairs.
            if (os == Decimator::kRetainedPhase)
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

} // namespace cs80
