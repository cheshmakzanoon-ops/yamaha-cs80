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
    hostRate = hostSampleRate > 0.0 ? hostSampleRate : 48000.0;
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
    ringKeyDown = false;
    ringAdsr.reset();
    ringPhase = 0.0f;
    ringEnv = 0.0f;
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
    decL.prepare((float) internalRate);
    decR.prepare((float) internalRate);
    lastNote = 60;
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
    arpGateOpen = false;
    arpWasOn = false;
    arpClock = 0.0f;
    arpGateTime = 0.0f;
    dcL = dcR = dcInL = dcInR = 0.0f;
}

void Engine::setParams(const ParamValues& values)
{
    params = values;
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
        r.hpfRes = rp(CP_hpfRes);
        r.hpfOn = rp(CP_hpfOn) >= 0.5f;
        r.lpfCutoff = rp(CP_lpfCutoff);
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
    for (int i = 0; i < kNumVoices; ++i)
        if (! voices[(size_t) i].active)
            return i;

    int best = 0;
    uint64_t bestAge = UINT64_MAX;
    for (int i = 0; i < kNumVoices; ++i)
    {
        auto& v = voices[(size_t) i];
        if (v.ranks[0].aenv.isIdle() && v.ranks[1].aenv.isIdle())
        {
            if (v.age < bestAge)
            {
                best = i;
                bestAge = v.age;
            }
        }
    }
    if (bestAge != UINT64_MAX)
        return best;

    for (int i = 0; i < kNumVoices; ++i)
    {
        if (voices[(size_t) i].age < bestAge)
        {
            best = i;
            bestAge = voices[(size_t) i].age;
        }
    }
    return best;
}

void Engine::rememberHeldKey(int note, float velocity, int midiChannel)
{
    for (int i = 0; i < heldKeyCount; ++i)
    {
        if (heldKeys[(size_t) i].note == note
            && (! global.mpe || heldKeys[(size_t) i].midiChannel == midiChannel))
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
            && (! global.mpe || heldKeys[(size_t) i].midiChannel == midiChannel))
        {
            if (arpCurrentNote == note)
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

    int base[kMaxHeldKeys] {};
    int baseCount = 0;

    if (global.arpMode == 4)
    {
        // Played order: retain the order the keys arrived in.
        for (int i = 0; i < heldKeyCount; ++i)
            base[baseCount++] = heldKeys[(size_t) i].note;
    }
    else
    {
        // Pitch order, with duplicates removed.
        for (int i = 0; i < heldKeyCount; ++i)
        {
            const int n = heldKeys[(size_t) i].note;
            bool exists = false;
            for (int j = 0; j < baseCount; ++j)
                exists = exists || base[j] == n;
            if (! exists)
                base[baseCount++] = n;
        }
        for (int i = 1; i < baseCount; ++i)
        {
            const int value = base[i];
            int j = i - 1;
            while (j >= 0 && base[j] > value)
            {
                base[j + 1] = base[j];
                --j;
            }
            base[j + 1] = value;
        }
    }

    const int octaves = std::clamp(global.arpOctaves, 1, 4);
    for (int oct = 0; oct < octaves; ++oct)
        for (int i = 0; i < baseCount; ++i)
        {
            const int note = base[i] + oct * 12;
            if (note >= 0 && note <= 127 && arpSequenceLength < (int) arpSequence.size())
                arpSequence[(size_t) arpSequenceLength++] = note;
        }

    if (global.arpMode == 1)
        std::reverse(arpSequence.begin(), arpSequence.begin() + arpSequenceLength);
    else if (global.arpMode == 2 && arpSequenceLength > 2)
    {
        // Up / down. The endpoints are not repeated; the pattern cycles.
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

    const int note = arpSequence[(size_t) index];
    const int voice = chooseVoice();
    float velocity = 0.82f;
    if (heldKeyCount > 0)
        velocity = heldKeys[(size_t) (index % heldKeyCount)].velocity;
    startVoice(voices[(size_t) voice], note, velocity, 1);
    arpVoice = voice;
    arpCurrentNote = note;
}

void Engine::stopArpVoice()
{
    if (arpVoice >= 0 && arpVoice < kNumVoices)
        releaseVoice(voices[(size_t) arpVoice]);
    arpVoice = -1;
    arpCurrentNote = -1;
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
        arpGateOpen = true;
        arpGateTime = interval * (global.arpMode == 3 ? 0.45f : 0.58f);
    }
}

void Engine::startVoice(Voice& v, int note, float velocity, int midiChannel)
{
    if (v.active && v.held)
        --heldCount;

    v.active = true;
    v.held = true;
    v.sustained = false;
    v.note = note;
    v.midiChannel = midiChannel;
    v.velocity = clampf(velocity, 0.0f, 1.0f);
    v.pressure = 0.0f;
    v.timbre = 0.0f;
    v.mpeBend = 0.0f;
    v.targetPitch = (float) note;
    v.glidePitch = global.portamento > 0.0001f ? (float) lastNote : (float) note;
    v.initBendValue = -global.initBend * v.velocity * 1.0f;
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
        r.fenv.noteOn((rs.fIl - 0.5f) * 3.0f,
                      (rs.fAl - 0.5f) * 3.0f,
                      std::max(0.001f, rs.fA * global.envTimeScale),
                      std::max(0.001f, rs.fD * global.envTimeScale),
                      std::max(0.001f, rs.fR * global.envTimeScale),
                      global.masterSustain * 2.0f);
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
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& r = v.ranks[ch];
        const auto& rs = rankSettings[ch];
        r.aenv.setRelease(std::max(rs.vR, global.keyRelease) * global.envTimeScale);
        r.fenv.setRelease(std::max(rs.fR, global.keyRelease) * global.envTimeScale);
        r.aenv.noteOff();
        r.fenv.noteOff();
    }
    if (heldCount <= 0)
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
                releaseVoice(v);
    }

    if (global.arpOn)
    {
        rememberHeldKey(midiNote, velocity, midiChannel);
        return;
    }

    const int idx = chooseVoice();
    startVoice(voices[(size_t) idx], midiNote, velocity, midiChannel);
}

