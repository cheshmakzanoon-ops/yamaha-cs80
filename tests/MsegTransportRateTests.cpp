#include "core/ModernLayer.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace cs80::modern;
namespace {
int failures = 0;
void check(bool condition, const char* label) {
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
ModernConfig configuration() {
    ModernConfig c;
    c.msegs[0] = defaultShape(2);
    c.msegs[0].sync = true;
    c.msegs[0].trigger = MsegTrigger::free;
    c.slots[0] = {ModSource::mseg1, ModSource::none, ModDestination::fx1Mix, ModTarget::both};
    c.slots[1] = {ModSource::constant, ModSource::none, ModDestination::mseg1Rate, ModTarget::both};
    return c;
}
ModernControls controls(float amount = 0.5f) {
    ModernControls p; p.enabled = true; p.amount[0] = 1; p.amount[1] = amount; return p;
}
double render(int block, double fs, float amount) {
    ModernLayer layer; layer.prepare(fs); layer.setConfig(configuration()); layer.setControls(controls(amount));
    for (int n = 0; n < 4800; ++n) {
        if (n % block == 0) layer.setTransport({120, 2.0 * n / fs, true, true});
        layer.beginSample({});
    }
    return layer.displayPosition(0);
}
bool near(double a, double b) { return std::abs(a - b) < 2e-9; }
constexpr double fs = 48000.0;
constexpr double step = 2.0 / fs;
double ratio(float amount) { return std::exp2(double(amount) * kMsegRateOctaves); }
struct Fixture {
    ModernLayer layer;
    ModernConfig c = configuration();
    ModernControls p = controls();
    Fixture() { layer.prepare(fs); layer.setConfig(c); layer.setControls(p); }
    void samples(int count) { for (int n = 0; n < count; ++n) layer.beginSample({}); }
    void warm() { layer.setTransport({120, 0, true, true}); samples(100); }
    double position() const { return layer.displayPosition(0); }
    void transport(double ppq, bool has = true, bool playing = true, double bpm = 120) {
        layer.setTransport({bpm, ppq, has, playing});
    }
};
void unmodulatedExactReference() {
    for (double bpm : {0.0, 90.0, 120.0, 999.0, std::numeric_limits<double>::quiet_NaN()})
    for (double ppq : {-0.1, 0.0, 0.2}) {
        Fixture f; f.p.amount[1] = 0; f.p.msegRate[0] = 1.25f; f.layer.setControls(f.p);
        MsegRunner reference;
        bool exact = true;
        for (int n = 0; n < 1024; ++n) {
            if (n % 17 == 0) {
                double position = ppq + n * effectiveBpm(bpm) / (60.0 * fs);
                f.transport(position, true, true, bpm);
                reference.seek(f.c.msegs[0], position * 1.25);
            }
            reference.tick(f.c.msegs[0], msegAdvance(1.25, true, bpm, fs), true);
            f.samples(1);
            exact = exact && f.position() == reference.getPosition()
                && f.layer.msegValue(-1, 0) == reference.getValue();
        }
        check(exact, "unmodulated position and output match original seek/tick behavior exactly");
    }
}

void transportDiscontinuities() {
    {
        Fixture f; f.c.msegs[0] = defaultShape(1);
        f.c.msegs[0].trigger = MsegTrigger::free; f.c.msegs[0].sync = true;
        f.layer.setConfig(f.c); f.p.amount[1] = 1; f.layer.setControls(f.p);
        f.transport(1e12); f.samples(10);
        f.transport(1e12); f.samples(1);
        MsegRunner reference; reference.seek(f.c.msegs[0], 1e12);
        reference.tick(f.c.msegs[0], step * ratio(1), true);
        check(near(f.position(), reference.getPosition()), "stationary large PPQ resets correction despite rounding tolerance");
    }

    for (double invalid : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()}) {
        Fixture f; f.warm(); const double before = f.position();
        f.transport(invalid); f.samples(20);
        check(near(f.position(), before + 20 * step * ratio(.5f)), "invalid PPQ keeps advancing locally");
        f.transport(.25); f.samples(1);
        check(near(f.position(), .25 + step * ratio(.5f)), "valid PPQ recovery discards old correction");
    }
    {
        Fixture f; f.warm(); f.transport(100 * step, false); f.samples(20);
        f.transport(120 * step); f.samples(1);
        check(near(f.position(), 121 * step + step * (ratio(.5f) - 1)), "missing PPQ recovery reanchors");
    }
    for (double seek : {.5, .001, -1.0}) {
        Fixture f; f.warm(); f.transport(seek); f.samples(1);
        check(near(f.position(), std::max(0.0, seek) + step * ratio(.5f)), "genuine forward/backward PPQ seek reanchors");
    }
    for (int count : {1, 64}) {
        Fixture f; f.transport(.2); f.samples(count); f.transport(.2); f.samples(1);
        check(near(f.position(), .2 + step * ratio(.5f)), "unchanged playing PPQ is a seek even after one sample");
    }
    {
        Fixture f; f.warm(); const double before = f.position();
        f.transport(100 * step, true, false); f.samples(25);
        check(near(f.position(), before + 25 * step * ratio(.5f)), "stopped free MSEG still runs");
        f.transport(125 * step); f.samples(1);
        check(near(f.position(), 125 * step + step * ratio(.5f)), "play restart clears accumulated correction");
    }
    {
        Fixture f; f.warm(); f.transport(.3); f.layer.reset(); f.samples(1);
        check(near(f.position(), .3 + step), "reset reanchors and clears delayed rate state");
    }
    {
        Fixture f; f.transport(0, false); f.samples(100); f.transport(.4); f.samples(1);
        check(near(f.position(), .4 + step * ratio(.5f)), "first valid PPQ discards local accumulated correction");
    }
    {
        Fixture f; f.warm(); double before = f.position();
        f.transport(100 * step, true, true, 90); f.samples(31);
        check(near(f.position(), before + 31 * (1.5 / fs) * ratio(.5f)), "tempo boundary continuity uses preceding BPM");
        before = f.position(); f.transport(100 * step + 31 * 1.5 / fs, true, true, 180); f.samples(1);
        check(near(f.position(), before + (3.0 / fs) * ratio(.5f)), "successive tempo boundary preserves correction");
    }
}
void controlChanges() {
    {
        Fixture f; f.warm(); const double before = f.position();
        f.p.amount[1] = 0; f.layer.setControls(f.p);
        f.samples(1); // Rate destinations are evaluated after the current sample's tick.
        const double settled = f.position(); f.transport(101 * step); f.samples(10);
        check(near(f.position(), settled + 10 * step), "removing rate modulation preserves phase with another global destination live");
        check(settled >= before, "route removal cannot rewind the runner");
    }
    {
        Fixture f; f.warm(); const double correction = f.position() - 100 * step;
        f.p.msegRate[0] = 1.5f; f.layer.setControls(f.p);
        f.transport(100 * step); f.samples(1);
        check(near(f.position(), 150 * step + correction + 1.5 * step * ratio(.5f)), "base-rate edit retains correction and PPQ reanchoring");
    }
    for (bool triggerChange : {false, true}) {
        Fixture f; f.warm();
        if (triggerChange) f.c.msegs[0].trigger = MsegTrigger::mono;
        else f.c.msegs[0].sync = false;
        f.layer.setConfig(f.c); f.samples(10);
        f.c.msegs[0].trigger = MsegTrigger::free; f.c.msegs[0].sync = true;
        f.layer.setConfig(f.c); f.transport(110 * step); f.samples(1);
        check(near(f.position(), 110 * step + step * ratio(.5f)), "sync or trigger edit clears old correction");
    }
    {
        Fixture f; f.warm(); f.c.slots[0].source = ModSource::constant;
        f.layer.setConfig(f.c); f.samples(10);
        f.c.slots[0].source = ModSource::mseg1; f.layer.setConfig(f.c);
        f.transport(110 * step); f.samples(1);
        check(near(f.position(), 110 * step + step * ratio(.5f)), "newly used MSEG clears stale correction");
    }
    {
        Fixture f; f.warm(); f.p.enabled = false; f.layer.setControls(f.p); f.samples(100);
        const auto& g = f.layer.getGlobalOffsets(); const auto& r = f.layer.rankOffsets(0, 0);
        check(!f.layer.isActive() && f.position() == -1 && g.subSpeedRatio == 1 && g.subDepthGain == 1
              && g.fxMix == std::array<float, 3>{} && r.pitch == 0 && r.hpQ == 1 && r.lpQ == 1,
              "inactive/PURE leaves offsets exactly at identity");
        f.p.enabled = true; f.layer.setControls(f.p); f.transport(200 * step); f.samples(1);
        check(near(f.position(), 201 * step), "reactivation starts from host without old correction");
    }
}
void independentCorrections() {
    for (bool changeTrigger : {false, true}) {
        Fixture f;
        f.c.msegs[1] = f.c.msegs[0];
        f.c.slots[2] = {ModSource::mseg2, ModSource::none, ModDestination::fx2Mix, ModTarget::both};
        f.c.slots[3] = {ModSource::constant, ModSource::none, ModDestination::mseg2Rate, ModTarget::both};
        f.p.amount[2] = 1; f.p.amount[3] = -.5f;
        f.layer.setConfig(f.c); f.layer.setControls(f.p);
        f.warm();
        const double secondBefore = f.layer.displayPosition(1);
        if (changeTrigger) f.c.msegs[0].trigger = MsegTrigger::mono;
        else f.c.msegs[0].sync = false;
        f.layer.setConfig(f.c); f.samples(10);
        f.c.msegs[0].trigger = MsegTrigger::free; f.c.msegs[0].sync = true;
        f.layer.setConfig(f.c); f.transport(110 * step); f.samples(1);
        check(near(f.position(), 110 * step + step * ratio(.5f)),
              "changed MSEG alone loses its correction");
        check(near(f.layer.displayPosition(1), secondBefore + 11 * step * ratio(-.5f)),
              "opposite-rate synced MSEG preserves correction when its neighbor changes mode");
    }
}

// Compare every sample, not just the final phase, against an uninterrupted runner.
// Control automation is sample-timed independently of host block boundaries.
void loopingAndMixedPartitions() {
    for (double rate : {44100.0, 48000.0, 96000.0})
    for (auto loop : {MsegLoop::off, MsegLoop::always, MsegLoop::sustain})
    for (int block : {1, 17, 127, 512}) {
        ModernLayer continuous, partitioned;
        auto c = configuration(); auto p = controls();
        c.msegs[0].points[1].time = .125f;
        c.msegs[0].loop = loop; c.msegs[0].loopEnd = 1;
        // Concurrent unsynced free and poly MSEGs must remain independent.
        c.msegs[1] = defaultShape(1); c.msegs[1].trigger = MsegTrigger::free;
        c.msegs[2] = defaultShape(2); c.msegs[2].trigger = MsegTrigger::poly;
        c.slots[2] = {ModSource::mseg2, ModSource::none, ModDestination::fx2Mix, ModTarget::both};
        c.slots[3] = {ModSource::mseg3, ModSource::none, ModDestination::pitch, ModTarget::both};
        p.amount[2] = p.amount[3] = 1;
        for (auto* layer : {&continuous, &partitioned}) {
            layer->prepare(rate); layer->setConfig(c); layer->setControls(p);
            layer->setTransport({120, 0, true, true});
        }
        VoiceInputs voice; voice.age = 1; voice.gate = true;
        bool matches = true;
        for (int n = 0; n < 12000; ++n) {
            if (n == 1733 || n == 4911) {
                p.amount[1] = n == 1733 ? -.25f : 0;
                continuous.setControls(p); partitioned.setControls(p);
            }
            if (n % block == 0) partitioned.setTransport({120, 2.0 * n / rate, true, true});
            continuous.beginSample({}); partitioned.beginSample({});
            continuous.processVoice(0, voice); partitioned.processVoice(0, voice);
            for (int m = 0; m < 3; ++m)
                matches = matches && near(continuous.displayPosition(m), partitioned.displayPosition(m))
                    && std::abs(continuous.msegValue(0, m) - partitioned.msegValue(0, m)) < 2e-6;
        }
        check(matches, "every-sample phase/value invariance through loops, rate edits/removal, and mixed triggers");
    }
}

}
int main() {
    unmodulatedExactReference();
    transportDiscontinuities();
    controlChanges();
    independentCorrections();
    loopingAndMixedPartitions();
    for (double fs : {44100.0, 48000.0, 96000.0})
        for (float amount : {0.0f, 0.5f, -0.5f}) {
            const double reference = render(4800, fs, amount);
            for (int block : {1, 16, 64, 127, 256, 1024})
                check(std::abs(render(block, fs, amount) - reference) < 1e-10,
                      "free synced MSEG phase must not depend on host block partition");
        }
    std::printf("MSEG transport/rate tests: %d failures\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
