// Conventional MIDI only: a held pitch wheel follows live bend-range edits.
// Circuit smoothing remains the declared 5 ms software controller law.
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
void check(bool condition, const char* label)
{
    ++checks;
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}

ParamValues controls(bool source, float quality)
{
    ParamValues p;
    p[P_sourcePerformance] = source ? 1.0f : 0.0f;
    p[P_oversampling] = quality;
    p[P_mpeOn] = 0.0f;
    p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = p[P_chorusOn] = 0.0f;
    p[P_detune] = p[P_initBend] = 0.0f;
    p[P_mod1Amount] = 1.0f;
    return p;
}

void setup(Engine& engine, double rate, const ParamValues& p)
{
    engine.prepare(rate);
    engine.setParams(p);
    modern::ModernConfig config;
    config.slots[0] = {modern::ModSource::pitchBend, modern::ModSource::none,
                       modern::ModDestination::fx1Mix, modern::ModTarget::both};
    engine.setModernConfig(config);
    engine.noteOn(60, 0.8f, 1);
    engine.noteOn(67, 0.8f, 4);
    std::array<float, 64> audio {};
    engine.render(audio.data(), 32); // Close the startup snap window.
}

void trajectory(double rate, float quality, bool source)
{
    Engine actual, eventReference;
    auto p = controls(source, quality);
    setup(actual, rate, p);
    setup(eventReference, rate, p);
    const std::array<double, 2> baseHz {actual.getRankInfo(0, 0).oscillatorHz,
                                      actual.getRankInfo(1, 0).oscillatorHz};
    const double decay = std::exp(-1.0 / (0.005 * rate));
    double bend = 0.0, wheel = 0.0;
    struct Step { float range, wheel; bool wheelEvent; double seconds; };
    const std::array<Step, 8> steps {{
        {2.0f, -0.75f, true, .001},
        {12.0f, -0.75f, false, .003}, // Edit while the initial wheel is still moving.
        {1.0f, -0.75f, false, .002},
        {48.0f, -0.75f, false, .003},
        {7.25f, -0.75f, false, .105},
        {7.25f, 0.5f, true, .002},
        {24.0f, 0.5f, false, .105},
        {1.0f, 0.0f, true, .105},
    }};
    for (const auto& step : steps)
    {
        p[P_pitchBendRange] = step.range;
        actual.setParams(p);
        eventReference.setParams(p);
        if (step.wheelEvent) actual.pitchBend(1, step.wheel);
        // Independent behavioral oracle: explicitly sending the unchanged wheel
        // with the new range has always selected the intended semitone target.
        eventReference.pitchBend(1, step.wheel);
        const double target = (double) (step.wheel * step.range);
        double worstPitch = 0.0, worstWheel = 0.0;
        bool exactAudio = true;
        for (int n = 0; n < (int) std::ceil(step.seconds * rate); ++n)
        {
            std::array<float, 2> a {}, b {};
            actual.render(a.data(), 1);
            eventReference.render(b.data(), 1);
            exactAudio = exactAudio && a == b;
            bend = source ? target + (bend - target) * decay : target;
            wheel = source ? step.wheel + (wheel - step.wheel) * decay : step.wheel;
            for (int voice = 0; voice < 2; ++voice)
            {
                const double heard = 12.0 * std::log2(actual.getRankInfo(voice, 0).oscillatorHz / baseHz[voice]);
                const double error = std::abs(heard - bend);
                worstPitch = std::isfinite(error) ? std::max(worstPitch, error)
                                                 : std::numeric_limits<double>::infinity();
            }
            worstWheel = std::max(worstWheel,
                std::abs((double) actual.getModernGlobalOffsets().fxMix[0] - wheel));
        }
        std::printf("%s %.0f Hz q%.0f range %.2f: pitch error %.9g, wheel error %.9g\n",
                    source ? "circuit" : "legacy", rate, quality, step.range, worstPitch, worstWheel);
        check(worstPitch < 2.0e-5, "range edit retargets all conventional voices with the existing smoothing law");
        check(worstWheel < 1.0e-6, "range edit leaves the normalized matrix wheel source unchanged");
        check(exactAudio, "range edit equals reissuing the held wheel at the new range sample-for-sample");
    }
}

