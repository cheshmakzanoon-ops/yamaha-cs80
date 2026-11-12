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
    int(cs80::P_ringAttack), int(cs80::P_ringDecay)
};
void captureTremolo(const std::filesystem::path& path, const std::vector<float>& audio)
{
    std::ofstream file(path, std::ios::binary);
    require(bool(file), "tremolo capture open");
    file.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
    require(bool(file), "complete tremolo capture write");
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 7 || argc == 8,
                "usage: tremolo-test MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE [--compat-only]");
        const bool compatibilityOnly = argc == 8;
        if (compatibilityOnly) require(std::string(argv[7]) == "--compat-only", "explicit compatibility-only mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported sample rate");
        require(mode == "realtime" || mode == "offline", "supported process mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances limits(argv[2], "host_automation"), tremoloLimits(argv[3], "host_automation_tremolo");
        require(tremoloLimits["key_parameter_count"] == 35 && cs80::kSampleAccurateParameters.size() == 35,
                "exact 35-lane tremolo capability");
        require(std::equal(previousRoster.begin(), previousRoster.end(), cs80::kSampleAccurateParameters.begin()),
                "entire preceding 33-lane roster/order unchanged");
        require(cs80::kSampleAccurateParameters[33] == cs80::P_chorusSpeed
                && cs80::kSampleAccurateParameters[34] == cs80::P_chorusDepth, "exact appended chorus lanes");
        std::cout << "rate=" << rate << " mode=" << mode
                  << " gate=" << (compatibilityOnly ? "compatibility-only (feature assertions not run)" : "full-tremolo") << '\n';
        std::filesystem::create_directories(argv[4]);
        const std::filesystem::path out(argv[4]);
        Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
        parameters[cs80::P_chorusOn] = 1;
        parameters[cs80::P_chorusMode] = 1;
        parameters[cs80::P_chorusSpeed] = 3;
        parameters[cs80::P_chorusDepth] = .7f;
        parameters[cs80::P_macro1] = parameters[cs80::P_macro2]
            = parameters[cs80::P_macro3] = parameters[cs80::P_macro4] = .5f;
        parameters[cs80::P_arpOn] = parameters[cs80::P_kasScanTiming] = 0;
        int scenarios = 0;
        for (int voices : {8, 16})
            for (int source : {13, 0, 11})
                for (int lane : {33, 34})
        {
            auto p = parameters;
            p[cs80::P_voiceMode] = voices == 16 ? 1.f : 0.f;
            const int parameter = cs80::kSampleAccurateParameters[lane];
            const double initial = normalized(parameter, p[parameter]);
            Curves curves, constant;
            constant[lane] = {{0, initial}, {4095, initial}};
            curves[lane] = {{0, initial},
                {255, normalized(parameter, lane == 33 ? 1.f : .15f)},
                {767, normalized(parameter, lane == 33 ? 7.f : .85f)},
                {1535, normalized(parameter, lane == 33 ? 2.f : .3f)},
                {2303, normalized(parameter, lane == 33 ? 6.f : .75f)},
                {3583, normalized(parameter, lane == 33 ? 1.5f : .2f)},
                {4095, normalized(parameter, lane == 33 ? 5.f : .6f)}};
            const auto tremoloRender = [&](const cs80::ParamValues& values, int block,
                                         bool automate = true, bool flatten = false,
                                         bool nullChanges = false)
            {
                return render(module, values, curves, block, automate, flatten, source, nullChanges);
            };
            const auto neutral = tremoloRender(p, 256, false);
            check(neutral == tremoloRender(p, 256, false, false, true), "tremolo empty/no queue identical");
            const std::string id = std::string(lane == 33 ? "speed" : "depth") + "-" + std::to_string(voices) + "-" + std::to_string(source);
            captureTremolo(out / (id + "-noqueue.pcm"), neutral);
            for (int policy : {0, 2})
            {
                auto protectedP = p;
                protectedP[cs80::P_sourcePerformance] = policy == 0 ? 0.f : 1.f;
                protectedP[cs80::P_parityMode] = policy == 2 ? 1.f : 0.f;
                const auto actual = tremoloRender(protectedP, 256);
                check(actual == tremoloRender(protectedP, 256, true, true), "tremolo Legacy/PURE preserve flattening");
                captureTremolo(out / (id + "-policy" + std::to_string(policy) + ".pcm"), actual);
            }
            if (!compatibilityOnly)
            {
                const auto scalar = tremoloRender(p, 1);
                double error = 0;
                for (int block : {7, 64, 256, 1024, 4096})
                    error = std::max(error, difference(scalar, tremoloRender(p, block)));
                check(error <= limits["audio_max_abs_error"], "tremolo queues match per-sample reference across partitions");
                const auto constantAudio = render(module, p, constant, 1, true, false, source);
                const double effect = difference(scalar, constantAudio);
                check(effect > .00001, "tremolo automation has positive PCM effect versus active hold");
                auto off = p;
                off[cs80::P_chorusOn] = 0;
                const auto offQueued = render(module, off, curves, 1, true, false, source);
                const auto offHold = render(module, off, constant, 1, true, false, source);
                check(offQueued == offHold, "tremolo off is PCM-output inert despite internal clocking");
                double depthZeroError = 0;
                if (lane == 33)
                {
                    auto depthZero = p;
                    depthZero[cs80::P_chorusDepth] = 0;
                    const auto queued = render(module, depthZero, curves, 1, true, false, source);
                    const auto held = render(module, depthZero, constant, 1, true, false, source);
                    check(queued == held, "tremolo speed at depth zero is PCM-output inert only");
                    depthZeroError = difference(queued, held);
                }
                std::cout << "tremolo=" << id << " max_error=" << error << " effect=" << effect
                          << " bypass_error=" << difference(offQueued, offHold)
                          << " depth_zero_control=" << (lane == 33 ? "speed-only" : "not-applicable")
                          << " depth_zero_error=" << depthZeroError << '\n';
            }
            else std::cout << "compatibility=" << id << '\n';
            ++scenarios;
        }
        std::cout << scenarios << (compatibilityOnly ? " tremolo compatibility scenarios, " : " native tremolo scenarios, ")
                  << failures << " failures\n";
        return failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
