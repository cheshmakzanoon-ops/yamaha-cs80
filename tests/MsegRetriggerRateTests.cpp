// A new poly envelope must not inherit the previous note's delayed rate route.
// JUCE-free modern-layer regressions, not an analog CS-80 fidelity claim.
#include "core/Engine.h"
#include "core/ModernLayer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

using namespace cs80;
using namespace cs80::modern;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok)
    {
        if (failures < 30) std::fprintf(stderr, "FAIL: %s\n", label.c_str());
        ++failures;
    }
}
ModSource source(int m) { return static_cast<ModSource>(static_cast<int>(ModSource::mseg1) + m); }
ModDestination destination(int m) { return static_cast<ModDestination>(static_cast<int>(ModDestination::mseg1Rate) + m); }
ModernConfig configuration(int m, MsegLoop loop = MsegLoop::off, bool sync = false, bool via = false)
{
    ModernConfig c;
    auto& shape = c.msegs[static_cast<std::size_t>(m)];
    shape = {};
    shape.count = 4;
    shape.points[0] = {0.0f, 0.0f, 0.0f};
    shape.points[1] = {0.002f, 1.0f, 0.0f};
    shape.points[2] = {0.004f, 0.4f, 0.5f};
    shape.points[3] = {0.006f, 0.0f, -0.5f};
    shape.loop = loop; shape.loopStart = 1; shape.loopEnd = 2;
    shape.sync = sync;
    c.slots[0] = {source(m), ModSource::none, ModDestination::pitch, ModTarget::both};
    for (int i = 1; i <= 3; ++i)
        c.slots[static_cast<std::size_t>(i)] = {
            via ? ModSource::constant : ModSource::velocity,
            via ? ModSource::velocity : ModSource::none, destination(m), ModTarget::both};
    return c;
}
ModernControls controls(float amount = 1.0f, float baseRate = 1.0f)
{
    ModernControls c; c.enabled = true; c.amount[0] = 1.0f;
    for (int i = 1; i <= 3; ++i) c.amount[static_cast<std::size_t>(i)] = amount;
    c.msegRate.fill(baseRate);
    return c;
}
VoiceInputs voice(std::uint64_t age, float velocity, bool gate = true)
{
    VoiceInputs v; v.age = age; v.velocity = velocity; v.gate = gate; return v;
}
void sample(ModernLayer& layer, const VoiceInputs& v, int index = 0)
{
    layer.beginSample({}); layer.processVoice(index, v);
}
void setup(ModernLayer& layer, double fs, const ModernConfig& c, const ModernControls& p)
{
    layer.prepare(fs); layer.setConfig(c); layer.setControls(p);
    layer.setTransport({90.0, 0.0, false, false});
}

void testShortEnvelope()
{
    auto c = configuration(0);
    c.msegs[0].count = 2;
    ModernLayer reused, fresh;
    setup(reused, 96000.0, c, controls()); setup(fresh, 96000.0, c, controls());
    sample(reused, voice(1, 1.0f));
    sample(reused, voice(2, 1.0f / 127.0f));
    sample(fresh, voice(2, 1.0f / 127.0f));
    check(reused.msegValue(0, 0) == fresh.msegValue(0, 0), "soft retrigger equals a fresh voice on its first sample");
    check(reused.msegValue(0, 0) > 0.0f && reused.msegValue(0, 0) < 0.01f,
          "2 ms one-shot is not completed by the previous hard note's 256x rate");
    check(reused.displayPosition(0) == 1.0 / 96000.0, "first sample advances at the base rate");
}

void testReusedVoices()
{
    for (double fs : {16000.0, 88200.0, 96000.0, 192000.0, 384000.0, 1536000.0})
    for (int m = 0; m < kMsegCount; ++m)
    for (auto loop : {MsegLoop::off, MsegLoop::sustain, MsegLoop::always})
    for (bool sync : {false, true})
    for (bool via : {false, true})
    for (float amount : {-1.0f, 1.0f})
    for (float baseRate : {0.125f, 1.0f, 8.0f})
    {
        auto c = configuration(m, loop, sync, via);
        auto p = controls(amount, baseRate);
        ModernLayer reused, fresh;
        setup(reused, fs, c, p); setup(fresh, fs, c, p);
        const int index = (m + static_cast<int>(loop)) % ModernLayer::kVoices;
        for (int n = 0; n < 12; ++n) sample(reused, voice(1, 1.0f, n < 9), index);
        bool exact = true;
        for (int n = 0; n < 64; ++n)
        {
            const auto v = voice(2, 1.0f / 127.0f, n < 31);
            sample(reused, v, index); sample(fresh, v, index);
            exact &= reused.msegValue(index, m) == fresh.msegValue(index, m)
                && reused.displayPosition(m) == fresh.displayPosition(m)
                && reused.rankOffsets(index, 0).pitch == fresh.rankOffsets(index, 0).pitch;
        }
        check(exact, "reused/fresh trajectory is exact: fs=" + std::to_string(fs)
            + " MSEG=" + std::to_string(m) + " loop=" + std::to_string(static_cast<int>(loop))
            + " sync=" + std::to_string(sync) + " via=" + std::to_string(via)
            + " amount=" + std::to_string(amount) + " base=" + std::to_string(baseRate));
    }
}

