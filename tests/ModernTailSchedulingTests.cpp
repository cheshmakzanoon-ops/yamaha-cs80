// Extra tail evaluations must not publish a gate or trigger the shared MSEG.
// This is a software scheduling contract, not a Yamaha circuit assertion.
#include "core/ModernLayer.h"
#include "Ws1Tolerances.h"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>

using namespace cs80::modern;
namespace {
int checks = 0, failures = 0;
void check(bool okay, const char* label)
{
    ++checks;
    if (!okay) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
bool same(float a, float b)
{
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}
void prepare(ModernLayer& layer)
{
    ModernConfig config;
    auto& shape = config.msegs[0];
    shape.count = 3; shape.trigger = MsegTrigger::mono;
    shape.sync = shape.bipolar = false;
    shape.loop = MsegLoop::sustain; shape.loopStart = shape.loopEnd = 1;
    shape.points[0] = { 0.0f, 0.2f, 0.0f };
    shape.points[1] = { 0.01f, 0.8f, 0.0f };
    shape.points[2] = { 0.05f, 0.0f, 0.0f };
    config.slots[0] = { ModSource::mseg1, ModSource::none, ModDestination::pan, ModTarget::both };
    ModernControls controls; controls.enabled = true; controls.amount[0] = 0.6f;
    layer.setConfig(config); layer.setControls(controls); layer.prepare(96000.0);
}
bool sameShared(const ModernLayer& a, const ModernLayer& b)
{
    return a.displayPosition(0) == b.displayPosition(0) && same(a.msegValue(-1, 0), b.msegValue(-1, 0));
}
void monoTrigger()
{
    ModernLayer actual, reference; prepare(actual); prepare(reference);
    VoiceInputs in; in.age = 17; in.gate = false; in.velocity = 0.7f;
    bool exact = true, silentBeforeDeadline = true;
    for (int sample = 0; sample < 32; ++sample)
    {
        actual.beginSample({}); reference.beginSample({});
        exact = exact && sameShared(actual, reference);
        if (sample <= 8)
            silentBeforeDeadline = silentBeforeDeadline && actual.displayPosition(0) < 0.0;
        if (sample == 8)
        {
            actual.processVoice(0, in); reference.processVoice(0, in);
        }
        else actual.processVoice(0, in, false);
    }
    check(reference.displayPosition(0) > 0.01, "mono control starts only after its real scheduled trigger");
    check(silentBeforeDeadline, "unscheduled unlatched tail never triggers the shared mono MSEG early");
    check(exact, "extra tail updates preserve shared mono trigger timing exactly");
}
void gatePublication()
{
    ModernLayer actual, reference; prepare(actual); prepare(reference);
    VoiceInputs in; in.age = 1; in.gate = true; in.velocity = 0.7f;
    for (int sample = 0; sample < 3; ++sample)
    {
        actual.beginSample({}); reference.beginSample({});
        actual.processVoice(0, in); reference.processVoice(0, in);
    }
    bool exact = true;
    for (int sample = 0; sample < 32; ++sample)
    {
        actual.beginSample({}); reference.beginSample({});
        exact = exact && sameShared(actual, reference);
        actual.processVoice(0, in, false);
    }
    check(reference.displayPosition(0) > 0.01, "omitted-update control releases its mono sustain loop");
    check(exact, "extra tail updates do not publish held-gate state between scheduled calls");
}
}

int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances limits(argc > 1 ? argv[1]
            : "tests/parity/tolerances-v2/vca-tail-modulation.json", "limits");
        check(limits["global_selection_bit_mismatches_max"] == 0.0,
              "registered exact tail ownership limit remains zero");
        monoTrigger(); gatePublication();
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
