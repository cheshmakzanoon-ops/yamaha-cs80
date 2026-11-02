// Registered current-sample modern modulation contract. Software integration,
// not a Yamaha law; the original phrase click gate remains independent.
#include "core/Engine.h"
#include "Ws1Tolerances.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>

namespace cs80 {
// Existing render-thread test friendship; no production observer or layout change.
struct EngineEventContinuityProbe
{
    struct Sample { float rankL, rankR, outputL, outputR; };
    static Sample observed(Engine& e)
    {
        const auto context = e.tickGlobals(0.0f);
        float l = 0.0f, r = 0.0f;
        e.tickRanks(context, l, r);
        Sample sample { l, r, 0.0f, 0.0f };
        e.mixBus(context, l, r);
        e.tickOutput(context, l, r, sample.outputL, sample.outputR);
        if (context.smoothControls) e.endSmoothedControls();
        return sample;
    }
    static std::array<float, 2> unobserved(Engine& e)
    {
        std::array<float, 2> out {};
        e.processInternalSample(out[0], out[1], 0.0f);
        return out;
    }
    static bool active(const Engine& e, int voice) { return e.voices[voice].active; }
    static int untilDeadline(const Engine& e, int voice)
    {
        const auto& v = e.voices[voice];
        const int due = v.idleSteps == 0 ? e.idleStepSamples - voice * e.idleStepSamples / 8 : e.idleStepSamples;
        return due - v.idleClock;
    }
};
}

using namespace cs80;
using namespace cs80::modern;
namespace {
using Probe = EngineEventContinuityProbe;
int checks = 0, failures = 0;
void check(bool okay, const char* message)
{
    ++checks;
    if (!okay) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
bool same(float a, float b) { return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b); }
ParamValues parameters(int quality = 1, bool pure = false, int rank = 0)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f; p[P_parityMode] = pure ? 1.0f : 0.0f;
    p[P_oversampling] = static_cast<float>(quality); p[P_mix] = static_cast<float>(rank);
    p[P_keyRelease] = p[P_masterSustain] = p[P_drift] = p[P_voiceDispersion] = 0.0f;
    p[P_analogNoise] = p[P_chorusOn] = p[P_arpOn] = 0.0f;
    p[P_mod1Amount] = 0.6f; p[P_mseg1Rate] = 1.0f; // exact 2 shape-seconds/second
    for (int ch = 0; ch < 2; ++ch)
    {
        p[chParam(ch, CP_saw)] = p[chParam(ch, CP_pulse)] = 0.0f;
        p[chParam(ch, CP_sine)] = p[chParam(ch, CP_noise)] = 0.0f;
        p[chParam(ch, CP_vcfLevel)] = 0.0f;
        p[chParam(ch, CP_pan)] = 0.0f;
        p[chParam(ch, CP_vLevel)] = 2.6f;
        p[chParam(ch, CP_initLevel)] = p[chParam(ch, CP_afterLevel)] = 0.0f;
        p[chParam(ch, CP_vA)] = 0.002f; p[chParam(ch, CP_vD)] = 0.015f;
        p[chParam(ch, CP_vS)] = 0.7f; p[chParam(ch, CP_vR)] = 0.005f;
    }
    return p;
}
ModernConfig config(bool pan, bool poly, ModSource global = ModSource::none)
{
    ModernConfig c;
    c.msegs[0].count = 2; c.msegs[0].loop = MsegLoop::off;
    c.msegs[0].sync = c.msegs[0].bipolar = false;
    c.msegs[0].trigger = poly ? MsegTrigger::poly : MsegTrigger::free;
    c.msegs[0].points[0] = { 0.0f, 0.2f, 0.0f };
    c.msegs[0].points[1] = { 4.0f, 0.8f, 0.0f };
    c.slots[0] = { pan ? ModSource::mseg1 : ModSource::none, ModSource::none,
                   pan ? ModDestination::pan : ModDestination::none, ModTarget::both };
    if (global != ModSource::none)
        c.slots[1] = { global, ModSource::none, ModDestination::subSpeed, ModTarget::both };
    return c;
}
void prepare(Engine& e, const ParamValues& p, const ModernConfig& c, int rate)
{
    e.setParams(p); e.setModernConfig(c); e.prepare(rate);
}
void renderTwins(Engine& a, Engine& b, int frames, bool* identical = nullptr)
{
    std::array<float, 128> x {}, y {};
    while (frames > 0)
    {
        const int n = std::min(frames, 64);
        a.render(x.data(), n); b.render(y.data(), n);
        if (identical)
            for (int i = 0; i < n * 2; ++i) *identical = *identical && same(x[i], y[i]);
        frames -= n;
    }
}

