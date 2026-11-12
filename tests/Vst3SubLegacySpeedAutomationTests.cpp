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
    int(cs80::P_subAtSpeed), int(cs80::P_subAtVco), int(cs80::P_subAtVcf)
};
void captureLegacySpeed(const std::filesystem::path& path, const std::vector<float>& audio)
{
    std::ofstream file(path, std::ios::binary);
    require(bool(file), "legacy speed capture open");
    file.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
    require(bool(file), "complete legacy speed capture write");
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 7 || argc == 8,
                "usage: sub-legacy-speed-test MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE [--compat-only]");
        const bool compatibilityOnly = argc == 8;
        if (compatibilityOnly) require(std::string(argv[7]) == "--compat-only", "explicit compatibility-only mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported sample rate");
        require(mode == "realtime" || mode == "offline", "supported process mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances limits(argv[2], "host_automation"), speedLimits(argv[3], "host_automation_sub_legacy_speed");
        require(speedLimits["key_parameter_count"] == 28 && cs80::kSampleAccurateParameters.size() == 28,
                "exact 28-lane legacy speed capability");
        require(std::equal(previousRoster.begin(), previousRoster.end(), cs80::kSampleAccurateParameters.begin()),
                "entire preceding 27-lane roster/order unchanged");
        require(cs80::kSampleAccurateParameters[27] == cs80::P_subSpeed, "exact appended legacy speed lane");
        std::cout << "rate=" << rate << " mode=" << mode
                  << " gate=" << (compatibilityOnly ? "compatibility-only (feature assertions not run)" : "full-legacy-speed") << '\n';
        std::filesystem::create_directories(argv[4]);
        const std::filesystem::path out(argv[4]);
        Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
        parameters[cs80::P_subOriginalMode] = 0;
        parameters[cs80::P_subWave] = 0;
        parameters[cs80::P_subSpeed] = 12;
        parameters[cs80::P_subOriginalFunction] = 0;
        parameters[cs80::P_subOriginalSpeed] = 12;
        int scenarios = 0;
        for (int voices : {8, 16})
            for (int source : {13, 0, 11})
        {
            auto p = parameters;
            p[cs80::P_voiceMode] = voices == 16 ? 1.f : 0.f;
            p[cs80::P_subAtSpeed] = p[cs80::P_subAtVco] = p[cs80::P_subAtVcf] = 0;
            p[cs80::P_subVco] = .5f;
            p[cs80::P_subVcf] = p[cs80::P_subVca] = 0;
            Curves curves;
            curves[27] = {{511, .15}, {1023, .7}, {1024, .25}, {2047, .8}, {3071, .2}, {4095, .6}};
            const auto speedRender = [&](const cs80::ParamValues& values, int block,
                                         bool automate = true, bool flatten = false,
                                         bool nullChanges = false)
            {
                return render(module, values, curves, block, automate, flatten, source, nullChanges);
            };
            const auto neutral = speedRender(p, 256, false);
            check(neutral == speedRender(p, 256, false, false, true), "legacy speed empty/no queue identical");
            const std::string id = std::to_string(voices) + "-" + std::to_string(source);
            captureLegacySpeed(out / (id + "-noqueue.pcm"), neutral);
            for (int policy : {0, 2})
            {
                auto protectedP = p;
                protectedP[cs80::P_sourcePerformance] = policy == 0 ? 0.f : 1.f;
                protectedP[cs80::P_parityMode] = policy == 2 ? 1.f : 0.f;
                const auto actual = speedRender(protectedP, 256);
                check(actual == speedRender(protectedP, 256, true, true), "legacy speed Legacy/PURE preserve flattening");
                captureLegacySpeed(out / (id + "-policy" + std::to_string(policy) + ".pcm"), actual);
            }
            if (!compatibilityOnly)
            {
                const auto scalar = speedRender(p, 1);
                double error = 0;
                for (int block : {7, 64, 256, 1024, 4096})
                    error = std::max(error, difference(scalar, speedRender(p, block)));
                check(error <= limits["audio_max_abs_error"], "legacy speed queues match per-sample reference across partitions");
                const double effect = difference(scalar, neutral);
                check(effect > .00001, "legacy speed automation positive PCM witness");
                auto original = p;
                original[cs80::P_subOriginalMode] = 1;
                const auto inactive = speedRender(original, 1);
                const auto inactiveNeutral = speedRender(original, 1, false);
                check(inactive == inactiveNeutral, "legacy speed automation is inert in original oscillator mode");
                std::cout << "legacy_speed=" << id << " max_error=" << error << " effect=" << effect
                          << " inactive_error=" << difference(inactive, inactiveNeutral) << '\n';
            }
            else std::cout << "compatibility=" << id << '\n';
            ++scenarios;
        }
        std::cout << scenarios << (compatibilityOnly ? " legacy speed compatibility scenarios, " : " native SUB legacy speed scenarios, ")
                  << failures << " failures\n";
        return failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
