// Only the supported-input native renderer is compiled. Historical broad
// boundary/fuzz bodies and their main are excluded by this macro.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include <fstream>

namespace {
constexpr std::array expectedRoster {
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
    int(cs80::P_chorusSpeed), int(cs80::P_chorusDepth), int(cs80::P_portamento), int(cs80::P_masterSustain), int(cs80::P_initBend), int(cs80::P_kbdLevelLow), int(cs80::P_kbdLevelHigh), int(cs80::P_kbdBrillianceLow), int(cs80::P_kbdBrillianceHigh), cs80::chParam(0, cs80::CP_initBrill), cs80::chParam(0, cs80::CP_initLevel), cs80::chParam(1, cs80::CP_initBrill), cs80::chParam(1, cs80::CP_initLevel), cs80::chParam(0, cs80::CP_afterBrill), cs80::chParam(0, cs80::CP_afterLevel), cs80::chParam(1, cs80::CP_afterBrill), cs80::chParam(1, cs80::CP_afterLevel), cs80::chParam(0, cs80::CP_pwmSpeed), cs80::chParam(0, cs80::CP_pwmDepth), cs80::chParam(1, cs80::CP_pwmSpeed), cs80::chParam(1, cs80::CP_pwmDepth), cs80::chParam(0, cs80::CP_vcfLevel), cs80::chParam(1, cs80::CP_vcfLevel), cs80::chParam(0, cs80::CP_vS), cs80::chParam(1, cs80::CP_vS), cs80::chParam(0, cs80::CP_fIl), cs80::chParam(1, cs80::CP_fIl), cs80::chParam(0, cs80::CP_fAl), cs80::chParam(1, cs80::CP_fAl), cs80::chParam(0, cs80::CP_fA), cs80::chParam(0, cs80::CP_vA), cs80::chParam(1, cs80::CP_fA), cs80::chParam(1, cs80::CP_vA), cs80::chParam(0, cs80::CP_fD), cs80::chParam(0, cs80::CP_vD), cs80::chParam(1, cs80::CP_fD), cs80::chParam(1, cs80::CP_vD), cs80::chParam(0, cs80::CP_fR), cs80::chParam(0, cs80::CP_vR), cs80::chParam(1, cs80::CP_fR), cs80::chParam(1, cs80::CP_vR), cs80::chParam(0, cs80::CP_sine), cs80::chParam(0, cs80::CP_noise), cs80::chParam(1, cs80::CP_sine), cs80::chParam(1, cs80::CP_noise), int(cs80::P_ribbon)
};
void captureRibbonReleaseModes(const std::filesystem::path& path, const std::vector<float>& audio)
{
    std::ofstream file(path, std::ios::binary);
    require(bool(file), "ribbon_release_modes capture open");
    file.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
    require(bool(file), "complete ribbon_release_modes capture write");
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 7 || argc == 8,
                "usage: ribbon_release_modes-test MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE [--compat-only]");
        const bool compatibilityOnly = argc == 8;
        if (compatibilityOnly) require(std::string(argv[7]) == "--compat-only", "explicit compatibility-only mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported sample rate");
        require(mode == "realtime" || mode == "offline", "supported process mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances limits(argv[2], "host_automation"), ribbon_release_modesLimits(argv[3], "host_automation_ribbon_release_modes");
        require(ribbon_release_modesLimits["key_parameter_count"] == 79 && cs80::kSampleAccurateParameters.size() == 79,
                "exact 79-lane ribbon_release_modes capability");
        require(std::equal(expectedRoster.begin(), expectedRoster.end(), cs80::kSampleAccurateParameters.begin()),
                "entire 79-lane roster/order unchanged");
        std::cout << "rate=" << rate << " mode=" << mode
                  << " gate=" << (compatibilityOnly ? "compatibility-only (feature assertions not run)" : "full-ribbon_release_modes") << '\n';
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
            parameters[cs80::chParam(rank, cs80::CP_fR)] = .5f;
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
        for (int sustainMode : {0, 1})
        for (int lane : {78})
        for (int voices : {8, 16})
            for (int source : {13, 0, 11})

        {
            auto p = parameters;
            p[cs80::P_sustainMode] = float(sustainMode);
            p[cs80::P_voiceMode] = voices == 16 ? 1.f : 0.f;
            const int parameter = cs80::kSampleAccurateParameters[lane];
            p[parameter] = 0;
            const double initial = normalized(parameter, p[parameter]);
            Curves curves, constant;
            constant[lane] = {{0, initial}, {4095, initial}};
            const std::array<float, 4> targets {.2f, -.15f, .3f, -.2f};
            curves[lane] = {{0, initial}, {3100, initial},
                {3327, normalized(parameter, targets[0])}, {3583, normalized(parameter, targets[1])},
                {3839, normalized(parameter, targets[2])}, {4095, normalized(parameter, targets[3])}};
            const auto ribbon_release_modesRender = [&](const cs80::ParamValues& values, int block,
                                         bool automate = true, bool flatten = false,
                                         bool nullChanges = false)
            {
                return render(module, values, curves, block, automate, flatten, source, nullChanges, -1, false);
            };
            const auto neutral = ribbon_release_modesRender(p, 256, false);
            check(neutral == ribbon_release_modesRender(p, 256, false, false, true), "ribbon_release_modes empty/no queue identical");
            const std::string id = std::string("ribbon_release_modes_mode") + std::to_string(sustainMode) + "-" + std::to_string(voices) + "-" + std::to_string(source);
            captureRibbonReleaseModes(out / (id + "-noqueue.pcm"), neutral);
            for (int policy : {0, 2})
            {
                auto protectedP = p;
                protectedP[cs80::P_sourcePerformance] = policy == 0 ? 0.f : 1.f;
                protectedP[cs80::P_parityMode] = policy == 2 ? 1.f : 0.f;
                const auto actual = ribbon_release_modesRender(protectedP, 256);
                check(actual == ribbon_release_modesRender(protectedP, 256, true, true), "ribbon_release_modes Legacy/PURE preserve flattening");
                captureRibbonReleaseModes(out / (id + "-policy" + std::to_string(policy) + ".pcm"), actual);
            }
            if (!compatibilityOnly)
            {
                const auto scalar = ribbon_release_modesRender(p, 1);
                double error = 0;
                for (int block : {7, 64, 256, 1024, 4096})
                    error = std::max(error, difference(scalar, ribbon_release_modesRender(p, block)));
                check(error <= limits["audio_max_abs_error"], "ribbon_release_modes queues match per-sample reference across partitions");
                const auto constantAudio = render(module, p, constant, 1, true, false, source, false, -1, false);
                require(scalar.size() == 8192 && constantAudio.size() == scalar.size(), "full stereo capture size");
                const size_t split = 2 * 3100;
                check(std::equal(scalar.begin(), scalar.begin() + split, constantAudio.begin()),
                      "RIBBON RELEASE MODES prefix before initial hold ends is exact");
                const std::vector<float> tail(scalar.begin() + split, scalar.end());
                const std::vector<float> heldTail(constantAudio.begin() + split, constantAudio.end());
                const double effect = difference(tail, heldTail);
                if (sustainMode == 0) check(scalar == constantAudio, "Sustain I released ribbon PCM equals its matched active hold");
                else check(effect > .00001, "Sustain II released ribbon has positive tail PCM effect");
                std::cout << "ribbon_release_modes=" << id << " max_error=" << error << " effect=" << effect
                          << " prefix_exact=" << std::equal(scalar.begin(), scalar.begin() + split, constantAudio.begin())
                          << '\n';
            }
            else std::cout << "compatibility=" << id << '\n';
            ++scenarios;
        }
        std::cout << scenarios << (compatibilityOnly ? " ribbon_release_modes compatibility scenarios, " : " native ribbon_release_modes scenarios, ")
                  << failures << " failures\n";
        return failures ? 1 : 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
