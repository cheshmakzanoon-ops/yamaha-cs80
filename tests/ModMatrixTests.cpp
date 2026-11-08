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
#include <limits>
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

void testIdleCircuitsDoNotDriveGlobalSources()
{
    for (bool idleFirst : { false, true })
    {
        ModernLayer layer;
        layer.prepare(96000.0);
        layer.setConfig(routed({
            { ModSource::velocity, ModSource::none, ModDestination::fx1Mix, ModTarget::both },
            { ModSource::constant, ModSource::polyPressure, ModDestination::fx2Mix, ModTarget::both },
            { ModSource::ampEnvelope, ModSource::none, ModDestination::fx3Mix, ModTarget::both },
            { ModSource::velocity, ModSource::none, ModDestination::pitch, ModTarget::both }
        }));
        layer.setControls(amounts({ 1.0f, 1.0f, 1.0f, 1.0f }));
        auto older = voiceAt(60, 0.2f, 5);
        auto newer = voiceAt(67, 0.9f, 7);
        auto cold = voiceAt(60, 0.0f, 0);
        cold.gate = false;
        newer.pressure = 0.75f;
        newer.ampEnvelope[0] = 0.8f;
        auto sample = [&](bool newerEligible, bool olderEligible)
        {
            layer.beginSample(globals());
            if (idleFirst)
                layer.processVoice(1, newer, newerEligible);
            layer.processVoice(0, older, olderEligible);
            if (! idleFirst)
                layer.processVoice(1, newer, newerEligible);
            layer.processVoice(2, cold, false);
        };
        sample(true, true);
        sample(true, true);
        check(near(layer.getGlobalOffsets().fxMix[0], 0.9, 1.0),
              "global source initially follows the newest active note in either traversal order");
        newer.gate = false;
        newer.pressure = 0.65f;
        newer.ampEnvelope[0] = 0.4f;
        sample(true, true);
        sample(true, true);
        const auto release = layer.getGlobalOffsets().fxMix;
        check(near(release[0], 0.9, 1.0) && near(release[1], 0.65, 1.0) && near(release[2], 0.4, 1.0),
              "a releasing active voice retains priority and updates source/via/envelope values");
        sample(false, true);
        sample(false, true);
        const auto held = layer.getGlobalOffsets().fxMix;
        check(near(held[0], 0.2, 1.0) && near(held[1], 0.4, 1.0) && near(held[2], 0.6, 1.0),
              "older sounding note resumes priority after the newer circuit becomes idle");
        check(near(layer.rankOffsets(1, 0).pitch, 0.9 * kPitchSemitones, kPitchSemitones),
              "excluding an idle circuit from global sources preserves its rank modulation");
        older.gate = false;
        sample(false, false);
        layer.beginSample(globals()); // No circuits processed on this sample.
        sample(false, false);
        check(layer.getGlobalOffsets().fxMix == held,
              "idle and empty samples retain the last global source when no active voices remain");
        newer = voiceAt(72, 0.7f, 8);
        sample(true, false);
        sample(true, false);
        check(near(layer.getGlobalOffsets().fxMix[0], 0.7, 1.0),
              "a new sounding note replaces the held global snapshot after silence");
    }
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

// Removing the last global destination must also remove its cached MSEG-rate
// offset. The layer stays active through the MSEG -> pitch route in every case.
void testRemovedGlobalRateIsIdentity()
{
    constexpr double sampleRate = 48000.0;
    constexpr int samples = 64;
    for (int m = 0; m < kMsegCount; ++m)
        for (auto trigger : { MsegTrigger::mono, MsegTrigger::free })
            for (float amount : { -0.5f, 0.5f })
                for (int removal = 0; removal < 3; ++removal)
                {
                    auto config = routed({
                        { (ModSource) ((int) ModSource::mseg1 + m), ModSource::none,
                          ModDestination::pitch, ModTarget::both },
                        { ModSource::constant, ModSource::none,
                          (ModDestination) ((int) ModDestination::mseg1Rate + m), ModTarget::both }
                    });
                    auto& shape = config.msegs[(std::size_t) m];
                    shape = defaultShape(2);
                    shape.trigger = trigger;
                    auto controls = amounts({ 0.5f, amount });
                    controls.msegRate[(std::size_t) m] = 1.25f;
                    ModernLayer layer;
                    layer.prepare(sampleRate);
                    layer.setConfig(config);
                    layer.setControls(controls);
                    auto advance = [&](int count)
                    {
                        for (int n = 0; n < count; ++n)
                        {
                            layer.beginSample(globals());
                            layer.processVoice(0, voiceAt(60, 0.8f));
                        }
                    };
                    advance(4); // Start the mono runner and establish the rate offset.
                    const double beforeModulated = layer.displayPosition(m);
                    advance(samples);
                    const double baseAdvance = samples * 1.25 / sampleRate;
                    check(std::abs(layer.displayPosition(m) - beforeModulated
                                   - baseAdvance * std::exp2((double) amount * kMsegRateOctaves)) < 1e-10,
                          "global MSEG-rate route changes the shared envelope speed");

                    if (removal == 0)
                    {
                        config.slots[1] = {};
                        layer.setConfig(config);
                    }
                    else if (removal == 1)
                    {
                        controls.amount[1] = 0.0f;
                        layer.setControls(controls);
                    }
                    else
                    {
                        config.slots[1].destination = ModDestination::pan;
                        layer.setConfig(config);
                    }
                    const double beforeRemoved = layer.displayPosition(m);
                    advance(samples);
                    check(layer.isActive(), "removing a global rate route leaves the rank route active");
                    check(std::abs(layer.displayPosition(m) - beforeRemoved - baseAdvance) < 1e-10,
                          "deleting, zeroing or rerouting the last global rate route restores the base speed");

                    // An unrelated rank-route edit must not clear a live rate offset.
                    config.slots[1].destination = (ModDestination) ((int) ModDestination::mseg1Rate + m);
                    config.slots[1].source = ModSource::constant;
                    controls.amount[1] = amount;
                    layer.setConfig(config);
                    layer.setControls(controls);
                    advance(2);
                    config.slots[0].target = ModTarget::rankII;
                    layer.setConfig(config);
                    const double beforeRankEdit = layer.displayPosition(m);
                    advance(samples);
                    check(std::abs(layer.displayPosition(m) - beforeRankEdit
                                   - baseAdvance * std::exp2((double) amount * kMsegRateOctaves)) < 1e-10,
                          "editing a rank route preserves a still-live shared MSEG-rate route");
                }
}

// A host may report a present but unusable PPQ. Treat it like an absent
// position: preserve the local clock, or start normally if it has not run yet.
// Check the scaled position too: a finite PPQ can overflow at a valid rate.
void testUnusableTransportPosition()
{
    constexpr double sr = 96000.0;
    const double invalid[] = { std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::max() };
    for (int m = 0; m < kMsegCount; ++m)
        for (float rate : { 0.125f, 1.0f, 4.0f, 8.0f })
            for (auto loop : { MsegLoop::off, MsegLoop::sustain, MsegLoop::always })
                for (bool warm : { false, true })
                    for (double ppq : invalid)
                    {
                        if (std::isfinite(ppq * (double) rate))
                            continue; // Large but usable positions remain valid seeks.
                        const auto source = (ModSource) ((int) ModSource::mseg1 + m);
                        auto config = routed({
                            { source, ModSource::none, ModDestination::pitch, ModTarget::both },
                            { source, ModSource::none, ModDestination::fx1Mix, ModTarget::both }
                        });
                        auto& shape = config.msegs[(std::size_t) m];
                        shape = defaultShape(1);
                        shape.trigger = MsegTrigger::free;
                        shape.sync = true;
                        shape.loop = loop;
                        auto controls = amounts({ 0.5f, 0.25f });
                        controls.msegRate[(std::size_t) m] = rate;
                        ModernLayer tested, missing;
                        for (auto* layer : { &tested, &missing })
                        {
                            layer->prepare(sr);
                            layer->setConfig(config);
                            layer->setControls(controls);
                        }
                        const auto voice = voiceAt(60, 0.8f);
                        auto advance = [&](int count)
                        {
                            bool same = true;
                            for (int n = 0; n < count; ++n)
                            {
                                for (auto* layer : { &tested, &missing })
                                {
                                    layer->beginSample(globals());
                                    layer->processVoice(0, voice);
                                }
                                same = same && tested.displayPosition(m) == missing.displayPosition(m)
                                    && tested.msegValue(-1, m) == missing.msegValue(-1, m)
                                    && tested.rankOffsets(0, 0).pitch == missing.rankOffsets(0, 0).pitch
                                    && tested.getGlobalOffsets().fxMix[0] == missing.getGlobalOffsets().fxMix[0];
                            }
                            return same;
                        };
                        if (warm)
                        {
                            for (auto* layer : { &tested, &missing })
                                layer->setTransport({ 90.0, 0.03125, true, true });
                            check(advance(17), "valid transport warm-up is identical");
                        }
                        tested.setTransport({ 90.0, ppq, true, true });
                        missing.setTransport({ 90.0, 0.0, false, true });
                        check(advance(13), "unusable PPQ preserves the clock exactly like absent position");
                        for (auto* layer : { &tested, &missing })
                            layer->setTransport({ 90.0, 0.0, false, true });
                        check(advance(19), "unusable PPQ cannot poison subsequent free-running modulation");
                        for (auto* layer : { &tested, &missing })
                            layer->setTransport({ 90.0, 0.375, true, true });
                        check(advance(1), "valid song position resumes transport lock after unusable PPQ");
                        check(std::isfinite(tested.displayPosition(m)) && std::isfinite(tested.msegValue(-1, m)),
                              "recovered free MSEG position and value are finite");
                    }
}

// Keep ordinary seeks, negative pre-roll, tempo fallback, and all non-locked
// trigger modes unchanged. Include enormous finite, non-overflowing positions.
void testFiniteTransportPosition()
{
    constexpr double sr = 96000.0;
    for (auto trigger : { MsegTrigger::poly, MsegTrigger::mono, MsegTrigger::free })
        for (bool sync : { false, true })
            for (bool playing : { false, true })
                for (bool hasPosition : { false, true })
                    for (double ppq : { -4.0, 0.0, 0.125, 7.375, 1.0e12,
                                        std::numeric_limits<double>::max() / 32.0 })
                        for (float rate : { 0.125f, 1.0f, 8.0f })
                            for (double bpm : { 90.0, 0.0 })
                            {
                                auto config = routed({ { ModSource::mseg1, ModSource::none,
                                                         ModDestination::pitch, ModTarget::both } });
                                auto& shape = config.msegs[0];
                                shape = defaultShape(1); // One-unit loop.
                                shape.trigger = trigger;
                                shape.sync = sync;
                                auto controls = amounts({ 0.5f });
                                controls.msegRate[0] = rate;
                                ModernLayer layer;
                                layer.prepare(sr);
                                layer.setConfig(config);
                                layer.setControls(controls);
                                layer.setTransport({ bpm, ppq, hasPosition, playing });
                                const auto voice = voiceAt(60, 0.8f);
                                bool matches = true;
                                const bool locked = trigger == MsegTrigger::free && sync && playing && hasPosition;
                                const double start = locked ? std::fmod(std::max(0.0, ppq * rate), 1.0) : 0.0;
                                const double step = rate * (sync ? effectiveBpm(bpm) / 60.0 : 1.0) / sr;
                                for (int n = 1; n <= 5; ++n)
                                {
                                    layer.beginSample(globals());
                                    layer.processVoice(0, voice);
                                    const int ticks = trigger == MsegTrigger::mono ? n - 1 : n;
                                    const double expectedPosition = std::fmod(start + ticks * step, 1.0);
                                    const double expected = cs80::reference::msegReferenceStatic(
                                        shape, expectedPosition, 0, shape.points[0].level);
                                    matches = matches && std::abs(layer.msegValue(0, 0) - expected)
                                        <= tol::kMsegLevelAbsTolerance;
                                }
                                check(matches, "finite transport preserves existing lock and trigger semantics");
                            }
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

// The matrix taps the physical wheel position, not a semitone displacement
// divided by the current bend range. Range automation is not a wheel event.
void testPitchWheelSourceIndependentOfRange()
{
    const auto config = routed({
        { ModSource::pitchBend, ModSource::none, ModDestination::subDepth, ModTarget::both },
        { ModSource::pitchBend, ModSource::none, ModDestination::pan, ModTarget::rankII },
    });
    const auto renderFrames = [](Engine& e, int frames)
    {
        std::vector<float> block(2 * (std::size_t) frames);
        e.render(block.data(), frames);
    };
    const auto setup = [&](Engine& e, ParamValues& p, int policy, float range)
    {
        p[P_sourcePerformance] = policy != 0 ? 1.0f : 0.0f;
        p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
        p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = 0.0f;
        p[P_pitchBendRange] = range;
        p[P_mod1Amount] = p[P_mod2Amount] = 0.5f;
        e.prepare(kRate);
        e.setModernConfig(config);
        e.setParams(p);
        check(e.getParams()[P_pitchBendRange] == range, "wheel-source fixture uses a legal bend range");
        e.noteOn(60, 0.8f);
    };
    for (int policy = 0; policy < 3; ++policy)
        for (float range : { 1.0f, 2.0f, 12.0f, 48.0f })
            for (float wheel : { -0.75f, 0.5f })
            {
                Engine e;
                ParamValues p;
                setup(e, p, policy, range);
                e.pitchBend(1, wheel);
                renderFrames(e, 64);
                const double expected = policy == 2 ? 0.0 : 0.5 * wheel;
                check(near(e.getModernGlobalOffsets().subDepthGain, 1.0 + expected, 1.0),
                      "wheel position drives global matrix routes independently of bend range");
                check(near(e.getModernRankOffsets(0, 1).pan, expected, 1.0),
                      "wheel position drives rank matrix routes independently of bend range");
                for (float changed : { 12.0f, 1.0f, 48.0f, 2.0f })
                {
                    p[P_pitchBendRange] = changed;
                    e.setParams(p);
                    renderFrames(e, 64);
                    check(near(e.getModernGlobalOffsets().subDepthGain, 1.0 + expected, 1.0),
                          "changing bend range while the wheel is held does not change the matrix source");
                }
                e.reset();
                e.noteOn(60, 0.8f);
                renderFrames(e, 64);
                check(e.getModernGlobalOffsets().subDepthGain == 1.0f
                          && e.getModernRankOffsets(0, 1).pan == 0.0f,
                      "reset clears the independent pitch-wheel matrix source");
            }

    // Source policy keeps its 5 ms control smoothing independently of pitch
    // depth. A range change during the gesture must not restart that glide.
    for (float range : { 1.0f, 2.0f, 12.0f, 48.0f })
    {
        Engine e;
        ParamValues p;
        setup(e, p, 1, range);
        renderFrames(e, 64);
        e.pitchBend(1, 0.8f);
        renderFrames(e, 120);
        p[P_pitchBendRange] = range == 1.0f ? 12.0f : 1.0f;
        e.setParams(p);
        renderFrames(e, 120);
        check(std::abs(e.getModernGlobalOffsets().subDepthGain
                       - (1.0 + 0.4 * (1.0 - std::exp(-1.0)))) < 1.0e-6,
              "pitch-wheel matrix gesture retains the 5 ms source-policy response across a range edit");
    }
}

// The channel-pressure source is shared event memory, but its value must take
// the same Circuit controller dezipper as a held voice's pressure. Otherwise
// selecting Channel pressure rather than Poly pressure restores abrupt jumps.
void testChannelPressureSourceSmoothing()
{
    const auto config = routed({
        { ModSource::channelPressure, ModSource::none, ModDestination::fx1Mix, ModTarget::both },
        { ModSource::channelPressure, ModSource::none, ModDestination::pan, ModTarget::rankII },
        { ModSource::polyPressure, ModSource::none, ModDestination::pan, ModTarget::rankI },
        { ModSource::constant, ModSource::channelPressure, ModDestination::fx2Mix, ModTarget::both },
    });
    const auto renderFrames = [](Engine& e, int frames)
    {
        std::vector<float> block(2 * (std::size_t) frames);
        e.render(block.data(), frames);
    };
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (float quality : { 0.0f, 1.0f, 2.0f })
            for (int policy = 0; policy < 3; ++policy)
            {
                ParamValues p;
                p[P_sourcePerformance] = policy != 0 ? 1.0f : 0.0f;
                p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
                p[P_oversampling] = quality;
                p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = 0.0f;
                p[P_mod1Amount] = p[P_mod2Amount] = p[P_mod3Amount] = p[P_mod4Amount] = 0.5f;
                Engine e;
                e.prepare(rate);
                e.setParams(p);
                e.setModernConfig(config);
                e.noteOn(60, 0.8f);
                renderFrames(e, 64); // End first-sample snap before the gesture.
                const auto checkRoutes = [&](double expected)
                {
                    const double offset = policy == 2 ? 0.0 : 0.5 * expected;
                    check(near(e.getModernGlobalOffsets().fxMix[0], offset, 1.0)
                              && near(e.getModernGlobalOffsets().fxMix[1], offset, 1.0),
                          "channel-pressure source and via follow the controller response globally");
                    check(near(e.getModernRankOffsets(0, 1).pan, offset, 1.0),
                          "channel-pressure source follows the controller response at rank destinations");
                    check(near(e.getModernRankOffsets(0, 0).pan,
                               e.getModernRankOffsets(0, 1).pan, 1.0),
                          "held voice and shared channel-pressure source have the same response");
                };
                e.channelPressure(1, 0.8f);
                int elapsed = 0;
                double response = 0.0;
                for (int frames : { 1, (int) (rate * 0.0025), (int) (rate * 0.0025) })
                {
                    renderFrames(e, frames);
                    elapsed += frames;
                    response = policy == 0 ? 0.8f : (double) 0.8f
                        * (1.0 - std::exp(-(double) elapsed / (rate * kControlSmoothingSeconds)));
                    checkRoutes(response);
                    // Repeated MIDI events at the same value do not restart
                    // the glide or replace the target with the running value.
                    e.channelPressure(1, 0.8f);
                }
                e.channelPressure(1, 0.2f);
                const int returnFrames = (int) (rate * kControlSmoothingSeconds);
                renderFrames(e, returnFrames);
                const double returned = policy == 0 ? 0.2f : (double) 0.2f
                    + (response - (double) 0.2f)
                        * std::exp(-(double) returnFrames / (rate * kControlSmoothingSeconds));
                checkRoutes(returned);
                e.reset();
                e.noteOn(60, 0.8f);
                renderFrames(e, 1);
                checkRoutes(0.0);
                e.reset();
                e.noteOn(60, 0.8f);
                e.channelPressure(1, 0.6f);
                renderFrames(e, 1);
                checkRoutes(0.6f); // First sample snaps, just like other controllers.
            }
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

void testEngineSameSampleMonoRetrigger()
{
    // A last key-up and following key-down share a host MIDI position. The
    // accepted onset, not a rendered silent gap, distinguishes it from legato.
    for (int policy : {0, 1})
        for (double sampleRate : {48000.0, 96000.0})
            for (int quality : {0, 2})
                for (int scenario = 0; scenario < 16; ++scenario)
                {
                    Engine e;
                    ParamValues p;
                    p[P_sourcePerformance] = static_cast<float>(policy);
                    p[P_parityMode] = 0.0f;
                    p[P_kasScanTiming] = scenario == 15 ? 1.0f : 0.0f;
                    p[P_oversampling] = static_cast<float>(quality);
                    p[P_mod1Amount] = p[P_mod2Amount] = p[P_mod3Amount] = 0.1f;
                    p[P_sustainMode] = scenario == 7 ? 1.0f : 0.0f;
                    p[P_sustainAssign] = p[P_footswitchPlugged] = p[P_footswitchPressed] = 1.0f;
                    p[P_voiceMode] = scenario == 8 ? 3.0f : 0.0f; // Four-voice unison.
                    ModernConfig config;
                    for (int m = 0; m < 3; ++m)
                    {
                        auto& shape = config.msegs[(size_t) m];
                        shape = MsegShape {};
                        shape.count = 2;
                        shape.points[0] = {0.0f, 0.0f, 0.0f};
                        shape.points[1] = {0.1f, 1.0f, 0.0f};
                        shape.trigger = m == 0 ? MsegTrigger::mono
                            : m == 1 ? MsegTrigger::poly : MsegTrigger::free;
                        config.slots[(size_t) m] = {static_cast<ModSource>(static_cast<int>(ModSource::mseg1) + m), ModSource::none,
                                                   ModDestination::pan, ModTarget::both};
                    }
                    e.prepare(sampleRate);
                    e.setParams(p);
                    e.setModernConfig(config);
                    if (scenario == 10)
                    {
                        tuning::TuningTable mapping;
                        mapping.mapped[64] = false;
                        e.setScalaTuning(mapping);
                        p[P_tuningMode] = 1.0f;
                        e.setParams(p);
                    }
                    std::vector<float> audio(2 * 480);
                    e.noteOn(60, 0.8f, 1);
                    if (scenario == 9)
                        for (int note = 61; note < 68; ++note) e.noteOn(note, 0.8f, 1);
                    e.render(audio.data(), 480);
                    const float beforeMono = e.getModernMsegValue(-1, 0);
                    const float beforeFree = e.getModernMsegValue(-1, 2);
                    if (scenario == 5 || scenario == 6) e.controlChange(64, 1.0f, 1);
                    if (scenario != 1 && scenario != 2 && scenario != 9)
                        e.noteOff(60, 1);
                    if (scenario == 4) e.render(audio.data(), 1); // Existing gap control.
                    if (scenario == 6) e.controlChange(64, 0.0f, 1);
                    int nextNote = scenario == 3 ? 60 : 64;
                    if (scenario == 7)
                    {
                        // The first accepted phrase voice disappears under Sustain II;
                        // its surviving legato members must still carry the restart.
                        e.noteOn(62, 0.8f, 1);
                        e.noteOn(64, 0.8f, 1);
                        e.noteOff(62, 1);
                        e.noteOn(65, 0.8f, 1);
                        nextNote = 65;
                        bool originalAlive = false;
                        for (int v = 0; v < 8; ++v)
                            originalAlive |= e.getVoiceInfo(v).active && e.getVoiceInfo(v).note == 62;
                        check(!originalAlive, "Sustain II removes the first voice of the unrendered phrase");
                    }
                    else if (scenario == 11)
                    {
                        e.noteOn(62, 0.8f, 1); e.noteOff(62, 1); // Earlier phrase survives as a tail.
                        e.noteOn(65, 0.8f, 2); e.allNotesOff(false, 2); // Later phrase is canceled.
                    }
                    else
                    {
                        e.noteOn(nextNote, 0.8f, 1);
                        if (scenario == 12) e.allNotesOff(false); // Canceled before any voice observes it.
                        if (scenario == 13)
                        {
                            e.reset();
                            nextNote = 67;
                            e.noteOn(nextNote, 0.8f, 1);
                        }
                        if (scenario == 14)
                        {
                            p[P_mod1Amount] = p[P_mod2Amount] = p[P_mod3Amount] = 0.0f;
                            e.setParams(p);
                            e.noteOff(nextNote, 1);
                            nextNote = 67;
                            e.noteOn(nextNote, 0.8f, 1);
                            p[P_mod1Amount] = p[P_mod2Amount] = p[P_mod3Amount] = 0.1f;
                            e.setParams(p);
                        }
                    }
                    if (scenario == 2) e.noteOff(60, 1); // Reverse order stays legato.
                    const int frames = scenario == 11 || scenario == 12 ? 1
                        : scenario == 15 ? static_cast<int>(sampleRate * 0.001) + 8 : 8;
                    e.render(audio.data(), frames);
                    const bool restart = scenario == 0 || scenario == 3 || scenario == 4
                        || scenario == 6 || scenario == 7 || scenario == 8 || scenario == 11
                        || scenario == 13 || scenario == 15;
                    const double duration = static_cast<double>(config.msegs[0].points[1].time);
                    const double factor = e.getOversamplingFactor();
                    const double scanDelay = scenario == 15 && policy == 1 ? sampleRate * factor * 0.001 - 1.0 : 0.0;
                    const double expectedMono = (frames * factor - scanDelay - 1.0) / (sampleRate * factor * duration);
                    if (restart)
                        check(std::abs(e.getModernMsegValue(-1, 0) - expectedMono) < 1.0e-7,
                              "non-legato mono restart keeps its one-internal-sample scheduling delay");
                    else
                        check(e.getModernMsegValue(-1, 0) > beforeMono,
                              "legato, pedal-held, refused and unmapped onsets do not restart mono");
                    check(scenario == 13 ? e.getModernMsegValue(-1, 2) > 0.0f && e.getModernMsegValue(-1, 2) < 0.005f
                                          : e.getModernMsegValue(-1, 2) > beforeFree,
                          "free MSEG continues across events and restarts only at explicit engine reset");
                    if (scenario != 9 && scenario != 10 && scenario != 11 && scenario != 12)
                    {
                        int newest = -1;
                        for (int v = 0; v < 8; ++v)
                            if (e.getVoiceInfo(v).held && e.getVoiceInfo(v).note == nextNote) newest = v;
                        check(newest >= 0 && std::abs(e.getModernMsegValue(newest, 1)
                                  - (frames * factor - scanDelay) / (sampleRate * factor * duration)) < 1.0e-7,
                              "poly MSEG retains its independent onset timing");
                    }
                }
}

void testEngineRateRouteRemoval()
{
    constexpr int frames = 480;
    constexpr double duration = (double) frames / kRate;
    for (int policy = 0; policy < 2; ++policy)
        for (auto trigger : { MsegTrigger::mono, MsegTrigger::free })
            for (bool removeRouting : { false, true })
            {
                ParamValues p;
                p[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
                p[P_mod1Amount] = 0.5f;
                p[P_mod2Amount] = 1.0f;
                auto config = routed({
                    { ModSource::mseg1, ModSource::none, ModDestination::pitch, ModTarget::both },
                    { ModSource::constant, ModSource::none, ModDestination::mseg1Rate, ModTarget::both }
                });
                config.msegs[0] = defaultShape(2); // Two-second linear ramp.
                config.msegs[0].trigger = trigger;
                Engine engine;
                engine.prepare(kRate);
                engine.setModernConfig(config);
                engine.setParams(p);
                engine.noteOn(60, 0.8f);
                std::vector<float> block(2 * frames);
                engine.render(block.data(), frames);
                const float modulatedStart = engine.getModernMsegValue(-1, 0);
                engine.render(block.data(), frames);
                check(std::abs(engine.getModernMsegValue(-1, 0) - modulatedStart - 4.0 * duration)
                          < tol::kMsegLevelAbsTolerance,
                      "engine consumes the routed shared MSEG's eight-times rate");
                if (removeRouting)
                {
                    config.slots[1] = {};
                    engine.setModernConfig(config);
                }
                else
                {
                    p[P_mod2Amount] = 0.0f;
                    engine.setParams(p);
                }
                const float removedStart = engine.getModernMsegValue(-1, 0);
                engine.render(block.data(), frames);
                check(std::abs(engine.getModernMsegValue(-1, 0) - removedStart - 0.5 * duration)
                          < tol::kMsegLevelAbsTolerance,
                      "engine removes the shared MSEG-rate offset after routing or amount edits");
            }
}

void testEngineSyncedRateBlockPartitions()
{
    // The host PPQ interval is measured in host frames, while the layer is
    // advanced at the selected internal oversampling rate. Compare audio and
    // telemetry with a runner receiving only its initial transport position.
    for (double sr : { 44100.0, 96000.0 })
        for (int quality : { 0, 1, 2 })
            for (int policy : { 0, 1 })
                for (int blockSize : { 1, 17, 128 })
                {
                    ParamValues p;
                    p[P_oversampling] = static_cast<float>(quality);
                    p[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
                    p[P_mod1Amount] = 0.5f;
                    p[P_mod2Amount] = 0.5f;
                    auto config = routed({
                        { ModSource::mseg1, ModSource::none, ModDestination::pitch, ModTarget::both },
                        { ModSource::constant, ModSource::none, ModDestination::mseg1Rate, ModTarget::both }
                    });
                    config.msegs[0] = defaultShape(2);
                    config.msegs[0].trigger = MsegTrigger::free;
                    config.msegs[0].sync = true;
                    Engine continuous, partitioned;
                    for (auto* engine : { &continuous, &partitioned })
                    {
                        engine->prepare(sr);
                        engine->setParams(p);
                        engine->setModernConfig(config);
                        engine->setTransport({ 132.0, 0.0, true, true });
                        engine->noteOn(60, 0.8f);
                    }
                    bool phasesMatch = true, audioMatches = true;
                    double peak = 0.0, maxAudioError = 0.0, maxPhaseError = 0.0;
                    constexpr int frames = 1024;
                    for (int frame = 0; frame < frames;)
                    {
                        const int count = std::min(blockSize, frames - frame);
                        partitioned.setTransport({ 132.0, frame * 132.0 / (60.0 * sr), true, true });
                        std::vector<float> a(2 * (std::size_t) count), b(a.size());
                        continuous.render(a.data(), count);
                        partitioned.render(b.data(), count);
                        maxPhaseError = std::max(maxPhaseError, (double) std::abs(continuous.getModernMsegValue(-1, 0)
                            - partitioned.getModernMsegValue(-1, 0)));
                        phasesMatch = phasesMatch && maxPhaseError < 1.0e-7;
                        for (std::size_t i = 0; i < a.size(); ++i)
                        {
                            peak = std::max(peak, std::abs((double) a[i]));
                            maxAudioError = std::max(maxAudioError, (double) std::abs(a[i] - b[i]));
                            audioMatches = audioMatches && std::isfinite(a[i]) && std::isfinite(b[i])
                                && std::abs(a[i] - b[i]) < 1.0e-6f;
                        }
                        frame += count;
                    }
                    if (!phasesMatch || !audioMatches || peak <= 1.0e-8)
                        std::fprintf(stderr, "MSEG partition sr=%.0f quality=%d policy=%d block=%d phaseError=%.9g audioError=%.9g peak=%.9g\n",
                                     sr, quality, policy, blockSize, maxPhaseError, maxAudioError, peak);
                    check(phasesMatch, "engine synced rate modulation preserves phase across host blocks and internal rates");
                    check(audioMatches && peak > 1.0e-8,
                          "engine synced rate modulation produces matching non-silent finite audio across host blocks");
                }
}

void testLiveTriggerModeChanges()
{
    // A mode edit is an envelope restart, not a new note. Exercise every
    // transition and every MSEG without changing either voice's onset age.
    for (auto from : { MsegTrigger::poly, MsegTrigger::mono, MsegTrigger::free })
        for (auto to : { MsegTrigger::poly, MsegTrigger::mono, MsegTrigger::free })
            for (int m = 0; m < kMsegCount; ++m)
            {
                if (from == to) continue;
                auto config = routed({
                    { ModSource::mseg1, ModSource::none, ModDestination::pitch, ModTarget::both },
                    { ModSource::mseg2, ModSource::none, ModDestination::cutoff, ModTarget::both },
                    { ModSource::mseg3, ModSource::none, ModDestination::pan, ModTarget::both },
                    { ModSource::noteRandom, ModSource::none, ModDestination::level, ModTarget::both }
                });
                for (auto& shape : config.msegs)
                {
                    shape = defaultShape(2); // Two-second linear ramp.
                    shape.trigger = from;
                }
                ModernLayer layer;
                layer.prepare(1000.0);
                layer.setConfig(config);
                layer.setControls(amounts({ 0.25f, 0.25f, 0.25f, 0.25f }));
                auto tick = [&](int count)
                {
                    for (int n = 0; n < count; ++n)
                    {
                        layer.beginSample({});
                        layer.processVoice(0, voiceAt(60, 0.8f, 1));
                        layer.processVoice(1, voiceAt(67, 0.3f, 2));
                    }
                };
                tick(100);
                const int other = (m + 1) % kMsegCount;
                const float beforeOther = layer.msegValue(0, other);
                const float noteRandom = layer.rankOffsets(0, 0).level;
                config.msegs[(std::size_t) m].trigger = to;
                layer.setConfig(config);
                tick(100);
                for (int v = 0; v < 2; ++v)
                    check(near(layer.msegValue(v, m), 0.05, 1.0),
                          "live MSEG trigger-mode edit restarts each held note or shared runner");
                check(near(layer.msegValue(0, other) - beforeOther, 0.05, 1.0),
                      "live trigger-mode edit leaves the other envelopes running");
                check(layer.rankOffsets(0, 0).level == noteRandom,
                      "live trigger-mode edit preserves the note's latched random value");
                layer.setConfig(config); // Identical publication must not restart again.
                tick(100);
                check(near(layer.msegValue(0, m), 0.1, 1.0),
                      "repeated unchanged MSEG configuration does not retrigger");
            }

    // A mode edit with no key held is not a new onset. Released notes and
    // decimated idle circuits must not acquire a fresh poly/mono envelope.
    for (auto to : { MsegTrigger::poly, MsegTrigger::mono })
        for (bool eligible : { false, true })
        {
            auto config = routed({ { ModSource::mseg1, ModSource::none,
                                     ModDestination::pitch, ModTarget::both } });
            config.msegs[0] = defaultShape(2);
            config.msegs[0].trigger = MsegTrigger::free;
            ModernLayer layer;
            layer.prepare(1000.0);
            layer.setConfig(config);
            layer.setControls(amounts({ 1.0f }));
            auto voice = voiceAt(60, 0.8f);
            for (int n = 0; n < 20; ++n)
            {
                layer.beginSample({});
                layer.processVoice(0, voice);
            }
            voice.gate = false;
            layer.beginSample({});
            layer.processVoice(0, voice, eligible);
            config.msegs[0].trigger = to;
            layer.setConfig(config);
            for (int n = 0; n < 20; ++n)
            {
                layer.beginSample({});
                layer.processVoice(0, voice, eligible);
            }
            check(layer.msegValue(0, 0) == 0.0f,
                  "mode edits do not restart envelopes for released notes or idle circuits");
            voice.gate = true;
            ++voice.age;
            for (int n = 0; n < 20; ++n)
            {
                layer.beginSample({});
                layer.processVoice(0, voice);
            }
            check(near(layer.msegValue(0, 0), to == MsegTrigger::mono ? 0.0095 : 0.01, 1.0),
                  "next note retains normal poly and mono onset timing after an idle mode edit");
        }

    // The Engine must apply the live edit to both held voices without another
    // MIDI onset, under both render policies and every supported host rate.
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int policy = 0; policy < 2; ++policy)
            for (int m = 0; m < kMsegCount; ++m)
                for (auto from : { MsegTrigger::poly, MsegTrigger::mono, MsegTrigger::free })
                    for (auto trigger : { MsegTrigger::poly, MsegTrigger::mono, MsegTrigger::free })
                    {
                        if (from == trigger) continue;
                        ParamValues p;
                        p[P_sourcePerformance] = (float) policy;
                        p[P_mod1Amount] = 0.25f;
                        auto config = routed({ { (ModSource) ((int) ModSource::mseg1 + m), ModSource::none,
                                                ModDestination::pitch, ModTarget::both } });
                        auto& shape = config.msegs[(std::size_t) m];
                        shape = defaultShape(2);
                        shape.trigger = from;
                        Engine e;
                        e.prepare(sr);
                        e.setParams(p);
                        e.setModernConfig(config);
                        e.noteOn(60, 0.8f);
                        e.noteOn(67, 0.4f);
                        const int frames = (int) (sr * 0.01);
                        std::vector<float> block(2 * (std::size_t) frames);
                        e.render(block.data(), frames);
                        shape.trigger = trigger;
                        e.setModernConfig(config);
                        e.render(block.data(), frames);
                        bool finite = true;
                        for (float sample : block) finite = finite && std::isfinite(sample);
                        check(finite, "live trigger-mode edit renders finite engine audio");
                        const double expected = frames / (2.0 * sr);
                        if (trigger == MsegTrigger::poly)
                        {
                            int held = 0;
                            for (int v = 0; v < 8; ++v)
                                if (e.getVoiceInfo(v).held)
                                {
                                    ++held;
                                    check(near(e.getModernMsegValue(v, m), expected, 1.0),
                                          "Engine restarts each held poly MSEG after a mode edit");
                                }
                            check(held == 2, "live mode-edit regression keeps both keys held");
                        }
                        else
                            check(near(e.getModernMsegValue(-1, m), expected, 1.0),
                                  "Engine restarts the shared MSEG after a mode edit");
                    }
}

void testEngineUnusableTransportPosition()
{
    const double invalid[] = { std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::max() };
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int policy = 0; policy < 3; ++policy)
            for (bool warm : { false, true })
            {
                ParamValues p;
                p[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
                p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
                p[P_mod1Amount] = p[P_mod2Amount] = p[P_mod3Amount] = p[P_mod4Amount] = 0.25f;
                p[P_mseg1Rate] = p[P_mseg2Rate] = p[P_mseg3Rate] = 2.0f; // Four times base speed.
                auto config = routed({
                    { ModSource::mseg1, ModSource::none, ModDestination::pitch, ModTarget::both },
                    { ModSource::mseg2, ModSource::none, ModDestination::cutoff, ModTarget::both },
                    { ModSource::mseg3, ModSource::none, ModDestination::pan, ModTarget::both },
                    { ModSource::mseg1, ModSource::none, ModDestination::subDepth, ModTarget::both }
                });
                for (auto& shape : config.msegs)
                {
                    shape = defaultShape(1);
                    shape.trigger = MsegTrigger::free;
                    shape.sync = true;
                }
                Engine tested, missing;
                for (auto* engine : { &tested, &missing })
                {
                    engine->prepare(sr);
                    engine->setParams(p);
                    engine->setModernConfig(config);
                    engine->noteOn(60, 0.8f);
                }
                auto renderPair = [&](int frames)
                {
                    std::vector<float> a(2 * (std::size_t) frames), b(a.size());
                    tested.render(a.data(), frames);
                    missing.render(b.data(), frames);
                    bool finite = true;
                    for (float value : a) finite = finite && std::isfinite(value);
                    return finite && identical(a, b);
                };
                if (warm)
                {
                    for (auto* engine : { &tested, &missing })
                        engine->setTransport({ 90.0, 0.03125, true, true });
                    check(renderPair(32), "engine valid transport warm-up is byte-identical");
                }
                for (double ppq : invalid)
                {
                    tested.setTransport({ 90.0, ppq, true, true });
                    missing.setTransport({ 90.0, 0.0, false, true });
                    check(renderPair(17), "engine ignores unusable PPQ with finite byte-identical audio");
                    for (auto* engine : { &tested, &missing })
                        engine->setTransport({ 90.0, 0.0, false, true });
                    check(renderPair(31), "engine stays finite and byte-identical without a later PPQ");
                    for (auto* engine : { &tested, &missing })
                        engine->setTransport({ 90.0, 0.375, true, true });
                    check(renderPair(29), "engine reacquires valid transport with byte-identical audio");
                }
            }
}

void testEngineGlobalSourcePriorityAfterRelease()
{
    // Circuit voices continue on staggered, decimated idle clocks after their
    // notes end. They must not periodically replace a still-sounding voice as
    // the source of global modulation. Legacy already skips those idle voices.
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int policy = 0; policy < 2; ++policy)
        {
            ParamValues p;
            p[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
            p[P_masterSustain] = p[P_keyRelease] = p[P_sustainMode] = p[P_sustainAssign] = 0.0f;
            p[P_mod1Amount] = 1.0f;
            for (int rank = 0; rank < 2; ++rank)
            {
                p[chParam(rank, CP_vA)] = p[chParam(rank, CP_vD)] = p[chParam(rank, CP_vR)] = 0.001f;
                p[chParam(rank, CP_vS)] = 0.7f;
            }
            Engine engine;
            engine.prepare(sr);
            engine.setParams(p);
            engine.setModernConfig(routed({
                { ModSource::velocity, ModSource::none, ModDestination::fx1Mix, ModTarget::both }
            }));
            std::vector<float> block(1024);
            engine.noteOn(60, 0.2f);
            engine.render(block.data(), 37); // Offset the two idle schedules.
            engine.noteOn(67, 0.9f);
            engine.render(block.data(), 512);
            check(near(engine.getModernGlobalOffsets().fxMix[0], 0.9, 1.0),
                  "newest sounding note drives the global source");
            engine.noteOff(67);
            for (int n = 0; n < (int) (sr * 0.1); n += 512)
                engine.render(block.data(), 512);
            check(engine.activeVoices() == 1,
                  "priority regression keeps the older note sounding after the newer note ends");
            bool steady = true;
            for (int n = 0; n < 1024; ++n)
            {
                engine.render(block.data(), 1);
                steady = steady && near(engine.getModernGlobalOffsets().fxMix[0], 0.2, 1.0);
            }
            check(steady, "idle circuits cannot interrupt the newest sounding global source");
        }
}

// Mixed global/rank routes must preserve slot order and refresh on both routing
// and amount edits. Compare MSEG evolution with an isolated rate/readout layer.
void testMixedRouteLifecycle()
{
    auto config = routed({
        { ModSource::constant, ModSource::none, ModDestination::pitch, ModTarget::both },
        { ModSource::constant, ModSource::none, ModDestination::fx1Mix, ModTarget::both },
        { ModSource::constant, ModSource::none, ModDestination::pitch, ModTarget::both },
        { ModSource::constant, ModSource::none, ModDestination::fx1Mix, ModTarget::both },
        { ModSource::constant, ModSource::none, ModDestination::pitch, ModTarget::both },
        { ModSource::constant, ModSource::none, ModDestination::fx1Mix, ModTarget::both },
        { ModSource::constant, ModSource::none, ModDestination::mseg1Rate, ModTarget::both },
        { ModSource::mseg1, ModSource::none, ModDestination::level, ModTarget::rankII }
    });
    config.msegs[0] = defaultShape(2);
    auto isolatedConfig = config;
    for (int s = 0; s < 6; ++s) isolatedConfig.slots[(std::size_t) s] = {};
    // Keep a global destination live in the reference too: removing its last
    // global route resets the previous-sample rate immediately by design.
    isolatedConfig.slots[8] = { ModSource::constant, ModSource::none,
                                ModDestination::fx3Mix, ModTarget::both };
    auto controls = amounts({ 0.7f, 0.7f, 1.e-8f, 1.e-8f, -0.7f, -0.7f, 0.5f, 1.0f, 0.1f });
    ModernLayer mixed, isolated;
    mixed.prepare(1000.0);
    isolated.prepare(1000.0);
    mixed.setConfig(config);
    isolated.setConfig(isolatedConfig);
    mixed.setControls(controls);
    isolated.setControls(controls);
    for (auto trigger : { MsegTrigger::poly, MsegTrigger::mono, MsegTrigger::free,
                          MsegTrigger::poly, MsegTrigger::free, MsegTrigger::mono })
    {
        config.msegs[0].trigger = isolatedConfig.msegs[0].trigger = trigger;
        mixed.setConfig(config);
        isolated.setConfig(isolatedConfig);
        bool observedEnvelope = false;
        for (int n = 0; n < 40; ++n)
        {
            if (n == 12 || n == 24)
            {
                controls.amount[6] = n == 12 ? 0.0f : 0.5f;
                mixed.setControls(controls);
                isolated.setControls(controls);
            }
            mixed.beginSample(globals());
            isolated.beginSample(globals());
            for (int v = 0; v < 2; ++v)
            {
                const auto input = voiceAt(60 + v * 7, 0.8f, 1 + v);
                mixed.processVoice(v, input);
                isolated.processVoice(v, input);
                observedEnvelope = observedEnvelope || mixed.msegValue(v, 0) > 0.0f;
                check(mixed.msegValue(v, 0) == isolated.msegValue(v, 0),
                      "rate route migration preserves exact envelope state");
                float expectedPitch = 0.0f, expectedFx = 0.0f;
                for (int s : { 0, 2, 4 }) expectedPitch += controls.amount[(std::size_t) s] * kPitchSemitones;
                for (int s : { 1, 3, 5 }) expectedFx += controls.amount[(std::size_t) s];
                check(mixed.rankOffsets(v, 0).pitch == expectedPitch
                      && mixed.rankOffsets(v, 1).pitch == expectedPitch
                      && mixed.getGlobalOffsets().fxMix[0] == expectedFx,
                      "interleaved global and rank routes retain exact slot accumulation order");
                check(mixed.rankOffsets(v, 0).level == 0.0f
                      && mixed.rankOffsets(v, 1).level == isolated.rankOffsets(v, 1).level,
                      "mixed rank route retains its target");
            }
        }
        check(observedEnvelope, "route migration reference exercises a moving envelope");
    }
    // Establish a nonzero global offset so the cleanup assertion is nonvacuous.
    controls.amount[1] = 0.5f;
    mixed.setControls(controls);
    mixed.beginSample(globals());
    check(mixed.getGlobalOffsets().fxMix[0] != 0.0f, "global cleanup starts with a live nonzero offset");
    // Remove globals and the rate route while leaving a rank route active.
    for (int s : { 1, 3, 5, 6 }) config.slots[(std::size_t) s] = {};
    mixed.setConfig(config);
    mixed.beginSample(globals());
    mixed.processVoice(0, voiceAt(60, 0.8f));
    check(mixed.isActive() && mixed.getGlobalOffsets().fxMix[0] == 0.0f,
          "removing global routes clears old offsets while rank routes remain active");
    controls.amount[0] = 0.5f;
    mixed.setControls(controls);
    mixed.beginSample(globals());
    mixed.processVoice(0, voiceAt(60, 0.8f));
    check(mixed.rankOffsets(0, 0).pitch != 0.0f,
          "disable cleanup starts with a nonzero rank offset");
    controls.enabled = false;
    mixed.setControls(controls);
    check(! mixed.isActive() && mixed.rankOffsets(0, 0).pitch == 0.0f
          && mixed.rankOffsets(0, 1).level == 0.0f,
          "disabled routes restore identity immediately");
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
    testMixedRouteLifecycle();
    testCurvesAndNoteValues();
    testIdleCircuitsDoNotDriveGlobalSources();
    testInactiveIsIdentity();
    testRemovedGlobalRateIsIdentity();
    testUnusableTransportPosition();
    testFiniteTransportPosition();
    testDefaultInvarianceAndPurity();
    testEngineConsumption();
    testPitchWheelSourceIndependentOfRange();
    testChannelPressureSourceSmoothing();
    testEngineSameSampleMonoRetrigger();
    testEngineRateRouteRemoval();
    testEngineSyncedRateBlockPartitions();
    testLiveTriggerModeChanges();
    testEngineUnusableTransportPosition();
    testEngineGlobalSourcePriorityAfterRelease();
    testStability();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
