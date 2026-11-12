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
    int(cs80::P_subOriginalSpeed), int(cs80::P_subVco), int(cs80::P_subVcf), int(cs80::P_subVca)
};
void captureTouch(const std::filesystem::path& path, const std::vector<float>& audio)
{
    std::ofstream file(path, std::ios::binary);
    require(bool(file), "touch capture open");
    file.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
    require(bool(file), "complete touch capture write");
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 7 || argc == 8,
                "usage: sub-touch-test MODULE ORIGINAL_CONTRACT TOUCH_CONTRACT OUTPUT_DIRECTORY RATE MODE [--compat-only]");
        const bool compatibilityOnly = argc == 8;
        if (compatibilityOnly) require(std::string(argv[7]) == "--compat-only", "explicit compatibility-only mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported sample rate");
        require(mode == "realtime" || mode == "offline", "supported process mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances limits(argv[2], "host_automation"), touchLimits(argv[3], "host_automation_sub_touch");
        require(touchLimits["key_parameter_count"] == 27 && cs80::kSampleAccurateParameters.size() >= 27,
                "historical 27-lane touch capability prefix");
        require(std::equal(previousRoster.begin(), previousRoster.end(), cs80::kSampleAccurateParameters.begin()),
                "entire preceding 24-lane roster/order unchanged");
        require(cs80::kSampleAccurateParameters[24] == cs80::P_subAtSpeed
                && cs80::kSampleAccurateParameters[25] == cs80::P_subAtVco
                && cs80::kSampleAccurateParameters[26] == cs80::P_subAtVcf, "exact appended touch amount lanes");
        std::cout << "rate=" << rate << " mode=" << mode
                  << " gate=" << (compatibilityOnly ? "compatibility-only (feature assertions not run)" : "full-touch") << '\n';
        std::filesystem::create_directories(argv[4]);
        const std::filesystem::path out(argv[4]);
        Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
#if defined(CS80_SUB_TOUCH_LEGACY_OSCILLATOR) && CS80_SUB_TOUCH_LEGACY_OSCILLATOR
        // Oscillator selection is fixed before processing. This is distinct
        // from the Legacy render policy exercised by compatibility checks.
        parameters[cs80::P_subOriginalMode] = 0;
        parameters[cs80::P_subWave] = 0;
        parameters[cs80::P_subSpeed] = 12;
        std::cout << "oscillator=legacy sine, static selection\n";
#else
        parameters[cs80::P_subOriginalMode] = 1;
        parameters[cs80::P_subOriginalFunction] = 0;
        parameters[cs80::P_subOriginalSpeed] = 12;
#endif
        int scenarios = 0;
        for (int voices : {8, 16}) for (int source : {13, 0, 11}) for (size_t lane : {24u, 25u, 26u})
        {
            auto p = parameters;
            p[cs80::P_voiceMode] = voices == 16 ? 1.f : 0.f;
            p[cs80::P_subAtSpeed] = p[cs80::P_subAtVco] = p[cs80::P_subAtVcf] = 0;
            p[cs80::kSampleAccurateParameters[lane]] = .2f;
            p[cs80::P_subVco] = lane == 24 ? .5f : 0.f;
            p[cs80::P_subVcf] = p[cs80::P_subVca] = 0;
            Curves curves;
            curves[lane] = {{511, .1}, {1023, .85}, {1024, .2}, {2047, .75}, {3071, .3}, {4095, .65}};
            const auto touchRender = [&](const cs80::ParamValues& values, int block,
                                         bool automate = true, bool flatten = false,
                                         bool nullChanges = false, bool pressure = true)
            {
                return render(module, values, curves, block, automate, flatten, source, nullChanges, -1, pressure);
            };
            const auto neutral = touchRender(p, 256, false);
            check(neutral == touchRender(p, 256, false, false, true), "touch empty/no queue identical");
            const std::string id = std::to_string(voices) + "-" + std::to_string(source) + "-" + std::to_string(lane);
            captureTouch(out / (id + "-noqueue.pcm"), neutral);
            for (int policy : {0, 2})
            {
                auto protectedP = p;
                protectedP[cs80::P_sourcePerformance] = policy == 0 ? 0.f : 1.f;
                protectedP[cs80::P_parityMode] = policy == 2 ? 1.f : 0.f;
                const auto actual = touchRender(protectedP, 256);
                check(actual == touchRender(protectedP, 256, true, true), "touch Legacy/PURE preserve flattening");
                captureTouch(out / (id + "-policy" + std::to_string(policy) + ".pcm"), actual);
            }
            if (!compatibilityOnly)
            {
                const auto scalar = touchRender(p, 1);
#if defined(CS80_SUB_TOUCH_LEGACY_OSCILLATOR) && CS80_SUB_TOUCH_LEGACY_OSCILLATOR
                auto originalMode = p;
                originalMode[cs80::P_subOriginalMode] = 1;
                const double modeEffect = difference(scalar, touchRender(originalMode, 1));
                check(modeEffect > .00001, "legacy SUB selection differs from original mode under identical queues");
                std::cout << "mode_witness=" << id << " mode_effect=" << modeEffect << '\n';
#endif
                double error = 0;
                for (int block : {7, 64, 256, 1024, 4096})
                    error = std::max(error, difference(scalar, touchRender(p, block)));
                check(error <= limits["audio_max_abs_error"], "touch queues match per-sample reference across partitions");
                const double effect = difference(scalar, neutral);
                const auto unpressured = touchRender(p, 1, true, false, false, false);
                const double pressureEffect = difference(scalar, unpressured);
                check(unpressured == touchRender(p, 1, false, false, false, false),
                      "touch amount automation is inert without pressure");
                check(effect > .00001, "touch automation positive PCM witness");
                check(pressureEffect > .00001, "valid poly-pressure positive PCM witness");
                std::cout << "touch=" << id << " max_error=" << error << " effect=" << effect
                          << " pressure_effect=" << pressureEffect << '\n';
            }
            else std::cout << "compatibility=" << id << '\n';
            ++scenarios;
        }
        std::cout << scenarios << (compatibilityOnly ? " touch compatibility scenarios, " : " native SUB touch scenarios, ")
                  << failures << " failures\n";
        return failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
