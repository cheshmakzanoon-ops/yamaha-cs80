// Release-colour contracts for the existing piano/tine/reed bodies. These
// establish routing, spectral loss, continuity and timing; they do not claim
// that authored damping constants match a measured acoustic instrument.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
constexpr double pi = 3.1415926535897932384626433832795;
void check(bool condition, const std::string& message)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}
double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
double amplitude(const std::vector<float>& audio, double frequency, double rate,
                 int first, int frames, int stride = 1)
{
    double real = 0.0, imaginary = 0.0, weight = 0.0;
    for (int n = 0; n < frames; ++n)
    {
        const double window = 0.5 - 0.5 * std::cos(2.0 * pi * n / (frames - 1));
        const double phase = 2.0 * pi * frequency * (first + n) / rate;
        const double value = audio[static_cast<std::size_t>(stride * (first + n))];
        real += window * value * std::cos(phase);
        imaginary += window * value * std::sin(phase);
        weight += window;
    }
    return 2.0 * std::hypot(real, imaginary) / weight;
}
double spectralRatio(const std::vector<float>& audio, int note, double rate,
                     double start, double length, int stride = 1)
{
    const int first = static_cast<int>(rate * start), frames = static_cast<int>(rate * length);
    return amplitude(audio, 3.0 * hz(note), rate, first, frames, stride)
         / std::max(1.0e-20, amplitude(audio, hz(note), rate, first, frames, stride));
}
body::InstrumentBody strike(int model, double rate, int note = 60,
                            int material = body::defaultMaterial)
{
    body::InstrumentBody result;
    result.prepare(rate, rate);
    result.noteOn(model, static_cast<float>(note), 0.85f, 0.75f, 1.0f,
                  0x614c8ce0u, 1.0f, body::standard, material);
    return result;
}
std::vector<float> render(body::InstrumentBody& instrument, double rate, int note, double seconds)
{
    std::vector<float> result(static_cast<std::size_t>(rate * seconds));
    for (auto& sample : result) sample = instrument.tick(static_cast<float>(hz(note)), 0.75f);
    return result;
}

void spectralReleaseAndRates()
{
    for (int model : {body::piano, body::tine, body::reed})
    {
        std::array<double, 4> colourChanges {};
        int row = 0;
        for (double rate : {32000.0, 44100.0, 48000.0, 96000.0})
        {
            auto held = strike(model, rate);
            (void) render(held, rate, 60, 0.2);
            auto released = held;
            released.noteOff(0.6f);
            const auto a = render(held, rate, 60, 0.35);
            const auto b = render(released, rate, 60, 0.35);
            const int first = static_cast<int>(0.15 * rate), frames = static_cast<int>(0.12 * rate);
            const double fundamental = amplitude(b, hz(60), rate, first, frames)
                                     / amplitude(a, hz(60), rate, first, frames);
            const double colour = spectralRatio(b, 60, rate, 0.15, 0.12)
                                / spectralRatio(a, 60, rate, 0.15, 0.12);
            colourChanges[static_cast<std::size_t>(row++)] = colour;
            check(std::abs(fundamental - 1.0) < 0.003,
                  std::string(body::name(model)) + ": body release leaves fundamental damping with the rank VCA");
            check(colour > 0.02 && colour < 0.80,
                  std::string(body::name(model)) + ": release removes additional upper-partial energy");
            check(std::abs(static_cast<double>(a.front()) - b.front()) < 0.00001,
                  "first released sample has no damper amplitude step");
            bool finite = true;
            double peak = 0.0;
            for (float sample : b) { finite = finite && std::isfinite(sample); peak = std::max(peak, std::abs(static_cast<double>(sample))); }
            check(finite && peak < 0.80, "released modal output stays finite and inside its excitation bound");
            std::printf("DAMPER %s rate=%.0f fundamental=%.8f upper/held=%.8f\n",
                        body::name(model), rate, fundamental, colour);
        }
        const auto [small, large] = std::minmax_element(colourChanges.begin(), colourChanges.end());
        check(*large - *small < 0.012,
              std::string(body::name(model)) + ": release spectral loss agrees in seconds across sample rates");
    }
}

