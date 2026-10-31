// Phase 2.2: KAS scan/priority, SH per-voice key hold and droop, ninth key.
// Numerical checks of the declared model (src/core/KasScanner.h) against
// src/reference/SampleHoldTruth.h with tolerances pre-registered in
// tests/Ws3Tolerances.h, plus allocation regressions. Not a hardware comparison.
#include "core/Engine.h"
#include "core/KasScanner.h"
#include "reference/SampleHoldTruth.h"
#include "Ws3Tolerances.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace cs80;
namespace sh = cs80::reference::sample_hold;
namespace tol = cs80::ws3_tolerances;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what.c_str()); }
}

void render(Engine& e, int frames)
{
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    for (int i = 0; i < frames; i += 256) e.render(out.data() + static_cast<std::size_t>(i) * 2, std::min(256, frames - i));
}

ParamValues circuit(float isolation = 1.0f)
{
    ParamValues p;
    p[P_parityMode] = isolation;
    p[P_sourcePerformance] = isolation >= 0.5f ? 0.0f : 1.0f;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_detune] = 0.0f;
    p[P_subVco] = p[P_subAtVco] = 0.0f;
    for (int r = 0; r < 2; ++r) { p[chParam(r, CP_vR)] = 8.0f; p[chParam(r, CP_vS)] = 1.0f; }
    return p;
}

void testTruthTable()
{
    for (int tr = 0; tr < 2; ++tr)
        for (int sus = 0; sus < 2; ++sus)
            check(kas::shHolds(tr != 0, sus != 0) == !sh::switchConducts(tr != 0, sus != 0),
                  "NAND(TR,SUS) hold matches SH7 truth table for TR=" + std::to_string(tr) + " SUS=" + std::to_string(sus));
}

void testDroopGateAndSensitivity()
{
    double worst = 0.0;
    for (double c : {0.02e-6, 0.033e-6, 0.22e-6})
        for (double i : {1.0e-12, 10.0e-12, 50.0e-12})
            for (double k0 : {0.0625, 0.125, 0.5, 2.0})
                for (double t : {0.0, 0.1, 1.0, 10.0, 100.0, 1000.0})
                {
                    kas::HeldKeyNode node;
                    node.hold(k0);
                    // Advance in uneven pieces, as the engine does (samples and idle steps).
                    double left = t;
                    for (double piece : {t * 0.37, t * 0.21, t * 0.42}) { node.advance(piece); left -= piece; }
                    node.advance(left);
                    const double realtime = k0 * node.ratio(i, c);
                    const double truth = sh::heldVolts(k0, i, c, t);
                    const double error = std::abs(realtime - truth) / std::max(k0, 1.0e-30);
                    worst = std::max(worst, error);
                }
    check(worst <= tol::kDroopRelativeErrorMax, "held-K droop matches the analytic constant-current law: "
          + std::to_string(worst));
    kas::HeldKeyNode tracking;
    tracking.advance(100.0);
    check(tracking.ratio() == 1.0, "a tracking K node never droops");

    // Sensitivity table (informational): cents flat after 1 s and 5 s of hold.
    std::printf("held-K droop sensitivity (cents flat after 1 s / 5 s):\n");
    for (double c : {0.02e-6, 0.033e-6, 0.22e-6})
        for (double i : {1.0e-12, 10.0e-12, 50.0e-12})
        {
            std::printf("  C=%.3f uF I=%4.0f pA:", c * 1e6, i * 1e12);
            for (int note : {24, 36, 60})
            {
                const double k0 = 0.5 * std::exp2((note - 60) / 12.0);
                kas::HeldKeyNode n; n.hold(k0);
                n.advance(1.0); const double one = -1200.0 * std::log2(n.ratio(i, c));
                n.advance(4.0); const double five = -1200.0 * std::log2(std::max(1e-9, n.ratio(i, c)));
                std::printf("  MIDI%d %.2f/%.2f", note, one, five);
            }
            std::printf("\n");
        }
    // Drift scales with I/C and with 1/K.
    kas::HeldKeyNode a, b, c;
    a.hold(0.5); b.hold(0.25); c.hold(0.5);
    a.advance(2.0); b.advance(2.0); c.advance(2.0);
    const double da = 1.0 - a.ratio(), db = 1.0 - b.ratio(), dc = 1.0 - c.ratio(2.0 * kas::kHoldLeakageAmps, kas::kHoldCapacitanceFarads);
    check(std::abs(db / da - 2.0) < 1e-9 && std::abs(dc / da - 2.0) < 1e-9, "droop proportional to I/C and to 1/K");
}

