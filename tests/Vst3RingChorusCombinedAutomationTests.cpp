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
                "usage: ring-chorus-combined-test MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE [--compat-only]");
        const bool compatibilityOnly = argc == 8;
        if (compatibilityOnly) require(std::string(argv[7]) == "--compat-only", "explicit compatibility-only mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported sample rate");
        require(mode == "realtime" || mode == "offline", "supported process mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances limits(argv[2], "host_automation"), ringLimits(argv[3], "host_automation_ring_chorus_combined");
        require(ringLimits["key_parameter_count"] == 35 && cs80::kSampleAccurateParameters.size() >= 35,
                "historical 35-lane ring/effect integration capability prefix");
        require(std::equal(previousRoster.begin(), previousRoster.end(), cs80::kSampleAccurateParameters.begin()),
                "entire preceding 31-lane roster/order unchanged");
        require(cs80::kSampleAccurateParameters[31] == cs80::P_ringAttack
                && cs80::kSampleAccurateParameters[32] == cs80::P_ringDecay
                && cs80::kSampleAccurateParameters[33] == cs80::P_chorusSpeed
                && cs80::kSampleAccurateParameters[34] == cs80::P_chorusDepth, "exact ring-time and chorus lanes");
        std::cout << "rate=" << rate << " mode=" << mode
                  << " gate=" << (compatibilityOnly ? "compatibility-only (feature assertions not run)" : "full-ring-chorus-combined") << '\n';
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
        parameters[cs80::P_ringAttack] = .08f;
        parameters[cs80::P_ringDecay] = .45f;
        parameters[cs80::P_chorusOn] = 1;
        parameters[cs80::P_chorusSpeed] = 3;
        parameters[cs80::P_chorusDepth] = .7f;
        parameters[cs80::P_macro1] = parameters[cs80::P_macro2]
            = parameters[cs80::P_macro3] = parameters[cs80::P_macro4] = .5f;
        int scenarios = 0;
        for (int effectMode : {0, 1})
        for (int voices : {8, 16})
            for (const auto& sources : std::array<std::array<int, 2>, 4> {{{13, 0}, {0, 13}, {13, 11}, {11, 13}}})
        {
            const int source = sources[0], secondSource = sources[1];
            auto p = parameters;
            p[cs80::P_chorusMode] = float(effectMode);
            p[cs80::P_voiceMode] = voices == 16 ? 1.f : 0.f;
            Curves curves, constant;
            for (int lane : {28, 29, 30, 31, 32, 33, 34})
            {
                const int parameter = cs80::kSampleAccurateParameters[lane];
                const double initial = normalized(parameter, p[parameter]);
                constant[lane] = {{0, initial}, {4095, initial}};
            }
            curves[28] = {{0, normalized(cs80::P_ringSpeed, p[cs80::P_ringSpeed])},
                          {255, .1}, {767, .35}, {1535, .2}, {2303, .55}, {3583, .15}, {4095, .3}};
            curves[29] = {{0, normalized(cs80::P_ringAmount, p[cs80::P_ringAmount])},
                          {383, .3}, {895, .65}, {1791, .45}, {2559, .8}, {3327, .4}, {4095, .6}};
            curves[30] = {{0, normalized(cs80::P_ringDepth, p[cs80::P_ringDepth])},
                          {511, .15}, {1023, .7}, {1536, .25}, {2047, .8}, {3071, .2}, {4095, .6}};
            for (int lane : {31, 32})
            {
                const int parameter = cs80::kSampleAccurateParameters[lane];
                const double initial = normalized(parameter, p[parameter]);
                const double target = normalized(parameter, lane == 31 ? .003f : .02f);
                // AD times remain initial through all first engine onsets17..32;
                // distinct knots reach the second targets before2048..2063.
                curves[lane] = {{0, initial}, {64, initial}, {lane == 31 ? 1408 : 1664, target}, {4095, target}};
            }
            for (int lane : {33, 34})
            {
                const int parameter = cs80::kSampleAccurateParameters[lane];
                curves[lane] = {{0, normalized(parameter, p[parameter])},
                    {255, normalized(parameter, lane == 33 ? 1.f : .15f)},
                    {767, normalized(parameter, lane == 33 ? 7.f : .85f)},
                    {1535, normalized(parameter, lane == 33 ? 2.f : .3f)},
                    {2303, normalized(parameter, lane == 33 ? 6.f : .75f)},
                    {3583, normalized(parameter, lane == 33 ? 1.5f : .2f)},
                    {4095, normalized(parameter, lane == 33 ? 5.f : .6f)}};
            }
            const auto renderCurves = [&](const cs80::ParamValues& values, const Curves& selected,
                                           int block, bool automate = true,
                                           bool flatten = false, bool nullChanges = false)
            {
                return render(module, values, selected, block, automate, flatten, source,
                              nullChanges, secondSource, false, true);
            };
            const auto ringRender = [&](const cs80::ParamValues& values, int block,
                                         bool automate = true, bool flatten = false,
                                         bool nullChanges = false)
            {
                return renderCurves(values, curves, block, automate, flatten, nullChanges);
            };
            const auto neutral = ringRender(p, 256, false);
            check(neutral == ringRender(p, 256, false, false, true), "ring empty/no queue identical");
            const std::string id = std::string(effectMode == 0 ? "chorus-" : "tremolo-") + std::to_string(voices) + "-" + std::to_string(source) + "-" + std::to_string(secondSource);
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
                const auto constantAudio = renderCurves(p, constant, 1);
                const double effect = difference(scalar, constantAudio);
                check(effect > .00001, "seven ring/effect queues have aggregate PCM effect versus active holds");
                std::cout << "ring=" << id << " max_error=" << error
                          << " aggregate_effect=" << effect << '\n';
            }
            else std::cout << "compatibility=" << id << '\n';
            ++scenarios;
        }
        std::cout << scenarios << (compatibilityOnly ? " ring-chorus-combined compatibility scenarios, " : " native ring-chorus-combined scenarios, ")
                  << failures << " failures\n";
        return failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
