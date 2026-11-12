// Only the supported-input native renderer is compiled. Historical broad
// boundary/fuzz bodies and their main are excluded by this macro.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include <fstream>
#include "Vst3Frozen81Roster.h"

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
    int(cs80::P_chorusSpeed), int(cs80::P_chorusDepth), int(cs80::P_portamento), int(cs80::P_masterSustain), int(cs80::P_initBend), int(cs80::P_kbdLevelLow), int(cs80::P_kbdLevelHigh), int(cs80::P_kbdBrillianceLow), int(cs80::P_kbdBrillianceHigh), cs80::chParam(0, cs80::CP_initBrill), cs80::chParam(0, cs80::CP_initLevel), cs80::chParam(1, cs80::CP_initBrill), cs80::chParam(1, cs80::CP_initLevel), cs80::chParam(0, cs80::CP_afterBrill), cs80::chParam(0, cs80::CP_afterLevel), cs80::chParam(1, cs80::CP_afterBrill), cs80::chParam(1, cs80::CP_afterLevel), cs80::chParam(0, cs80::CP_pwmSpeed), cs80::chParam(0, cs80::CP_pwmDepth), cs80::chParam(1, cs80::CP_pwmSpeed), cs80::chParam(1, cs80::CP_pwmDepth), cs80::chParam(0, cs80::CP_vcfLevel), cs80::chParam(1, cs80::CP_vcfLevel), cs80::chParam(0, cs80::CP_vS), cs80::chParam(1, cs80::CP_vS), cs80::chParam(0, cs80::CP_fIl), cs80::chParam(1, cs80::CP_fIl), cs80::chParam(0, cs80::CP_fAl), cs80::chParam(1, cs80::CP_fAl), cs80::chParam(0, cs80::CP_fA), cs80::chParam(0, cs80::CP_vA), cs80::chParam(1, cs80::CP_fA), cs80::chParam(1, cs80::CP_vA), cs80::chParam(0, cs80::CP_fD), cs80::chParam(0, cs80::CP_vD), cs80::chParam(1, cs80::CP_fD), cs80::chParam(1, cs80::CP_vD), cs80::chParam(0, cs80::CP_fR), cs80::chParam(0, cs80::CP_vR), cs80::chParam(1, cs80::CP_fR), cs80::chParam(1, cs80::CP_vR), cs80::chParam(0, cs80::CP_sine), cs80::chParam(0, cs80::CP_noise), cs80::chParam(1, cs80::CP_sine), cs80::chParam(1, cs80::CP_noise), int(cs80::P_ribbon), int(cs80::P_keyRelease)
};
void captureTuningRef(const std::filesystem::path& path, const std::vector<float>& audio)
{
    std::ofstream file(path, std::ios::binary);
    require(bool(file), "tuning_ref capture open");
    file.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
    require(bool(file), "complete tuning_ref capture write");
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 7 || argc == 8,
                "usage: tuning_ref-test MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE [--compat-only]");
        const bool compatibilityOnly = argc == 8;
        if (compatibilityOnly) require(std::string(argv[7]) == "--compat-only", "explicit compatibility-only mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported sample rate");
        require(mode == "realtime" || mode == "offline", "supported process mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances limits(argv[2], "host_automation"), tuning_refLimits(argv[3], "host_automation_tuning_ref");
        require(tuning_refLimits["key_parameter_count"] == 81 && cs80::kSampleAccurateParameters.size() >= 81,
                "historical 81-lane tuning_ref contract retained");
        require(std::equal(cs80::kFrozen81Roster.begin(), cs80::kFrozen81Roster.end(),
                           cs80::kSampleAccurateParameters.begin()), "literal 81-lane prefix unchanged");
        require(std::equal(previousRoster.begin(), previousRoster.end(), cs80::kSampleAccurateParameters.begin()),
                "entire preceding 80-lane roster/order unchanged");
        constexpr std::array added {int(cs80::P_tuningRef)};
        require(std::equal(added.begin(), added.end(), cs80::kSampleAccurateParameters.begin() + 80), "exact TUNING REF lanes");
        std::cout << "rate=" << rate << " mode=" << mode
                  << " gate=" << (compatibilityOnly ? "compatibility-only (feature assertions not run)" : "full-tuning_ref") << '\n';
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
        parameters[cs80::P_initBend] = 0;
        parameters[cs80::P_ribbonCcEnable] = 0;
        parameters[cs80::P_sustainMode] = 0;
        parameters[cs80::P_glideMode] = 0;
        parameters[cs80::P_glideAssign] = 0;
        parameters[cs80::P_footswitchPlugged] = parameters[cs80::P_footswitchPressed] = 0;
        parameters[cs80::P_sustainAssign] = 0;
        parameters[cs80::P_macro1] = parameters[cs80::P_macro2]
            = parameters[cs80::P_macro3] = parameters[cs80::P_macro4] = .5f;
        parameters[cs80::P_arpOn] = parameters[cs80::P_kasScanTiming] = 0;
        parameters[cs80::P_lfo1Depth] = parameters[cs80::P_lfo2Depth] = parameters[cs80::P_lfo3Depth] = 0;
        for (int i = cs80::P_mod1Amount; i <= cs80::P_mod16Amount; ++i) parameters[i] = 0;
        int scenarios = 0;
        parameters[cs80::P_mix] = .5f;
        parameters[cs80::P_masterSustain] = 0;
        parameters[cs80::P_keyRelease] = .5f;
        parameters[cs80::P_ribbon] = 0;
        // Neutral motion macro above yields Engine envTimeScale == 1; dispersion is zero.
        parameters[cs80::P_mpeOn] = parameters[cs80::P_pressureCurve] = parameters[cs80::P_pressureThreshold] = 0;
        for (int rank : {0, 1}) {
            parameters[cs80::chParam(rank, cs80::CP_vLevel)] = .35f;
            parameters[cs80::chParam(rank, cs80::CP_vA)] = .001f;
            parameters[cs80::chParam(rank, cs80::CP_vD)] = .002f;
            parameters[cs80::chParam(rank, cs80::CP_vS)] = .7f;
            parameters[cs80::chParam(rank, cs80::CP_vR)] = .5f;
            parameters[cs80::chParam(rank, cs80::CP_fA)] = .001f;
            parameters[cs80::chParam(rank, cs80::CP_fD)] = .002f;
            parameters[cs80::chParam(rank, cs80::CP_fAl)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_fIl)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_fR)] = .30f;
            parameters[cs80::chParam(rank, cs80::CP_lpfCutoff)] = 4000;
            parameters[cs80::chParam(rank, cs80::CP_hpfCutoff)] = 25;
            parameters[cs80::chParam(rank, cs80::CP_lpfRes)] = parameters[cs80::chParam(rank, cs80::CP_hpfRes)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_vcfLevel)] = .78f;
            parameters[cs80::chParam(rank, cs80::CP_pulse)] = 1;
            parameters[cs80::chParam(rank, cs80::CP_saw)] = parameters[cs80::chParam(rank, cs80::CP_sine)] = parameters[cs80::chParam(rank, cs80::CP_noise)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_pw)] = .5f;
            parameters[cs80::chParam(rank, cs80::CP_pwmSpeed)] = 3;
            parameters[cs80::chParam(rank, cs80::CP_pwmDepth)] = 0;
            for (int local : {cs80::CP_initBrill, cs80::CP_initLevel, cs80::CP_afterBrill, cs80::CP_afterLevel})
                parameters[cs80::chParam(rank, local)] = 0;
        }
        for (int lane : {80})
        for (int voices : {8, 16})
            for (int source : {13, 0, 11})

        {
            auto p = parameters;
            p[cs80::P_voiceMode] = voices == 16 ? 1.f : 0.f;
            const int parameter = cs80::kSampleAccurateParameters[lane];
            p[parameter] = 440.f;
            const double initial = normalized(parameter, p[parameter]);
            Curves curves, constant;
            constant[lane] = {{0, initial}, {4095, initial}};
            const std::array<float, 6> targets {432.f, 452.f, 438.f, 460.f, 425.f, 446.f};
            curves[lane] = {{0, initial}, {128, initial},
                {511, normalized(parameter, targets[0])}, {1023, normalized(parameter, targets[1])},
                {1791, normalized(parameter, targets[2])}, {2559, normalized(parameter, targets[3])},
                {3583, normalized(parameter, targets[4])}, {4095, normalized(parameter, targets[5])}};
            const auto tuning_refRender = [&](const cs80::ParamValues& values, int block,
                                         bool automate = true, bool flatten = false,
                                         bool nullChanges = false)
            {
                return render(module, values, curves, block, automate, flatten, source, nullChanges, -1, false);
            };
            const auto neutral = tuning_refRender(p, 256, false);
            check(neutral == tuning_refRender(p, 256, false, false, true), "tuning_ref empty/no queue identical");
            const std::string id = std::string("tuning_ref_lane") + std::to_string(lane) + "-" + std::to_string(voices) + "-" + std::to_string(source);
            captureTuningRef(out / (id + "-noqueue.pcm"), neutral);
            for (int policy : {0, 2})
            {
                auto protectedP = p;
                protectedP[cs80::P_sourcePerformance] = policy == 0 ? 0.f : 1.f;
                protectedP[cs80::P_parityMode] = policy == 2 ? 1.f : 0.f;
                const auto actual = tuning_refRender(protectedP, 256);
                check(actual == tuning_refRender(protectedP, 256, true, true), "tuning_ref Legacy/PURE preserve flattening");
                captureTuningRef(out / (id + "-policy" + std::to_string(policy) + ".pcm"), actual);
            }
            if (!compatibilityOnly)
            {
                const auto scalar = tuning_refRender(p, 1);
                double error = 0;
                for (int block : {7, 64, 256, 1024, 4096})
                    error = std::max(error, difference(scalar, tuning_refRender(p, block)));
                check(error <= limits["audio_max_abs_error"], "tuning_ref queues match per-sample reference across partitions");
                const auto constantAudio = render(module, p, constant, 1, true, false, source, false, -1, false);
                require(scalar.size() == 8192 && constantAudio.size() == scalar.size(), "full stereo capture size");
                const size_t split = 2 * 128;
                check(std::equal(scalar.begin(), scalar.begin() + split, constantAudio.begin()),
                      "TUNING REF prefix before initial hold ends is exact");
                const std::vector<float> tail(scalar.begin() + split, scalar.begin() + 2 * 3072);
                const std::vector<float> heldTail(constantAudio.begin() + split, constantAudio.begin() + 2 * 3072);
                const double effect = difference(tail, heldTail);
                check(effect > .00001, "global TUNING REF has positive held-window PCM effect for each source");
                std::cout << "tuning_ref=" << id << " max_error=" << error << " effect=" << effect
                          << " prefix_exact=" << std::equal(scalar.begin(), scalar.begin() + split, constantAudio.begin())
                          << '\n';
            }
            else std::cout << "compatibility=" << id << '\n';
            ++scenarios;
        }
        std::cout << scenarios << (compatibilityOnly ? " tuning_ref compatibility scenarios, " : " native tuning_ref scenarios, ")
                  << failures << " failures\n";
        return failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
