// Phase 2.4: TWS a/b/C touch nodes, TRG5 bus and controller curves.
// Part A: realtime model (src/core/TouchCircuit.h) against the independent
// implicit truth model (src/reference/TouchCircuitTruth.h), tolerances
// pre-registered in tests/Ws3Tolerances.h. Part B: the implementation gate of
// docs/INITIAL_TOUCH_CIRCUIT_EVIDENCE.md as engine regressions (destination
// separation, zero-depth neutrality, retrigger, held notes, release, staggered
// polyphony). Part C: interface-layer controller curves. Declared-model checks,
// not hardware comparisons.
#include "core/ControllerCurves.h"
#include "core/Engine.h"
#include "core/TouchCircuit.h"
#include "reference/TouchCircuitTruth.h"
#include "Ws3Tolerances.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace cs80;
namespace tol = cs80::ws3_tolerances;
namespace tt = cs80::reference::touch_truth;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what.c_str()); }
}

void testGate()
{
    double worstB = 0.0, worstC = 0.0, worstA = 0.0, worstPole = 0.0;
    for (double host : {44100.0, 48000.0, 96000.0, 192000.0})
    {
        const double rate = 2.0 * host;
        touch::Coefficients k;
        k.prepare(rate);
        // Strike sequences on one channel: fresh, equal repeat, softer, harder.
        touch::TouchVoice rt;
        tt::TouchVoiceTruth truth;
        rt.reset();
        double t = 0.0;
        for (double strike : {0.8, 0.8, 0.3, 1.0})
        {
            rt.trigger(strike);
            truth.trigger(strike);
            const int length = static_cast<int>(0.4 * rate);
            double peakC = 0.0, peakTime = 0.0;
            for (int i = 0; i < length; ++i)
            {
                const double pressure = i < length / 2 ? 0.6 : 0.0;
                const auto a = rt.tick(k, pressure);
                const auto b = truth.tick(1.0 / rate, pressure);
                worstC = std::max(worstC, std::abs(a.c - b.c));
                worstA = std::max(worstA, std::abs(a.a - b.a) / 0.6);
                if (i > static_cast<int>(touch::kWindowSeconds * rate) + 2)
                    worstB = std::max(worstB, std::abs(a.b - b.b) / std::max(strike, 1e-9));
                if (a.c < peakC) { peakC = a.c; peakTime = i / rate; }
            }
            // C pole: after its peak, C recovers with the 50 ms coupling pole.
            touch::TouchVoice probe;
            probe.trigger(strike);
            double c0 = 0.0;
            const int start = static_cast<int>((touch::kWindowSeconds + 0.005) * rate);
            const int later = start + static_cast<int>(std::lround(touch::kCPathSeconds * rate));
            for (int i = 0; i <= later; ++i)
            {
                const auto o = probe.tick(k, 0.0);
                if (i == start) c0 = o.c;
                if (i == later) worstPole = std::max(worstPole, std::abs(o.c / c0 - std::exp(-1.0)));
            }
            (void) peakTime;
            t += 0.4;
        }
    }
    check(worstB <= tol::kTouchBHoldRelativeErrorMax, "b hold vs truth: " + std::to_string(worstB));
    check(worstC <= tol::kTouchCTransientAbsErrorMax, "C transient vs truth: " + std::to_string(worstC));
    check(worstA <= tol::kTouchASmoothingRelativeErrorMax, "a smoothing vs truth: " + std::to_string(worstA));
    check(worstPole <= tol::kTouchCPoleE1Tolerance, "C recovers e^-1 at 50 ms: " + std::to_string(worstPole));
    std::printf("touch gate: b %.3g, C %.3g, a %.3g, pole %.3g\n", worstB, worstC, worstA, worstPole);

    // TRG5 bus: the realtime per-sample warm-started 2-iteration solve against
    // the converged solution, along slowly varying random pressures.
    std::mt19937 random(99u);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    double worstBus = 0.0;
    for (double host : {48000.0, 96000.0})
    {
        const double rate = 2.0 * host;
        touch::Coefficients k;
        k.prepare(rate);
        std::array<touch::TouchVoice, 8> voices;
        for (auto& v : voices) v.reset();
        std::array<double, 8> target {};
        double warm = 0.0;
        for (int i = 0; i < static_cast<int>(rate * 2.0); ++i)
        {
            if (i % static_cast<int>(rate * 0.05) == 0)
                for (auto& x : target) x = unit(random) < 0.4 ? unit(random) : 0.0;
            std::array<double, 8> a {};
            double top = 0.0;
            for (int v = 0; v < 8; ++v) { a[(std::size_t) v] = voices[(std::size_t) v].tick(k, target[(std::size_t) v]).a; top = std::max(top, a[(std::size_t) v]); }
            warm = top > 0.0 ? touch::busVolts(a, warm, 2) : 0.0;
            worstBus = std::max(worstBus, std::abs(warm - tt::busVolts(a)));
        }
    }
    check(worstBus <= tol::kTrg5BusVoltsAbsErrorMax, "TRG5 bus vs converged truth: " + std::to_string(worstBus) + " V");
    // The diode OR: two keys read close to the larger, never the sum or mean.
    const double one = touch::busVolts(std::array<double, 2> {0.8, 0.0}, 0.0, 64);
    const double both = touch::busVolts(std::array<double, 2> {0.8, 0.5}, 0.0, 64);
    const double sum = touch::busVolts(std::array<double, 1> {1.3}, 0.0, 64);
    check(both >= one && both - one < 0.01 && both < sum, "diode-OR follows the strongest key");
    std::printf("TRG5: full-scale %.4f V, one key at 0.8 -> %.4f V, worst realtime error %.3g V\n",
                touch::fullScaleBusVolts(), one, worstBus);
}

