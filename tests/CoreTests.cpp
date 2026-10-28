#include "core/Dsp.h"
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "core/Params.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
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
          what + " (actual " + std::to_string(actual) + ", expected "
              + std::to_string(expected) + ")");
}

std::vector<float> render(Engine& engine, double sampleRate, int frames,
                          int blockSize = 256)
{
    (void) sampleRate;
    std::vector<float> buffer((size_t) frames * 2, 0.0f);
    for (int i = 0; i < frames; i += blockSize)
    {
        const int n = std::min(blockSize, frames - i);
        engine.render(buffer.data() + (size_t) i * 2, n);
    }
    return buffer;
}

struct Analysis
{
    float peak = 0.0f;
    double rms = 0.0;
    double mean = 0.0;
    bool finite = true;
};

Analysis analyse(const std::vector<float>& buffer)
{
    Analysis a;
    double sum = 0.0;
    double square = 0.0;
    for (float s : buffer)
    {
        if (! std::isfinite(s))
        {
            a.finite = false;
            continue;
        }
        a.peak = std::max(a.peak, std::abs(s));
        sum += s;
        square += (double) s * (double) s;
    }
    if (! buffer.empty())
    {
        a.mean = sum / (double) buffer.size();
        a.rms = std::sqrt(square / (double) buffer.size());
    }
    return a;
}

void testParameterTable()
{
    check((int) P_numParams == (int) P_ch2Start + (int) CP_count, "parameter count");
    check((int) P_ch2Start - (int) P_ch1Start == (int) CP_count, "channel blocks are contiguous");
    check(std::string(paramId(0)) == "master_tune", "global parameter id");
    check(std::string(paramId(P_ch2Start)) == "ch2_saw", "channel parameter id");

    for (int i = 0; i < P_numParams; ++i)
    {
        const auto& d = paramDef(i);
        check(d.min <= d.def && d.def <= d.max, std::string("default in range: ") + d.id);
        check(! paramId(i).empty(), std::string("parameter id: ") + d.id);
        check(normaliseParam(i, d.min) == 0.0f, std::string("normalise min: ") + d.id);
        check(normaliseParam(i, d.max) == 1.0f, std::string("normalise max: ") + d.id);
    }
}

void testSineFrequency()
{
    constexpr float sr = 48000.0f;
    constexpr float freq = 440.0f;
    Oscillator osc;
    Rng rng;
    int crossings = 0;
    float last = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const auto o = osc.tick(freq, sr, 0.5f, false, false, 0.0f, rng);
        if (i > 0 && ((last <= 0.0f && o.sine > 0.0f) || (last >= 0.0f && o.sine < 0.0f)))
            ++crossings;
        last = o.sine;
    }
    check(std::abs(crossings - 880) <= 3, "sine oscillator frequency");
}

void testFeetRatios()
{
    const float base = midiToHz(60.0f, 440.0f);
    const float fifth = midiToHz(67.0f, 440.0f);
    checkNear(fifth / base, 1.4983f, 0.01f, "5 1/3 foot is a fifth");
    checkNear(midiToHz(72.0f) / base, 2.0f, 0.001f, "octave ratio");
    checkNear(midiToHz(69.0f, 440.0f), 440.0f, 0.01f, "A4 reference");
    checkNear(kFeetSemitones[0], -12.0f, 0.001f, "16 foot is one octave down");
    checkNear(kFeetSemitones[1], 0.0f, 0.001f, "8 foot is unison");
    checkNear(kFeetSemitones[2], 7.0f, 0.001f, "5 1/3 foot is a fifth up");
    checkNear(kFeetSemitones[3], 12.0f, 0.001f, "4 foot is one octave up");
    checkNear(kFeetSemitones[4], 19.0f, 0.001f, "2 2/3 foot is octave and fifth");
    checkNear(kFeetSemitones[5], 24.0f, 0.001f, "2 foot is two octaves up");
}