void testScanner()
{
    kas::Scanner scanner;
    scanner.prepare(96000.0);
    const int period = scanner.scanPeriodSamples();
    check(period == 96, "nominal 1 ms scan period at 96 kHz");
    std::mt19937 random(1234u);
    int sample = 0;
    bool latencyOk = true;
    for (int trial = 0; trial < 500; ++trial)
    {
        const int wait = static_cast<int>(random() % 300);
        for (int i = 0; i < wait; ++i) { scanner.tick([](const kas::Scanner::Event&) {}); ++sample; }
        const int pushedAt = sample;
        scanner.push(60, 1, 0.5f, true);
        int dispatchedAt = -1;
        while (dispatchedAt < 0)
        {
            ++sample;
            scanner.tick([&](const kas::Scanner::Event&) { dispatchedAt = sample; });
        }
        latencyOk = latencyOk && dispatchedAt - pushedAt >= 1 && dispatchedAt - pushedAt <= period;
    }
    check(latencyOk, "detection latency is within one scan period for every event");
    // Priority within one scan: lowest key first, release before strike of the same key.
    scanner.reset();
    scanner.push(72, 1, 1.0f, true);
    scanner.push(48, 1, 1.0f, true);
    scanner.push(60, 1, 0.0f, false);
    scanner.push(60, 1, 1.0f, true);
    scanner.push(36, 2, 1.0f, true);
    std::vector<kas::Scanner::Event> order;
    while (order.empty())
        scanner.tick([&](const kas::Scanner::Event& e) { order.push_back(e); });
    check(order.size() == 5 && order[0].note == 36 && order[1].note == 48 && order[2].note == 60 && !order[2].on
              && order[3].note == 60 && order[3].on && order[4].note == 72,
          "one scan dispatches in scan order: lowest key first, release before strike");
    scanner.push(40, 3, 1.0f, true);
    scanner.push(41, 4, 1.0f, true);
    scanner.discard(3);
    check(scanner.pending() == 1, "panic discards only that channel's queued events");
}