// ---------------------------------------------------------------------------
ParamValues circuit()
{
    ParamValues p;
    p[P_parityMode] = 1.0f;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_detune] = 0.0f;
    for (int r = 0; r < 2; ++r)
    {
        p[chParam(r, CP_initBrill)] = p[chParam(r, CP_initLevel)] = 0.0f;
        p[chParam(r, CP_afterBrill)] = p[chParam(r, CP_afterLevel)] = 0.0f;
        p[chParam(r, CP_vS)] = 1.0f;
        p[chParam(r, CP_fIl)] = p[chParam(r, CP_fAl)] = 0.0f;
    }
    p[P_initBend] = 0.0f;
    return p;
}

std::vector<float> render(Engine& e, int frames)
{
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    for (int i = 0; i < frames; i += 128) e.render(out.data() + static_cast<std::size_t>(i) * 2, std::min(128, frames - i));
    return out;
}

void testEngineGate()
{
    for (double host : {48000.0, 96000.0})
    {
        // (6) zero-depth neutrality: with every touch depth zero, velocity is inaudible.
        {
            Engine a, b;
            a.prepare(host); b.prepare(host);
            a.setParams(circuit()); b.setParams(circuit());
            a.noteOn(60, 0.2f); b.noteOn(60, 1.0f);
            check(render(a, 9600) == render(b, 9600), "zero touch depths: velocity is inaudible");
        }
        // (1) destination separation: initial level uses the held b, the
        // pitch transient uses C; neither leaks into the other.
        {
            auto p = circuit();
            p[chParam(0, CP_initBrill)] = 0.8f;
            Engine e; e.prepare(host); e.setParams(p);
            e.noteOn(60, 0.9f);
            render(e, static_cast<int>(host * 0.02));
            const float early = e.getRankInfo(0, 0).lpfHz;
            render(e, static_cast<int>(host * 1.5));
            const float late = e.getRankInfo(0, 0).lpfHz;
            check(std::abs(late / early - 1.0f) < 1e-4f, "initial brilliance holds b for the whole note");
            check(e.getRankInfo(0, 0).oscillatorHz == midiToHz(60.0f), "no pitch transient without PB depth");
            auto q = circuit(); q[P_initBend] = 1.0f;
            Engine bend; bend.prepare(host); bend.setParams(q);
            bend.noteOn(60, 0.9f);
            render(bend, static_cast<int>(host * 0.015));
            const float onset = 12.0f * std::log2(bend.getRankInfo(0, 0).oscillatorHz / midiToHz(60.0f));
            render(bend, static_cast<int>(host * 0.5));
            const float settled = 12.0f * std::log2(bend.getRankInfo(0, 0).oscillatorHz / midiToHz(60.0f));
            // Expected at 15 ms: -v * exp(-(15 ms - contact) / 50 ms), the 0.33 ms
            // acquisition rise aside.
            const float expected = -0.9f * std::exp(-(0.015f - (float) touch::kContactSeconds) / (float) touch::kCPathSeconds);
            check(std::abs(onset - expected) < 0.02f && std::abs(settled) < 0.002f,
                  "C bends flat at onset and recovers (" + std::to_string(onset) + " vs " + std::to_string(expected) + ")");
            Engine flat; flat.prepare(host); flat.setParams(circuit()); flat.noteOn(60, 0.9f);
            render(flat, static_cast<int>(host * 0.015));
            check(bend.getRankInfo(0, 0).lpfHz == flat.getRankInfo(0, 0).lpfHz,
                  "the pitch transient does not move the filters");
        }
        // (6) retrigger: an equal repeat strike on the same circuit gives the
        // same onset as a fresh strike (the clamp resets the coupling memory).
        {
            auto q = circuit(); q[P_initBend] = 1.0f;
            Engine e; e.prepare(host); e.setParams(q);
            for (int k = 0; k < 8; ++k) { e.noteOn(60 + k, 0.8f); render(e, 64); e.noteOff(60 + k); }
            render(e, static_cast<int>(host * 0.3));
            e.noteOn(72, 0.8f); // voice 0 again, b already 0.8 from its last strike
            render(e, static_cast<int>(host * 0.015));
            const float repeat = 12.0f * std::log2(e.getRankInfo(0, 0).oscillatorHz / midiToHz(72.0f));
            Engine fresh; fresh.prepare(host); fresh.setParams(q);
            fresh.noteOn(72, 0.8f);
            render(fresh, static_cast<int>(host * 0.015));
            const float first = 12.0f * std::log2(fresh.getRankInfo(0, 0).oscillatorHz / midiToHz(72.0f));
            check(std::abs(repeat - first) < 0.02f, "equal repeat strike reproduces the fresh onset ("
                  + std::to_string(repeat) + " vs " + std::to_string(first) + ")");
        }
        // (6) release: aftertouch decays with the 100 ms a path after the key lifts.
        {
            auto p = circuit(); p[chParam(0, CP_afterBrill)] = 0.8f;
            Engine e; e.prepare(host); e.setParams(p);
            e.noteOn(60, 0.5f); e.polyPressure(60, 1.0f);
            render(e, static_cast<int>(host * 0.5));
            const float pressed = e.getRankInfo(0, 0).lpfHz;
            e.noteOff(60);
            render(e, static_cast<int>(host * 0.1));
            const float after = e.getRankInfo(0, 0).lpfHz;
            check(after < pressed, "aftertouch brilliance relaxes after release");
        }
        // (3) acquisition sequence: b is at rest until the declared contact
        // instant, so initial brilliance arrives a few ms after TR.
        {
            auto p = circuit();
            p[chParam(0, CP_initBrill)] = 0.8f;
            Engine e; e.prepare(host); e.setParams(p);
            e.noteOn(60, 0.9f);
            render(e, static_cast<int>(host * 0.001));
            const float atContact = e.getRankInfo(0, 0).lpfHz;
            render(e, static_cast<int>(host * 0.02));
            check(atContact < e.getRankInfo(0, 0).lpfHz * 0.8f, "initial touch arrives after the contact lead");
        }
        // TRG5: a light touch below the 1S1555 drop does not reach the shared
        // SUB speed bus; a firm one does (circuit engine, original SUB).
        {
            auto p = circuit();
            p[P_subOriginalMode] = 1.0f; p[P_subAtSpeed] = 1.0f;
            auto rate = [&](float pressure) {
                Engine e; e.prepare(host); e.setParams(p);
                e.noteOn(60, 0.5f); e.polyPressure(60, pressure);
                render(e, static_cast<int>(host * 0.8));
                return e.getSubOscillatorRate();
            };
            const float none = rate(0.0f), light = rate(0.04f), firm = rate(1.0f);
            // The former linear maximum would have added this much at 0.04.
            const float linear = OriginalSubOscillator::targetRate(4.2f, 1.0f, 0.04f)
                               - OriginalSubOscillator::targetRate(4.2f, 1.0f, 0.0f);
            check(light - none < 0.1f * linear && firm > none * 1.2f,
                  "TRG5 diode drop: light pressure inert, firm pressure speeds the SUB ("
                  + std::to_string(none) + ", " + std::to_string(light) + ", " + std::to_string(firm) + ")");
        }
        // (6) staggered polyphony: each circuit keeps its own b.
        {
            auto p = circuit(); p[chParam(0, CP_initLevel)] = 0.6f;
            Engine e; e.prepare(host); e.setParams(p);
            e.noteOn(60, 0.2f); render(e, 4800); e.noteOn(64, 1.0f); render(e, 4800);
            check(e.getVoiceInfo(0).velocity == 0.2f && e.getVoiceInfo(1).velocity == 1.0f, "independent strikes");
        }
    }
}

