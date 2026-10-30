// Keyboard contour, footswitch assigners, EXP/EXP-WAH and output HIGH/LOW.
//
// These tests establish that the implementation obeys its stated specification:
// route topology, neutral identity and edge behavior. They do not establish that
// the provisional gains match an original CS-80 (see docs/PERFORMANCE_CONTROLS.md).
#include "core/Engine.h"
#include "core/Params.h"
#include "core/PerformanceControls.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace cs80;

namespace
{
int failures = 0;
int checks = 0;

void check(bool condition, const std::string& what)
{
    ++checks;
    if (! condition)
    {
        std::printf("FAIL: %s\n", what.c_str());
        ++failures;
    }
}

void checkNear(float actual, float expected, float tolerance, const std::string& what)
{
    check(std::abs(actual - expected) <= tolerance,
          what + " (actual " + std::to_string(actual) + ", expected " + std::to_string(expected) + ")");
}

constexpr double kRate = 48000.0;

std::vector<float> render(Engine& e, int frames)
{
    std::vector<float> out((size_t) frames * 2, 0.0f);
    for (int i = 0; i < frames; i += 256)
        e.render(out.data() + (size_t) i * 2, std::min(256, frames - i));
    return out;
}

double rms(const std::vector<float>& x, size_t from = 0)
{
    double s = 0.0;
    size_t n = 0;
    for (size_t i = from; i < x.size(); ++i, ++n)
        s += (double) x[i] * x[i];
    return n ? std::sqrt(s / (double) n) : 0.0;
}

bool finiteBounded(const std::vector<float>& x, float bound = 1.5f)
{
    for (float v : x)
        if (! std::isfinite(v) || std::abs(v) > bound)
            return false;
    return true;
}

// Deterministic fixture: no per-voice scatter, drift or noise so ratios are exact.
ParamValues quietPatch()
{
    ParamValues p;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    return p;
}

Engine::RankInfo settle(const ParamValues& p, int note, int frames = 9600, int rank = 0)
{
    Engine e;
    e.prepare(kRate);
    e.setParams(p);
    e.noteOn(note, 0.8f);
    render(e, frames);
    return e.getRankInfo(0, rank);
}

// --------------------------------------------------------------------------

void testParameterTable()
{
    check(P_subExtLevel == 126 && P_kbdBrillianceLow == 127 && P_kbdBrillianceHigh == 128
              && P_kbdLevelLow == 129 && P_kbdLevelHigh == 130 && P_footswitchPlugged == 131
              && P_footswitchPressed == 132 && P_sustainAssign == 133 && P_glideAssign == 134
              && P_footMode == 135 && P_outputLevel == 136 && P_numParams == 139,
          "performance controls are appended after the shipped extension block");
    const char* ids[] = { "kbd_brilliance_low", "kbd_brilliance_high", "kbd_level_low", "kbd_level_high",
                          "footswitch_plugged", "footswitch_pressed", "sustain_assign", "glide_assign",
                          "foot_mode", "output_level" };
    for (int i = 0; i < 10; ++i)
        check(paramId(P_kbdBrillianceLow + i) == ids[i], std::string("stable parameter id ") + ids[i]);
    const ParamValues defaults;
    for (int i = P_kbdBrillianceLow; i <= P_kbdLevelHigh; ++i)
        check(defaults[i] == 0.0f && paramDef(i).min == -1.0f && paramDef(i).max == 1.0f,
              "contour lever is bipolar and centred by default");
    check(defaults[P_footswitchPlugged] == 0.0f && defaults[P_footswitchPressed] == 0.0f
              && defaults[P_sustainAssign] == 0.0f && defaults[P_glideAssign] == 0.0f
              && defaults[P_footMode] == 0.0f && defaults[P_outputLevel] == 1.0f,
          "defaults are the previous behavior: no assignment, EXP, HIGH");
}

void testContourWeights()
{
    auto w = [](int key) { return keyboardContourWeights(kKeyboardLowestNote + key); };
    checkNear(w(0).low, 1.0f, 1e-6f, "lowest key takes the full LOW lever");
    checkNear(w(0).high, 0.0f, 1e-6f, "lowest key ignores the HIGH lever");
    checkNear(w(60).high, 1.0f, 1e-6f, "highest key takes the full HIGH lever");
    checkNear(w(60).low, 0.0f, 1e-6f, "highest key ignores the LOW lever");
    checkNear(w(6).low, 1.0f, 0.03f, "first octave group is still on the LOW plateau");
    checkNear(w(18).low, 2.0f / 3.0f, 0.03f, "third octave tap is 2/3 of LOW");
    checkNear(w(43).high, 2.0f / 3.0f, 0.03f, "fifth octave tap is 2/3 of HIGH");
    check(w(30).high == 0.0f && w(31).low == 0.0f, "LOW ends at the centre knot and HIGH begins there");
    checkNear(w(30).low + w(30).high, 0.0f, 0.03f, "keyboard centre is neutral");

    bool monotonic = true, exclusive = true;
    for (int key = 1; key < kKeyboardKeyCount; ++key)
    {
        monotonic = monotonic && w(key).low <= w(key - 1).low + 1e-6f && w(key).high >= w(key - 1).high - 1e-6f;
        exclusive = exclusive && (w(key).low < 1e-6f || w(key).high < 1e-6f);
    }
    check(monotonic, "LOW only falls and HIGH only rises across the keyboard");
    check(exclusive, "no key is driven by both ends: a contour, not a split with overlap");
    check(keyboardContourWeights(0).low == 1.0f && keyboardContourWeights(127).high == 1.0f,
          "keys outside the 61-key span clamp to the end taps");
    check(keyboardBrillianceOctaves(0.0f, 0.0f, w(0)) == 0.0f && keyboardLevelOffset(0.0f, 0.0f, w(60)) == 0.0f,
          "centred levers contribute exactly zero");
}

void testContourRouting()
{
    const ParamValues neutral = quietPatch();
    const int lowKey = 28, highKey = 82, midKey = 55; // keys 4, 58, 31: LOW weight exactly 0 at 31
    const auto base = [&](int n) { return settle(neutral, n); };

    // Brilliance: both filters move by 2^octaves, only at the lever's own end.
    ParamValues p = neutral;
    p[P_kbdBrillianceLow] = 1.0f;
    const auto lowBright = settle(p, lowKey);
    const float expectedLow = std::exp2(kProvisionalKeyboardBrillianceOctaves
                                        * keyboardContourWeights(lowKey).low);
    checkNear(lowBright.lpfHz / base(lowKey).lpfHz, expectedLow, expectedLow * 1e-3f,
              "LOW BRILLIANCE raises the low-pass cutoff at a low key");
    checkNear(lowBright.hpfHz / base(lowKey).hpfHz, expectedLow, expectedLow * 1e-3f,
              "LOW BRILLIANCE moves the high-pass too (brilliance changes both filters)");
    check(settle(p, highKey).lpfHz == base(highKey).lpfHz, "LOW BRILLIANCE leaves the top of the keyboard alone");
    check(settle(p, midKey).lpfHz == base(midKey).lpfHz, "LOW BRILLIANCE is neutral at the keyboard centre");
    check(lowBright.lpfHz > base(lowKey).lpfHz && settle(p, lowKey, 9600, 1).lpfHz > base(lowKey).lpfHz,
          "contour reaches both ranks");

    p = neutral;
    p[P_kbdBrillianceHigh] = -1.0f;
    const auto highDark = settle(p, highKey);
    checkNear(highDark.lpfHz / base(highKey).lpfHz, std::exp2(-kProvisionalKeyboardBrillianceOctaves), 1e-3f,
              "negative HIGH BRILLIANCE darkens the top of the keyboard");
    check(settle(p, lowKey).lpfHz == base(lowKey).lpfHz, "HIGH BRILLIANCE leaves the bottom alone");

    // Level: additive VCA control; must not move either filter.
    p = neutral;
    p[P_kbdLevelHigh] = 1.0f;
    check(settle(p, highKey).lpfHz == base(highKey).lpfHz && settle(p, highKey).hpfHz == base(highKey).hpfHz,
          "KEYBOARD LEVEL never touches the filters");
    auto loudness = [&](const ParamValues& values, int note)
    {
        Engine e;
        e.prepare(kRate);
        e.setParams(values);
        e.noteOn(note, 0.8f);
        render(e, 12000);
        return rms(render(e, 12000));
    };
    const double neutralHigh = loudness(neutral, highKey);
    check(loudness(p, highKey) > neutralHigh * 1.05, "HIGH LEVEL raises the top of the keyboard");
    p[P_kbdLevelHigh] = -1.0f;
    check(loudness(p, highKey) < neutralHigh * 0.95, "negative HIGH LEVEL lowers the top of the keyboard");
    check(std::abs(loudness(p, lowKey) - loudness(neutral, lowKey)) < 1e-12,
          "HIGH LEVEL leaves the bottom of the keyboard unchanged");
    p = neutral;
    p[P_kbdLevelLow] = 1.0f;
    check(loudness(p, lowKey) > loudness(neutral, lowKey) * 1.05, "LOW LEVEL raises the bottom of the keyboard");
    check(std::abs(loudness(p, highKey) - neutralHigh) < 1e-12, "LOW LEVEL leaves the top unchanged");

    // Neutral identity: explicitly writing the centred values changes nothing.
    ParamValues explicitCentre = neutral;
    for (int i = P_kbdBrillianceLow; i <= P_kbdLevelHigh; ++i)
        explicitCentre[i] = 0.0f;
    Engine a, b;
    a.prepare(kRate);
    b.prepare(kRate);
    a.setParams(neutral);
    b.setParams(explicitCentre);
    for (int n : { 30, 54, 80 })
    {
        a.noteOn(n, 0.7f);
        b.noteOn(n, 0.7f);
    }
    check(render(a, 24000) == render(b, 24000), "centred contour levers are sample-identical to the default");

    // The weights are fixed when the key goes down; sweeping a lever acts on held notes.
    Engine live;
    live.prepare(kRate);
    live.setParams(neutral);
    live.noteOn(lowKey, 0.8f);
    render(live, 9600);
    const float before = live.getRankInfo(0, 0).lpfHz;
    p = neutral;
    p[P_kbdBrillianceLow] = 1.0f;
    live.setParams(p);
    render(live, 256);
    check(live.getRankInfo(0, 0).lpfHz > before * 1.5f, "a lever moved during a held note takes effect immediately");
}

// --------------------------------------------------------------------------

void testFootswitchTruthTable()
{
    for (int bits = 0; bits < 8; ++bits)
    {
        const bool assigned = bits & 1, plugged = bits & 2, pressed = bits & 4;
        const bool expected = ! assigned || ! plugged || pressed;
        check(footswitchGate(assigned, plugged, pressed) == expected,
              "footswitch truth table row " + std::to_string(bits));
    }
    check(footswitchGate(true, false, false), "no plug behaves as an active switch");
    check(! footswitchGate(true, true, false), "plugged but open dummy plug disables an assigned effect");
    check(footswitchGate(false, true, false), "assigner OFF bypasses the gate");
}

float ampAfterRelease(const ParamValues& p, double holdSeconds = 0.3, double afterSeconds = 1.2)
{
    Engine e;
    e.prepare(kRate);
    e.setParams(p);
    e.noteOn(60, 0.8f);
    render(e, (int) (holdSeconds * kRate));
    e.noteOff(60);
    render(e, (int) (afterSeconds * kRate));
    return e.getRankInfo(0, 0).ampEnvelope;
}

ParamValues sustainPatch()
{
    ParamValues p = quietPatch();
    p[P_masterSustain] = 1.0f;
    return p;
}

void testSustainAssigner()
{
    const float open = ampAfterRelease(sustainPatch());
    ParamValues closed = sustainPatch();
    closed[P_sustainAssign] = 1.0f;
    closed[P_footswitchPlugged] = 1.0f;
    const float cut = ampAfterRelease(closed);
    check(open > 0.2f, "SUSTAIN amount lengthens the release with no assigner");
    check(cut < 0.01f, "assigned, plugged and open footswitch removes the SUSTAIN release");

    // Every row of both assigners' truth table against observed release length.
    for (int bits = 0; bits < 16; ++bits)
    {
        ParamValues p = sustainPatch();
        p[P_sustainAssign] = (bits & 1) ? 1.0f : 0.0f;
        p[P_glideAssign] = (bits & 2) ? 1.0f : 0.0f;
        p[P_footswitchPlugged] = (bits & 4) ? 1.0f : 0.0f;
        p[P_footswitchPressed] = (bits & 8) ? 1.0f : 0.0f;
        const bool enabled = footswitchGate(bits & 1, bits & 4, bits & 8);
        const float amp = ampAfterRelease(p);
        check(enabled ? amp > 0.2f : amp < 0.01f,
              "sustain assigner row " + std::to_string(bits) + " observed amp " + std::to_string(amp));
    }

    // The gate only removes the performance term: per-rank release and KEY RELEASE survive.
    ParamValues rank = closed;
    rank[chParam(0, CP_vR)] = rank[chParam(1, CP_vR)] = 5.0f;
    check(ampAfterRelease(rank) > 0.05f, "an independently programmed long release is not removed by the gate");
    ParamValues pressedSwitch = closed;
    pressedSwitch[P_footswitchPressed] = 1.0f;
    check(ampAfterRelease(pressedSwitch) > 0.2f, "pressing the switch restores SUSTAIN");
}

void testSustainGateEdges()
{
    // Close the gate in the middle of a released tail.
    ParamValues open = sustainPatch();
    ParamValues closed = open;
    closed[P_sustainAssign] = 1.0f;
    closed[P_footswitchPlugged] = 1.0f;

    Engine e;
    e.prepare(kRate);
    e.setParams(open);
    e.noteOn(60, 0.8f);
    render(e, 14400);
    e.noteOff(60);
    render(e, 24000);
    const float before = e.getRankInfo(0, 0).ampEnvelope;
    const float filterBefore = e.getRankInfo(0, 0).filterEnvelope;
    check(before > 0.3f && e.activeVoices() == 1, "fixture: SUSTAIN tail is sounding");
    e.setParams(closed);
    check(e.getRankInfo(0, 0).ampEnvelope == before && e.getRankInfo(0, 0).filterEnvelope == filterBefore,
          "closing the gate does not jump the envelope value");
    render(e, 96000);
    check(e.activeVoices() == 0, "closing the gate lets the tail finish at its ordinary release");

    // Re-opening must neither resurrect a dead voice nor restart anything.
    e.setParams(open);
    render(e, 4800);
    check(e.activeVoices() == 0, "re-opening the gate does not resurrect a finished voice");

    // Gate edges never alter a held note or retrigger it.
    Engine h;
    h.prepare(kRate);
    h.setParams(open);
    h.noteOn(60, 0.8f);
    render(h, 24000);
    const auto held = h.getVoiceInfo(0);
    h.setParams(closed);
    render(h, 256);
    h.setParams(open);
    render(h, 256);
    const auto still = h.getVoiceInfo(0);
    check(held.held && still.held && h.activeVoices() == 1 && still.note == 60,
          "gate edges do not release, retrigger or reassign a held note");

    // A tail that began with the gate closed stays short when the gate re-opens.
    Engine r;
    r.prepare(kRate);
    r.setParams(closed);
    r.noteOn(60, 0.8f);
    render(r, 14400);
    r.noteOff(60);
    r.setParams(open);
    render(r, 96000);
    check(r.activeVoices() == 0, "re-opening the gate does not lengthen a tail that began without SUSTAIN");
}

void testSustainModeTwoWithGate()
{
    for (int gate = 0; gate < 2; ++gate)
    {
        // A long independent rank release keeps the tail audible even with the
        // footswitch gate closed, so only the mode II privilege can differ.
        ParamValues p = quietPatch();
        p[P_sustainMode] = 1.0f;
        p[P_keyRelease] = 0.0f;
        p[chParam(0, CP_vR)] = p[chParam(1, CP_vR)] = 5.0f;
        p[chParam(0, CP_fR)] = p[chParam(1, CP_fR)] = 5.0f;
        if (gate == 1)
        {
            p[P_sustainAssign] = 1.0f;
            p[P_footswitchPlugged] = 1.0f;
        }
        Engine e;
        e.prepare(kRate);
        e.setParams(p);
        e.noteOn(60, 0.8f);
        render(e, 12000);
        e.noteOff(60);
        render(e, 24000);
        check(e.activeVoices() == 1, "fixture: released tail is still sounding");
        e.noteOn(67, 0.8f);
        render(e, 32);
        // Gate open: mode II cuts the previous tail. Gate closed: it is an ordinary release.
        check(e.activeVoices() == (gate == 0 ? 1 : 2),
              "sustain II tail privilege follows the footswitch gate (gate " + std::to_string(gate) + ")");
    }
}

// --------------------------------------------------------------------------

void testGlideAssigner()
{
    auto startHz = [](ParamValues p, bool closeGate)
    {
        p[P_portamento] = 1.0f;
        p[P_glideMode] = 1.0f;
        if (closeGate)
        {
            p[P_glideAssign] = 1.0f;
            p[P_footswitchPlugged] = 1.0f;
        }
        Engine e;
        e.prepare(kRate);
        e.setParams(p);
        e.noteOn(48, 0.8f);
        render(e, 4800);
        e.noteOff(48);
        render(e, 4800);
        e.noteOn(72, 0.8f);
        render(e, 8);
        // The cyclic allocator puts the second note on voice 1.
        return e.getRankInfo(1, 0).oscillatorHz;
    };
    const float target = midiToHz(72.0f);
    const float source = midiToHz(48.0f);
    const float glided = startHz(quietPatch(), false);
    check(glided < target * 0.6f && glided > source * 0.9f, "with no assigner PORTA glides from the previous key");
    checkNear(startHz(quietPatch(), true), target, target * 1e-4f,
              "an open assigned footswitch suppresses a new glide");

    // Closing the gate mid-glide snaps to the target on the next sample.
    ParamValues p = quietPatch();
    p[P_portamento] = 2.0f;
    Engine e;
    e.prepare(kRate);
    e.setParams(p);
    e.noteOn(48, 0.8f);
    render(e, 4800);
    e.noteOff(48);
    e.noteOn(72, 0.8f);
    render(e, 2400);
    check(e.getRankInfo(1, 0).oscillatorHz < midiToHz(72.0f) * 0.9f, "fixture: glide is in progress");
    p[P_glideAssign] = 1.0f;
    p[P_footswitchPlugged] = 1.0f;
    e.setParams(p);
    render(e, 2);
    checkNear(e.getRankInfo(1, 0).oscillatorHz, midiToHz(72.0f), midiToHz(72.0f) * 1e-4f,
              "closing the gate snaps a running glide to its target");
    // Re-opening does not resurrect the discarded transition.
    p[P_footswitchPressed] = 1.0f;
    e.setParams(p);
    render(e, 2);
    checkNear(e.getRankInfo(1, 0).oscillatorHz, midiToHz(72.0f), midiToHz(72.0f) * 1e-4f,
              "re-opening the gate does not resurrect the old glide");

    // Glissando quantization is part of the gated P/G effect.
    auto pitchOf = [](bool closeGate)
    {
        ParamValues q = quietPatch();
        q[P_glideMode] = 2.0f;
        q[P_masterTune] = 0.4f;
        if (closeGate)
        {
            q[P_glideAssign] = 1.0f;
            q[P_footswitchPlugged] = 1.0f;
        }
        return settle(q, 60, 4800).oscillatorHz;
    };
    checkNear(pitchOf(false), midiToHz(60.0f), 0.01f, "glissando rounds the pitch to a semitone");
    checkNear(pitchOf(true), midiToHz(60.4f), 0.05f, "an open assigned footswitch disables glissando rounding");

    // The two assigners are independent: assigning only glide leaves SUSTAIN alone.
    ParamValues s = sustainPatch();
    s[P_glideAssign] = 1.0f;
    s[P_footswitchPlugged] = 1.0f;
    check(ampAfterRelease(s) > 0.2f, "the P/G assigner does not gate SUSTAIN");
}

// --------------------------------------------------------------------------

double highBandShare(const std::vector<float>& x)
{
    // First-difference energy over total energy: a cheap spectral-tilt measure.
    double diff = 0.0, tot = 0.0;
    for (size_t i = 2; i < x.size(); i += 2)
    {
        const double d = (double) x[i] - x[i - 2];
        diff += d * d;
        tot += (double) x[i] * x[i];
    }
    return tot > 0.0 ? diff / tot : 0.0;
}

void testExpWah()
{
    ParamValues exp = quietPatch();
    exp[chParam(0, CP_lpfCutoff)] = exp[chParam(1, CP_lpfCutoff)] = 18000.0f;
    ParamValues wah = exp;
    wah[P_footMode] = 1.0f;

    auto renderPedal = [&](const ParamValues& p, float pedal)
    {
        Engine e;
        e.prepare(kRate);
        e.setParams(p);
        e.controlChange(11, pedal);
        e.noteOn(45, 0.8f);
        e.noteOn(52, 0.8f);
        render(e, 24000);
        return render(e, 24000);
    };
    const auto heel = renderPedal(wah, 0.05f);
    const auto toe = renderPedal(wah, 1.0f);
    check(finiteBounded(heel) && finiteBounded(toe), "EXP-WAH output is finite and bounded");
    check(rms(heel) > 1e-4 && rms(toe) > 1e-4, "EXP-WAH output is audible at both pedal extremes");
    check(highBandShare(toe) > highBandShare(heel) * 1.5,
          "EXP-WAH moves the spectrum upward as the pedal goes down");
    check(renderPedal(exp, 1.0f) != toe, "EXP-WAH differs from EXP at the same pedal position");

    // In EXP the pedal is purely a level control: spectral tilt does not move with it.
    const double tiltHeel = highBandShare(renderPedal(exp, 0.3f));
    const double tiltToe = highBandShare(renderPedal(exp, 1.0f));
    check(std::abs(tiltHeel - tiltToe) < tiltToe * 0.05, "EXP leaves the spectrum alone and only scales level");
    check(rms(renderPedal(exp, 1.0f)) > rms(renderPedal(exp, 0.3f)) * 2.0, "EXP pedal controls level");

    // Mode switching while sounding must be click-free and finite.
    Engine e;
    e.prepare(kRate);
    e.setParams(exp);
    e.noteOn(48, 0.9f);
    e.noteOn(55, 0.9f);
    std::vector<float> all;
    for (int i = 0; i < 8; ++i)
    {
        ParamValues p = (i & 1) ? wah : exp;
        e.setParams(p);
        e.controlChange(11, 0.2f + 0.1f * (float) i);
        const auto b = render(e, 6000);
        all.insert(all.end(), b.begin(), b.end());
    }
    check(finiteBounded(all), "toggling EXP/EXP-WAH while playing stays finite and bounded");
    double maxStep = 0.0;
    for (size_t i = 2; i < all.size(); i += 2)
        maxStep = std::max(maxStep, (double) std::abs(all[i] - all[i - 2]));
    check(maxStep < 0.25, "mode switch does not produce a full-scale discontinuity");

    // Pedal automation inside the wah band is smoothed rather than stepped.
    Engine s;
    s.prepare(kRate);
    s.setParams(wah);
    s.controlChange(11, 0.1f);
    s.noteOn(48, 0.9f);
    render(s, 24000);
    s.controlChange(11, 1.0f);
    const auto jump = render(s, 2000);
    double stepMax = 0.0;
    for (size_t i = 2; i < jump.size(); i += 2)
        stepMax = std::max(stepMax, (double) std::abs(jump[i] - jump[i - 2]));
    check(finiteBounded(jump) && stepMax < 0.25, "abrupt pedal travel does not click");
}

// --------------------------------------------------------------------------

void testOutputLevel()
{
    ParamValues high = quietPatch();
    ParamValues low = high;
    low[P_outputLevel] = 0.0f;
    auto level = [](const ParamValues& p)
    {
        Engine e;
        e.prepare(kRate);
        e.setParams(p);
        e.noteOn(60, 0.8f);
        e.noteOn(67, 0.8f);
        render(e, 24000);
        return render(e, 24000);
    };
    const auto h = level(high), l = level(low);
    checkNear((float) (rms(l) / rms(h)), kProvisionalLowOutputGain, 1e-4f, "LOW applies the explicit -20 dB trim");
    check(outputLevelGain(true) == 1.0f, "HIGH is exactly unity");

    // Mono fold still sums the trimmed channels.
    ParamValues mono = low;
    mono[P_outputMode] = 0.0f;
    const auto m = level(mono);
    bool same = true;
    for (size_t i = 0; i < m.size(); i += 2)
        same = same && m[i] == m[i + 1];
    check(same && finiteBounded(m), "LOW still folds to a finite mono signal");

    // Flipping the switch while sounding ramps instead of stepping.
    Engine e;
    e.prepare(kRate);
    e.setParams(high);
    e.noteOn(60, 0.8f);
    render(e, 24000);
    e.setParams(low);
    const auto sw = render(e, 2400);
    double maxStep = 0.0;
    for (size_t i = 2; i < sw.size(); i += 2)
        maxStep = std::max(maxStep, (double) std::abs(sw[i] - sw[i - 2]));
    check(maxStep < 0.05, "HIGH to LOW is smoothed");
}

void testBoundaryInputs()
{
    // Extreme and non-finite control values must leave a finite, bounded render.
    for (int mode = 0; mode < 3; ++mode)
    {
        ParamValues p = quietPatch();
        for (int i = P_kbdBrillianceLow; i < P_numParams; ++i)
        {
            const auto& d = paramDef(i);
            p[i] = mode == 0 ? d.max + 1.0e6f : mode == 1 ? d.min - 1.0e6f
                                                          : std::numeric_limits<float>::quiet_NaN();
        }
        p[P_footMode] = 1.0f;
        Engine e;
        e.prepare(kRate);
        e.setParams(p);
        e.controlChange(11, std::numeric_limits<float>::quiet_NaN());
        e.noteOn(24, 1.0f);
        e.noteOn(84, 1.0f);
        check(finiteBounded(render(e, 12000), 2.0f), "extreme performance-control values stay finite");
    }
    for (double sr : { 8000.0, 44100.0, 96000.0, 192000.0 })
    {
        ParamValues p = quietPatch();
        p[P_footMode] = 1.0f;
        p[P_kbdBrillianceHigh] = 1.0f;
        p[P_kbdLevelLow] = -1.0f;
        p[P_sustainAssign] = p[P_footswitchPlugged] = 1.0f;
        p[P_outputLevel] = 0.0f;
        Engine e;
        e.prepare(sr);
        e.setParams(p);
        e.controlChange(11, 0.7f);
        e.noteOn(40, 0.8f);
        e.noteOn(80, 0.8f);
        std::vector<float> out((size_t) 8192 * 2);
        e.render(out.data(), 8192);
        check(finiteBounded(out), "performance controls are finite at sample rate " + std::to_string((int) sr));
    }
}
} // namespace

int main()
{
    testParameterTable();
    testContourWeights();
    testContourRouting();
    testFootswitchTruthTable();
    testSustainAssigner();
    testSustainGateEdges();
    testSustainModeTwoWithGate();
    testGlideAssigner();
    testExpWah();
    testOutputLevel();
    testBoundaryInputs();

    if (failures == 0)
        std::printf("ALL %d PERFORMANCE CONTROL CHECKS PASSED\n", checks);
    else
        std::printf("%d OF %d PERFORMANCE CONTROL CHECKS FAILED\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
