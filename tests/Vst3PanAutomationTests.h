#pragma once
// Actual built-module pan witnesses. Included by Vst3AutomationTests.cpp so
// every trajectory uses its native IParamValueQueue, Module and Instance host.
// These are numerical PCM checks, not listening or real-DAW qualification.
namespace {
Curves panCurves(int rank)
{
    Curves curves;
    const int parameter = cs80::chParam(rank, cs80::CP_pan);
    const auto found = std::find(cs80::kSampleAccurateParameters.begin(),
                                 cs80::kSampleAccurateParameters.end(), parameter);
    require(found != cs80::kSampleAccurateParameters.end(), "each rank pan is a native automation key");
    // No point at zero: interpolation must include the implicit sample -1.
    // Long left/right plateaus make the stereo sign observable despite the
    // retained DSP smoother and output latency, even at the tested 96 kHz.
    // Consecutive knots also exercise a discontinuity and the final sample.
    curves[static_cast<size_t>(found - cs80::kSampleAccurateParameters.begin())] = {
        {511, 0.05}, {1023, 0.05}, {1024, 0.95}, {2047, 0.95},
        {2048, 0.20}, {2559, 0.80}, {3071, 0.20}, {4095, 0.65}
    };
    return curves;
}

double panPeak(const std::vector<float>& audio)
{
    require(audio.size() == 8192, "complete 4096-frame stereo pan render");
    double peak = 0.0;
    for (float sample : audio)
    {
        require(std::isfinite(sample), "finite native pan PCM");
        peak = std::max(peak, std::abs(double(sample)));
    }
    return peak;
}

double panEnergyBalance(const std::vector<float>& audio, int begin, int end)
{
    double left = 0.0, right = 0.0;
    for (int sample = begin; sample < end; ++sample)
    {
        const double l = audio[static_cast<size_t>(sample) * 2];
        const double r = audio[static_cast<size_t>(sample) * 2 + 1];
        left += l * l;
        right += r * r;
    }
    require(std::isfinite(left + right) && left + right > 0.0, "nonzero finite pan measurement window");
    return (right - left) / (right + left);
}

bool panBytesEqual(const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

std::uint64_t panPcmHash(const std::vector<float>& audio)
{
    std::uint64_t hash = 14695981039346656037ull;
    const auto* bytes = reinterpret_cast<const unsigned char*>(audio.data());
    for (size_t i = 0; i < audio.size() * sizeof(float); ++i)
        hash = (hash ^ bytes[i]) * 1099511628211ull;
    return hash;
}

void testVst3PanAutomation(Module& module, double audioTolerance)
{
    // Appending pan must not move any original lane or change the old
    // lane-index-dependent curves in the shared partition/boundary fixtures.
    constexpr std::array originalKeys {
        int(cs80::P_volume), int(cs80::P_mix), int(cs80::P_brilliance), int(cs80::P_resonance),
        int(cs80::P_masterTune), int(cs80::P_detune),
        cs80::chParam(0, cs80::CP_hpfCutoff), cs80::chParam(0, cs80::CP_hpfRes),
        cs80::chParam(0, cs80::CP_lpfCutoff), cs80::chParam(0, cs80::CP_lpfRes),
        cs80::chParam(0, cs80::CP_vLevel), cs80::chParam(0, cs80::CP_pw),
        cs80::chParam(1, cs80::CP_hpfCutoff), cs80::chParam(1, cs80::CP_hpfRes),
        cs80::chParam(1, cs80::CP_lpfCutoff), cs80::chParam(1, cs80::CP_lpfRes),
        cs80::chParam(1, cs80::CP_vLevel), cs80::chParam(1, cs80::CP_pw)
    };
    require(cs80::kSampleAccurateParameters.size() == originalKeys.size() + 2,
            "exactly two native pan keys are appended to the original roster");
    check(std::equal(originalKeys.begin(), originalKeys.end(), cs80::kSampleAccurateParameters.begin()),
          "original eighteen native automation keys retain their lane order");
    check(cs80::kSampleAccurateParameters[originalKeys.size()] == cs80::chParam(0, cs80::CP_pan)
        && cs80::kSampleAccurateParameters[originalKeys.size() + 1] == cs80::chParam(1, cs80::CP_pan),
          "the appended pan lanes retain rank order");

    for (bool sixteen : {false, true})
        for (int source : {13, 0, 11, 12})
            for (int rank : {0, 1})
            {
                auto parameters = cs80::defaultValues();
                parameters[cs80::P_sourcePerformance] = 1.0f;
                parameters[cs80::P_parityMode] = 0.0f;
                parameters[cs80::P_voiceMode] = sixteen ? 1.0f : 0.0f;
                parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion]
                    = parameters[cs80::P_analogNoise] = 0.0f;
                parameters[cs80::P_outputMode] = 1.0f;
                parameters[cs80::P_chorusOn] = 0.0f;
                parameters[cs80::P_mix] = static_cast<float>(rank); // Solo the measured rank.
                parameters[cs80::chParam(0, cs80::CP_pan)] = 0.0f;
                parameters[cs80::chParam(1, cs80::CP_pan)] = 0.0f;
                const auto curves = panCurves(rank);

                // The scalar oracle supplies one independently evaluated
                // normalized point per process call. It never asks the module
                // to interpolate, so an old final-point-only module fails the
                // multi-point comparison rather than sharing the same defect.
                const auto scalar = render(module, parameters, curves, 1, true, false, source);
                const auto whole = render(module, parameters, curves, 4096, true, false, source);
                double maxError = difference(scalar, whole);
                for (int block : {7, 64, 256, 1024})
                    maxError = std::max(maxError,
                        difference(scalar, render(module, parameters, curves, block, true, false, source)));
                check(maxError <= audioTolerance,
                      "native rank pan matches independent scalar PCM at every host partition and source");

                const auto neutral = render(module, parameters, curves, 256, false, false, source);
                const auto nullQueue = render(module, parameters, curves, 256, false, false, source, true);
                check(panBytesEqual(neutral, nullQueue), "pan no-queue and empty-queue controls are byte-identical");
                const double peak = panPeak(whole);
                const double neutralPeak = panPeak(neutral);
                const double changed = difference(whole, neutral);
                check(peak > 0.00001 && neutralPeak > 0.00001, "pan and neutral controls both produce nonzero PCM");
                check(changed > 0.00001, "native pan trajectory measurably changes stereo PCM");
                const double leftBalance = panEnergyBalance(whole, 640, 1024);
                const double rightBalance = panEnergyBalance(whole, 1536, 2048);
                check(leftBalance < -0.2 && rightBalance > 0.2,
                      "native pan moves measured energy left then right within one process call");

                const auto otherRank = render(module, parameters, panCurves(1 - rank), 256, true, false, source);
                const double silentRankError = difference(otherRank, neutral);
                check(silentRankError <= audioTolerance, "pan automation is isolated to its own rank");

                std::array<std::uint64_t, 2> protectedHashes {};
                for (int policy = 0; policy < 2; ++policy)
                {
                    auto protectedParameters = parameters;
                    if (policy == 0) protectedParameters[cs80::P_sourcePerformance] = 0.0f;
                    else protectedParameters[cs80::P_parityMode] = 1.0f;
                    const auto actual = render(module, protectedParameters, curves, 256, true, false, source);
                    const auto flattened = render(module, protectedParameters, curves, 256, true, true, source);
                    panPeak(actual);
                    check(panBytesEqual(actual, flattened),
                          "Legacy and PURE pan retain final-point-at-block-start PCM byte-for-byte");
                    protectedHashes[static_cast<size_t>(policy)] = panPcmHash(actual);
                }
                // Exact hashes also permit comparing these unchanged paths
                // against a preserved pre-pan module with this same executable.
                std::cout << "PAN voices=" << (sixteen ? 16 : 8) << " source=" << source << " rank=" << rank + 1
                    << " max_audio_error=" << maxError << " peak=" << peak << " positive_difference=" << changed
                    << " left_balance=" << leftBalance << " right_balance=" << rightBalance
                    << " silent_rank_error=" << silentRankError << " no_queue_fnv1a=" << panPcmHash(neutral)
                    << " legacy_fnv1a=" << protectedHashes[0] << " pure_fnv1a=" << protectedHashes[1] << std::endl;
            }
}
} // namespace