void testOscillatorDiscontinuities()
{
    // Subtract the explicitly modelled reset spike to isolate the falling saw.
    // At phase zero a band-limited discontinuity crosses its midpoint, rather
    // than doubling to +2 as the old inverted BLEP correction did.
    for (float sr : { 44100.0f, 48000.0f, 96000.0f, 192000.0f })
    {
        Oscillator osc;
        Rng rng;
        const float frequency = sr / 64.0f;
        osc.reset(1.0f - frequency / sr);
        const auto edge = osc.tick(frequency, sr, 0.5f, true, false, 0.0f, rng);
        checkNear(edge.saw - edge.narrow * 0.70f, 0.0f, 1.0e-6f,
                  "falling saw BLEP reaches edge midpoint");
        float peak = 0.0f;
        for (int i = 0; i < 640; ++i)
        {
            const auto o = osc.tick(frequency, sr, 0.5f, true, false, 0.0f, rng);
            peak = std::max(peak, std::abs(o.saw - o.narrow * 0.70f));
        }
        check(peak <= 1.00001f, "falling saw BLEP does not reinforce discontinuity");
    }
    // A 30us reset at 100Hz occupies 0.003 of a cycle. Test the generic
    // band-limited pulse rather than baking an unsupported 1% width limit in.
    checkNear(blepPulse(0.006f, 0.003f, 0.001f), -1.0f, 1.0e-6f,
              "fixed-time pulse falls before 1 percent phase");
    checkNear(blepPulse(0.0015f, 0.003f, 0.001f), 1.0f, 1.0e-6f,
              "sub-percent pulse retains positive interior");
}

void testHardwareRingAndRibbon()
{
    checkNear(ribbonFrequencyRatio(0.0f), 1.0f, 1.0e-7f, "ribbon first contact is unison");
    checkNear(ribbonFrequencyRatio(1.0f), 2.0f, 1.0e-7f, "ribbon full upward stroke is octave");
    checkNear(ribbonFrequencyRatio(-1.0f), 0.0f, 1.0e-7f, "ribbon downward endpoint reaches zero");
    check(ribbonFrequencyRatio(-0.9999f) < 0.001f, "ribbon reaches sub-audio ratio");
    checkNear(ringModulationFrequency(80.0f, 0.0f, 1.0f), 80.0f, 1.0e-7f,
              "zero ring depth keeps base speed");
    checkNear(ringModulationFrequency(80.0f, 1.0f, 0.0f), 80.0f, 1.0e-7f,
              "ring envelope returns to base speed");
    check(ringModulationFrequency(80.0f, 1.0f, 1.0f) > 80.0f,
          "ring envelope increases frequency, not amplitude");
    check(ringModulationFrequency(10.0f, 1.0f, 1.0f)
          < ringModulationFrequency(80.0f, 1.0f, 1.0f),
          "ring depth peak depends on base speed");

    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        ParamValues p;
        p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
        p[P_ringSpeed] = 80.0f;
        p[P_ringDepth] = 1.0f;
        p[P_ringAttack] = 0.1f;
        p[P_ringDecay] = 0.1f;
        p[P_ringAmount] = 0.0f; // envelope must advance even when wet path off
        Engine e;
        e.prepare(sr);
        e.setParams(p);
        e.noteOn(60, 0.8f);
        render(e, sr, (int) (sr * 0.02));
        check(e.getRingFrequency() > 80.0f, "ring speed rises with AD while bypassed");
        const float beforeLegato = e.getRingEnvelope();
        e.noteOn(64, 0.8f);
        checkNear(e.getRingEnvelope(), beforeLegato, 1.0e-7f,
                  "legato chord does not retrigger ring envelope");
        render(e, sr, (int) (sr * 0.5));
        checkNear(e.getRingFrequency(), 80.0f, 0.1f, "ring speed decays to panel speed");
        e.allNotesOff(false);
        e.noteOn(67, 0.8f);
        checkNear(e.getRingEnvelope(), 0.0f, 1.0e-7f, "new phrase retriggers ring AD");
        p[P_ringAmount] = 1.0f;
        p[P_ringSpeed] = paramDef(P_ringSpeed).max;
        e.setParams(p);
        e.setRibbon(-1.0f);
        auto audio = analyse(render(e, sr, (int) (sr * 0.1)));
        check(audio.finite && audio.peak < 2.0f, "zero-frequency ribbon with max ring finite");
        e.allNotesOff(true);
        render(e, sr, (int) (sr * 2.0));
        check(e.activeVoices() == 0, "zero-frequency ribbon does not hold envelope open");
    }

    for (int sustainMode : { 0, 1 })
    {
        Engine e;
        e.prepare(48000);
        ParamValues p;
        p[P_sustainMode] = (float) sustainMode;
        p[chParam(0, CP_vR)] = p[chParam(1, CP_vR)] = 3.0f;
        e.setParams(p);
        e.noteOn(60, 0.8f);
        e.setRibbon(0.5f);
        render(e, 48000, 32);
        checkNear(e.getVoiceInfo(0).ribbonRatio, 1.5f, 1.0e-6f, "held note tracks ribbon");
        e.noteOff(60);
        e.setRibbon(-0.5f);
        render(e, 48000, 32);
        checkNear(e.getVoiceInfo(0).ribbonRatio, sustainMode == 0 ? 1.5f : 0.5f,
                  1.0e-6f, "ribbon release-tail sustain mode routing");
    }
    Engine automated;
    automated.prepare(48000);
    ParamValues p;
    p[P_ribbon] = 0.5f;
    automated.setParams(p);
    automated.noteOn(60, 0.8f);
    render(automated, 48000, 32);
    checkNear(automated.getVoiceInfo(0).ribbonRatio, 1.5f, 1.0e-6f,
              "host ribbon parameter reaches engine");
}