void testCurves()
{
    bool identity = true;
    for (int i = 0; i <= 1000; ++i)
    {
        const float x = i / 1000.0f;
        identity = identity && applyVelocityCurve(x, 0.0f) == x && applyPressureCurve(x, 0.0f, 0.0f) == x;
    }
    check(identity, "default curves are the exact identity");
    check(applyVelocityCurve(0.5f, 1.0f) > 0.5f && applyVelocityCurve(0.5f, -1.0f) < 0.5f,
          "positive curve lifts light playing, negative needs harder playing");
    check(applyVelocityCurve(0.0f, 0.7f) == 0.0f && applyVelocityCurve(1.0f, -0.7f) == 1.0f, "curve endpoints fixed");
    check(applyPressureCurve(0.1f, 0.0f, 0.2f) == 0.0f && applyPressureCurve(1.0f, 0.0f, 0.2f) == 1.0f
              && std::abs(applyPressureCurve(0.6f, 0.0f, 0.2f) - 0.5f) < 1e-6f,
          "pressure threshold removes resting load and rescales");
    bool monotone = true;
    float prev = -1.0f;
    for (int i = 0; i <= 1000; ++i)
    {
        const float y = applyVelocityCurve(i / 1000.0f, 0.4f);
        monotone = monotone && y >= prev;
        prev = y;
    }
    check(monotone, "curves are monotone");
    // Engine: the default render is unchanged; a curve changes the strike.
    ParamValues p;
    Engine a; a.prepare(48000.0); a.setParams(p); a.noteOn(60, 0.5f);
    check(a.getVoiceInfo(0).velocity == 0.5f, "default velocity curve leaves velocity alone");
    p[P_velocityCurve] = 1.0f;
    Engine b; b.prepare(48000.0); b.setParams(p); b.noteOn(60, 0.5f);
    check(std::abs(b.getVoiceInfo(0).velocity - std::pow(0.5f, 0.25f)) < 1e-6f, "velocity curve applied at the MIDI input");
}
} // namespace

int main()
{
    testGate();
    testEngineGate();
    testCurves();
    std::printf("Touch circuit: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
