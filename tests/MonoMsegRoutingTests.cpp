#include "core/Engine.h"
#include "core/ModernLayer.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace cs80;
using namespace cs80::modern;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
bool near(double a, double b) { return std::abs(a - b) < 1.0e-6; }

ModSource source(int m) { return (ModSource) ((int) ModSource::mseg1 + m); }

ModernConfig configuration(int m)
{
    ModernConfig c;
    for (auto& shape : c.msegs)
    {
        shape = defaultShape(2); // Two-second ramp, holding one at its end.
        shape.trigger = MsegTrigger::mono;
    }
    c.slots[0] = {ModSource::constant, ModSource::none, ModDestination::level, ModTarget::both};
    c.slots[1] = {source(m), ModSource::none, ModDestination::pan, ModTarget::both};
    c.slots[2] = {source((m + 1) % kMsegCount), ModSource::none, ModDestination::pitch, ModTarget::both};
    c.slots[3] = {ModSource::noteRandom, ModSource::none, ModDestination::cutoff, ModTarget::both};
    return c;
}

void testDormantOnset()
{
    // Route the envelope once with no gate, then leave it unused during the
    // next onset. Cover both an unstarted runner and a finished prior phrase.
    for (int m = 0; m < kMsegCount; ++m)
        for (bool oldPhrase : {false, true})
            for (int edit = 0; edit < 4; ++edit)
            {
                ModernLayer layer;
                layer.prepare(1000.0);
                auto config = configuration(m);
                ModernControls controls;
                controls.enabled = true;
                controls.amount[0] = controls.amount[1] = controls.amount[2] = controls.amount[3] = 1.0f;
                layer.setConfig(config);
                layer.setControls(controls);
                layer.beginSample({}); // Consume the initial mode-change request.
                VoiceInputs voice;
                voice.age = 1;
                voice.gate = true;
                voice.velocity = 0.8f;
                auto tick = [&](int count) {
                    for (int n = 0; n < count; ++n)
                    {
                        layer.beginSample({});
                        layer.processVoice(0, voice);
                    }
                };
                if (oldPhrase)
                {
                    tick(2100);
                    check(layer.msegValue(-1, m) == 1.0f, "prior mono phrase finishes before the route is removed");
                    voice.gate = false;
                    tick(2);
                    ++voice.age;
                    voice.gate = true;
                }
                const auto liveSlot = config.slots[1];
                if (edit == 0) controls.amount[1] = 0.0f;
                if (edit == 1) config.slots[1].source = ModSource::constant;
                if (edit == 2) config.slots[1].destination = ModDestination::none;
                if (edit == 3) config.slots[1] = {ModSource::constant, ModSource::none,
                                                 ModDestination::pan, ModTarget::both};
                layer.setConfig(config);
                layer.setControls(controls);
                tick(100);
                const float sibling = layer.msegValue(-1, (m + 1) % kMsegCount);
                const float randomOffset = layer.rankOffsets(0, 0).hpCutoff;
                config.slots[1] = liveSlot;
                if (edit == 3) // MSEG used as a via is just as live as a main source.
                    config.slots[1] = {ModSource::constant, source(m), ModDestination::pan, ModTarget::both};
                controls.amount[1] = 1.0f;
                layer.setConfig(config);
                layer.setControls(controls);
                tick(100);
                check(near(layer.msegValue(-1, m), 0.05), "dormant mono envelope remembers the current phrase onset");
                check(near(layer.rankOffsets(0, 0).pan, 0.05), "new mono route produces modulation without another note-on");
                check(near(layer.msegValue(-1, (m + 1) % kMsegCount) - sibling, 0.05),
                      "activating a route does not restart its continuously used sibling");
                check(layer.rankOffsets(0, 0).hpCutoff == randomOffset, "route activation preserves latched note randomness");
                layer.setConfig(config);
                layer.setControls(controls);
                tick(100);
                check(near(layer.msegValue(-1, m), 0.1), "unchanged route publication does not restart the envelope");
            }
}

void testDormantReleaseTail()
{
    // Removing a route pauses its clock, not its phrase identity. Re-adding
    // it during release must resume the existing tail, not invent an onset.
    ModernLayer layer;
    layer.prepare(1000.0);
    auto config = configuration(0);
    config.msegs[0] = defaultShape(0);
    config.msegs[0].trigger = MsegTrigger::mono;
    ModernControls controls;
    controls.enabled = true;
    controls.amount[0] = controls.amount[1] = 1.0f;
    layer.setConfig(config);
    layer.setControls(controls);
    VoiceInputs voice;
    voice.age = 1;
    voice.gate = true;
    auto tick = [&](int count) {
        for (int n = 0; n < count; ++n)
        {
            layer.beginSample({});
            layer.processVoice(0, voice);
        }
    };
    tick(500);
    voice.gate = false;
    tick(21);
    const float before = layer.msegValue(-1, 0);
    controls.amount[1] = 0;
    layer.setControls(controls);
    tick(100);
    check(layer.msegValue(-1, 0) == before, "unused mono release clock stays paused");
    controls.amount[1] = 1;
    layer.setControls(controls);
    tick(1);
    const float after = layer.msegValue(-1, 0);
    check(after < before && after > 0.0f, "re-enabled mono route resumes its release instead of retriggering");
}

void testEngineDormantOnset()
{
    for (double sr : {44100.0, 48000.0, 96000.0, 192000.0})
        for (int policy = 0; policy < 3; ++policy)
            for (int m = 0; m < kMsegCount; ++m)
            {
                Engine engine;
                engine.prepare(sr);
                ParamValues params;
                params[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
                params[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
                params[P_mod1Amount] = 0.1f; // Keep modern processing active.
                params[P_mod2Amount] = 1.0f;
                engine.setModernConfig(configuration(m));
                engine.setParams(params);
                const int frames = (int) (sr * 0.01);
                std::vector<float> block(2 * (std::size_t) frames);
                engine.render(block.data(), 1);
                params[P_mod2Amount] = 0.0f;
                engine.setParams(params);
                engine.noteOn(60, 0.8f);
                engine.render(block.data(), frames);
                params[P_mod2Amount] = 1.0f;
                engine.setParams(params);
                engine.render(block.data(), frames);
                check(near(engine.getModernMsegValue(-1, m), policy == 2 ? 0.0 : frames / (2.0 * sr)),
                      "Engine restores held mono modulation at each host rate while PURE stays inert");
                bool finite = true;
                for (float x : block) finite = finite && std::isfinite(x);
                check(finite, "dormant mono route activation renders finite audio");
            }
}
}

int main()
{
    testDormantOnset();
    testDormantReleaseTail();
    testEngineDormantOnset();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
