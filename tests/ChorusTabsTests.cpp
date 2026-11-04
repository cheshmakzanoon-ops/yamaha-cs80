#include "core/Engine.h"
#include "core/ChorusTabs.h"
#include "reference/Oe1Modulation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* name) {
    ++checks;
    if (!ok) { ++failures; if (failures < 12) std::fprintf(stderr, "FAIL %s\n", name); }
}
std::vector<float> render(int policy, bool oldTremolo, cs80::ChorusTabMode tabs) {
    cs80::Engine engine;
    engine.prepare(48000);
    cs80::ParamValues p;
    p[cs80::P_sourcePerformance] = policy == 1;
    p[cs80::P_parityMode] = policy == 2;
    p[cs80::P_chorusOn] = 1;
    p[cs80::P_chorusMode] = oldTremolo;
    p[cs80::P_chorusTabs] = static_cast<float>(tabs);
    p[cs80::P_chorusDepth] = 0.8f;
    p[cs80::P_chorusSpeed] = 4.0f;
    engine.setParams(p);
    engine.noteOn(60, .7f);
    std::vector<float> audio(8192 * 2);
    engine.render(audio.data(), 8192);
    return audio;
}
}

int main() {
    using namespace cs80;
    using M = ChorusTabMode;
    check(ParamValues{}[P_chorusTabs] == 0, "new parameter neutral by default");
    for (bool old : {false, true}) {
        const auto original = chorusSwitches(true, old, M::historical);
        check(original.chorus == !old && original.tremolo == old, "historical mapping");
        for (M m : {M::historical, M::off, M::chorus, M::tremolo, M::both}) {
            const auto off = chorusSwitches(false, old, m);
            check(!off.chorus && !off.tremolo, "section master gates both tabs");
        }
    }
    double maxOracle = 0, distinct = 0;
    for (double rate : {88200., 96000., 192000.}) {
        BbdChorus historical, explicitMode, chorus, both, off;
        for (auto* c : {&historical, &explicitMode, &chorus, &both, &off}) c->prepare((float)rate);
        for (int n = 0; n < 24000; ++n) {
            const float x = (float)(.3 * std::sin(2 * 3.141592653589793 * 997 * n / rate));
            const bool oldTremolo = n >= 12000;
            const auto a = historical.process(x, x, 4, .8f, true, oldTremolo);
            const auto b = explicitMode.processIndependent(x, x, 4, .8f, !oldTremolo, oldTremolo);
            check(a.l == b.l && a.r == b.r, "old two-state audio exact");
            const auto dry = off.processIndependent(x, x, 4, .8f, false, false);
            check(dry.l == x && dry.r == x, "both off dry path exact");
            const auto c = chorus.processIndependent(x, x, 4, .8f, true, false);
            const auto d = both.processIndependent(x, x, 4, .8f, true, true);
            // Independent closed-time OE1 truth model; chorus and both must
            // share the same OE2 moving-clock packet history.
            const auto o = reference::oe1::oscillators(4, (n + 1) / rate);
            const auto g = reference::oe1::vcaGains(o, (double).8f, true, 1);
            const double wantL = x + (c.l - x) * (g[0] + g[1]);
            const double wantR = x + (c.r - x) * (g[2] + g[3]);
            maxOracle = std::max({maxOracle, std::abs(d.l - wantL), std::abs(d.r - wantR)});
            distinct = std::max({distinct, std::abs((double)d.l - c.l), std::abs((double)d.r - c.r)});
        }
    }
    check(maxOracle <= 2e-6, "both tabs follow independent OE1 gain oracle on moving OE2 clock");
    check(distinct > 1e-6, "both tabs differ from chorus alone");
    for (int policy : {0, 1, 2}) for (bool old : {false, true}) {
        const auto original = render(policy, old, M::historical);
        const auto single = render(policy, old, old ? M::tremolo : M::chorus);
        check(original == single, "historical engine sound exact in all policies");
        if (policy == 0) for (M m : {M::off, M::chorus, M::tremolo, M::both})
            check(original == render(policy, old, m), "new physical tab parameter inert in legacy");
        else {
            check(original != render(policy, old, M::off), "engine wires both-off state");
            check(original != render(policy, old, M::both), "engine wires both-on state");
        }
    }
    std::printf("Chorus tabs: %d checks, %d failures; oracle %.9g, both-state difference %.9g\n",
                checks, failures, maxOracle, distinct);
    return failures ? 1 : 0;
}