void testFilterStability()
{
    Svf f;
    f.set(1200.0f, 8.5f, 48000.0f);
    Rng rng;
    float peak = 0.0f;
    for (int i = 0; i < 48000 * 4; ++i)
    {
        const float x = (i == 0 ? 1.0f : 0.0f) + rng.nextBipolar() * 0.00001f;
        const auto o = f.process(x);
        if (! std::isfinite(o.lp) || ! std::isfinite(o.hp))
        {
            check(false, "filter finite");
            return;
        }
        peak = std::max(peak, std::abs(o.lp));
    }
    check(peak < 20.0f, "filter does not run away");
}

void testEnvelope()
{
    Adsr e;
    e.reset();
    e.noteOn(0.01f, 0.1f, 0.5f, 0.2f);
    int attackSamples = 0;
    for (int i = 0; i < 48000; ++i)
    {
        const float v = e.tick(48000.0f, true);
        if (v > 0.99f)
        {
            attackSamples = i;
            break;
        }
    }
    check(attackSamples > 100 && attackSamples < 1200, "envelope attack time");
    for (int i = 0; i < 48000; ++i)
        e.tick(48000.0f, true);
    checkNear(e.getValue(), 0.5f, 0.02f, "envelope sustain");
    e.noteOff();
    for (int i = 0; i < 48000; ++i)
        e.tick(48000.0f, true);
    check(e.isIdle(), "envelope release finishes");

    Adsr gate;
    gate.reset();
    gate.noteOn(0.0f, 0.2f, 1.0f, 0.2f);
    checkNear(gate.tick(48000.0f, true), 1.0f, 0.0001f, "zero attack reaches full level");
}

void testSustainPedal()
{
    ParamValues p;
    p[P_volume] = 0.6f;
    p[P_keyRelease] = 0.05f;

    Engine engine;
    engine.prepare(48000.0);
    engine.setParams(p);
    engine.noteOn(60, 0.8f, 1);
    render(engine, 48000.0, 2048);

    engine.controlChange(64, 1.0f, 1);
    engine.noteOff(60, 1);
    const auto held = render(engine, 48000.0, 24000);
    check(engine.activeVoices() > 0, "sustain pedal holds a released note");
    check(analyse(held).finite, "sustain pedal render finite");

    engine.controlChange(64, 0.0f, 1);
    const auto tail = render(engine, 48000.0, 96000);
    check(engine.activeVoices() == 0, "sustain pedal releases when lifted");
    check(analyse(tail).finite, "sustain pedal release finite");
}

