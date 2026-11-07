// Combined mono-onset/poly-retrigger regression. This checks modern modulation
// scheduling, not hardware fidelity or plugin/GUI validation.
#include "core/ModernLayer.h"

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
        ++failures;
        if (failures < 25) std::fprintf(stderr, "FAIL %s\n", label.c_str());
    }
}
ModSource source(int m)
{
    return static_cast<ModSource>(static_cast<int>(ModSource::mseg1) + m);
}
ModDestination destination(int m)
{
    return static_cast<ModDestination>(static_cast<int>(ModDestination::mseg1Rate) + m);
}

void scenario(double fs, int mono, int poly, bool sync, float sign, int edit)
{
    const int free = 3 - mono - poly;
    ModernConfig config;
    ModernControls controls;
    controls.enabled = true;
    for (int m = 0; m < kMsegCount; ++m)
    {
        config.msegs[m] = defaultShape(2);
        config.msegs[m].sync = sync;
    }
    config.msegs[mono].trigger = MsegTrigger::mono;
    config.msegs[poly].trigger = MsegTrigger::poly;
    config.msegs[free].trigger = MsegTrigger::free;
    config.slots[0] = {source(poly), ModSource::none, ModDestination::pitch, ModTarget::both};
    config.slots[1] = {source(mono), ModSource::none, ModDestination::pan, ModTarget::both};
    config.slots[2] = {source(free), ModSource::none, ModDestination::level, ModTarget::both};
    config.slots[3] = {ModSource::velocity, ModSource::none, destination(poly), ModTarget::both};
    config.slots[4] = {ModSource::constant, source(mono), destination(poly), ModTarget::both};
    config.slots[5] = {source(poly), ModSource::none, destination(mono), ModTarget::both};
    config.slots[6] = {ModSource::constant, ModSource::none, destination(free), ModTarget::both};
    controls.amount[0] = 1.0f;
    controls.amount[2] = 0.1f;
    controls.amount[3] = 0.7f * sign;
    controls.amount[5] = 0.2f * sign;
    controls.amount[6] = 0.3f;
    const auto route = config.slots[1];
    if (edit == 1 || edit == 3)
    {
        config.slots[1].source = ModSource::constant;
        controls.amount[1] = 1.0f;
    }
    if (edit == 2)
    {
        config.slots[1].destination = ModDestination::none;
        controls.amount[1] = 1.0f;
    }

    ModernLayer layer;
    layer.prepare(fs);
    layer.setTransport({90.0, 0.0, false, false});
    // Consume the initial mode-change request before dormancy. Otherwise the
    // old implementation defers it until activation, masking the lost onset.
    auto initialConfig = config;
    initialConfig.slots[1] = route;
    auto initialControls = controls;
    initialControls.amount[1] = 1.0f;
    layer.setConfig(initialConfig);
    layer.setControls(initialControls);
    layer.beginSample({});
    layer.setConfig(config);
    layer.setControls(controls);

    // Independent runners explicitly model the scheduling contract. Global
    // routes see the prior voice sample; rate values affect the following tick.
    MsegRunner monoRunner, polyRunner, freeRunner;
    freeRunner.trigger(config.msegs[free]);
    freeRunner.tick(config.msegs[free], msegAdvance(1.0, sync, 90.0, fs), true);
    float monoRate = 0.0f, polyRate = 0.0f, lastPoly = 0.0f;
    float freeRate = controls.amount[6] * destinationScale(destination(free));
    bool startedMono = false;
    auto advance = [&](int m, float rate) {
        return msegAdvance(std::exp2(static_cast<double>(std::clamp(rate, -8.0f, 8.0f))),
                           config.msegs[m].sync, 90.0, fs);
    };
    bool exact = true, first = true, restart = true, freeExact = true;
    for (int n = 0; n < 160; ++n)
    {
        const bool used = n >= 25;
        const bool polyRestart = n == 0 || n == 24 || n == 80;
        const float velocity = n < 24 ? 1.0f : 1.0f / 127.0f;
        if (n == 25)
        {
            config.slots[1] = route;
            if (edit == 3)
                config.slots[1] = {ModSource::constant, source(mono), ModDestination::pan, ModTarget::both};
            controls.amount[1] = 1.0f;
            controls.amount[4] = 0.4f * sign;
            layer.setConfig(config);
            layer.setControls(controls);
        }
        if (n == 80)
        {
            config.msegs[poly].trigger = MsegTrigger::mono;
            layer.setConfig(config);
            config.msegs[poly].trigger = MsegTrigger::poly;
            layer.setConfig(config);
        }
        if (n == 1)
        {
            monoRunner.trigger(config.msegs[mono]);
            startedMono = true;
        }
        float monoValue = monoRunner.getValue();
        if (used)
            monoValue = monoRunner.tick(config.msegs[mono], advance(mono, monoRate), true);
        const float freeValue = freeRunner.tick(config.msegs[free], advance(free, freeRate), true);
        monoRate = controls.amount[5] * lastPoly * destinationScale(destination(mono));
        freeRate = controls.amount[6] * destinationScale(destination(free));
        if (polyRestart)
        {
            polyRunner.trigger(config.msegs[poly]);
            polyRate = 0.0f;
        }
        const float polyValue = polyRunner.tick(config.msegs[poly], advance(poly, polyRate), true);
        polyRate = (controls.amount[3] * destinationScale(destination(poly))) * velocity;
        if (used)
        {
            // Match the matrix's rounded float slot product before its separate
            // accumulation. An oracle-only fused multiply-add changes the
            // trajectory by one float ULP on FMA-capable targets. Keep the
            // exact assertions and production arithmetic unchanged.
            const volatile float crossRate =
                (controls.amount[4] * destinationScale(destination(poly))) * monoValue;
            polyRate += crossRate;
        }
        lastPoly = polyValue;

        VoiceInputs voice;
        voice.age = n < 24 ? 1 : 2;
        voice.velocity = velocity;
        voice.gate = true;
        layer.beginSample({});
        layer.processVoice(0, voice);
        bool agree = layer.msegValue(-1, mono) == monoValue && layer.msegValue(0, poly) == polyValue;
        if (used) agree &= layer.rankOffsets(0, 0).pan == monoValue;
        exact &= agree;
        freeExact &= layer.msegValue(-1, free) == freeValue;
        if (n == 25)
            first &= startedMono && monoValue > 0.0f && layer.msegValue(-1, mono) == monoValue;
        if (polyRestart)
            restart &= layer.msegValue(0, poly) == polyValue && layer.displayPosition(poly) == advance(poly, 0.0f);
    }
    const auto label = "fs=" + std::to_string(fs) + " mono=" + std::to_string(mono)
        + " poly=" + std::to_string(poly) + " sync=" + std::to_string(sync)
        + " sign=" + std::to_string(sign) + " edit=" + std::to_string(edit);
    check(first, "dormant mono onset survives simultaneous poly reuse " + label);
    check(exact, "mixed mono/poly cross-rate trajectory equals explicit oracle " + label);
    check(restart, "new-note and explicit poly restart use base rate " + label);
    check(freeExact, "continuous free sibling is unchanged " + label);
}
} // namespace

int main()
{
    for (double fs : {16000.0, 88200.0, 96000.0, 192000.0, 384000.0, 1536000.0})
    for (int mono = 0; mono < kMsegCount; ++mono)
    for (int poly = 0; poly < kMsegCount; ++poly)
    if (mono != poly)
    for (bool sync : {false, true})
    for (float sign : {-1.0f, 1.0f})
    for (int edit = 0; edit < 4; ++edit)
        scenario(fs, mono, poly, sync, sign, edit);
    std::printf("Combined MSEG interactions: %d/%d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