void testHeldRateAndSiblings()
{
    for (int m = 0; m < kMsegCount; ++m)
    {
        auto c = configuration(m);
        c.msegs[static_cast<std::size_t>(m)] = defaultShape(2);
        ModernLayer changed, reference;
        const auto p = controls(0.25f);
        setup(changed, 96000.0, c, p); setup(reference, 96000.0, c, p);
        MsegRunner oracle; oracle.trigger(c.msegs[static_cast<std::size_t>(m)]);
        bool exact = true;
        for (int n = 0; n < 64; ++n)
        {
            for (auto* layer : {&changed, &reference})
            {
                layer->beginSample({});
                layer->processVoice(0, voice(layer == &changed && n >= 10 ? 3 : 1, 0.75f));
                layer->processVoice(1, voice(2, 0.5f));
            }
            const double rate = n == 0 ? 1.0 : std::exp2(3.0 * 0.25 * 0.5 * kMsegRateOctaves);
            const float expected = oracle.tick(c.msegs[static_cast<std::size_t>(m)], rate / 96000.0, true);
            exact &= changed.msegValue(1, m) == expected && reference.msegValue(1, m) == expected;
        }
        check(exact, "held rate retains its one-sample delay and another voice's restart cannot clear it");
    }
}

void testRouteEditsAndExplicitRestarts()
{
    for (int m = 0; m < kMsegCount; ++m)
    for (int edit = 0; edit < 3; ++edit)
    {
        auto c = configuration(m); auto p = controls();
        ModernLayer reused, fresh;
        setup(reused, 96000.0, c, p);
        sample(reused, voice(1, 1.0f));
        if (edit == 0)
        {
            for (int s = 1; s <= 3; ++s) p.amount[static_cast<std::size_t>(s)] = 0.0f;
            reused.setControls(p);
        }
        else if (edit == 1)
        {
            for (int s = 1; s <= 3; ++s) c.slots[static_cast<std::size_t>(s)] = {};
            reused.setConfig(c);
        }
        else
        {
            c.msegs[static_cast<std::size_t>(m)].trigger = MsegTrigger::mono;
            reused.setConfig(c);
            c.msegs[static_cast<std::size_t>(m)].trigger = MsegTrigger::poly;
            reused.setConfig(c); // same voice age: only the pending restart can act
        }
        setup(fresh, 96000.0, c, p);
        const auto v = voice(edit == 2 ? 1 : 2, 1.0f);
        bool exact = true;
        for (int n = 0; n < 4; ++n)
        {
            sample(reused, v); sample(fresh, v);
            exact &= reused.msegValue(0, m) == fresh.msegValue(0, m)
                && reused.displayPosition(m) == fresh.displayPosition(m);
        }
        check(exact, "same-boundary route edit or explicit poly restart discards old rate history");
    }
}

void testSharedRunners()
{
    for (auto trigger : {MsegTrigger::mono, MsegTrigger::free})
    {
        auto c = configuration(0); c.msegs[0] = defaultShape(2); c.msegs[0].trigger = trigger;
        for (int s = 1; s <= 3; ++s) c.slots[static_cast<std::size_t>(s)].source = ModSource::constant;
        ModernLayer changed, reference;
        setup(changed, 96000.0, c, controls(0.25f)); setup(reference, 96000.0, c, controls(0.25f));
        bool exact = true;
        for (int n = 0; n < 64; ++n)
        {
            sample(changed, voice(n >= 10 ? 2 : 1, 1.0f));
            sample(reference, voice(1, 1.0f));
            exact &= changed.msegValue(-1, 0) == reference.msegValue(-1, 0)
                && changed.displayPosition(0) == reference.displayPosition(0);
        }
        check(exact, "legato voice reuse leaves shared mono/free rate and position unchanged");
    }
}