void Engine::noteOff(int midiNote, int midiChannel)
{
    if (global.arpOn)
    {
        forgetHeldKey(midiNote, midiChannel);
        return;
    }

    if (auto* v = findVoiceForNote(midiNote, midiChannel))
    {
        if (global.sustainMode == 1 && v->held)
        {
            v->held = false;
            v->sustained = true;
            --heldCount;
            if (heldCount < 0)
                heldCount = 0;
        }
        else
        {
            releaseVoice(*v);
        }
    }
}

Engine::Voice* Engine::findVoiceForNote(int note, int midiChannel)
{
    for (auto& v : voices)
        if (v.active && v.note == note && (v.midiChannel == midiChannel || ! global.mpe))
            return &v;
    for (auto& v : voices)
        if (v.active && v.note == note)
            return &v;
    return nullptr;
}

void Engine::allNotesOff(bool release)
{
    heldKeyCount = 0;
    arpSequenceLength = 0;
    arpIndex = 0;
    arpVoice = -1;
    arpCurrentNote = -1;
    arpGateOpen = false;
    arpClock = 0.0f;

    for (auto& v : voices)
    {
        if (release)
            releaseVoice(v);
        else
        {
            v.active = false;
            v.held = false;
            v.sustained = false;
        }
    }
    heldCount = 0;
    ringKeyDown = false;
    ringAdsr.noteOff();
}

void Engine::polyPressure(int midiNote, float pressure)
{
    pressure = clampf(pressure, 0.0f, 1.0f);
    lastPressure = pressure;
    for (auto& v : voices)
        if (v.active && v.note == midiNote)
            v.pressure = pressure;
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
                v.mpeBend = value * global.pitchBendRange;
    }
    else
    {
        globalBend = value * global.pitchBendRange;
    }
}