void testEngineHold()
{
    for (double host : {44100.0, 48000.0, 96000.0})
    {
        // Sustain I: a released key's K holds and droops at the declared rate.
        Engine e;
        e.prepare(host);
        auto p = circuit();
        e.setParams(p);
        e.noteOn(36, 0.8f);
        render(e, static_cast<int>(host * 0.1));
        const float heldHz = e.getRankInfo(0, 0).oscillatorHz;
        e.noteOff(36);
        render(e, static_cast<int>(host * 1.0));
        const float releasedHz = e.getRankInfo(0, 0).oscillatorHz;
        const double k0 = filterKeyVolts(36.0f);
        const double expected = sh::heldVolts(k0, kas::kHoldLeakageAmps, kas::kHoldCapacitanceFarads, 1.0) / k0;
        check(std::abs(double(releasedHz) / heldHz - expected) < 2.0e-5,
              "Sustain I release tail droops by the held-node law (" + std::to_string(releasedHz / heldHz) + ")");
        // Sustain II: K keeps tracking, no droop.
        Engine e2;
        e2.prepare(host);
        p[P_sustainMode] = 1.0f;
        e2.setParams(p);
        e2.noteOn(36, 0.8f);
        render(e2, static_cast<int>(host * 0.1));
        const float held2 = e2.getRankInfo(0, 0).oscillatorHz;
        e2.noteOff(36);
        render(e2, static_cast<int>(host * 1.0));
        check(e2.getRankInfo(0, 0).oscillatorHz == held2, "Sustain II release tail tracks KI without droop");
        // Legacy: no hold, no droop.
        Engine legacy;
        legacy.prepare(host);
        auto lp = circuit(0.0f);
        lp[P_sourcePerformance] = 0.0f;
        legacy.setParams(lp);
        legacy.noteOn(36, 0.8f);
        render(legacy, static_cast<int>(host * 0.1));
        const float lh = legacy.getRankInfo(0, 0).oscillatorHz;
        legacy.noteOff(36);
        render(legacy, static_cast<int>(host * 0.5));
        check(legacy.getRankInfo(0, 0).oscillatorHz == lh, "Legacy keeps its historical release pitch");

        // Sustain I freezes a glide at release; Sustain II lets it continue.
        for (int mode : {0, 1})
        {
            Engine g;
            g.prepare(host);
            auto gp = circuit();
            gp[P_portamento] = 2.0f;
            gp[P_sustainMode] = static_cast<float>(mode);
            g.setParams(gp);
            g.noteOn(48, 0.8f); g.noteOff(48);
            render(g, static_cast<int>(host * 0.01));
            for (int k = 0; k < 7; ++k) { g.noteOn(49 + k, 0.8f); g.noteOff(49 + k); }
            g.noteOn(72, 0.8f); // voice 0 again, gliding up from 55
            render(g, static_cast<int>(host * 0.2));
            g.noteOff(72);
            render(g, 64);
            const float atRelease = g.getRankInfo(0, 0).oscillatorHz;
            render(g, static_cast<int>(host * 0.3));
            const float later = g.getRankInfo(0, 0).oscillatorHz;
            if (mode == 0)
                check(later <= atRelease && later > atRelease * 0.999f, "Sustain I holds the glide at release");
            else
                check(later > atRelease * 1.01f, "Sustain II keeps gliding after release");
        }
    }
}

