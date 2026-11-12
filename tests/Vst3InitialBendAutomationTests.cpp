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
    int(cs80::P_chorusSpeed), int(cs80::P_chorusDepth), int(cs80::P_portamento), int(cs80::P_masterSustain)
};
void captureInitialBend(const std::filesystem::path& path, const std::vector<float>& audio)
{
    std::ofstream file(path, std::ios::binary);
    require(bool(file), "initial_bend capture open");
    file.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
    require(bool(file), "complete initial_bend capture write");
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 7 || argc == 8,
                "usage: initial_bend-test MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE [--compat-only]");
        const bool compatibilityOnly = argc == 8;
        if (compatibilityOnly) require(std::string(argv[7]) == "--compat-only", "explicit compatibility-only mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported sample rate");
        require(mode == "realtime" || mode == "offline", "supported process mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances limits(argv[2], "host_automation"), initial_bendLimits(argv[3], "host_automation_initial_bend");
        require(initial_bendLimits["key_parameter_count"] == 38 && cs80::kSampleAccurateParameters.size() == 38,
                "exact 38-lane initial_bend capability");
        require(std::equal(previousRoster.begin(), previousRoster.end(), cs80::kSampleAccurateParameters.begin()),
                "entire preceding 37-lane roster/order unchanged");
        require(cs80::kSampleAccurateParameters[37] == cs80::P_initBend, "exact initial_bend lane");
        std::cout << "rate=" << rate << " mode=" << mode
                  << " gate=" << (compatibilityOnly ? "compatibility-only (feature assertions not run)" : "full-initial_bend") << '\n';
        std::filesystem::create_directories(argv[4]);
        const std::filesystem::path out(argv[4]);
        Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
        parameters[cs80::P_initBend] = .2f;
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
        for (int voices : {8, 16})
            for (int source : {13, 0, 11})

        {
            auto p = parameters;
            p[cs80::P_voiceMode] = voices == 16 ? 1.f : 0.f;
            constexpr int lane = 37;
            const int parameter = cs80::P_initBend;
            const double initial = normalized(parameter, p[parameter]);
            Curves curves, constant;
            constant[lane] = {{0, initial}, {4095, initial}};
            // Fixed positive-velocity strikes drive modern TWS C; knob changes
            // follow the exact initial normalized hold through sample64.
            curves[lane] = {{0, initial}, {64, initial},
                {255, normalized(parameter, .8f)},
                {767, normalized(parameter, .3f)},
                {1535, normalized(parameter, .9f)},
                {2303, normalized(parameter, .15f)},
                {3583, normalized(parameter, .65f)},
                {4095, normalized(parameter, .4f)}};
            const auto initial_bendRender = [&](const cs80::ParamValues& values, int block,
                                         bool automate = true, bool flatten = false,
                                         bool nullChanges = false)
            {
                return render(module, values, curves, block, automate, flatten, source, nullChanges);
            };
            const auto neutral = initial_bendRender(p, 256, false);
            check(neutral == initial_bendRender(p, 256, false, false, true), "initial_bend empty/no queue identical");
            const std::string id = std::string("initial_bend") + "-" + std::to_string(voices) + "-" + std::to_string(source);
            captureInitialBend(out / (id + "-noqueue.pcm"), neutral);
            for (int policy : {0, 2})
            {
                auto protectedP = p;
                protectedP[cs80::P_sourcePerformance] = policy == 0 ? 0.f : 1.f;
                protectedP[cs80::P_parityMode] = policy == 2 ? 1.f : 0.f;
                const auto actual = initial_bendRender(protectedP, 256);
                check(actual == initial_bendRender(protectedP, 256, true, true), "initial_bend Legacy/PURE preserve flattening");
                captureInitialBend(out / (id + "-policy" + std::to_string(policy) + ".pcm"), actual);
            }
            if (!compatibilityOnly)
            {
                const auto scalar = initial_bendRender(p, 1);
                double error = 0;
                for (int block : {7, 64, 256, 1024, 4096})
                    error = std::max(error, difference(scalar, initial_bendRender(p, block)));
                check(error <= limits["audio_max_abs_error"], "initial_bend queues match per-sample reference across partitions");
                const auto constantAudio = render(module, p, constant, 1, true, false, source);
                require(scalar.size() == 8192 && constantAudio.size() == scalar.size(), "full stereo capture size");
                const size_t split = 2 * 64;
                check(std::equal(scalar.begin(), scalar.begin() + split, constantAudio.begin()),
                      "initial bend prefix before initial hold ends is exact");
                const std::vector<float> tail(scalar.begin() + split, scalar.end());
                const std::vector<float> heldTail(constantAudio.begin() + split, constantAudio.end());
                const double effect = difference(tail, heldTail);
                check(effect > .00001, "initial_bend automation has positive later PCM effect versus active hold");
                std::cout << "initial_bend=" << id << " max_error=" << error << " effect=" << effect
                          << " prefix_exact=" << std::equal(scalar.begin(), scalar.begin() + split, constantAudio.begin())
                          << '\n';
            }
            else std::cout << "compatibility=" << id << '\n';
            ++scenarios;
        }
        std::cout << scenarios << (compatibilityOnly ? " initial_bend compatibility scenarios, " : " native initial_bend scenarios, ")
                  << failures << " failures\n";
        return failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