void Engine::controlChange(int controller, float value, int midiChannel)
{
    value = clampf(value, 0.0f, 1.0f);
    if (controller == 1)
        modWheel = value;
    else if (controller == 11)
        expression = value;
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

    const float glideCoef = global.portamento > 0.0001f
                                ? std::exp(-1.0f / (global.portamento * sr))
                                : 0.0f;

    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[(size_t) vi];
        if (! v.active)
            continue;

        if (v.held)
            ++stillHeld;

        if (global.portamento > 0.0001f)
            v.glidePitch = v.targetPitch + (v.glidePitch - v.targetPitch) * glideCoef;
        else
            v.glidePitch = v.targetPitch;

        if (std::abs(v.initBendValue) > 0.0001f)
            v.initBendValue *= std::exp(-1.0f / (0.075f * sr));

        const float glideOffset = v.glidePitch - (float) v.note;
        const float dispersion = global.voiceDispersion;
        const float driftVoice = global.drift * std::sin(kTwoPi * (driftPhase + voiceDrift[vi])) * 0.9f;

        for (int ch = 0; ch < 2; ++ch)
        {
            auto& r = v.ranks[ch];
            auto& rs = rankSettings[ch];

            float pitch = (float) v.note
                          + cs80::kFeetSemitones[std::clamp((int) rs.feet, 0, 5)]
                          + global.masterTune
                          + (ch == 1 ? global.detuneCents / 100.0f : 0.0f)
                          + ribbon * (ribbon >= 0.0f ? 12.0f : 18.0f)
                          + globalBend
                          + v.mpeBend
                          + glideOffset
                          + v.initBendValue
                          + lfoPitch
                          + subValue * (global.subVco + global.subAtVco * v.pressure) * 1.5f
                          + modWheel * global.modWheelSub * 2.0f
                          + r.trimTune * dispersion * 7.0f
                          + driftVoice;

            if (global.glideMode == 2)
                pitch = std::round(pitch);

            const float freq = midiToHz(pitch, global.tuningRef);
            const float pwm = std::sin(kTwoPi * pwmPhase[ch]);
            const float pw = clampf(rs.pulseWidth + rs.pwmDepth * 0.35f * pwm + lfoPwm, 0.05f, 0.95f);

            const auto osc = r.osc.tick(freq, sr, pw, rs.saw, rs.pulse, 0.0f, audioRng);

            float x = osc.saw + osc.pulse;
            x += audioRng.nextBipolar() * rs.noise * 0.8f;
            x *= 0.9f;

            const float keyOct = ((float) v.note - 60.0f) / 12.0f * 0.8f;
            const float brillOct = global.brilliance
                                   + rs.initBrill * v.velocity * 2.0f
                                   + rs.afterBrill * v.pressure * 2.0f
                                   + v.timbre * 0.8f
                                   + modWheel * global.modWheelVcf * 1.5f
                                   + subValue * global.subVcf * 1.5f
                                   + lfoFilter;

            const float fenv = r.fenv.tick(sr, true);
            const float envLp = fenv + global.masterSustain * 1.5f;
            const float envHp = envLp * 0.5f;

            const float cutoffLp = rs.lpfCutoff * std::pow(2.0f, brillOct + keyOct + envLp);
            const float cutoffHp = rs.hpfCutoff * std::pow(2.0f, brillOct + keyOct + envHp);
            const float qLp = 0.55f + (rs.lpfRes * 0.8f + global.resonance * 0.6f) * 6.0f;
            const float qHp = 0.55f + (rs.hpfRes * 0.8f + global.resonance * 0.6f) * 5.0f;

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

    // Global ring modulator. The AD envelope is triggered by the first key
    // and does not restart until every key has been released.
    if (global.ringAmount > 0.001f)
    {
        const float renv = ringAdsr.tick(sr, false);
        ringEnv = renv;
        const float carrierFreq = clampf(global.ringSpeed, 0.05f, sr * 0.4f);
        ringPhase += carrierFreq * invSr;
        ringPhase -= std::floor(ringPhase);
        const float carrier = std::sin(kTwoPi * ringPhase);
        const float amount = clampf(global.ringAmount, 0.0f, 1.0f);
        const float depth = clampf(global.ringDepth, 0.0f, 1.0f);
        // Depth is the modulator's own amplitude, amount is the dry/ring
        // balance. Both were exposed as separate panel controls for a reason.
        const float ringedL = mixL * carrier * (0.15f + 0.85f * depth * renv);
        const float ringedR = mixR * carrier * (0.15f + 0.85f * depth * renv);
        mixL = lerp(mixL, ringedL, amount);
        mixR = lerp(mixR, ringedR, amount);
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
            const float dl = decL.process(l);
            const float dr = decR.process(r);
            if (os == oversample - 1)
            {
                lastL = dl;
                lastR = dr;
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
    return { v.active, v.note, v.midiChannel, v.velocity, v.pressure };
}

} // namespace cs80
