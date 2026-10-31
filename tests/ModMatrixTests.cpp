// Modern modulation layer (src/core/ModernLayer.h, ModMatrix.h) and its Engine hooks:
// routing math, response curves, MSEG trigger modes and tempo sync through the engine,
// default-render invariance and PURE CS-80 inertness. Modern controls only; nothing here
// is CS-80 evidence.
#include "core/Engine.h"
#include "core/ModernLayer.h"
#include "reference/MsegReference.h"
#include "ModernLayerTolerances.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace cs80;
using namespace cs80::modern;
namespace tol = cs80::tests::ws5;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* what)
{
    ++checks;
    if (! ok)
    {
        if (failures < 40)
            std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

bool near(double a, double b, double fullScale)
{
    return std::abs(a - b) <= tol::kMatrixRelativeTolerance * std::max(1.0, std::abs(fullScale));
}

ModernConfig routed(std::initializer_list<ModSlot> slots)
{
    ModernConfig c;
    int i = 0;
    for (const auto& s : slots)
        c.slots[(std::size_t) i++] = s;
    return c;
}

ModernControls amounts(std::initializer_list<float> values)
{
    ModernControls c;
    c.enabled = true;
    int i = 0;
    for (float v : values)
        c.amount[(std::size_t) i++] = v;
    return c;
}

GlobalInputs globals()
{
    GlobalInputs g;
    g.modWheel = 0.6f;
    g.ribbon = -0.35f;
    g.channelPressure = 0.45f;
    g.sharedPressure = 0.7f;
    g.expression = 0.8f;
    g.pitchBend = 0.25f;
    g.sub = -0.5f;
    g.ringEnvelope = 0.9f;
    g.lfo = { 0.3f, -0.8f, 0.55f };
    return g;
}

VoiceInputs voiceAt(int note, float velocity, std::uint64_t age = 1)
{
    VoiceInputs v;
    v.age = age;
    v.gate = true;
    v.note = note;
    v.velocity = velocity;
    v.pressure = 0.4f;
    v.timbre = 0.2f;
    v.contour = keyboardContourWeights(note);
    v.filterEnvelope = { 1.5f, -0.75f };
    v.ampEnvelope = { 0.6f, 0.9f };
    return v;
}

// One sample through a fresh layer.
ModernLayer run(const ModernConfig& config, const ModernControls& controls, const VoiceInputs& voice,
                const GlobalInputs& g = globals(), int samples = 1)
{
    ModernLayer layer;
    layer.prepare(96000.0);
    layer.setConfig(config);
    layer.setControls(controls);
    for (int n = 0; n < samples; ++n)
    {
        layer.beginSample(g);
        layer.processVoice(0, voice);
    }
    return layer;
}

void testRoutingMath()
{
    const auto v = voiceAt(72, 0.8f);
    const auto g = globals();
    // Every rank destination with a plain source: amount * source * scale.
    struct Case { ModSource source; float value; ModDestination dest; float RankOffsets::* field; };
    const Case cases[] = {
        { ModSource::velocity, 0.8f, ModDestination::pitch, &RankOffsets::pitch },
        { ModSource::modWheel, 0.6f, ModDestination::pulseWidth, &RankOffsets::pulseWidth },
        { ModSource::lfo2, -0.8f, ModDestination::hpCutoff, &RankOffsets::hpCutoff },
        { ModSource::ribbon, -0.35f, ModDestination::lpCutoff, &RankOffsets::lpCutoff },
        { ModSource::polyPressure, 0.4f, ModDestination::level, &RankOffsets::level },
        { ModSource::sub, -0.5f, ModDestination::pan, &RankOffsets::pan },
        { ModSource::constant, 1.0f, ModDestination::pitch, &RankOffsets::pitch },
        { ModSource::timbre, 0.2f, ModDestination::pitch, &RankOffsets::pitch },
        { ModSource::expression, 0.8f, ModDestination::lpCutoff, &RankOffsets::lpCutoff },
        { ModSource::pitchBend, 0.25f, ModDestination::lpCutoff, &RankOffsets::lpCutoff },
        { ModSource::ringEnvelope, 0.9f, ModDestination::level, &RankOffsets::level },
        { ModSource::channelPressure, 0.45f, ModDestination::pan, &RankOffsets::pan },
        { ModSource::sharedPressure, 0.7f, ModDestination::pan, &RankOffsets::pan },
        { ModSource::lfo1, 0.3f, ModDestination::pitch, &RankOffsets::pitch },
        { ModSource::lfo3, 0.55f, ModDestination::pitch, &RankOffsets::pitch },
    };
    for (const auto& c : cases)
        for (float amount : { 1.0f, -0.37f })
        {
            const auto layer = run(routed({ { c.source, ModSource::none, c.dest, ModTarget::both } }), amounts({ amount }), v, g);
            const double expected = (double) amount * c.value * destinationScale(c.dest);
            check(near(layer.rankOffsets(0, 0).*c.field, expected, destinationScale(c.dest))
                      && near(layer.rankOffsets(0, 1).*c.field, expected, destinationScale(c.dest)),
                  "slot offset = amount * source * destination scale");
        }
    // Cutoff (HP+LP) moves both filters; resonance is a Q ratio of 2^(octaves).
    {
        const auto layer = run(routed({ { ModSource::modWheel, ModSource::none, ModDestination::cutoff, ModTarget::both },
                                        { ModSource::velocity, ModSource::none, ModDestination::lpResonance, ModTarget::both },
                                        { ModSource::ribbon, ModSource::none, ModDestination::hpResonance, ModTarget::both } }),
                               amounts({ 0.5f, 0.25f, 1.0f }), v, g);
        const auto& o = layer.rankOffsets(0, 0);
        check(near(o.hpCutoff, 0.5 * 0.6 * kCutoffOctaves, kCutoffOctaves) && near(o.lpCutoff, 0.5 * 0.6 * kCutoffOctaves, kCutoffOctaves),
              "cutoff destination moves both filters");
        check(near(o.lpQ, std::exp2(0.25 * 0.8 * kResonanceOctaves), 4.0) && near(o.hpQ, std::exp2(-0.35 * kResonanceOctaves), 4.0),
              "resonance destination is a Q ratio of 2^(octaves)");
        check(modulatedQ(3.0f, 1.0f) == 3.0f && modulatedQ(3.0f, 4.0f) == kMaxModulatedQ && modulatedQ(0.7f, 0.25f) == kMinModulatedQ,
              "modulated Q passes unmodulated values and stays inside the documented 0.5..5");
    }
    // Via multiplies; two slots on one destination add; the rank target selects the rank.
    {
        const auto layer = run(routed({ { ModSource::lfo1, ModSource::modWheel, ModDestination::pitch, ModTarget::both },
                                        { ModSource::velocity, ModSource::none, ModDestination::pitch, ModTarget::rankII },
                                        { ModSource::constant, ModSource::ribbon, ModDestination::pan, ModTarget::rankI } }),
                               amounts({ 0.5f, -0.25f, 1.0f }), v, g);
        const double viaPitch = 0.5 * 0.3 * 0.6 * kPitchSemitones;
        check(near(layer.rankOffsets(0, 0).pitch, viaPitch, kPitchSemitones), "via source multiplies the slot");
        check(near(layer.rankOffsets(0, 1).pitch, viaPitch - 0.25 * 0.8 * kPitchSemitones, kPitchSemitones),
              "slots on one destination add, rank II target reaches rank II only");
        check(near(layer.rankOffsets(0, 0).pan, -0.35, 1.0) && layer.rankOffsets(0, 1).pan == 0.0f,
              "rank I target reaches rank I only");
    }
    // Rank sources: each rank reads its own EG.
    {
        const auto layer = run(routed({ { ModSource::filterEnvelope, ModSource::none, ModDestination::lpCutoff, ModTarget::both },
                                        { ModSource::ampEnvelope, ModSource::none, ModDestination::level, ModTarget::both } }),
                               amounts({ 1.0f, 0.5f }), v, g);
        check(near(layer.rankOffsets(0, 0).lpCutoff, 1.5 / 3.0 * kCutoffOctaves, kCutoffOctaves)
                  && near(layer.rankOffsets(0, 1).lpCutoff, -0.75 / 3.0 * kCutoffOctaves, kCutoffOctaves),
              "filter EG source is each rank's own EG, normalised by its 3-octave scale");
        check(near(layer.rankOffsets(0, 0).level, 0.5 * 0.6, 1.0) && near(layer.rankOffsets(0, 1).level, 0.5 * 0.9, 1.0),
              "VCA EG source is each rank's own EG");
    }
    // Key follow spans the 61-key surface; keyboard contour is the KBC ladder HIGH - LOW.
    for (int note : { 24, 40, 54, 70, 84, 100, 10 })
    {
        const auto layer = run(routed({ { ModSource::keyFollow, ModSource::none, ModDestination::pitch, ModTarget::both },
                                        { ModSource::keyboardContour, ModSource::none, ModDestination::pan, ModTarget::both } }),
                               amounts({ 1.0f, 1.0f }), voiceAt(note, 0.5f), g);
        const double key = std::clamp(((double) note - 24.0 - 30.0) / 30.0, -1.0, 1.0);
        const auto w = keyboardContourWeights(note);
        check(near(layer.rankOffsets(0, 0).pitch, key * kPitchSemitones, kPitchSemitones), "key follow spans MIDI 24..84 as -1..1");
        check(near(layer.rankOffsets(0, 0).pan, (double) w.high - w.low, 1.0), "keyboard contour source is HIGH - LOW weight");
    }
    // Global destinations: SUB speed ratio, SUB depth gain (clamped), FX mixes.
    {
        const auto layer = run(routed({ { ModSource::modWheel, ModSource::none, ModDestination::subSpeed, ModTarget::both },
                                        { ModSource::lfo2, ModSource::none, ModDestination::subDepth, ModTarget::both },
                                        { ModSource::expression, ModSource::none, ModDestination::fx2Mix, ModTarget::both },
                                        { ModSource::constant, ModSource::none, ModDestination::subDepth, ModTarget::both } }),
                               amounts({ 0.5f, 1.0f, -0.5f, 0.0f }), v, g, 2);
        const auto& o = layer.getGlobalOffsets();
        check(near(o.subSpeedRatio, std::exp2(0.5 * 0.6 * kSubSpeedOctaves), 8.0), "SUB speed offset is a ratio of 2^(octaves)");
        check(near(o.subDepthGain, 1.0 - 0.8, 1.0), "SUB depth gain is 1 + offset");
        check(near(o.fxMix[1], -0.4, 1.0) && o.fxMix[0] == 0.0f && o.fxMix[2] == 0.0f, "FX mix offsets reach their own slot");
        const auto clamped = run(routed({ { ModSource::constant, ModSource::none, ModDestination::subDepth, ModTarget::both } }),
                                 amounts({ 1.0f }), v, g, 2);
        check(clamped.getGlobalOffsets().subDepthGain == 2.0f, "SUB depth gain is clamped to 0..2");
    }
    // Global destinations read per-voice sources from the newest voice (last-note priority).
    {
        ModernLayer layer;
        layer.prepare(96000.0);
        layer.setConfig(routed({ { ModSource::velocity, ModSource::none, ModDestination::fx1Mix, ModTarget::both } }));
        layer.setControls(amounts({ 1.0f }));
        for (int n = 0; n < 3; ++n)
        {
            layer.beginSample(globals());
            layer.processVoice(0, voiceAt(60, 0.3f, 5));
            layer.processVoice(1, voiceAt(64, 0.9f, 7));
            layer.processVoice(2, voiceAt(67, 0.5f, 6));
        }
        check(near(layer.getGlobalOffsets().fxMix[0], 0.9, 1.0), "global destination follows the newest voice");
    }
}

void testCurvesAndNoteValues()
{
    for (float shape : { -1.0f, -0.5f, 0.0f, 0.4f, 1.0f })
        for (float x : { 0.0f, 0.1f, 0.5f, 0.93f, 1.0f })
        {
            const double expected = std::pow((double) x, std::exp2(-2.0 * shape));
            check(std::abs(responseCurve(x, shape) - expected) < 1e-6, "response curve is x^(2^(-2 shape))");
            check(std::abs(bipolarResponseCurve(-x, shape) + expected) < 1e-6, "bipolar response curve is odd");
        }
    check(responseCurve(0.37f, 0.0f) == 0.37f, "linear response curve is the exact input");
    // Each curve shapes its own sources.
    ModernControls c = amounts({ 1.0f, 1.0f, 1.0f, 1.0f });
    c.velocityCurve = 1.0f;
    c.pressureCurve = -1.0f;
    c.wheelCurve = 0.5f;
    c.keyCurve = -0.5f;
    const auto layer = run(routed({ { ModSource::velocity, ModSource::none, ModDestination::pitch, ModTarget::rankI },
                                    { ModSource::polyPressure, ModSource::none, ModDestination::pan, ModTarget::rankI },
                                    { ModSource::modWheel, ModSource::none, ModDestination::level, ModTarget::rankI },
                                    { ModSource::keyFollow, ModSource::none, ModDestination::lpCutoff, ModTarget::rankI } }),
                           c, voiceAt(36, 0.8f));
    const auto& o = layer.rankOffsets(0, 0);
    check(near(o.pitch, std::pow(0.8, 0.25) * kPitchSemitones, kPitchSemitones), "velocity curve shapes velocity");
    check(near(o.pan, std::pow(0.4, 4.0), 1.0), "aftertouch curve shapes pressure");
    check(near(o.level, std::pow(0.6, 0.5), 1.0), "mod wheel curve shapes the wheel");
    check(near(o.lpCutoff, -std::pow(18.0 / 30.0, 2.0) * kCutoffOctaves, kCutoffOctaves), "key curve shapes key follow");

    // Note random: fixed for a note, new for the next note on the same voice.
    ModernLayer r;
    r.prepare(96000.0);
    r.setConfig(routed({ { ModSource::noteRandom, ModSource::none, ModDestination::pitch, ModTarget::both } }));
    r.setControls(amounts({ 1.0f }));
    r.beginSample(globals());
    r.processVoice(0, voiceAt(60, 0.5f, 11));
    const float first = r.rankOffsets(0, 0).pitch;
    bool steady = true;
    for (int n = 0; n < 100; ++n)
    {
        r.beginSample(globals());
        r.processVoice(0, voiceAt(60, 0.5f, 11));
        steady = steady && r.rankOffsets(0, 0).pitch == first;
    }
    r.beginSample(globals());
    r.processVoice(0, voiceAt(60, 0.5f, 12));
    check(steady && r.rankOffsets(0, 0).pitch != first && std::abs(first) <= kPitchSemitones,
          "note random holds for a note and changes on the next");
}

void testInactiveIsIdentity()
{
    // No live slot (no source, no destination or a zero amount) or disabled: identity.
    const ModSlot full { ModSource::lfo1, ModSource::none, ModDestination::pitch, ModTarget::both };
    const ModSlot noSource { ModSource::none, ModSource::none, ModDestination::pitch, ModTarget::both };
    const ModSlot noDest { ModSource::lfo1, ModSource::none, ModDestination::none, ModTarget::both };
    auto identity = [](const ModernLayer& l)
    {
        const auto& o = l.rankOffsets(0, 0);
        const auto& g = l.getGlobalOffsets();
        return ! l.isActive() && o.pitch == 0.0f && o.hpQ == 1.0f && o.lpQ == 1.0f && o.level == 0.0f
               && g.subSpeedRatio == 1.0f && g.subDepthGain == 1.0f && g.fxMix[0] == 0.0f;
    };
    check(identity(run(routed({ full }), amounts({ 0.0f }), voiceAt(60, 1.0f))), "zero amount is inert");
    check(identity(run(routed({ noSource }), amounts({ 1.0f }), voiceAt(60, 1.0f))), "slot without a source is inert");
    check(identity(run(routed({ noDest }), amounts({ 1.0f }), voiceAt(60, 1.0f))), "slot without a destination is inert");
    auto disabled = amounts({ 1.0f });
    disabled.enabled = false;
    check(identity(run(routed({ full }), disabled, voiceAt(60, 1.0f))), "disabled layer (PURE CS-80) is inert");
    // Turning the last slot off returns to the identity at once.
    ModernLayer layer;
    layer.prepare(96000.0);
    layer.setConfig(routed({ full, { ModSource::modWheel, ModSource::none, ModDestination::fx1Mix, ModTarget::both } }));
    layer.setControls(amounts({ 1.0f, 1.0f }));
    for (int n = 0; n < 4; ++n) { layer.beginSample(globals()); layer.processVoice(0, voiceAt(60, 1.0f)); }
    layer.setControls(amounts({ 0.0f, 0.0f }));
    check(identity(layer), "removing the last route restores the identity immediately");
}

// ---- Engine level -------------------------------------------------------------------

constexpr double kRate = 48000.0;

ModernConfig fullyConfigured()
{
    ModernConfig c;
    const ModSource sources[] = { ModSource::velocity, ModSource::polyPressure, ModSource::channelPressure,
                                  ModSource::sharedPressure, ModSource::modWheel, ModSource::ribbon, ModSource::keyFollow,
                                  ModSource::keyboardContour, ModSource::sub, ModSource::ringEnvelope, ModSource::lfo1,
                                  ModSource::mseg1, ModSource::mseg2, ModSource::mseg3, ModSource::filterEnvelope,
                                  ModSource::noteRandom };
    const ModDestination dests[] = { ModDestination::pitch, ModDestination::pulseWidth, ModDestination::cutoff,
                                     ModDestination::hpCutoff, ModDestination::lpCutoff, ModDestination::hpResonance,
                                     ModDestination::lpResonance, ModDestination::level, ModDestination::pan,
                                     ModDestination::subSpeed, ModDestination::subDepth, ModDestination::fx1Mix,
                                     ModDestination::fx2Mix, ModDestination::mseg1Rate, ModDestination::mseg2Rate,
                                     ModDestination::mseg3Rate };
    for (int s = 0; s < kModSlots; ++s)
        c.slots[(std::size_t) s] = { sources[s], s % 3 == 0 ? ModSource::lfo2 : ModSource::none, dests[s], (ModTarget) (s % 3) };
    c.msegs[0].trigger = MsegTrigger::poly;
    c.msegs[1].trigger = MsegTrigger::mono;
    c.msegs[1].sync = true;
    c.msegs[2].trigger = MsegTrigger::free;
    c.msegs[2].loop = MsegLoop::always;
    c.msegs[2].loopEnd = 1;
    return c;
}

void setModernParameters(ParamValues& p, float amount)
{
    for (int s = 0; s < kModSlots; ++s)
        p[P_mod1Amount + s] = amount * (s % 2 ? -0.6f : 0.8f);
    p[P_mseg1Rate] = 1.3f;   // octaves
    p[P_mseg2Rate] = -1.7f;
    p[P_mseg3Rate] = 2.8f;
    p[P_modVelocityCurve] = 0.7f;
    p[P_modPressureCurve] = -0.4f;
    p[P_modWheelCurve] = 0.2f;
    p[P_modKeyCurve] = -0.9f;
    p[P_lfo1Wave] = 6.0f;
    p[P_lfo2Wave] = 5.0f;
    p[P_lfo3Wave] = 3.0f;
    p[P_lfo1Depth] = p[P_lfo2Depth] = 0.0f; // only matrix taps of the LFOs
    p[P_fx1Type] = 2.0f;
    p[P_fx2Type] = 4.0f;
}

std::vector<float> perform(Engine& e, bool transport)
{
    std::vector<float> out;
    std::vector<float> block(2 * 256);
    for (int b = 0; b < 160; ++b)
    {
        if (transport) e.setTransport({ 132.0, b * 0.0115, true, true });
        if (b == 0) { e.noteOn(48, 0.35f, 1); e.noteOn(67, 0.9f, 2); }
        if (b == 10) e.controlChange(1, 0.7f);
        if (b == 14) e.channelPressure(2, 0.6f);
        if (b == 18) e.polyPressure(48, 0.8f, 1);
        if (b == 22) e.setRibbon(0.3f);
        if (b == 30) e.pitchBend(1, 0.4f);
        if (b == 40) e.noteOff(48, 1);
        if (b == 50) e.noteOn(55, 0.6f, 1);
        if (b == 90) { e.noteOff(67, 2); e.noteOff(55, 1); }
        if (b == 100) e.noteOn(72, 1.0f, 3);
        if (b == 140) e.noteOff(72, 3);
        e.render(block.data(), 256);
        out.insert(out.end(), block.begin(), block.end());
    }
    return out;
}

std::vector<float> renderPolicy(int policy, bool configured, float amount, bool transport)
{
    ParamValues p;
    p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
    p[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
    if (configured)
        setModernParameters(p, amount);
    Engine e;
    e.prepare(kRate);
    if (configured)
        e.setModernConfig(fullyConfigured());
    e.setParams(p);
    return perform(e, transport);
}

bool identical(const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

void testDefaultInvarianceAndPurity()
{
    for (int policy = 0; policy < 3; ++policy)
    {
        const auto reference = renderPolicy(policy, false, 0.0f, false);
        // Fully routed, every MSEG/curve/wave set, but all amounts zero: identical bytes.
        // (LFO waves only differ through depth, which is zero; FX types need a mix.)
        check(identical(reference, renderPolicy(policy, true, 0.0f, true)),
              "configured modern layer with zero amounts leaves the render byte-identical");
    }
    // PURE CS-80: the fully configured, fully enabled layer equals PURE defaults.
    check(identical(renderPolicy(2, false, 0.0f, false), renderPolicy(2, true, 1.0f, true)),
          "PURE CS-80 render is byte-identical with the modern layer fully configured");
    // The same configuration is audible outside PURE.
    for (int policy = 0; policy < 2; ++policy)
        check(! identical(renderPolicy(policy, false, 0.0f, false), renderPolicy(policy, true, 1.0f, true)),
              "configured modern layer is audible in Legacy and Source Performance");
}

void testEngineConsumption()
{
    // velocity -> pitch, mod wheel -> LP cutoff on rank II: read the rank telemetry against
    // an unmodulated render of the same note.
    auto render = [](bool routedNote, double sr)
    {
        ParamValues p;
        p[P_mod1Amount] = 0.5f;
        p[P_mod2Amount] = -0.4f;
        Engine e;
        e.prepare(sr);
        if (routedNote)
            e.setModernConfig(routed({ { ModSource::velocity, ModSource::none, ModDestination::pitch, ModTarget::both },
                                       { ModSource::modWheel, ModSource::none, ModDestination::lpCutoff, ModTarget::rankII } }));
        e.setParams(p);
        e.controlChange(1, 0.75f);
        e.noteOn(60, 0.8f);
        std::vector<float> block(2 * 64);
        e.render(block.data(), 64);
        return std::pair { e.getRankInfo(0, 0), e.getRankInfo(0, 1) };
    };
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto plain = render(false, sr), modulated = render(true, sr);
        const double pitchRatio = modulated.first.oscillatorHz / plain.first.oscillatorHz;
        check(std::abs(12.0 * std::log2(pitchRatio) - 0.5 * 0.8 * kPitchSemitones) < 1e-3,
              "pitch destination reaches the oscillator frequency");
        check(std::abs(std::log2(modulated.second.lpfHz / plain.second.lpfHz) + 0.4 * 0.75 * kCutoffOctaves) < 1e-3
                  && std::abs(modulated.first.lpfHz - plain.first.lpfHz) < 1e-3f,
              "LP cutoff destination reaches rank II's filter only");
    }

    // A synced mono MSEG follows the host tempo through the engine; fallback is 120 BPM.
    for (double bpm : { 90.0, 0.0 })
    {
        ParamValues p;
        p[P_mod1Amount] = 1.0f;
        ModernConfig c = routed({ { ModSource::mseg1, ModSource::none, ModDestination::pitch, ModTarget::both } });
        MsegShape ramp;
        ramp.count = 2;
        ramp.points[0] = { 0.0f, 0.0f, 0.0f };
        ramp.points[1] = { 1.0f, 1.0f, 0.0f }; // one beat
        ramp.sync = true;
        ramp.trigger = MsegTrigger::mono;
        c.msegs[0] = ramp;
        Engine e;
        e.prepare(48000.0);
        e.setModernConfig(c);
        e.setParams(p);
        e.setTransport({ bpm, 0.0, false, false });
        e.noteOn(60, 0.8f);
        const double beat = 60.0 / effectiveBpm(bpm);
        std::vector<float> block(2);
        long reached = -1;
        for (long n = 1; n < (long) (3.0 * 48000.0) && reached < 0; ++n)
        {
            e.render(block.data(), 1);
            if (e.getModernMsegValue(-1, 0) >= 1.0f) reached = n;
        }
        // Two internal samples per host sample; the mono trigger acts one internal sample late.
        check(std::abs((double) reached - beat * 48000.0) <= 2.0, "synced MSEG follows host tempo through the engine");
    }

    // Mono MSEG restarts only when a key goes down with none held; free never restarts.
    {
        ParamValues p;
        p[P_mod1Amount] = p[P_mod2Amount] = 1.0f;
        ModernConfig c = routed({ { ModSource::mseg1, ModSource::none, ModDestination::pitch, ModTarget::both },
                                  { ModSource::mseg2, ModSource::none, ModDestination::pan, ModTarget::both } });
        c.msegs[0] = defaultShape(2); // 2 s one-shot ramp
        c.msegs[0].trigger = MsegTrigger::mono;
        c.msegs[1] = defaultShape(2);
        c.msegs[1].trigger = MsegTrigger::free;
        Engine e;
        e.prepare(48000.0);
        e.setModernConfig(c);
        e.setParams(p);
        std::vector<float> block(2 * 4800);
        e.noteOn(60, 0.8f);
        e.render(block.data(), 4800);              // 0.1 s
        e.noteOn(64, 0.8f);                        // legato: no restart
        e.render(block.data(), 4800);
        const float legato = e.getModernMsegValue(-1, 0);
        const float freeBefore = e.getModernMsegValue(-1, 1);
        check(std::abs(legato - 0.1f) < 2e-3f, "mono MSEG does not restart on a legato note");
        e.noteOff(60);
        e.noteOff(64);
        e.render(block.data(), 4800);
        e.noteOn(67, 0.8f);                        // first key after all released: restart
        e.render(block.data(), 4800);
        check(e.getModernMsegValue(-1, 0) < 0.06f, "mono MSEG restarts when a key goes down with none held");
        check(e.getModernMsegValue(-1, 1) > freeBefore + 0.09f, "free MSEG never restarts");
    }
}

void testStability()
{
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int policy = 0; policy < 2; ++policy)
        {
            ParamValues p;
            p[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
            for (int s = 0; s < kModSlots; ++s)
                p[P_mod1Amount + s] = s % 2 ? -1.0f : 1.0f;
            Engine e;
            e.prepare(sr);
            e.setModernConfig(fullyConfigured());
            e.setParams(p);
            for (int note = 36; note < 84; note += 6)
                e.noteOn(note, 1.0f, 1 + note % 3);
            e.controlChange(1, 1.0f);
            std::vector<float> block(2 * 512);
            bool finite = true;
            float peak = 0.0f;
            for (int b = 0; b < (int) (2.0 * sr / 512); ++b)
            {
                e.render(block.data(), 512);
                for (float x : block) { finite = finite && std::isfinite(x); peak = std::max(peak, std::abs(x)); }
            }
            check(finite && peak < 8.0f, "fully modulated engine stays finite and bounded at every rate");
        }
}

} // namespace

int main()
{
    testRoutingMath();
    testCurvesAndNoteValues();
    testInactiveIsIdentity();
    testDefaultInvarianceAndPurity();
    testEngineConsumption();
    testStability();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