void testSiblingEnvelopesAndFeedback()
{
    for (int selected = 0; selected < kMsegCount; ++selected)
    {
        ModernConfig c;
        ModernControls p; p.enabled = true;
        for (int m = 0; m < kMsegCount; ++m)
        {
            c.msegs[static_cast<std::size_t>(m)] = defaultShape(2);
            c.slots[static_cast<std::size_t>(2 * m)] = {
                source(m), ModSource::none, ModDestination::pitch, ModTarget::both};
            c.slots[static_cast<std::size_t>(2 * m + 1)] = {
                ModSource::velocity, ModSource::none, destination(m), ModTarget::both};
            p.amount[static_cast<std::size_t>(2 * m)] = 1.0f;
            p.amount[static_cast<std::size_t>(2 * m + 1)] = 0.5f;
        }
        ModernLayer changed, reference;
        setup(changed, 96000.0, c, p); setup(reference, 96000.0, c, p);
        for (int n = 0; n < 32; ++n) { sample(changed, voice(1, 1.0f)); sample(reference, voice(1, 1.0f)); }
        c.msegs[static_cast<std::size_t>(selected)].trigger = MsegTrigger::mono;
        changed.setConfig(c);
        c.msegs[static_cast<std::size_t>(selected)].trigger = MsegTrigger::poly;
        changed.setConfig(c);
        bool exact = true;
        for (int n = 0; n < 32; ++n)
        {
            sample(changed, voice(1, 1.0f)); sample(reference, voice(1, 1.0f));
            for (int m = 0; m < kMsegCount; ++m)
                if (m != selected)
                    exact &= changed.msegValue(0, m) == reference.msegValue(0, m);
        }
        check(exact, "explicit restart preserves the other MSEGs and their delayed rates on the same voice");

        for (float amount : {-1.0f, 1.0f})
        {
            auto self = configuration(selected);
            self.msegs[static_cast<std::size_t>(selected)].count = 2;
            for (int s = 1; s <= 3; ++s) self.slots[static_cast<std::size_t>(s)].source = source(selected);
            ModernLayer reused, fresh;
            setup(reused, 96000.0, self, controls(amount)); setup(fresh, 96000.0, self, controls(amount));
            for (int n = 0; n < 4096; ++n) sample(reused, voice(1, 1.0f));
            bool identical = reused.msegValue(0, selected) > 0.0f;
            for (int n = 0; n < 64; ++n)
            {
                sample(reused, voice(2, 1.0f)); sample(fresh, voice(2, 1.0f));
                identical &= reused.msegValue(0, selected) == fresh.msegValue(0, selected)
                    && reused.displayPosition(selected) == fresh.displayPosition(selected);
            }
            check(identical, "self-modulated envelope restarts without its previous feedback history");
        }
    }
}

bool render(Engine& e, int frames)
{
    std::array<float, 128> out {};
    bool finite = true;
    while (frames > 0)
    {
        const int count = std::min(frames, 64); e.render(out.data(), count);
        finite &= std::all_of(out.begin(), out.begin() + count * 2, [](float x) { return std::isfinite(x); });
        frames -= count;
    }
    return finite;
}
void testEngineReuse()
{
    for (double fs : {44100.0, 48000.0, 96000.0, 192000.0})
    for (int policy = 0; policy < 3; ++policy)
    for (int m = 0; m < kMsegCount; ++m)
    {
        ParamValues p; p[P_sourcePerformance] = policy == 1; p[P_parityMode] = policy == 2;
        p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
        for (int s = 0; s < 4; ++s) p[P_mod1Amount + s] = 1.0f;
        auto c = configuration(m); c.msegs[static_cast<std::size_t>(m)].count = 2;
        Engine reused, fresh;
        for (auto* e : {&reused, &fresh}) { e->prepare(fs); e->setParams(p); e->setModernConfig(c); }
        for (int note = 60; note < 68; ++note) reused.noteOn(note, 1.0f, 1);
        check(render(reused, 8), "Engine hard-note fixture has finite audio");
        for (int note = 60; note < 68; ++note) reused.noteOff(note, 1);
        reused.noteOn(72, 1.0f / 127.0f, 1); fresh.noteOn(72, 1.0f / 127.0f, 1);
        check(reused.getVoiceInfo(0).held && reused.getVoiceInfo(0).note == 72,
              "cyclic Engine allocator really reuses the first circuit");
        bool exact = true, finite = true;
        for (int n = 0; n < 16; ++n)
        {
            finite &= render(reused, 1) && render(fresh, 1);
            exact &= reused.getModernMsegValue(0, m) == fresh.getModernMsegValue(0, m)
                && reused.getModernRankOffsets(0, 0).pitch == fresh.getModernRankOffsets(0, 0).pitch;
        }
        check(exact && finite, "public Engine retrigger trajectory matches fresh allocation across rates/policies");
        if (policy == 2)
            check(!reused.isModernLayerActive() && reused.getModernMsegValue(0, m) == 0.0f,
                  "PURE CS-80 remains inert");
    }
}
} // namespace
int main()
{
    testShortEnvelope(); testReusedVoices(); testHeldRateAndSiblings();
    testRouteEditsAndExplicitRestarts(); testSharedRunners(); testSiblingEnvelopesAndFeedback(); testEngineReuse();
    std::printf("MSEG retrigger rate: %d/%d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
