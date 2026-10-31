#pragma once

#include "Dsp.h"
#include "FxLoudness.h"
#include "FxModels.h"

#include <cmath>

// One of the engine's three serial FX slots (modern layer; PURE CS-80 empties them).
// Moved out of Engine.h unchanged in behaviour so the slot can be tested on its own; the
// classic effects below keep their exact historical arithmetic (pinned by
// tests/ModernFxTests.cpp against a frozen copy of the original Engine.cpp code).
namespace cs80::fx
{

struct SlotControls
{
    int type = 0;      // classic: 0 off, 1 chorus, 2 phaser, 3 tape delay, 4 plate reverb, 5 drive
    int model = 0;     // modern model (Model); 0 keeps the classic type
    float mix = 0.0f;  // effective mix, 0..1
    float a = 0.5f, b = 0.5f;
    int oversampling = 2; // drive: 2^n of the internal rate
    bool gainComp = false; // FX LOUDNESS MATCH
};

// Each slot owns one instance of every effect, so two slots set to the same type run two
// independent effects instead of sharing (and double-advancing) one set of delay lines,
// and a slot that is switched off or changes type can drop the stale audio of its
// previous effect instead of replaying it when it is next switched on.
struct FxSlot
{
    Chorus chorus;
    Phaser phaser;
    Reverb reverb;
    DelayLine delayL, delayR;
    OnePole dampL, dampR;
    ModernFxSlot modern;   // modern models (liveType kModernTypeBase + model)
    LoudnessMatcher match; // FX LOUDNESS MATCH: slot in/out loudness
    int liveType = 0;      // effect whose state may hold audio; 0 = none

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
        modern.prepare(rate);
        match.prepare(rate);
        liveType = 0;
    }
    // Delay-line and filter history only; the tape-delay damping OnePoles are left alone so
    // that Engine::reset keeps its historical arithmetic (see Engine::applyParams).
    void resetAudio() noexcept
    {
        chorus.reset();
        phaser.reset();
        reverb.reset();
        delayL.reset();
        delayR.reset();
        modern.reset();
        match.reset();
        liveType = 0;
    }
    void resetDamping() noexcept { dampL.reset(); dampR.reset(); }
    // Drop the audio held by one effect type (1 chorus, 2 phaser, 3 tape, 4 plate, 5 drive,
    // or a modern model) and the loudness matcher's history.
    void clear(int type) noexcept
    {
        match.reset();
        if (type > kModernTypeBase)
            modern.clear(type - kModernTypeBase);
        switch (type)
        {
            case 1: chorus.reset(); break;
            case 2: phaser.reset(); break;
            case 3: delayL.reset(); delayR.reset(); resetDamping(); break;
            case 4: reverb.reset(); break;
            default: break;
        }
    }

    static int liveTypeFor(const SlotControls& c) noexcept
    {
        return (c.mix <= 0.001f || (c.type <= 0 && c.model <= 0)) ? 0
               : c.model > 0 ? kModernTypeBase + c.model : c.type;
    }

    // One sample through the slot. wowPhase is the engine's slow drift phase (classic tape
    // wow), sr the internal rate.
    StereoSample process(const SlotControls& c, float l, float r, float wowPhase, float sr) noexcept
    {
        const int live = liveTypeFor(c);
        if (live != liveType)
        {
            // A slot that goes quiet, or changes effect, must not keep the old effect's audio:
            // it would replay it, late, the next time the slot is switched on.
            clear(liveType);
            liveType = live;
        }
        if (live == 0)
            return { l, r };
        StereoSample out = live > kModernTypeBase
            ? modern.process(c.model, l, r, c.mix, c.a, c.b, c.oversampling)
            : processClassic(live, l, r, c.mix, wowPhase, sr);
        if (c.gainComp)
        {
            // K-weighted slot output loudness follows the slot input loudness.
            const float g = match.process(l, r, out.l, out.r);
            out.l *= g;
            out.r *= g;
        }
        return out;
    }

    // The classic slot effects, exactly as they were written in Engine::processInternalSample.
    StereoSample processClassic(int type, float mixL, float mixR, float amount, float driftPhase, float sr) noexcept
    {
        switch (type)
        {
            case 1: // modern stereo chorus
            {
                const auto m = chorus.process(mixL, mixR, 0.62f, 0.62f, true, false);
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
                const float wl = dampL.process(delayL.read(d));
                const float wr = dampR.process(delayR.read(d));
                delayL.write(mixL + wr * 0.36f);
                delayR.write(mixR + wl * 0.36f);
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
        return { mixL, mixR };
    }
};

} // namespace cs80::fx