void isolationAndLifetime()
{
    for (int model = body::off; model <= body::maxModel; ++model)
    {
        if (model == body::piano || model == body::tine || model == body::reed) continue;
        auto held = strike(model, 48000.0);
        (void) render(held, 48000.0, 60, 0.05);
        auto released = held;
        released.noteOff(0.01f);
        released.setRelease(2.0f);
        check(render(held, 48000.0, 60, 0.07) == render(released, 48000.0, 60, 0.07),
              std::string(body::name(model)) + ": unsupported body is sample-identical after damper calls");
    }
    for (const auto pair : {std::array<int, 3> {body::tine, body::kalimba, 60},
                           std::array<int, 3> {body::piano, body::defaultMaterial, 96}})
    {
        auto held = strike(pair[0], 48000.0, pair[2], pair[1]);
        auto released = held;
        released.noteOff(0.01f);
        check(render(held, 48000.0, pair[2], 0.07) == render(released, 48000.0, pair[2], 0.07),
              "kalimba and undamped high piano registers retain exact held modal losses");
    }
    for (int model : {body::piano, body::tine, body::reed})
    {
        auto a = strike(model, 48000.0), b = a;
        a.setRelease(0.025f);
        check(render(a, 48000.0, 60, 0.12) == render(b, 48000.0, 60, 0.12),
              "live release timing cannot damp a held body");
        a.noteOff(0.6f); b.noteOff(0.6f);
        (void) render(a, 48000.0, 60, 0.017);
        (void) render(b, 48000.0, 60, 0.017);
        b.noteOff(0.6f);
        check(render(a, 48000.0, 60, 0.08) == render(b, 48000.0, 60, 0.08),
              "duplicate accepted release neither resets contact nor compounds damping");
        auto copied = a;
        check(render(a, 48000.0, 60, 0.04) == render(copied, 48000.0, 60, 0.04),
              "voice handoff copies the complete damper state exactly");
        a.noteOn(model, 60.0f, 0.85f, 0.75f, 1.0f, 0x614c8ce0u);
        auto fresh = strike(model, 48000.0);
        check(render(a, 48000.0, 60, 0.12) == render(fresh, 48000.0, 60, 0.12),
              "accepted restrike resets an outgoing damper without inheriting its loss");
    }
}

void liveTimingAndPitch()
{
    for (int model : {body::piano, body::tine, body::reed})
    {
        auto seed = strike(model, 48000.0);
        (void) render(seed, 48000.0, 60, 0.2);
        auto fast = seed, longer = seed;
        fast.noteOff(0.3f); longer.noteOff(0.3f);
        check(render(fast, 48000.0, 60, 0.06) == render(longer, 48000.0, 60, 0.06),
              "live timing fixture begins with identical accumulated losses");
        longer.setRelease(2.0f);
        const auto a = render(fast, 48000.0, 60, 0.25);
        const auto b = render(longer, 48000.0, 60, 0.25);
        check(std::abs(static_cast<double>(a.front()) - b.front()) < 0.00001,
              "live release edit does not step the body waveform");
        check(spectralRatio(b, 60, 48000.0, 0.12, 0.10)
                > 1.25 * spectralRatio(a, 60, 48000.0, 0.12, 0.10),
              "longer live release slows subsequent spectral damping without restoring energy");

        // A changing sounding frequency must remain a phase-continuous pitch
        // input. Isolate its fundamental with a long analysis window after
        // the existing control-rate coefficient transition has completed.
        auto held = seed, released = seed;
        released.noteOff(0.6f);
        std::vector<float> h(12000), r(12000);
        const double bentHz = hz(60) * std::exp2(3.0 / 12.0);
        for (int n = 0; n < 12000; ++n)
        {
            h[static_cast<std::size_t>(n)] = held.tick(static_cast<float>(bentHz), 0.75f);
            r[static_cast<std::size_t>(n)] = released.tick(static_cast<float>(bentHz), 0.75f);
        }
        double strongestHeld = 0.0, strongestReleased = 0.0, heldHz = 0.0, releasedHz = 0.0;
        for (int n = -12; n <= 12; ++n)
        {
            const double frequency = bentHz + 0.05 * n;
            const double ah = amplitude(h, frequency, 48000.0, 1200, 10800);
            const double ar = amplitude(r, frequency, 48000.0, 1200, 10800);
            if (ah > strongestHeld) { strongestHeld = ah; heldHz = frequency; }
            if (ar > strongestReleased) { strongestReleased = ar; releasedHz = frequency; }
        }
        check(heldHz == releasedHz, "release retains the same spectral fundamental under a live pitch bend");
    }
}