void currentSample(const ws1::Tolerances& limits, int rate, int quality, bool pure, bool poly, int rank)
{
    auto p = parameters(quality, pure, rank); const auto c = config(true, poly);
    Engine observed, untouched;
    prepare(observed, p, c, rate); prepare(untouched, p, c, rate);
    observed.noteOn(60, 0.9f); untouched.noteOn(60, 0.9f);
    bool prefix = true; renderTwins(observed, untouched, rate / 4, &prefix);
    observed.noteOff(60); untouched.noteOff(60);
    check(prefix, "observer twin has identical host PCM before release");
    bool observerExact = true, mixExact = true;
    double maxPanError = 0.0, maxTimeError = 0.0, phaseAtRetirement = 0.0;
    float firstPan = 0.0f, lastPan = 0.0f;
    int tailSamples = 0, totalSamples = 0;
    const double internal = observed.getInternalSampleRate();
    while (tailSamples < 2048 && totalSamples < static_cast<int>(internal))
    {
        const bool idle = !Probe::active(observed, 0);
        if (idle && tailSamples == 0)
        {
            check(std::abs(observed.getRankVcaState(0, rank).coupling) > limits["minimum_retired_coupling_abs"],
                  "current-pan fixture has a genuinely charged retired rank");
            phaseAtRetirement = observed.getModernMsegDisplayPosition(0);
        }
        const auto actual = Probe::observed(observed);
        const auto control = Probe::unobserved(untouched);
        observerExact = observerExact && same(actual.outputL, control[0]) && same(actual.outputR, control[1]);
        ++totalSamples;
        if (!idle) continue;
        ++tailSamples;
        const float source = observed.getModernMsegValue(poly ? 0 : -1, 0);
        const float pan = pure ? 0.0f : p[P_mod1Amount] * source;
        if (tailSamples == 1) firstPan = pan;
        lastPan = pan;
        maxPanError = std::max(maxPanError,
            std::abs(static_cast<double>(observed.getModernRankOffsets(0, rank).pan) - pan));
        float left, right; constantPowerPan(pan, left, right);
        const float closed = static_cast<float>(-observed.getRankVcaState(0, rank).coupling);
        const float voice = closed * 1.0f * 0.17f;
        mixExact = mixExact && same(actual.rankL, voice * left) && same(actual.rankR, voice * right);
        if (poly)
        {
            const double elapsed = pure ? observed.getModernMsegDisplayPosition(0) - phaseAtRetirement
                : (observed.getModernMsegDisplayPosition(0) - phaseAtRetirement) / 2.0;
            const double expected = pure ? 0.0 : tailSamples / internal;
            maxTimeError = std::max(maxTimeError, std::abs(elapsed - expected));
        }
    }
    check(tailSamples == 2048, "current-pan fixture covers 2048 consecutive retired internal samples");
    check(observerExact, "rank-mix observer preserves unobserved internal output bits");
    check(maxPanError <= limits["current_pan_max_abs"], "retired rank pan is from the current modulation sample");
    check(mixExact, "retired rank mix consumes current pan before downstream processing");
    if (poly) check(maxTimeError <= limits["poly_elapsed_seconds_max_abs"], "tail poly MSEG advances once per actual internal sample");
    check(pure ? !observed.isModernLayerActive() : std::abs(lastPan - firstPan) > limits["minimum_current_pan_change"],
          "current-pan control is changing in Source and inert in Pure");
    std::printf("current-pan policy=%s rate=%d quality=%d rank=%d mode=%s error=%.12g elapsed_error=%.12g\n",
                pure ? "pure" : "source", rate, quality, rank, poly ? "poly" : "free", maxPanError, maxTimeError);
}

