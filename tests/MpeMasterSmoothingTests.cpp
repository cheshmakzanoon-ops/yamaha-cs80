// Software controller contract: both MPE masters use the circuit engine's
// declared 5 ms bend smoother. This is not a claim about original hardware.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

using namespace cs80;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}

ParamValues controls(float policy, float quality)
{
    ParamValues p;
    p[P_sourcePerformance] = policy;
    p[P_oversampling] = quality;
    p[P_mpeOn] = 1.0f;
    p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = p[P_chorusOn] = 0.0f;
    p[P_detune] = p[P_initBend] = 0.0f;
    return p;
}

void startZones(Engine& e)
{
    const auto rpn = [&](int channel, int selector, int value)
    {
        e.controlChange(101, 0.0f, channel);
        e.controlChange(100, selector / 127.0f, channel);
        e.controlChange(6, value / 127.0f, channel);
    };
    rpn(1, 6, 7);
    rpn(16, 6, 7);
    rpn(1, 0, 12);
    rpn(16, 0, 12);
    e.noteOn(60, 0.8f, 3);
    e.noteOn(60, 0.8f, 10);
}

void render(Engine& e, int samples)
{
    std::array<float, 256> audio {};
    while (samples > 0)
    {
        const int count = std::min(samples, 128);
        e.render(audio.data(), count);
        samples -= count;
    }
}

double pitchDelta(const Engine& e, int voice, double reference)
{
    return 12.0 * std::log2(e.getRankInfo(voice, 0).oscillatorHz / reference);
}

void testTrajectory(double rate, float quality, bool source)
{
    Engine e;
    e.prepare(rate);
    e.setParams(controls(source ? 1.0f : 0.0f, quality));
    startZones(e);
    render(e, 32); // End the startup snap window before sending any bend.
    const std::array<double, 2> reference {
        e.getRankInfo(0, 0).oscillatorHz, e.getRankInfo(1, 0).oscillatorHz };
    std::array<double, 2> current {};
    // Independent host-time oracle: the internal one-pole advances N times
    // per host sample, whose exact composition is exp(-1 / (tau * hostRate)).
    const double decay = std::exp(-1.0 / (0.005 * rate));
    struct Step { std::array<double, 2> target; double seconds; };
    const std::array<Step, 3> steps {{
        { { 0.0, 12.0 }, 0.003 }, // Upper-only movement must not move the lower zone.
        { { -6.0, -12.0 }, 0.007 }, // Reverse the upper master before it settles.
        { { 6.0, 6.0 }, 0.105 }, // Both must reach and retain the event-set target.
    }};
    for (const auto& step : steps)
    {
        e.pitchBend(1, (float) (step.target[0] / 12.0));
        e.pitchBend(16, (float) (step.target[1] / 12.0));
        std::array<double, 2> worst {};
        for (int n = 0; n < (int) std::ceil(step.seconds * rate); ++n)
        {
            render(e, 1);
            for (int voice = 0; voice < 2; ++voice)
            {
                current[voice] = source ? step.target[voice] + (current[voice] - step.target[voice]) * decay
                                        : step.target[voice];
                const double error = std::abs(pitchDelta(e, voice, reference[voice]) - current[voice]);
                worst[voice] = std::isfinite(error) ? std::max(worst[voice], error)
                                                  : std::numeric_limits<double>::infinity();
            }
        }
        std::printf("master bend %s %.0f Hz q%.0f -> (%g,%g): max pitch error %.9g / %.9g semitones\n",
                    source ? "circuit" : "legacy", rate, quality, step.target[0], step.target[1],
                    worst[0], worst[1]);
        check(worst[0] < 2.0e-5, "lower master follows the declared time-domain trajectory");
        check(worst[1] < 2.0e-5, "upper master follows the same declared time-domain trajectory");
    }
}

void testSnapAndReset()
{
    Engine e;
    e.prepare(48000.0);
    e.setParams(controls(1.0f, 0.0f));
    startZones(e);
    e.pitchBend(1, 0.5f);
    e.pitchBend(16, 1.0f);
    render(e, 1);
    const double lower = e.getRankInfo(0, 0).oscillatorHz;
    const double upper = e.getRankInfo(1, 0).oscillatorHz;
    check(std::abs(12.0 * std::log2(upper / lower) - 6.0) < 2.0e-5,
          "both masters start exactly on their latest targets before first audio");
    e.pitchBend(16, -1.0f);
    render(e, 120); // Reset during a transition, not only after it settles.
    e.reset();
    startZones(e);
    render(e, 1);
    check(e.getRankInfo(0, 0).oscillatorHz == e.getRankInfo(1, 0).oscillatorHz,
          "reset clears both master targets and their smoothing history");
    e.pitchBend(16, 1.0f);
    render(e, 1);
    const double delta = 12.0 * std::log2(e.getRankInfo(1, 0).oscillatorHz
                                        / e.getRankInfo(0, 0).oscillatorHz);
    check(std::abs(delta - 12.0 * (1.0 - std::exp(-1.0 / (0.005 * 48000.0)))) < 2.0e-5,
          "the first later upper-master event glides from the reset target");
}

std::vector<float> partitionedRender(int chunk)
{
    Engine e;
    e.prepare(48000.0);
    e.setParams(controls(1.0f, 0.0f));
    startZones(e);
    render(e, 32);
    e.pitchBend(16, 1.0f);
    e.pitchBend(1, -0.5f);
    std::vector<float> audio(2048);
    for (int start = 0; start < 1024; start += chunk)
        e.render(audio.data() + 2 * start, std::min(chunk, 1024 - start));
    return audio;
}
} // namespace

int main()
{
    for (auto [rate, quality] : { std::pair { 44100.0, 0.0f }, { 48000.0, 1.0f },
                                  { 48000.0, 2.0f }, { 96000.0, 0.0f } })
        testTrajectory(rate, quality, true);
    testTrajectory(48000.0, 1.0f, false);
    testSnapAndReset();
    const auto whole = partitionedRender(1024);
    check(whole == partitionedRender(1), "master bend rendering is sample/block partition invariant");
    check(whole == partitionedRender(127), "master bend rendering is irregular-block partition invariant");
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
