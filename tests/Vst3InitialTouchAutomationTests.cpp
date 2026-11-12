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
    int(cs80::P_ringSpeed), int(cs80::P_ringAmount), int(cs80::P_ringDepth),
    int(cs80::P_ringAttack), int(cs80::P_ringDecay),
    int(cs80::P_chorusSpeed), int(cs80::P_chorusDepth), int(cs80::P_portamento), int(cs80::P_masterSustain), int(cs80::P_initBend), int(cs80::P_kbdLevelLow), int(cs80::P_kbdLevelHigh), int(cs80::P_kbdBrillianceLow), int(cs80::P_kbdBrillianceHigh)
};
void captureInitialTouch(const std::filesystem::path& path, const std::vector<float>& audio)
{
    std::ofstream file(path, std::ios::binary);
    require(bool(file), "initial_touch capture open");
    file.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
    require(bool(file), "complete initial_touch capture write");
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 7 || argc == 8,
                "usage: initial_touch-test MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE [--compat-only]");
        const bool compatibilityOnly = argc == 8;
        if (compatibilityOnly) require(std::string(argv[7]) == "--compat-only", "explicit compatibility-only mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported sample rate");
        require(mode == "realtime" || mode == "offline", "supported process mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances limits(argv[2], "host_automation"), initial_touchLimits(argv[3], "host_automation_initial_touch");
        require(initial_touchLimits["key_parameter_count"] == 46 && cs80::kSampleAccurateParameters.size() >= 46,
                "historical 46-lane initial_touch capability");
        require(std::equal(previousRoster.begin(), previousRoster.end(), cs80::kSampleAccurateParameters.begin()),
                "entire preceding 42-lane roster/order unchanged");
        constexpr std::array added {cs80::chParam(0, cs80::CP_initBrill), cs80::chParam(0, cs80::CP_initLevel),
                                    cs80::chParam(1, cs80::CP_initBrill), cs80::chParam(1, cs80::CP_initLevel)};
        require(std::equal(added.begin(), added.end(), cs80::kSampleAccurateParameters.begin() + 42), "exact initial touch lanes");
        std::cout << "rate=" << rate << " mode=" << mode
                  << " gate=" << (compatibilityOnly ? "compatibility-only (feature assertions not run)" : "full-initial_touch") << '\n';
        std::filesystem::create_directories(argv[4]);
        const std::filesystem::path out(argv[4]);
        Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
        parameters[cs80::P_kbdLevelLow] = 0;
        parameters[cs80::P_kbdLevelHigh] = 0;
        parameters[cs80::P_kbdBrillianceLow] = parameters[cs80::P_kbdBrillianceHigh] = 0;
        parameters[cs80::P_portamento] = 0;
        parameters[cs80::P_sustainMode] = 0;
        parameters[cs80::P_glideMode] = 0;
        parameters[cs80::P_glideAssign] = 0;
        parameters[cs80::P_footswitchPlugged] = parameters[cs80::P_footswitchPressed] = 0;
        parameters[cs80::P_sustainAssign] = 0;
        parameters[cs80::P_macro1] = parameters[cs80::P_macro2]
            = parameters[cs80::P_macro3] = parameters[cs80::P_macro4] = .5f;
        parameters[cs80::P_arpOn] = parameters[cs80::P_kasScanTiming] = 0;
        int scenarios = 0;
        parameters[cs80::P_mix] = .5f;
        for (int rank : {0, 1}) {
            parameters[cs80::chParam(rank, cs80::CP_vLevel)] = .35f;
            for (int local : {cs80::CP_initBrill, cs80::CP_initLevel, cs80::CP_afterBrill, cs80::CP_afterLevel})
                parameters[cs80::chParam(rank, local)] = 0;
        }
        for (int lane : {42, 43, 44, 45})
        for (int voices : {8, 16})
            for (int source : {13, 0, 11})

        {
            auto p = parameters;
            p[cs80::P_voiceMode] = voices == 16 ? 1.f : 0.f;
            const int parameter = cs80::kSampleAccurateParameters[lane];
            p[parameter] = .15f;
            const double initial = normalized(parameter, p[parameter]);
            Curves curves, constant;
            constant[lane] = {{0, initial}, {4095, initial}};
            // Live held initial touch b; selected sources keep their original masks.
            curves[lane] = {{0, initial}, {128, initial},
                {511, normalized(parameter, .6f)}, {1023, normalized(parameter, .2f)},
                {1791, normalized(parameter, .7f)}, {2559, normalized(parameter, .1f)},
                {3583, normalized(parameter, .5f)}, {4095, normalized(parameter, .25f)}};
            const auto initial_touchRender = [&](const cs80::ParamValues& values, int block,
                                         bool automate = true, bool flatten = false,
                                         bool nullChanges = false)
            {
                return render(module, values, curves, block, automate, flatten, source, nullChanges);
            };
            const auto neutral = initial_touchRender(p, 256, false);
            check(neutral == initial_touchRender(p, 256, false, false, true), "initial_touch empty/no queue identical");
            const std::string id = std::string("initial_touch_lane") + std::to_string(lane) + "-" + std::to_string(voices) + "-" + std::to_string(source);
            captureInitialTouch(out / (id + "-noqueue.pcm"), neutral);
            for (int policy : {0, 2})
            {
                auto protectedP = p;
                protectedP[cs80::P_sourcePerformance] = policy == 0 ? 0.f : 1.f;
                protectedP[cs80::P_parityMode] = policy == 2 ? 1.f : 0.f;
                const auto actual = initial_touchRender(protectedP, 256);
                check(actual == initial_touchRender(protectedP, 256, true, true), "initial_touch Legacy/PURE preserve flattening");
                captureInitialTouch(out / (id + "-policy" + std::to_string(policy) + ".pcm"), actual);
            }
            if (!compatibilityOnly)
            {
                const auto scalar = initial_touchRender(p, 1);
                double error = 0;
                for (int block : {7, 64, 256, 1024, 4096})
                    error = std::max(error, difference(scalar, initial_touchRender(p, block)));
                check(error <= limits["audio_max_abs_error"], "initial_touch queues match per-sample reference across partitions");
                const auto constantAudio = render(module, p, constant, 1, true, false, source, false);
                require(scalar.size() == 8192 && constantAudio.size() == scalar.size(), "full stereo capture size");
                const size_t split = 2 * 128;
                check(std::equal(scalar.begin(), scalar.begin() + split, constantAudio.begin()),
                      "initial touch prefix before initial hold ends is exact");
                const std::vector<float> tail(scalar.begin() + split, scalar.end());
                const std::vector<float> heldTail(constantAudio.begin() + split, constantAudio.end());
                const double effect = difference(tail, heldTail);
                if (source == 13) check(effect > .00001, "PANEL initial touch has positive later PCM effect");
                else check(scalar == constantAudio, "factory/MEMORY preserves exact hidden-panel initial-touch mask");
                std::cout << "initial_touch=" << id << " max_error=" << error << " effect=" << effect
                          << " prefix_exact=" << std::equal(scalar.begin(), scalar.begin() + split, constantAudio.begin())
                          << '\n';
            }
            else std::cout << "compatibility=" << id << '\n';
            ++scenarios;
        }
        std::cout << scenarios << (compatibilityOnly ? " initial_touch compatibility scenarios, " : " native initial_touch scenarios, ")
                  << failures << " failures\n";
        return failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
