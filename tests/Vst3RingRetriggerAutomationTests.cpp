// Only the supported-input native renderer is compiled. Historical broad
// boundary/fuzz bodies and their main are excluded by this macro.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include <fstream>

namespace {
constexpr std::array previousRoster {
    int(cs80::P_volume), int(cs80::P_mix), int(cs80::P_brilliance), int(cs80::P_resonance), int(cs80::P_masterTune), int(cs80::P_detune),
    cs80::chParam(0, cs80::CP_hpfCutoff), cs80::chParam(0, cs80::CP_hpfRes), cs80::chParam(0, cs80::CP_lpfCutoff),
    cs80::chParam(0, cs80::CP_lpfRes), cs80::chParam(0, cs80::CP_vLevel), cs80::chParam(0, cs80::CP_pw),
    cs80::chParam(1, cs80::CP_hpfCutoff), cs80::chParam(1, cs80::CP_hpfRes), cs80::chParam(1, cs80::CP_lpfCutoff),
    cs80::chParam(1, cs80::CP_lpfRes), cs80::chParam(1, cs80::CP_vLevel), cs80::chParam(1, cs80::CP_pw),
    cs80::chParam(0, cs80::CP_pan), cs80::chParam(1, cs80::CP_pan),
    int(cs80::P_subOriginalSpeed), int(cs80::P_subVco), int(cs80::P_subVcf), int(cs80::P_subVca),
    int(cs80::P_subAtSpeed), int(cs80::P_subAtVco), int(cs80::P_subAtVcf), int(cs80::P_subSpeed),
    int(cs80::P_ringSpeed), int(cs80::P_ringAmount), int(cs80::P_ringDepth)
};
void captureRing(const std::filesystem::path& path, const std::vector<float>& audio)
{
    std::ofstream file(path, std::ios::binary);
    require(bool(file), "ring capture open");
    file.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
    require(bool(file), "complete ring capture write");
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 7 || argc == 8,
                "usage: ring-retrigger-test MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE [--compat-only]");
        const bool compatibilityOnly = argc == 8;
        if (compatibilityOnly) require(std::string(argv[7]) == "--compat-only", "explicit compatibility-only mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported sample rate");
        require(mode == "realtime" || mode == "offline", "supported process mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances limits(argv[2], "host_automation"), ringLimits(argv[3], "host_automation_ring_retrigger");
        require(ringLimits["key_parameter_count"] == 33 && cs80::kSampleAccurateParameters.size() == 33,
                "exact 33-lane ring-retrigger capability");
        require(std::equal(previousRoster.begin(), previousRoster.end(), cs80::kSampleAccurateParameters.begin()),
                "entire preceding 31-lane roster/order unchanged");
        require(cs80::kSampleAccurateParameters[31] == cs80::P_ringAttack
                && cs80::kSampleAccurateParameters[32] == cs80::P_ringDecay, "exact appended ring-time lanes");
        std::cout << "rate=" << rate << " mode=" << mode
                  << " gate=" << (compatibilityOnly ? "compatibility-only (feature assertions not run)" : "full-ring-retrigger") << '\n';
        std::filesystem::create_directories(argv[4]);
        const std::filesystem::path out(argv[4]);
        Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        for (int parameter : std::array<int, 8> {cs80::P_arpOn, cs80::P_kasScanTiming,
                              cs80::P_footswitchPlugged, cs80::P_footswitchPressed,
                              cs80::P_sustainAssign, cs80::P_glideAssign,
                              cs80::P_masterSustain, cs80::P_sustainMode})
            parameters[parameter] = 0;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
        parameters[cs80::P_ringSpeed] = 20;
        parameters[cs80::P_ringAmount] = .5f;
        parameters[cs80::P_ringDepth] = .6f;
        parameters[cs80::P_ringAttack] = .01f;
        parameters[cs80::P_ringDecay] = .45f;
        int scenarios = 0;
        for (int voices : {8, 16})
            for (int source : {13, 0, 11})
                for (int lane : {31, 32})
        {
            auto p = parameters;
            p[cs80::P_voiceMode] = voices == 16 ? 1.f : 0.f;
            // Times use the existing skewed normalized conversion, not linear seconds.
            p[cs80::P_ringAttack] = lane == 31 ? .08f : .003f;
            p[cs80::P_ringDecay] = .45f;
            const int parameter = cs80::kSampleAccurateParameters[lane];
            const double initial = normalized(parameter, p[parameter]);
            const double target = normalized(parameter, lane == 31 ? .003f : .02f);
            Curves curves, early, sameOffset, constant;
            // Identical initial latch, changed stored time before the next onset.
            curves[lane] = {{0, initial}, {64, initial}, {1536, target}, {4095, target}};
            constant[lane] = {{0, initial}, {64, initial}, {4095, initial}};
            // Distinct trajectories witness publication before second MIDI onset.
            early[lane] = {{0, initial}, {2046, initial}, {2047, target}, {4095, target}};
            sameOffset[lane] = {{0, initial}, {2047, initial}, {2048, target}, {4095, target}};
            const auto ringRender = [&](const cs80::ParamValues& values, int block,
                                         bool automate = true, bool flatten = false,
                                         bool nullChanges = false)
            {
                return render(module, values, curves, block, automate, flatten, source,
                              nullChanges, -1, false, true);
            };
            const auto neutral = ringRender(p, 256, false);
            check(neutral == ringRender(p, 256, false, false, true), "ring empty/no queue identical");
            const std::string id = std::string(lane == 31 ? "attack" : "decay") + "-" + std::to_string(voices) + "-" + std::to_string(source);
            captureRing(out / (id + "-noqueue.pcm"), neutral);
            for (int policy : {0, 2})
            {
                auto protectedP = p;
                protectedP[cs80::P_sourcePerformance] = policy == 0 ? 0.f : 1.f;
                protectedP[cs80::P_parityMode] = policy == 2 ? 1.f : 0.f;
                const auto actual = ringRender(protectedP, 256);
                check(actual == ringRender(protectedP, 256, true, true), "ring Legacy/PURE preserve flattening");
                captureRing(out / (id + "-policy" + std::to_string(policy) + ".pcm"), actual);
            }
            if (!compatibilityOnly)
            {
                const auto scalar = ringRender(p, 1);
                double error = 0;
                for (int block : {7, 64, 256, 1024, 4096})
                    error = std::max(error, difference(scalar, ringRender(p, block)));
                check(error <= limits["audio_max_abs_error"], "ring queues match per-sample reference across partitions");
                // Stereo interleaving: sample2048 begins at float index4096.
                const auto constantAudio = render(module, p, constant, 1, true, false,
                                                  source, false, -1, false, true);
                require(scalar.size() == 8192 && constantAudio.size() == scalar.size(),
                        "exact4096 stereo endpoint window");
                const bool prefixEqual = std::equal(scalar.begin(), scalar.begin() + 4096,
                                                   constantAudio.begin());
                double postEffect = 0;
                for (size_t i = 4096; i < scalar.size(); ++i)
                    postEffect = std::max(postEffect, std::abs(double(scalar[i]) - constantAudio[i]));
                check(prefixEqual, "stored time changes preserve PCM before next trigger");
                check(postEffect > .00001, "newly latched time changes PCM after second trigger");
                const auto earlyAudio = render(module, p, early, 256, true, false,
                                               source, false, -1, false, true);
                const auto sameAudio = render(module, p, sameOffset, 256, true, false,
                                              source, false, -1, false, true);
                check(sameAudio == earlyAudio, "second-offset2048 target equals pre-onset2047 target");
                std::cout << "ring=" << id << " max_error=" << error
                          << " prefix_exact=" << prefixEqual << " post_effect=" << postEffect
                          << " same_offset_error=" << difference(sameAudio, earlyAudio) << '\n';
            }
            else std::cout << "compatibility=" << id << '\n';
            ++scenarios;
        }
        std::cout << scenarios << (compatibilityOnly ? " ring-retrigger compatibility scenarios, " : " native ring-retrigger scenarios, ")
                  << failures << " failures\n";
        return failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