void ownership(const ws1::Tolerances& limits, ModSource source, bool late)
{
    auto p = parameters(); p[P_mod2Amount] = 0.25f;
    Engine actual, reference;
    prepare(actual, p, late ? ModernConfig {} : config(true, false, source), 48000);
    prepare(reference, p, late ? ModernConfig {} : config(false, false, source), 48000);
    actual.noteOn(48, 0.2f); reference.noteOn(48, 0.2f);
    renderTwins(actual, reference, 64);
    actual.noteOn(60, 0.8f); reference.noteOn(60, 0.8f);
    renderTwins(actual, reference, 12000);
    actual.noteOff(60); reference.noteOff(60);
    int released = 0;
    while (Probe::active(actual, 1) && released++ < 48000) renderTwins(actual, reference, 1);
    check(!Probe::active(actual, 1) && Probe::active(actual, 0)
          && std::abs(actual.getRankVcaState(1, 0).coupling) > limits["minimum_retired_coupling_abs"],
          "ownership witness has an older held voice and newer charged retired voice");
    if (late)
    {
        actual.setModernConfig(config(true, false, source));
        reference.setModernConfig(config(false, false, source));
        check(actual.getModernRankOffsets(1, 0).pan == 0.0f,
              "late enable starts before the retired note identity was latched");
        check(Probe::untilDeadline(actual, 1) > 3,
              "later note precedes the retired voice's original identity deadline");
    }
    bool exact = true; int changes = 0;
    float previous = reference.getModernGlobalOffsets().subSpeedRatio;
    for (int sample = 0; sample < 2048; ++sample)
    {
        if (late && sample == 2)
        {
            // Before voice1's original idle deadline, another note must get
            // the next RNG draw. Extra pan evaluations cannot steal that draw.
            actual.noteOn(67, 0.5f); reference.noteOn(67, 0.5f);
        }
        Probe::unobserved(actual); Probe::unobserved(reference);
        const float expected = reference.getModernGlobalOffsets().subSpeedRatio;
        exact = exact && same(actual.getModernGlobalOffsets().subSpeedRatio, expected);
        changes += !same(previous, expected); previous = expected;
    }
    check(changes > 0, "ownership reference varies and does not pass vacuously");
    check(exact, late ? "late pan enable preserves note identity RNG and global selection timing"
                      : "tail pan updates preserve scheduled newest-voice global selection");
}

void neutralPolicies(const ws1::Tolerances& limits)
{
    for (bool pure : {false, true})
    {
        auto p = parameters(1, pure); if (!pure) p[P_mod1Amount] = 0.0f;
        Engine configured, empty;
        prepare(configured, p, config(true, true), 48000); prepare(empty, p, ModernConfig {}, 48000);
        configured.noteOn(60, 0.8f); empty.noteOn(60, 0.8f);
        bool exact = true; renderTwins(configured, empty, 12000, &exact);
        configured.noteOff(60); empty.noteOff(60); renderTwins(configured, empty, 4800, &exact);
        check(exact, pure ? "Pure nonzero matrix remains bit-identical to empty matrix"
                          : "zero-amount Source matrix remains bit-identical to neutral default");
    }
    auto p = parameters(); Engine legacy;
    prepare(legacy, p, config(true, false), 48000);
    legacy.noteOn(60, 0.8f); std::array<float, 128> audio {};
    for (int i = 0; i < 200; ++i) legacy.render(audio.data(), 64);
    legacy.noteOff(60);
    for (int i = 0; Probe::active(legacy, 0) && i < 48000; ++i) legacy.render(audio.data(), 1);
    p[P_sourcePerformance] = 0.0f; legacy.setParams(p);
    check(std::abs(legacy.getRankVcaState(0, 0).coupling) > limits["minimum_retired_coupling_abs"],
          "Legacy policy control retains an otherwise unused prior Source capacitor state");
    bool inactivePan = true;
    for (int sample = 0; sample < 2048; ++sample)
    {
        Probe::unobserved(legacy);
        inactivePan = inactivePan && std::abs(legacy.getModernRankOffsets(0, 0).pan)
            <= limits["legacy_inactive_pan_max_abs"];
    }
    check(inactivePan, "Legacy inactive path does not evaluate source coupling-tail pan");
}

void prepareRetired(Engine& e, const ParamValues& p, const ModernConfig& c, bool expression = false)
{
    prepare(e, p, c, 48000); e.noteOn(60, 0.9f);
    if (expression) { e.polyPressure(60, 0.75f); e.controlChange(74, 0.75f); }
    std::array<float, 128> audio {};
    for (int i = 0; i < 200; ++i) e.render(audio.data(), 64);
    e.noteOff(60);
    int samples = 0;
    while (Probe::active(e, 0) && samples++ < e.getInternalSampleRate()) Probe::unobserved(e);
    check(!Probe::active(e, 0), "boundary fixture retires its source voice");
}

bool expectedMix(const Probe::Sample& sample, const Engine& e, int rank, float pan)
{
    float left, right; constantPowerPan(pan, left, right);
    const float closed = static_cast<float>(-e.getRankVcaState(0, rank).coupling);
    const float voice = closed * 1.0f * 0.17f;
    return same(sample.rankL, voice * left) && same(sample.rankR, voice * right);
}