void testAllSoundOff()
{
    Engine engine;
    engine.prepare(48000.0);
    engine.noteOn(60, 0.8f, 1);
    render(engine, 48000.0, 2048);
    check(engine.activeVoices() > 0, "panic test voice active");
    engine.allNotesOff(false);
    check(engine.activeVoices() == 0, "all sound off stops voices immediately");
}

void testMpeExpression()
{
    ParamValues p;
    p[P_mpeOn] = 1.0f;
    p[P_pitchBendRange] = 2.0f;
    p[P_mpeBendRange] = 48.0f;

    Engine engine;
    engine.prepare(48000.0);
    engine.setParams(p);
    engine.noteOn(60, 0.8f, 2);
    engine.noteOn(60, 0.8f, 3);
    engine.polyPressure(60, 0.9f, 2);
    engine.pitchBend(2, 1.0f);

    bool sawChannel2 = false;
    bool sawChannel3 = false;
    for (int i = 0; i < 8; ++i)
    {
        const auto info = engine.getVoiceInfo(i);
        if (! info.active || info.note != 60)
            continue;
        if (info.channel == 2)
        {
            checkNear(info.pressure, 0.9f, 0.001f, "MPE pressure is channel-scoped");
            checkNear(info.bend, 48.0f, 0.001f, "MPE bend uses MPE range");
            sawChannel2 = true;
        }
        else if (info.channel == 3)
        {
            checkNear(info.pressure, 0.0f, 0.001f, "MPE pressure leaves other channel alone");
            checkNear(info.bend, 0.0f, 0.001f, "MPE bend leaves other channel alone");
            sawChannel3 = true;
        }
    }
    check(sawChannel2 && sawChannel3, "MPE voices retain their member channels");
}

void testSubOscillatorWaves()
{
    for (int wave = 0; wave < 6; ++wave)
    {
        Lfo lfo;
        Rng rng(0x51c80u + (uint32_t) wave);
        lfo.reset(0.0f);
        float lo = 1.0f;
        float hi = -1.0f;
        bool finite = true;
        for (int i = 0; i < 48000; ++i)
        {
            const float value = lfo.tick(7.0f, 48000.0f, wave, rng);
            finite = finite && std::isfinite(value);
            lo = std::min(lo, value);
            hi = std::max(hi, value);
        }
        check(finite, "sub oscillator wave finite: " + std::to_string(wave));
        check(hi - lo > 0.05f, "sub oscillator wave moves: " + std::to_string(wave));
    }
}

void testRepeatedNoteRelease()
{
    Engine engine;
    engine.prepare(48000.0);
    engine.noteOn(60, 0.2f, 1);
    engine.noteOn(60, 0.9f, 1);
    engine.noteOff(60, 1);
    // The released voice stays active while its release tail fades; let it
    // finish before checking which voice is still held.
    render(engine, 48000.0, 96000);

    int activeSixty = 0;
    for (int i = 0; i < 8; ++i)
    {
        const auto info = engine.getVoiceInfo(i);
        if (info.active && info.note == 60)
        {
            ++activeSixty;
            checkNear(info.velocity, 0.2f, 0.001f,
                      "repeated note-off releases the newest voice");
        }
    }
    check(activeSixty == 1, "repeated note leaves the older voice active");
    engine.noteOff(60, 1);
    engine.allNotesOff(false);
    check(engine.activeVoices() == 0, "repeated note cleanup");
}