void beforeFirstAudioAndReset()
{
    for (bool source : {false, true})
    {
        Engine engine;
        auto p = controls(source, 0.0f);
        engine.prepare(48000.0);
        engine.setParams(p);
        engine.pitchBend(1, 0.5f);
        p[P_pitchBendRange] = 12.0f;
        engine.setParams(p);
        engine.noteOn(60, 0.8f);
        std::array<float, 2> audio {};
        engine.render(audio.data(), 1);
        const double bent = engine.getRankInfo(0, 0).oscillatorHz;
        engine.reset();
        p[P_pitchBendRange] = 24.0f;
        engine.setParams(p);
        engine.noteOn(60, 0.8f);
        engine.render(audio.data(), 1);
        const double unbent = engine.getRankInfo(0, 0).oscillatorHz;
        check(std::abs(12.0 * std::log2(bent / unbent) - 6.0) < 2.0e-5,
              "latest pre-render range is honored and reset cannot resurrect a held wheel");
        p[P_pitchBendRange] = 48.0f;
        engine.setParams(p);
        engine.render(audio.data(), 1);
        check(engine.getRankInfo(0, 0).oscillatorHz == unbent,
              "editing range with a centered wheel does not retune notes");
    }
}

void resetTransitions()
{
    for (int transition = 0; transition < 3; ++transition)
    {
        Engine engine, fresh;
        auto p = controls(true, 0.0f);
        setup(engine, 48000.0, p);
        engine.pitchBend(1, 0.75f);
        std::array<float, 128> audio {};
        engine.render(audio.data(), 64);
        p[P_pitchBendRange] = 12.0f;
        if (transition == 0) p[P_oversampling] = 2.0f;
        if (transition == 1) p[P_sourcePerformance] = 0.0f;
        if (transition == 2) p[P_parityMode] = 1.0f;
        engine.setParams(p);
        engine.noteOn(60, 0.8f);
        engine.render(audio.data(), 1);
        setup(fresh, 48000.0, p);
        check(engine.getRankInfo(0, 0).oscillatorHz == fresh.getRankInfo(0, 0).oscillatorHz,
              "simultaneous quality/policy reset and range edit keeps the wheel cleared");
    }
}

std::vector<float> partitioned(int block, bool source)
{
    Engine engine;
    auto p = controls(source, 0.0f);
    setup(engine, 48000.0, p);
    engine.pitchBend(1, -0.5f);
    std::vector<float> audio(2 * 2048);
    for (int frame = 0; frame < 2048;)
    {
        int boundary = 2048;
        if (frame < 113) boundary = 113;
        else if (frame < 701) boundary = 701;
        if (frame == 113 || frame == 701)
        {
            p[P_pitchBendRange] = frame == 113 ? 24.0f : 1.0f;
            engine.setParams(p);
        }
        const int count = std::min(block, boundary - frame);
        engine.render(audio.data() + 2 * frame, count);
        frame += count;
    }
    return audio;
}
} // namespace

int main()
{
    trajectory(48000.0, 0.0f, false);
    trajectory(44100.0, 0.0f, true);
    trajectory(48000.0, 1.0f, true);
    trajectory(48000.0, 2.0f, true);
    trajectory(96000.0, 0.0f, true);
    beforeFirstAudioAndReset();
    resetTransitions();
    for (bool source : {false, true})
    {
        const auto reference = partitioned(1, source);
        check(reference == partitioned(17, source) && reference == partitioned(256, source),
              "live range edits are independent of host render partitioning");
    }
    std::printf("Pitch bend range: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
