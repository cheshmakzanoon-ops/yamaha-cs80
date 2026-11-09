#include "core/Engine.h"
#include "LegacySubLfoOracle.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace {
int checks = 0, failures = 0, scenarios = 0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; if (failures <= 20) std::printf("FAIL: %s\n", label); }
}
void validator()
{
    using namespace legacy_sub_oracle;
    Pair fusedOnly, splitOnly, splice;
    bool distinguished = false, spliced = false;
    unsigned distinctFrames = 0;
    for (int frame = 0; frame < 4096; ++frame)
    {
        for (int os = 0; os < 2; ++os)
        {
            fusedOnly.tick(7.0f, 96000.0f, 0);
            splitOnly.tick(7.0f, 96000.0f, 0);
            splice.tick(7.0f, 96000.0f, 0);
        }
        check(fusedOnly.observe(fusedOnly.fused.current), "complete fused trace accepted");
        check(splitOnly.observe(splitOnly.split.current), "complete split trace accepted");
        const bool different = !sameBits(splice.split.current.speed, splice.fused.current.speed)
            || !sameBits(splice.split.current.value, splice.fused.current.value);
        if (different) ++distinctFrames;
        if (!distinguished)
        {
            check(splice.observe(splice.split.current), "split prefix accepted");
            if (different) { distinguished = true; check(splice.eligible == 1, "distinct prefix eliminates fused track"); }
        }
        else if (different && !spliced)
        {
            check(!splice.observe(splice.fused.current), "distinct opposite suffix rejects whole splice");
            spliced = true;
        }
        else if (spliced)
        {
            check(!splice.observe(splice.split.current), "rejected splice cannot revive split track");
            check(!splice.observe(splice.fused.current), "rejected splice cannot revive fused track");
        }
        else check(splice.observe(splice.split.current), "prefix remains accepted before distinct suffix");
    }
    check(distinguished && spliced && distinctFrames > 1, "splice really distinguishes both trajectories");
    check(fusedOnly.eligible == 2 && splitOnly.eligible == 1, "each complete trace keeps only its exact contract");

    Pair reference;
    for (int i = 0; i < 2048; ++i) reference.tick(7.0f, 96000.0f, 0);
    const auto expected = reference.split.current;
    for (bool damageSpeed : {false, true})
    {
        auto damaged = expected;
        float& field = damageSpeed ? damaged.speed : damaged.value;
        const float other = damageSpeed ? reference.fused.current.speed : reference.fused.current.value;
        // Step outwards from the larger endpoint, provably outside BOTH.
        field = std::nextafter(std::max(field, other), std::numeric_limits<float>::infinity());
        auto r = reference;
        check(!r.observe(damaged), "one-ULP outside both admissible fields rejects");
        check(!r.observe(expected), "one-ULP rejection cannot revive");
    }
    for (float bad : {std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
    {
        auto a = reference, b = reference;
        check(!a.observe({bad, expected.value}), "nonfinite speed rejects");
        check(!b.observe({expected.speed, bad}), "nonfinite waveform rejects");
    }
    Pair synthetic;
    synthetic.split.current = {1.0f, 2.0f}; synthetic.fused.current = {3.0f, 4.0f};
    check(!synthetic.observe({1.0f,4.0f}), "cannot choose speed and value from different contracts");
    check(!sameBits(0.0f,-0.0f), "signed-zero comparisons remain exact");
}

void scenario(float rate, int wave, bool schedule, bool shortOriginal = false)
{
    using namespace cs80;
    Engine engine;
    engine.prepare(rate);
    ParamValues params;
    params[P_sourcePerformance] = params[P_parityMode] = 0.0f;
    params[P_subOriginalMode] = 0.0f;
    params[P_subWave] = static_cast<float>(wave);
    params[P_subSpeed] = shortOriginal || schedule ? 7.0f : 30.0f;
    engine.setParams(params);
    check(engine.getRenderPolicy() == RenderPolicy::legacy, "actual Legacy policy");
    check(engine.getHostSampleRate() == rate && engine.getInternalSampleRate() == 2.0 * rate
          && engine.getOversamplingFactor() == 2, "actual requested rate and factor2");
    check(engine.activeVoices() == 0, "fixed no-voice fixture");
    legacy_sub_oracle::Pair reference;
    const int segment = static_cast<int>(std::lround(0.15 * static_cast<double>(rate)));
    const int frames = shortOriginal ? 512 : schedule ? 3 * segment
        : static_cast<int>(std::lround(0.4 * static_cast<double>(rate)));
    float pair[2] {};
    bool finiteAudio = true;
    for (int frame = 0; frame < frames; ++frame)
    {
        if (schedule && (frame == segment || frame == 2 * segment))
        {
            params[P_subSpeed] = frame == segment ? 30.0f : 0.05f;
            engine.setParams(params);
        }
        for (int os = 0; os < 2; ++os)
            reference.tick(params[P_subSpeed], rate * 2.0f, wave);
        engine.render(pair, 1);
        finiteAudio = finiteAudio && std::isfinite(pair[0]) && std::isfinite(pair[1]);
        check(reference.observe({engine.getSubOscillatorRate(), engine.getSubOscillatorValue()}),
              "one entire exact speed-and-waveform reference survives");
    }
    check(finiteAudio, "finite no-voice audio");
    for (const auto* track : {&reference.split, &reference.fused})
    {
        check(track->ticks == static_cast<std::uint64_t>(frames) * 2, "every internal tick represented");
        if (!shortOriginal) check(track->wraps >= 4, "fixture reaches multiple real phase wraps");
        const std::uint64_t expectedDraws = wave == 4 ? track->wraps + 1 : wave == 5 ? track->ticks : 0;
        check(track->draws == expectedDraws, "independent RNG consumption follows waveform/wrap contract");
    }
    std::printf("scenario rate=%.0f wave=%d mode=%s frames=%d eligible=%u wraps=%llu/%llu draws=%llu/%llu\n",
                rate,wave,shortOriginal?"original":schedule?"scheduled":"wrap",frames,reference.eligible,
                static_cast<unsigned long long>(reference.split.wraps),static_cast<unsigned long long>(reference.fused.wraps),
                static_cast<unsigned long long>(reference.split.draws),static_cast<unsigned long long>(reference.fused.draws));
    ++scenarios;
}
}
int main()
{
    validator();
    for (int wave = 0; wave < 6; ++wave) scenario(48000.0f,wave,false,true);
    for (float rate : {44100.0f,48000.0f,96000.0f,192000.0f})
        for (int wave = 0; wave < 6; ++wave)
            for (bool schedule : {false,true}) scenario(rate,wave,schedule);
    std::printf("%d scenarios, %d checks, %d failures\n",scenarios,checks,failures);
    return failures ? 1 : 0;
}