void testAllocation()
{
    for (int policy = 0; policy < 3; ++policy)
    {
        const bool circuitEngine = policy != 0;
        ParamValues p;
        p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
        p[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
        Engine e;
        e.prepare(48000.0);
        e.setParams(p);
        // Cyclic rotation and held protection are unchanged.
        for (int press = 0; press < 24; ++press)
        {
            e.noteOn(60, 0.8f);
            check(e.getVoiceInfo(press % 8).held, "repeated key rotates through all eight circuits");
            e.noteOff(60);
        }
        e.reset();
        for (int i = 0; i < 8; ++i) e.noteOn(48 + i, 0.8f);
        e.noteOn(80, 0.9f); // ninth held key
        bool untouched = true;
        for (int i = 0; i < 8; ++i) untouched = untouched && e.getVoiceInfo(i).note == 48 + i && e.getVoiceInfo(i).held;
        check(untouched, "ninth held key never steals an occupied circuit");
        e.noteOff(51); // frees circuit 3
        const auto v3 = e.getVoiceInfo(3);
        if (circuitEngine)
            check(v3.held && v3.note == 80 && v3.velocity == 0.0f,
                  "circuit engine: the still-held ninth key takes the freed circuit, with no strike");
        else
            check(!v3.held && v3.note == 51, "Legacy: the ignored ninth key is not reconsidered");
        // A ninth key released before any circuit frees never sounds.
        e.reset();
        for (int i = 0; i < 8; ++i) e.noteOn(48 + i, 0.8f);
        e.noteOn(81, 0.9f);
        e.noteOff(81);
        e.noteOff(50);
        check(e.getVoiceInfo(2).note == 50 && !e.getVoiceInfo(2).held, "a released ninth key is forgotten");
        // Lowest held key first when several wait.
        e.reset();
        for (int i = 0; i < 8; ++i) e.noteOn(48 + i, 0.8f);
        e.noteOn(90, 0.9f); e.noteOn(85, 0.9f);
        e.noteOff(52);
        if (circuitEngine) check(e.getVoiceInfo(4).note == 85, "scan priority: lowest waiting key first");
        // Pedal-held circuits stay protected; releasing the pedal frees them for a waiting key.
        if (policy == 1)
        {
            e.reset();
            e.controlChange(64, 1.0f, 1);
            for (int i = 0; i < 8; ++i) { e.noteOn(48 + i, 0.8f); e.noteOff(48 + i); }
            e.noteOn(48 + 8, 0.9f);
            bool protectedAll = true;
            for (int i = 0; i < 8; ++i) protectedAll = protectedAll && e.getVoiceInfo(i).pedalHeld;
            check(protectedAll, "pedal-held circuits are not stolen");
            e.controlChange(64, 0.0f, 1);
            bool assigned = false;
            for (int i = 0; i < 8; ++i) assigned = assigned || (e.getVoiceInfo(i).held && e.getVoiceInfo(i).note == 56);
            check(assigned, "pedal release lets the waiting key take a circuit");
        }
        // Panic forgets waiting keys.
        e.reset();
        for (int i = 0; i < 8; ++i) e.noteOn(48 + i, 0.8f);
        e.noteOn(95, 0.9f);
        e.allNotesOff(false, 0);
        e.noteOn(60, 0.5f);
        bool none95 = true;
        for (int i = 0; i < 8; ++i) none95 = none95 && e.getVoiceInfo(i).note != 95;
        check(none95, "panic forgets a waiting key");
    }
}

void testEngineScanTiming()
{
    auto p = circuit();
    p[P_kasScanTiming] = 1.0f;
    for (double host : {48000.0, 96000.0})
    {
        Engine e;
        e.prepare(host);
        e.setParams(p);
        const int period = static_cast<int>(std::lround(2.0 * host * kas::kNominalScanSeconds));
        e.noteOn(72, 0.8f); e.noteOn(48, 0.8f); e.noteOn(60, 0.8f); // pushed out of scan order
        check(e.activeVoices() == 0, "a strike waits for the next scan instant");
        render(e, (period + 1) / 2 + 1);
        check(e.activeVoices() == 3, "the strike is acted on within one scan period");
        check(e.getVoiceInfo(0).note == 48 && e.getVoiceInfo(1).note == 60 && e.getVoiceInfo(2).note == 72,
              "one scan assigns circuits in scan order (lowest key first)");
        // Off by default: immediate as before.
        Engine immediate;
        immediate.prepare(host);
        p[P_kasScanTiming] = 0.0f;
        immediate.setParams(p);
        immediate.noteOn(60, 0.8f);
        check(immediate.activeVoices() == 1, "scan timing is opt-in");
        p[P_kasScanTiming] = 1.0f;
        // Legacy ignores the option.
        auto lp = p; lp[P_parityMode] = 0.0f; lp[P_sourcePerformance] = 0.0f;
        Engine legacy; legacy.prepare(host); legacy.setParams(lp);
        legacy.noteOn(60, 0.8f);
        check(legacy.activeVoices() == 1, "Legacy ignores KAS scan timing");
        // Turning the option off flushes queued events at once.
        Engine flush; flush.prepare(host); flush.setParams(p);
        flush.noteOn(64, 0.8f);
        p[P_kasScanTiming] = 0.0f; flush.setParams(p); p[P_kasScanTiming] = 1.0f;
        render(flush, 1);
        check(flush.activeVoices() == 1, "disabling scan timing flushes queued strikes");
    }
}
} // namespace

int main()
{
    testTruthTable();
    testDroopGateAndSensitivity();
    testScanner();
    testEngineHold();
    testAllocation();
    testEngineScanTiming();
    std::printf("KAS scan / sample-hold: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