void testVoiceAllocation()
{
    Engine engine;
    engine.prepare(48000.0);
    for (int i = 0; i < 12; ++i)
        engine.noteOn(48 + i, 0.8f, 1);
    check(engine.activeVoices() <= 8, "voice limit is eight");
    const auto buffer = render(engine, 48000.0, 4096);
    const auto a = analyse(buffer);
    check(a.finite, "voice allocation render finite");
    check(a.peak > 0.001f, "voice allocation produces audio");
}

void testRenderAndRelease()
{
    Engine engine;
    engine.prepare(48000.0);
    engine.noteOn(60, 0.9f, 1);
    engine.noteOn(64, 0.9f, 1);
    engine.noteOn(67, 0.9f, 1);

    const auto held = render(engine, 48000.0, 512 * 40);
    const auto a = analyse(held);
    check(a.finite, "audio finite");
    check(a.peak > 0.001f && a.peak < 2.0f, "audio has sensible level");
    check(std::abs(a.mean) < 0.01, "audio has no large DC offset");

    engine.noteOff(60, 1);
    engine.noteOff(64, 1);
    engine.noteOff(67, 1);
    const auto tail = render(engine, 48000.0, 512 * 300);
    check(analyse(tail).finite, "release tail finite");
    check(engine.activeVoices() == 0, "voices release");
}

void testVelocityResponse()
{
    auto measure = [](float velocity) {
        ParamValues p;
        for (int ch = 0; ch < 2; ++ch)
        {
            p[chParam(ch, CP_initLevel)] = 0.45f;
            p[chParam(ch, CP_initBrill)] = 0.2f;
        }
        Engine engine;
        engine.prepare(48000.0);
        engine.setParams(p);
        engine.noteOn(60, velocity, 1);
        const auto buffer = render(engine, 48000.0, 4800);
        return analyse(buffer).rms;
    };

    const double soft = measure(0.25f);
    const double hard = measure(1.0f);
    check(hard > soft * 1.5, "velocity changes level");
}

void testSampleRates()
{
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        ParamValues p;
        p[P_volume] = 0.55f;
        p[P_chorusOn] = 1.0f;
        Engine engine;
        engine.prepare(sr);
        engine.setParams(p);
        checkNear((float) engine.getInternalSampleRate(), (float) (sr * 2.0), 0.1f,
                  "2x internal rate at " + std::to_string((int) sr));
        engine.noteOn(57, 0.8f, 1);
        const auto buffer = render(engine, sr, (int) (sr * 0.35));
        const auto a = analyse(buffer);
        check(a.finite, "sample-rate render finite");
        check(a.peak > 0.001f && a.peak < 2.0f, "sample-rate render level");
    }
}

void testArpeggiator()
{
    ParamValues p;
    p[P_arpOn] = 1.0f;
    p[P_arpRate] = 8.0f;
    p[P_arpMode] = 0.0f;
    p[P_arpOctaves] = 2.0f;
    p[P_volume] = 0.6f;

    Engine engine;
    engine.prepare(48000.0);
    engine.setParams(p);
    engine.noteOn(60, 0.8f, 1);
    engine.noteOn(64, 0.8f, 1);
    engine.noteOn(67, 0.8f, 1);

    std::set<int> notes;
    bool finite = true;
    for (int block = 0; block < 160; ++block)
    {
        const auto buffer = render(engine, 48000.0, 256);
        finite = finite && analyse(buffer).finite;
        for (int v = 0; v < 8; ++v)
        {
            const auto info = engine.getVoiceInfo(v);
            if (info.active)
                notes.insert(info.note);
        }
    }

    check(finite, "arpeggiator output finite");
    check(engine.activeVoices() >= 1, "arpeggiator keeps a voice running");
    check(notes.size() >= 4, "arpeggiator visits multiple notes");
    check(notes.count(72) > 0 && notes.count(76) > 0, "arpeggiator transposes octaves");

    engine.noteOff(60, 1);
    engine.noteOff(64, 1);
    engine.noteOff(67, 1);
    const auto tail = render(engine, 48000.0, 48000);
    check(analyse(tail).finite, "arpeggiator release finite");
    check(engine.activeVoices() == 0, "arpeggiator stops when chord is released");
}

