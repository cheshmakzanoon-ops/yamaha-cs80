// Phase 3 output listening modes: GENERAL mono jack and the headphone-amplifier
// model, against the output network and src/reference/HeadphoneAmp.h.
// Numerical contracts, not measurements of an instrument.
#include "core/Engine.h"
#include "reference/HeadphoneAmp.h"
#include "Ws2EngineTolerances.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

using namespace cs80;
namespace ref = cs80::reference::headphone;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
constexpr double pi = 3.14159265358979323846;

ref::Profile toReference(const HeadphoneAmpCircuit::Profile& p)
{
    ref::Profile r;
    r.coupling = p.coupling == HeadphoneAmpCircuit::Coupling::direct ? ref::Coupling::direct : ref::Coupling::capacitor;
    r.loadOhms = p.loadOhms;
    return r;
}

void testHeadphoneStage()
{
    for (auto coupling : { HeadphoneAmpCircuit::Coupling::direct, HeadphoneAmpCircuit::Coupling::capacitor })
        for (double load : { 8.0, 32.0, 300.0, 600.0 })
            for (double rate : { 88200.0, 192000.0, 384000.0 })
            {
                HeadphoneAmpCircuit::Profile p;
                p.coupling = coupling;
                p.loadOhms = load;
                const auto r = toReference(p);
                double worstDb = 0, worstDeg = 0;
                for (double f : { 20.0, 50.0, 100.0, 1000.0, 5000.0, 10000.0, 20000.0 })
                {
                    HeadphoneAmpCircuit amp;
                    amp.prepare(rate, p);
                    const double a = 0.1; // small signal, below the rail knee
                    const int settle = (int) (rate * 1.5), n = (int) (rate * 0.5);
                    double re = 0, im = 0;
                    for (int i = 0; i < settle + n; ++i)
                    {
                        const double x = a * std::sin(2 * pi * f * i / rate);
                        const double y = amp.process(x);
                        if (i < settle) continue;
                        re += y * std::sin(2 * pi * f * i / rate);
                        im += y * std::cos(2 * pi * f * i / rate);
                    }
                    const std::complex<double> measured(2 * re / n / a, 2 * im / n / a);
                    const auto expected = ref::response(f, r);
                    worstDb = std::max(worstDb, std::abs(20 * std::log10(std::abs(measured) / std::abs(expected))));
                    worstDeg = std::max(worstDeg, std::abs(std::arg(measured / expected)) * 180 / pi);
                }
                if (load == 32.0)
                    std::printf("phones %s %3.0f ohm @%6.0f: |H(1k)| %.4f (%+.2f dB), worst %.4f dB %.3f deg\n",
                                coupling == HeadphoneAmpCircuit::Coupling::direct ? "direct   " : "capacitor", load, rate,
                                std::abs(ref::response(1000.0, r)), 20 * std::log10(std::abs(ref::response(1000.0, r))),
                                worstDb, worstDeg);
                check(worstDb <= ws2::kHeadphoneResponseDb && worstDeg <= ws2::kHeadphonePhaseDegrees,
                      "headphone stage response follows the reference");
            }
    // Static large-signal transfer through the rail knee (direct coupling).
    HeadphoneAmpCircuit amp;
    amp.prepare(96000.0, {});
    double worst = 0;
    for (int i = -400; i <= 400; ++i)
    {
        const double u = i / 200.0;
        worst = std::max(worst, std::abs(amp.process(u) - ref::staticTransfer(u, ref::Profile {})));
    }
    check(worst <= ws2::kHeadphoneStaticError, "headphone large-signal transfer follows the reference");
    check(std::abs(ref::staticTransfer(2.0, ref::Profile {})) <= 32.0 / 54.0 + 1e-12,
          "phones limit at the shared rail ceiling");
}

ParamValues patch(float listening, bool isolation = false)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f;
    p[P_parityMode] = isolation ? 1.0f : 0.0f;
    p[P_outputJack] = listening;
    p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = 0.0f;
    p[P_chorusOn] = 1.0f;
    return p;
}