ParamValues fixture(int model)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f;
    p[P_parityMode] = p[P_kasScanTiming] = p[P_analogNoise] = 0.0f;
    p[P_drift] = p[P_voiceDispersion] = p[P_chorusOn] = 0.0f;
    p[P_kbdBrillianceLow] = p[P_kbdBrillianceHigh] = 0.0f;
    p[P_kbdLevelLow] = p[P_kbdLevelHigh] = 0.0f;
    p[P_mix] = p[P_detune] = p[P_portamento] = p[P_initBend] = 0.0f;
    p[P_subVco] = p[P_subAtVco] = p[P_subVcf] = p[P_subAtVcf] = p[P_subVca] = 0.0f;
    p[P_volume] = 0.2f;
    p[P_masterSustain] = p[P_keyRelease] = 0.0f;
    p[P_body1Model] = static_cast<float>(model);
    p[P_body1Mix] = 1.0f;
    p[P_body1Brightness] = 0.75f;
    p[P_body1Decay] = 1.0f;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_hpfOn)] = p[chParam(rank, CP_lpfOn)] = 0.0f;
        p[chParam(rank, CP_feet)] = 1.0f;
        p[chParam(rank, CP_vA)] = 0.003f;
        p[chParam(rank, CP_vD)] = 1.0f;
        p[chParam(rank, CP_vS)] = 1.0f;
        p[chParam(rank, CP_vR)] = 0.6f;
        p[chParam(rank, CP_fIl)] = p[chParam(rank, CP_fAl)] = 0.0f;
        p[chParam(rank, CP_initBrill)] = p[chParam(rank, CP_afterBrill)] = 0.0f;
        p[chParam(rank, CP_initLevel)] = p[chParam(rank, CP_afterLevel)] = 0.0f;
        p[chParam(rank, CP_saw)] = p[chParam(rank, CP_pulse)] = 0.0f;
        p[chParam(rank, CP_sine)] = p[chParam(rank, CP_noise)] = 0.0f;
    }
    return p;
}
std::unique_ptr<Engine> engine(const ParamValues& params)
{
    auto result = std::make_unique<Engine>();
    result->setParams(params);
    result->prepare(48000.0);
    return result;
}
std::vector<float> render(Engine& e, int frames, int block = 113)
{
    std::vector<float> result(static_cast<std::size_t>(2 * frames));
    for (int at = 0; at < frames; at += block)
        e.render(result.data() + 2 * at, std::min(block, frames - at));
    return result;
}
void engineGateAndTiming()
{
    for (int model : {body::piano, body::tine, body::reed})
    {
        auto p = fixture(model);
        auto pedal = engine(p), held = engine(p);
        pedal->noteOn(60, 0.85f, 2); held->noteOn(60, 0.85f, 2);
        check(render(*pedal, 4800) == render(*held, 4800), "pedal pair begins identically");
        pedal->controlChange(64, 1.0f, 2); pedal->noteOff(60, 2);
        check(render(*pedal, 4800) == render(*held, 4800), "CC64 key-up cannot begin damper contact early");
        pedal->controlChange(64, 0.0f, 3);
        check(render(*pedal, 1200) == render(*held, 1200), "another channel's pedal-up cannot damp the held body");
        pedal->controlChange(64, 0.0f, 2); held->noteOff(60, 2);
        check(render(*pedal, 7200, 7200) == render(*held, 7200, 17),
              "accepted pedal-up and key-up release identically across host block partitions");

        auto shortParams = fixture(model), longParams = shortParams;
        longParams[chParam(0, CP_vR)] = 3.0f;
        auto shortRelease = engine(shortParams), longRelease = engine(longParams);
        shortRelease->noteOn(60, 0.85f); longRelease->noteOn(60, 0.85f);
        check(render(*shortRelease, 9600) == render(*longRelease, 9600),
              "release duration never changes the held excitation");
        shortRelease->noteOff(60); longRelease->noteOff(60);
        const auto a = render(*shortRelease, 7200), b = render(*longRelease, 7200);
        const double fast = spectralRatio(a, 60, 48000.0, 0.06, 0.07, 2);
        const double slow = spectralRatio(b, 60, 48000.0, 0.06, 0.07, 2);
        check(fast < slow * 0.90, "engine release duration reaches the body's spectral damping");
        std::printf("DAMPER ENGINE %s fast/slow normalized upper=%.8f\n", body::name(model), fast / slow);

        // Equivalent explicit performance extensions own the same timing;
        // body damping must neither cap nor bypass them.
        for (bool master : {false, true})
        {
            auto extension = fixture(model);
            extension[chParam(0, CP_vR)] = 0.05f;
            extension[master ? P_masterSustain : P_keyRelease] = master ? 0.3f : 3.0f;
            auto extended = engine(extension), rankRelease = engine(longParams);
            extended->noteOn(60, 0.85f); rankRelease->noteOn(60, 0.85f);
            (void) render(*extended, 4800); (void) render(*rankRelease, 4800);
            extended->noteOff(60); rankRelease->noteOff(60);
            check(render(*extended, 4800) == render(*rankRelease, 4800),
                  "master sustain/key release preserves equivalent full rank release timing");
        }

        // Changing the next-note model must not retime the already sounding
        // body's captured damper family, even while its release is edited.
        auto unchanged = engine(p), changed = engine(p);
        unchanged->noteOn(60, 0.85f); changed->noteOn(60, 0.85f);
        (void) render(*unchanged, 4800); (void) render(*changed, 4800);
        unchanged->noteOff(60); changed->noteOff(60);
        (void) render(*unchanged, 960); (void) render(*changed, 960);
        p[chParam(0, CP_vR)] = 2.0f;
        unchanged->setParams(p);
        p[P_body1Model] = static_cast<float>(body::organ);
        changed->setParams(p);
        check(render(*unchanged, 4800) == render(*changed, 4800),
              "live release edits retain the accepted body's damper after a next-note model change");

        // Eight sounding slots force the next onset to reuse this particular
        // released body. Release the new key before its 3 ms handoff finishes.
        p = fixture(model);
        auto whole = engine(p), split = engine(p);
        for (int note : {48, 52, 55, 60, 64, 67, 71, 74})
        {
            whole->noteOn(note, 0.65f);
            split->noteOn(note, 0.65f);
        }
        check(render(*whole, 4800, 4800) == render(*split, 4800, 29),
              "handoff fixture fills identical sounding bodies");
        whole->noteOff(48); split->noteOff(48);
        check(render(*whole, 720, 720) == render(*split, 720, 17),
              "outgoing body accumulates identical damper contact");
        whole->noteOn(79, 0.85f); split->noteOn(79, 0.85f);
        check(render(*whole, 48, 48) == render(*split, 48, 7),
              "a reused circuit preserves the outgoing body and its release state");
        whole->noteOff(79); split->noteOff(79);
        check(render(*whole, 960, 960) == render(*split, 960, 13),
              "new key release during body handoff is sample-partition invariant");

        // Both operations remove the same performance-sustain term. One
        // enters the footswitch cancellation route; the other is a live
        // master-control edit, so both body timing integration sites matter.
        p = fixture(model);
        p[P_masterSustain] = 0.3f;
        p[P_footswitchPlugged] = p[P_footswitchPressed] = p[P_sustainAssign] = 1.0f;
        auto footswitch = engine(p), master = engine(p);
        footswitch->noteOn(60, 0.85f); master->noteOn(60, 0.85f);
        (void) render(*footswitch, 4800); (void) render(*master, 4800);
        footswitch->noteOff(60); master->noteOff(60);
        check(render(*footswitch, 1440) == render(*master, 1440),
              "performance-sustain fixture has identical accumulated body loss");
        auto opened = p;
        opened[P_footswitchPressed] = 0.0f;
        footswitch->setParams(opened);
        p[P_masterSustain] = 0.0f;
        master->setParams(p);
        check(render(*footswitch, 4800) == render(*master, 4800),
              "footswitch cancellation and live master sustain both retime body damping continuously");
    }
}