void testArpeggiatorModes()
{
    for (int mode = 0; mode < 5; ++mode)
    {
        ParamValues p;
        p[P_arpOn] = 1.0f;
        p[P_arpRate] = 12.0f;
        p[P_arpMode] = (float) mode;
        p[P_arpOctaves] = 1.0f;
        Engine engine;
        engine.prepare(48000.0);
        engine.setParams(p);
        engine.noteOn(60, 0.8f, 1);
        engine.noteOn(63, 0.8f, 1);
        engine.noteOn(67, 0.8f, 1);
        const auto buffer = render(engine, 48000.0, 24000);
        check(analyse(buffer).finite,
              "arpeggiator mode finite: " + std::to_string(mode));
        check(engine.activeVoices() > 0,
              "arpeggiator mode active: " + std::to_string(mode));
    }
}

void testEffects()
{
    for (int type = 1; type <= 5; ++type)
    {
        ParamValues p;
        p[P_fx1Type] = (float) type;
        p[P_fx1Mix] = 0.9f;
        p[P_chorusOn] = 0.0f;
        p[P_volume] = 0.55f;
        Engine engine;
        engine.prepare(48000.0);
        engine.setParams(p);
        engine.noteOn(60, 0.85f, 1);
        const auto buffer = render(engine, 48000.0, 48000);
        const auto a = analyse(buffer);
        check(a.finite, "FX finite: type " + std::to_string(type));
        check(a.peak > 0.0005f && a.peak < 2.0f,
              "FX level: type " + std::to_string(type));

        const auto right = [&] {
            double sum = 0.0;
            for (size_t i = 1; i < buffer.size(); i += 2)
                sum += (double) buffer[i] * (double) buffer[i];
            return std::sqrt(sum / (double) (buffer.size() / 2));
        }();
        check(right > 0.0001, "stereo FX has right channel: type " + std::to_string(type));
    }
}

void testMacros()
{
    ParamValues base;
    base[P_macro1] = 0.5f;
    base[P_macro2] = 0.5f;
    base[P_macro3] = 0.5f;
    base[P_macro4] = 0.5f;

    ParamValues wide = base;
    wide[P_macro3] = 1.0f;
    wide[P_macro4] = 1.0f;

    Engine a;
    Engine b;
    a.prepare(48000.0);
    b.prepare(48000.0);
    a.setParams(base);
    b.setParams(wide);
    a.noteOn(60, 0.85f, 1);
    b.noteOn(60, 0.85f, 1);

    const auto aa = analyse(render(a, 48000.0, 24000));
    const auto bb = analyse(render(b, 48000.0, 24000));
    check(aa.finite && bb.finite, "macro renders finite");
    check(std::abs(aa.rms - bb.rms) > 1.0e-5, "macros change the sound");
    check(bb.peak < 2.0f, "macro output remains bounded");
}

void testPresetDefaults()
{
    ParamValues p;
    check(p[P_volume] > 0.0f, "volume default");
    check(p[chParam(0, CP_lpfCutoff)] > 100.0f, "channel cutoff default");
    check(p[chParam(1, CP_saw)] >= 0.5f, "channel two saw default");
}