void testEngineSelection()
{
    for (bool isolation : { false, true })
    {
        // Selection reads the existing always-clocked nodes exactly.
        for (float listening : { 0.0f, 1.0f, 2.0f })
        {
            Engine e;
            e.prepare(48000.0);
            e.setParams(patch(listening, isolation));
            e.noteOn(48, 0.9f);
            e.noteOn(64, 0.7f);
            std::vector<float> one(2);
            bool exact = true;
            for (int i = 0; i < 3000; ++i)
            {
                e.render(one.data(), 1);
                const auto s = e.getListeningSample();
                const auto general = (float) e.getOutputTapState(2).nodes.audibleJackVolts;
                const auto left = (float) e.getOutputTapState(0).nodes.audibleJackVolts;
                const auto right = (float) e.getOutputTapState(1).nodes.audibleJackVolts;
                if (listening == 1.0f) exact = exact && s.left == general && s.right == general;
                if (listening == 0.0f) exact = exact && s.left == left && s.right == right;
            }
            if (listening != 2.0f)
                check(exact && ws2::kGeneralNodeError == 0.0,
                      listening == 1.0f ? "GENERAL listening is the GENERAL node exactly"
                                        : "LEFT/RIGHT listening is unchanged");
        }

        // GENERAL is mono and 0.94 of an equal stereo pair (ideal active sum).
        auto render = [&](float listening)
        {
            auto p = patch(listening, isolation);
            p[P_chorusOn] = 0.0f;
            p[P_mix] = 0.0f;
            Engine e;
            e.prepare(48000.0);
            e.setParams(p);
            e.noteOn(57, 0.8f);
            std::vector<float> audio(2 * 24000);
            e.render(audio.data(), 24000);
            return audio;
        };
        const auto stereo = render(0.0f), general = render(1.0f), phones = render(2.0f);
        double gl = 0, sl = 0, pl = 0;
        bool mono = true;
        for (std::size_t i = 2 * 8000; i < stereo.size(); i += 2)
        {
            mono = mono && general[i] == general[i + 1];
            gl += general[i] * (double) general[i];
            sl += stereo[i] * (double) stereo[i];
            pl += phones[i] * (double) phones[i];
        }
        const double generalDb = 10 * std::log10(gl / sl), phonesDb = 10 * std::log10(pl / sl);
        // The LEFT jack itself is a loaded divider (HIGH, 100 ohm source, 100 k load).
        OutputTapCircuit jack;
        jack.prepare(96000.0, OutputTapCircuit::Profile {}, OutputTapCircuit::Mode::high);
        const double leftGain = jack.selectedCoefficients().jackGain;
        std::printf("%s: GENERAL %+.3f dB, PHONES %+.3f dB relative to LEFT\n",
                    isolation ? "PURE CS-80" : "circuit   ", generalDb, phonesDb);
        check(mono, "GENERAL feeds both host channels");
        check(std::abs(generalDb - 20 * std::log10(0.94)) < 0.05, "GENERAL is the 0.94 active sum of an equal pair");
        check(std::abs(phonesDb - 20 * std::log10(1.5 * 32.0 / 54.0 / leftGain)) < 0.05,
              "PHONES: -1.5 gain into the 22 ohm / 32 ohm divider, from the same preamp source");
    }

    // Legacy (compat) keeps its historical output arithmetic for every choice.
    std::vector<float> reference;
    for (float listening : { 0.0f, 1.0f, 2.0f })
    {
        auto p = patch(listening);
        p[P_sourcePerformance] = 0.0f;
        Engine e;
        e.prepare(48000.0);
        e.setParams(p);
        e.noteOn(60, 0.8f);
        std::vector<float> audio(2 * 4096);
        e.render(audio.data(), 4096);
        if (reference.empty()) reference = audio;
        check(audio == reference, "legacy ignores the listening output");
    }
    check(paramDef(P_outputJack).def == 0.0f, "LEFT/RIGHT is the default listening output");
}
} // namespace

int main()
{
    testHeadphoneStage();
    testEngineSelection();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