void removePan(const ws1::Tolerances& limits)
{
    for (int rank : {0, 1})
        for (int edit = 0; edit < 3; ++edit)
        {
            auto p = parameters(1, false, rank); p[P_mod2Amount] = 0.25f;
            auto c = config(true, false, ModSource::velocity);
            Engine e; prepareRetired(e, p, c);
            check(std::abs(e.getModernRankOffsets(0, rank).pan) > limits["minimum_current_pan_change"]
                  && std::abs(e.getRankVcaState(0, rank).coupling) > limits["minimum_retired_coupling_abs"]
                  && Probe::untilDeadline(e, 0) > 3,
                  "pan-removal fixture starts with cached pan and charge before its idle deadline");
            if (edit == 0) { p[P_mod1Amount] = 0.0f; e.setParams(p); }
            else
            {
                if (edit == 1) c.slots[0] = {};
                else c.slots[0].destination = ModDestination::pitch;
                e.setModernConfig(c);
            }
            check(e.isModernLayerActive(), "another live route keeps the matrix active after pan removal");
            bool current = true, mix = true;
            for (int sample = 0; sample < 128; ++sample)
            {
                const auto actual = Probe::observed(e);
                current = current && e.getModernRankOffsets(0, rank).pan == 0.0f;
                mix = mix && expectedMix(actual, e, rank, 0.0f);
            }
            check(current, "removing zeroing or rerouting pan clears its cache at the next tail sample");
            check(mix, "removed pan cannot reach the next charged-tail mix");
        }
}

void releasedExpression(const ws1::Tolerances& limits)
{
    for (ModSource source : {ModSource::polyPressure, ModSource::timbre})
    {
        const auto p = parameters(); auto c = config(true, false); c.slots[0].source = source;
        Engine e; prepareRetired(e, p, c, true);
        const float before = e.getModernRankOffsets(0, 0).pan;
        const int deadline = Probe::untilDeadline(e, 0);
        check(std::abs(before) > limits["minimum_current_pan_change"] && deadline > 3
              && std::abs(e.getRankVcaState(0, 0).coupling) > limits["minimum_retired_coupling_abs"],
              "expression fixture retains nonzero controller pan and charge before its original deadline");
        bool exactPan = true, exactMix = true;
        for (int sample = 0; sample < deadline * 2 + 5; ++sample)
        {
            const float expected = sample + 1 < deadline ? before : 0.0f;
            const auto actual = Probe::observed(e);
            exactPan = exactPan && same(e.getModernRankOffsets(0, 0).pan, expected);
            exactMix = exactMix && expectedMix(actual, e, 0, expected);
        }
        check(exactPan, "released expression stays cleared from its original idle deadline onward");
        check(exactMix, "tail mix consumes released-controller clearing on the original deadline");
    }
}

void noPanCadence(const ws1::Tolerances& limits)
{
    const auto p = parameters(); auto c = config(true, true);
    c.slots[0].destination = ModDestination::pitch;
    Engine e; prepareRetired(e, p, c);
    check(e.isModernLayerActive() && e.getModernRankOffsets(0, 0).pan == 0.0f,
          "no-pan cadence control has an active poly MSEG routed only to pitch");
    const double position = e.getModernMsegDisplayPosition(0), rate = e.getInternalSampleRate();
    int scheduled = 0; double error = 0.0;
    for (int sample = 0; sample < 2048; ++sample)
    {
        scheduled += Probe::untilDeadline(e, 0) == 1;
        Probe::unobserved(e);
        error = std::max(error, std::abs((e.getModernMsegDisplayPosition(0) - position) / 2.0
                                       - static_cast<double>(scheduled) / rate));
    }
    check(scheduled > 1 && scheduled < 2048, "no-pan fixture spans several original decimated deadlines");
    check(error <= limits["poly_elapsed_seconds_max_abs"],
          "active modern routes without present or cached pan keep their original idle cadence");
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances limits(argc > 1 ? argv[1]
            : "tests/parity/tolerances-v2/vca-tail-modulation.json", "limits");
        for (const char* name : {"current_pan_max_abs", "current_rank_mix_bit_mismatches_max",
             "observer_internal_output_bit_mismatches_max", "global_selection_bit_mismatches_max",
             "neutral_policy_pcm_bit_mismatches_max", "legacy_inactive_pan_max_abs"})
            check(limits[name] == 0.0, "registered exact tail-modulation limits remain zero");
        check(limits["poly_elapsed_seconds_max_abs"] == 1e-12, "registered poly elapsed-time bound remains unchanged");
        check(limits["minimum_retired_coupling_abs"] == 1e-6 && limits["minimum_current_pan_change"] == 1e-5,
              "registered tail-modulation non-vacuity floors remain unchanged");
        for (bool pure : {false, true})
            for (int rate : {44100, 48000, 96000})
                for (int quality : {0, 1, 2})
                    for (bool poly : {false, true})
                        for (int rank : {0, 1}) currentSample(limits, rate, quality, pure, poly, rank);
        for (ModSource source : {ModSource::velocity, ModSource::keyFollow, ModSource::noteRandom})
            for (bool late : {false, true}) ownership(limits, source, late);
        neutralPolicies(limits);
        removePan(limits);
        releasedExpression(limits);
        noPanCadence(limits);
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
