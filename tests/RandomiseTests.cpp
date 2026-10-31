// "Randomise within original ranges" rules (src/core/Randomise.h,
// docs/workstreams/ws6-library.md). Software checks, not hardware evidence.
#include "core/Engine.h"
#include "core/Randomise.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace cs80;

namespace
{
// Acceptance criteria, fixed before the rules were evaluated.
constexpr int kSeeds = 400;          // invariant checks
constexpr int kAudioSeeds = 48;      // rendered checks (circuit engine), plus 16 on legacy
constexpr double kMaxPeakDbfs = -1.0; // 4-note chord, full velocity and pressure, default VOLUME
constexpr double kMinWindowRmsDbfs = -45.0;
constexpr double kSampleRate = 44100.0;

int failures = 0;
void check(bool ok, const std::string& message)
{
    if (!ok)
    {
        ++failures;
        if (failures < 60) std::printf("FAIL: %s\n", message.c_str());
    }
}

ParamValues circuitBase()
{
    ParamValues base;
    base[P_sourcePerformance] = 1.0f;
    return base;
}

void testInvariants()
{
    const auto base = circuitBase();
    const library::RandomiseBounds bounds;
    for (std::uint32_t seed = 1; seed <= (std::uint32_t) kSeeds; ++seed)
    {
        const auto v = library::randomiseWithinOriginalRanges(base, seed);
        const auto again = library::randomiseWithinOriginalRanges(base, seed);
        check(v.v == again.v, "same seed gives identical values");
        const auto other = library::randomiseWithinOriginalRanges(base, seed + 7919u);
        int changed = 0;
        for (int i = 0; i < P_numParams; ++i) changed += v[i] != other[i] ? 1 : 0;
        check(changed > 20, "different seeds give different sounds");
        for (int i = 0; i < P_numParams; ++i)
        {
            const auto& d = paramDef(i);
            check(std::isfinite(v[i]) && v[i] >= d.min && v[i] <= d.max, "in range: " + paramId(i));
            if (!library::isRandomisedParameter(i))
                check(v[i] == base[i], "non-original or protected parameter unchanged: " + paramId(i));
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto p = [&](int local) { return v[chParam(ch, local)]; };
            check(p(CP_saw) == 0.0f || p(CP_saw) == 1.0f, "saw switch 0/1");
            check(p(CP_pulse) == 0.0f || p(CP_pulse) == 1.0f, "pulse switch 0/1");
            check(p(CP_feet) == std::round(p(CP_feet)), "feet is a footage position");
            check(p(CP_saw) >= 0.5f || p(CP_pulse) >= 0.5f || p(CP_sine) >= 0.6f, "R2 audible source on every rank");
            check(p(CP_lpfCutoff) >= bounds.minCutoffRatio * p(CP_hpfCutoff) * 0.999f
                      && p(CP_lpfCutoff) >= bounds.minLowpassHz * 0.999f,
                  "R3 open filter passband");
            check(p(CP_vA) <= bounds.maxAttackSeconds && (p(CP_vS) >= 0.3f || p(CP_vD) >= 0.4f), "R4 audible duration");
            check(p(CP_vLevel) + p(CP_initLevel) + p(CP_afterLevel) <= bounds.maxLevelSum + 1.0e-6f, "R5 bounded level");
            check(p(CP_lpfRes) * 0.8f + v[P_resonance] * 0.6f <= bounds.maxCombinedQ + 1.0e-6f, "R6 bounded resonance");
        }
        check(v[P_ringAmount] <= bounds.maxRingAmount, "R7 bounded ring modulation");
        check(v[P_subOriginalFunction] >= 0.0f && v[P_subOriginalFunction] <= 4.0f
                  && v[P_subOriginalFunction] == std::round(v[P_subOriginalFunction]),
              "SUB function excludes EXT");
        check(v[P_chorusOn] == 0.0f || v[P_chorusOn] == 1.0f, "chorus switch 0/1");
    }
    // Protected values survive whatever the base holds.
    auto custom = circuitBase();
    custom[P_volume] = 0.33f; custom[P_parityMode] = 1.0f; custom[P_fx1Type] = 3.0f; custom[P_fx1Mix] = 0.4f;
    custom[P_arpOn] = 1.0f; custom[P_footswitchPlugged] = 1.0f; custom[P_masterTune] = 0.5f;
    custom[chParam(0, CP_pan)] = -0.7f; custom[chParam(1, CP_lpfOn)] = 0.0f;
    const auto v = library::randomiseWithinOriginalRanges(custom, 99u);
    check(v[P_volume] == 0.33f && v[P_parityMode] == 1.0f && v[P_sourcePerformance] == 1.0f && v[P_fx1Type] == 3.0f
              && v[P_fx1Mix] == 0.4f && v[P_arpOn] == 1.0f && v[P_footswitchPlugged] == 1.0f && v[P_masterTune] == 0.5f
              && v[chParam(0, CP_pan)] == -0.7f && v[chParam(1, CP_lpfOn)] == 0.0f,
          "R8 policy, volume, physical setup, tuning, modern layer and software extensions are kept");
}

double dbfs(double x) { return 20.0 * std::log10(std::max(x, 1.0e-12)); }

void render(const ParamValues& values, const std::string& label)
{
    Engine e;
    e.prepare(kSampleRate);
    e.setParams(values);
    const int notes[4] = { 48, 55, 60, 64 };
    for (int n : notes) e.noteOn(n, 1.0f, 1);
    std::vector<float> block(512);
    double peak = 0.0, best = 0.0, sum = 0.0;
    int count = 0;
    bool finite = true;
    const int window = (int) (0.05 * kSampleRate);
    for (int done = 0; done < (int) (2.0 * kSampleRate); done += 256)
    {
        const double t = done / kSampleRate;
        e.channelPressure(1, (float) std::clamp((t - 0.2) / 0.3, 0.0, 1.0));
        if (t >= 1.5 && t < 1.5 + 256.0 / kSampleRate)
            for (int n : notes) e.noteOff(n, 1);
        e.render(block.data(), 256);
        for (int i = 0; i < 256; ++i)
        {
            const double l = block[(size_t) i * 2], r = block[(size_t) i * 2 + 1];
            finite = finite && std::isfinite(l) && std::isfinite(r);
            peak = std::max({ peak, std::abs(l), std::abs(r) });
            sum += 0.5 * (l * l + r * r);
            if (++count == window)
            {
                best = std::max(best, std::sqrt(sum / window));
                sum = 0.0;
                count = 0;
            }
        }
    }
    check(finite, "finite audio: " + label);
    check(dbfs(peak) <= kMaxPeakDbfs, "no clipping-prone peak: " + label + " " + std::to_string(dbfs(peak)));
    check(dbfs(best) >= kMinWindowRmsDbfs, "never silent: " + label + " " + std::to_string(dbfs(best)));
}

void testAudio()
{
    for (std::uint32_t seed = 1; seed <= (std::uint32_t) kAudioSeeds; ++seed)
        render(library::randomiseWithinOriginalRanges(circuitBase(), seed * 2654435761u), "circuit seed " + std::to_string(seed));
    for (std::uint32_t seed = 1; seed <= 16u; ++seed)
        render(library::randomiseWithinOriginalRanges(ParamValues {}, seed * 40503u), "legacy seed " + std::to_string(seed));
}
} // namespace

int main()
{
    testInvariants();
    testAudio();
    if (failures != 0)
    {
        std::printf("%d randomise failures\n", failures);
        return 1;
    }
    std::printf("randomise rules passed: %d seeds checked, %d rendered\n", kSeeds, kAudioSeeds + 16);
    return 0;
}