void damperBoundaryRetimingAndReuse()
{
    for (int model : {body::piano, body::tine, body::reed})
    {
        auto firstPending = strike(model, 96000.0, 36);
        firstPending.noteOff(0.7f);
        firstPending.setRelease(0.25f);
        auto firstPendingCopy = firstPending;
        check(render(firstPending, 96000.0, 36, 0.002) == render(firstPendingCopy, 96000.0, 36, 0.002),
              "first pending release copies before any geometry or target is consumed");
        auto everySample = strike(model, 96000.0, 36);
        (void) render(everySample, 96000.0, 36, 16.0 / 96000.0);
        auto atBoundary = everySample;
        everySample.noteOff(0.4f); atBoundary.noteOff(0.4f);
        bool same = true;
        for (int n = 0; n < 2048; ++n)
        {
            const float seconds = 0.025f + 1.75f * static_cast<float>((n * 17) % 257) / 256.0f;
            everySample.setRelease(seconds);
            // The two inputs agree at every consumption boundary. Intermediate
            // timing values must not alter energy, phase or contact progress.
            if ((n & 15) == 0) atBoundary.setRelease(seconds);
            const float frequency = static_cast<float>(hz(36) * (n < 777 ? 1.0 : 1.122462048));
            same = everySample.tick(frequency, 0.75f) == atBoundary.tick(frequency, 0.75f) && same;
        }
        check(same, "release retargets consume the latest exact value at the existing control boundary");

        // Leave a pending retarget between boundaries, then return A->B->A.
        (void) everySample.tick(static_cast<float>(hz(36)), 0.75f);
        everySample.setRelease(0.7f);
        auto copied = everySample;
        everySample.setRelease(0.2f); everySample.setRelease(0.7f);
        check(everySample.tick(std::numeric_limits<float>::quiet_NaN(), 0.75f) == 0.0f
              && everySample.tick(0.0f, 0.75f) == 0.0f,
              "invalid frequency cannot consume a pending damper target");
        check(render(everySample, 96000.0, 36, 0.002) == render(copied, 96000.0, 36, 0.002),
              "pending damper state copies exactly and superseded targets leave no history");
    }

    for (double rate : {48000.0, 96000.0})
        for (int model : {body::off, body::piano, body::tine, body::reed})
            for (int note : {0, 36, 84, 91, 96})
                for (float size : {0.5f, 2.5f})
                {
                    auto reused = strike(body::piano, rate, 12);
                    reused.noteOff(0.025f);
                    (void) render(reused, rate, 12, 0.003);
                    // Different mode count, ratios, damper family and strength
                    // must replace the outgoing geometry on the next strike.
                    body::InstrumentBody fresh;
                    fresh.prepare(rate, rate);
                    for (auto* b : {&reused, &fresh})
                        b->noteOn(model, static_cast<float>(note), 0.85f, 0.75f,
                                  1.0f, 0x614c8ce0u, size);
                    reused.noteOff(0.6f); fresh.noteOff(0.6f);
                    bool same = true;
                    for (int n = 0; n < 1024; ++n)
                    {
                        const float seconds = 0.025f + 1.3f * static_cast<float>((n * 13) % 193) / 192.0f;
                        reused.setRelease(seconds); fresh.setRelease(seconds);
                        const float frequency = static_cast<float>(hz(note) * (n < 333 ? 1.0 : 0.840896415));
                        const float a = reused.tick(frequency, 0.75f), b = fresh.tick(frequency, 0.75f);
                        same = a == b && std::isfinite(a) && same;
                    }
                    check(same, "released restrike replaces cached damper geometry and timing exactly");
                }
}
}

int main()
{
    spectralReleaseAndRates();
    isolationAndLifetime();
    liveTimingAndPitch();
    engineGateAndTiming();
    damperBoundaryRetimingAndReuse();
    std::printf("Instrument damper: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
