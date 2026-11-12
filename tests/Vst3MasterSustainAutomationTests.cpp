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
    int(cs80::P_chorusSpeed), int(cs80::P_chorusDepth), int(cs80::P_portamento)
};
void captureMasterSustain(const std::filesystem::path& path, const std::vector<float>& audio)
{
    std::ofstream file(path, std::ios::binary);
    require(bool(file), "master_sustain capture open");
    file.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
    require(bool(file), "complete master_sustain capture write");
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 7 || argc == 8,
                "usage: master_sustain-test MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE [--compat-only]");
        const bool compatibilityOnly = argc == 8;
        if (compatibilityOnly) require(std::string(argv[7]) == "--compat-only", "explicit compatibility-only mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported sample rate");
        require(mode == "realtime" || mode == "offline", "supported process mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances limits(argv[2], "host_automation"), master_sustainLimits(argv[3], "host_automation_master_sustain");
        require(master_sustainLimits["key_parameter_count"] == 37 && cs80::kSampleAccurateParameters.size() >= 37,
                "historical 37-lane master_sustain capability");
        require(std::equal(previousRoster.begin(), previousRoster.end(), cs80::kSampleAccurateParameters.begin()),
                "entire preceding 36-lane roster/order unchanged");
        require(cs80::kSampleAccurateParameters[36] == cs80::P_masterSustain, "exact master_sustain lane");
        std::cout << "rate=" << rate << " mode=" << mode
                  << " gate=" << (compatibilityOnly ? "compatibility-only (feature assertions not run)" : "full-master_sustain") << '\n';
        std::filesystem::create_directories(argv[4]);
        const std::filesystem::path out(argv[4]);
        Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
        parameters[cs80::P_masterSustain] = .12f;
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
            constexpr int lane = 36;
            const int parameter = cs80::P_masterSustain;
            const double initial = normalized(parameter, p[parameter]);
            Curves curves, constant;
            constant[lane] = {{0, initial}, {4095, initial}};
            // All initial note-offs (3072..3087) precede the release-only change.
            // Minimum performance release .4s exceeds the selected source floors.
            curves[lane] = {{0, initial}, {3100, initial},
                {3327, normalized(parameter, .04f)},
                {3583, normalized(parameter, .20f)},
                {3839, normalized(parameter, .06f)},
                {4095, normalized(parameter, .15f)}};
            const auto master_sustainRender = [&](const cs80::ParamValues& values, int block,
                                         bool automate = true, bool flatten = false,
                                         bool nullChanges = false)
            {
                return render(module, values, curves, block, automate, flatten, source, nullChanges);
            };
            const auto neutral = master_sustainRender(p, 256, false);
            check(neutral == master_sustainRender(p, 256, false, false, true), "master_sustain empty/no queue identical");
            const std::string id = std::string("master_sustain") + "-" + std::to_string(voices) + "-" + std::to_string(source);
            captureMasterSustain(out / (id + "-noqueue.pcm"), neutral);
            for (int policy : {0, 2})
            {
                auto protectedP = p;
                protectedP[cs80::P_sourcePerformance] = policy == 0 ? 0.f : 1.f;
                protectedP[cs80::P_parityMode] = policy == 2 ? 1.f : 0.f;
                const auto actual = master_sustainRender(protectedP, 256);
                check(actual == master_sustainRender(protectedP, 256, true, true), "master_sustain Legacy/PURE preserve flattening");
                captureMasterSustain(out / (id + "-policy" + std::to_string(policy) + ".pcm"), actual);
            }
            if (!compatibilityOnly)
            {
                const auto scalar = master_sustainRender(p, 1);
                double error = 0;
                for (int block : {7, 64, 256, 1024, 4096})
                    error = std::max(error, difference(scalar, master_sustainRender(p, block)));
                check(error <= limits["audio_max_abs_error"], "master_sustain queues match per-sample reference across partitions");
                const auto constantAudio = render(module, p, constant, 1, true, false, source);
                require(scalar.size() == 8192 && constantAudio.size() == scalar.size(), "full stereo capture size");
                const size_t split = 2 * 3100;
                check(std::equal(scalar.begin(), scalar.begin() + split, constantAudio.begin()),
                      "master sustain prefix before release-only change is exact");
                const std::vector<float> tail(scalar.begin() + split, scalar.end());
                const std::vector<float> heldTail(constantAudio.begin() + split, constantAudio.end());
                const double effect = difference(tail, heldTail);
                check(effect > .00001, "master_sustain automation has positive released-tail PCM effect versus active hold");
                auto off = p;
                off[cs80::P_sustainAssign] = 1;
                off[cs80::P_footswitchPlugged] = 1;
                off[cs80::P_footswitchPressed] = 0;
                const auto offQueued = render(module, off, curves, 1, true, false, source);
                const auto offHold = render(module, off, constant, 1, true, false, source);
                check(offQueued == offHold, "master_sustain off is PCM-output inert under explicit open assigned gate");
                std::cout << "master_sustain=" << id << " max_error=" << error << " effect=" << effect
                          << " prefix_exact=" << std::equal(scalar.begin(), scalar.begin() + split, constantAudio.begin())
                          << " bypass_error=" << difference(offQueued, offHold) << '\n';
            }
            else std::cout << "compatibility=" << id << '\n';
            ++scenarios;
        }
        std::cout << scenarios << (compatibilityOnly ? " master_sustain compatibility scenarios, " : " native master_sustain scenarios, ")
                  << failures << " failures\n";
        return failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