void testFactoryBank()
{
    const auto& bank = factoryPresets();
    check(bank.size() >= 200, "factory bank has at least 200 presets");
    check(std::string(hardwareToneName(0, 0)) == "String 1", "rank I hardware tone identity");
    check(std::string(hardwareToneName(0, 1)) == "String 2", "rank II hardware tone identity");
    check(std::string(hardwareToneName(3, 0)) == "Flute", "rank I fourth tab identity");
    check(std::string(hardwareToneName(3, 1)) == "Brass 3", "rank II fourth tab identity");

    const char* expected[2][11] = {
        { "String 1", "String 3", "Brass 1", "Flute", "E. Piano", "Clavichord 1",
          "Harpsichord 1", "Organ 1", "Guitar 1", "Funky 1", "Funky 3" },
        { "String 2", "String 4", "Brass 2", "Brass 3", "Bass", "Clavichord 2",
          "Harpsichord 2", "Organ 2", "Guitar 2", "Funky 2", "Funky 4" }
    };
    for (int channel = 0; channel < 2; ++channel)
        for (int tone = 0; tone < 11; ++tone)
        {
            check(std::string(hardwareToneName(tone, channel)) == expected[channel][tone],
                  "original hardware tab order, bank " + std::to_string(channel));
            ParamValues selected;
            selected[P_volume] = 0.37f;
            selected[P_ringSpeed] = 17.0f;
            selected[chParam(1 - channel, CP_noise)] = 0.67f;
            const auto before = selected;
            applyHardwareTone(selected, channel, tone);
            bool globalsPreserved = true;
            for (int i = 0; i < P_numGlobals; ++i)
                globalsPreserved = globalsPreserved && selected[i] == before[i];
            check(globalsPreserved, "hardware selection preserves every global");
            bool otherPreserved = true;
            for (int i = 0; i < CP_count; ++i)
                otherPreserved = otherPreserved
                    && selected[chParam(1 - channel, i)] == before[chParam(1 - channel, i)];
            check(otherPreserved, "hardware selection preserves entire other rank");
        }

    ParamValues p;
    const float globalBefore = p[P_volume];
    applyHardwareTone(p, 0, 2);
    check(p[chParam(0, CP_lpfCutoff)] < 2000.0f, "hardware brass tone changes the channel");
    checkNear(p[P_volume], globalBefore, 1.0e-7f, "hardware tone preserves globals");
    check(p[chParam(1, CP_lpfCutoff)] > 2000.0f, "hardware tone preserves other channel");

    std::set<std::string> names;
    for (const auto& preset : bank)
    {
        check(! preset.name.empty(), "preset has a name");
        check(! preset.category.empty(), "preset has a category");
        check(names.insert(preset.name).second, "preset name unique: " + preset.name);
        for (int i = 0; i < P_numParams; ++i)
        {
            const auto& d = paramDef(i);
            if (preset.values[i] < d.min - 0.001f || preset.values[i] > d.max + 0.001f)
            {
                check(false, "preset value in range: " + preset.name + " / " + d.id);
                break;
            }
        }
    }
}

void testEveryFactoryPresetRenders()
{
    Engine engine;
    engine.prepare(48000.0);
    int rendered = 0;
    for (const auto& preset : factoryPresets())
    {
        engine.reset();
        engine.allNotesOff(false);
        engine.setParams(preset.values);
        engine.noteOn(60, 0.75f, 1);
        const auto buffer = render(engine, 48000.0, 2048);
        const auto a = analyse(buffer);
        if (! a.finite || a.peak > 2.0f)
        {
            check(false, "preset renders cleanly: " + preset.name
                             + " (peak " + std::to_string(a.peak) + ")");
            break;
        }
        ++rendered;
    }
    check(rendered == (int) factoryPresets().size(), "all factory presets render cleanly");
}

} // namespace

int main()
{
    testParameterTable();
    testSineFrequency();
    testFeetRatios();
    testOscillatorDiscontinuities();
    testHardwareRingAndRibbon();
    testFilterStability();
    testEnvelope();
    testSustainPedal();
    testAllSoundOff();
    testMpeExpression();
    testSubOscillatorWaves();
    testRepeatedNoteRelease();
    testVoiceAllocation();
    testRenderAndRelease();
    testVelocityResponse();
    testSampleRates();
    testArpeggiator();
    testArpeggiatorModes();
    testEffects();
    testMacros();
    testPresetDefaults();
    testFactoryBank();
    testEveryFactoryPresetRenders();

    if (failures == 0)
        std::printf("ALL %d CHECKS PASSED\n", checks);
    else
        std::printf("%d OF %d CHECKS FAILED\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
