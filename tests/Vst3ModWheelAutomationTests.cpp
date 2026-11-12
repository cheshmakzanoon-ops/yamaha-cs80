// Only the supported finite-input renderer is compiled; no historical broad main.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include "Vst3Frozen81Roster.h"
#include <pluginterfaces/vst/ivstmidicontrollers.h>
#include <fstream>
namespace {
ParamID connectCc1(Instance& instance)
{
    TUID id {};
    require(instance.component->getControllerClassId(id) == kResultOk, "CC1 controller class");
    require(instance.factory->createInstance(id, IEditController::iid,
        reinterpret_cast<void**>(&instance.controller)) == kResultOk, "CC1 controller instance");
    require(instance.controller->initialize(nullptr) == kResultOk, "CC1 controller initialize");
    require(instance.component->queryInterface(IConnectionPoint::iid,
        reinterpret_cast<void**>(&instance.componentConnection)) == kResultOk, "CC1 component connection");
    require(instance.controller->queryInterface(IConnectionPoint::iid,
        reinterpret_cast<void**>(&instance.controllerConnection)) == kResultOk, "CC1 controller connection");
    require(instance.componentConnection->connect(instance.controllerConnection) == kResultOk
        && instance.controllerConnection->connect(instance.componentConnection) == kResultOk, "CC1 bidirectional connection");
    IMidiMapping* mapping = nullptr;
    require(instance.controller->queryInterface(IMidiMapping::iid,
        reinterpret_cast<void**>(&mapping)) == kResultOk, "CC1 MIDI mapping interface");
    ParamID cc1 = 0;
    const auto result = mapping->getMidiControllerAssignment(0, 0, kCtrlModWheel, cc1);
    mapping->release();
    require(result == kResultOk, "CC1 mapped on event bus zero/channel one");
    // Instance owns connections until after rendering and tears them down normally.
    return cc1;
}

using NamedCurves = std::vector<std::pair<int, Curve>>;
int renderCount = 0;
std::vector<float> wheelRender(Module& module, const cs80::ParamValues& parameters,
    const NamedCurves& curves, int block, int source, int secondSource,
    int wheel, bool flatten = false)
{
    ++renderCount;
    Instance instance(module, parameters, source, secondSource);
    ParamID cc1 = 0;
    if (wheel >= 0) cc1 = connectCc1(instance);
    std::vector<float> output;
    output.reserve(8192);
    for (int start = 0; start < 4096; start += block) {
        const int count = std::min(block, 4096 - start);
        Changes changes;
        if (wheel >= 0 && start == 0) changes.add(cc1, {{0, double(wheel)}});
        for (const auto& [parameter, curve] : curves) {
            const double initial = normalized(parameter, parameters[parameter]);
            Curve points;
            if (!flatten)
                for (const auto& point : curve)
                    if (point.first >= start && point.first < start + count)
                        points.emplace_back(point.first - start, point.second);
            if (points.empty() || points.back().first != count - 1)
                points.emplace_back(flatten ? 0 : count - 1, absoluteValue(curve, initial, start + count - 1));
            changes.add(parameterId(cs80::paramId(parameter)), std::move(points));
        }
        instance.process(start, count, &changes, output, parameters[cs80::P_voiceMode] > .5f);
    }
    return output;
}
bool exact(const std::vector<float>& a, const std::vector<float>& b)
{
    (void) difference(a, b); // checks sizes and every sample's finiteness
    return std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}
void capture(const std::filesystem::path& path, const std::vector<float>& audio)
{
    (void) difference(audio, audio);
    std::ofstream file(path, std::ios::binary);
    require(bool(file), "capture open");
    file.write(reinterpret_cast<const char*>(audio.data()), std::streamsize(audio.size() * sizeof(float)));
    require(bool(file), "complete capture write");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 7 || argc == 8, "usage: wheel-test MODULE HOST_CONTRACT WHEEL_CONTRACT OUTPUT RATE MODE [--compat-only]");
        const bool compat = argc == 8;
        if (compat) require(std::string(argv[7]) == "--compat-only", "explicit compatibility mode");
        const std::string rate(argv[5]), mode(argv[6]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported rate");
        require(mode == "realtime" || mode == "offline", "supported mode");
        testSampleRate = std::stod(rate); testProcessMode = mode == "offline" ? kOffline : kRealtime;
        const ws1::Tolerances host(argv[2], "host_automation"), limits(argv[3], "host_automation_mod_wheel");
        require(cs80::kSampleAccurateParameters.size() >= 81, "81-lane base available");
        require(std::equal(cs80::kFrozen81Roster.begin(), cs80::kFrozen81Roster.end(),
                           cs80::kSampleAccurateParameters.begin()), "literal frozen81 prefix");
        require(limits["key_parameter_count"] == 83, "new83 contract");
        std::cout << "compiled_lanes=" << cs80::kSampleAccurateParameters.size() << "\n";
        if (!compat) {
            require(cs80::kSampleAccurateParameters.size() == 83, "exact83 capability");
            require(cs80::kSampleAccurateParameters[81] == cs80::P_modWheelSub
                 && cs80::kSampleAccurateParameters[82] == cs80::P_modWheelVcf, "SUB81 VCF82 append order");
        }
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

        // Pin deterministic compatibility SUB oscillator: sine=0, 8 Hz, no external input.
        parameters[cs80::P_subOriginalMode] = 0;
        parameters[cs80::P_subWave] = 0;
        parameters[cs80::P_subSpeed] = 8;
        parameters[cs80::P_subExtLevel] = 0;
        parameters[cs80::P_subVco] = parameters[cs80::P_subVcf] = parameters[cs80::P_subVca] = 0;
        parameters[cs80::P_subAtSpeed] = parameters[cs80::P_subAtVco] = parameters[cs80::P_subAtVcf] = 0;
        for (int route : {0, 1, 2})
        for (int voices : {8, 16})
        for (int source : {13, 0}) {
            auto p = parameters;
            p[cs80::P_voiceMode] = voices == 16 ? 1.f : 0.f;
            p[cs80::P_modWheelSub] = route == 1 ? 0.f : .15f;
            p[cs80::P_modWheelVcf] = route == 0 ? 0.f : .15f;
            NamedCurves curves, constant;
            for (int parameter : {int(cs80::P_modWheelSub), int(cs80::P_modWheelVcf)}) {
                if ((route == 0 && parameter == cs80::P_modWheelVcf)
                    || (route == 1 && parameter == cs80::P_modWheelSub)) continue;
                const double initial = normalized(parameter, p[parameter]);
                curves.push_back({parameter, {{0, initial}, {1023, initial}, {1535, .8},
                                              {2047, .25}, {3071, .9}, {4095, .4}}});
                constant.push_back({parameter, {{0, initial}, {4095, initial}}});
            }
            const std::string id = "wheel-route" + std::to_string(route) + "-voices" + std::to_string(voices)
                                 + "-sources" + std::to_string(source) + "-13";
            // Same captures/settings under frozen81 and candidate83 binaries.
            for (int wheel : {-1, 0, 1})
                capture(out / (id + "-fixed-wheel" + std::to_string(wheel) + ".pcm"),
                        wheelRender(module, p, {}, 256, source, 13, wheel));
            NamedCurves preceding;
            for (int parameter : {int(cs80::P_volume), int(cs80::P_masterTune), int(cs80::P_tuningRef), cs80::chParam(0, cs80::CP_pan)}) {
                const double initial = normalized(parameter, p[parameter]);
                preceding.push_back({parameter, {{0, initial}, {1023, initial}, {1535, .4}, {4095, .6}}});
            }
            capture(out / (id + "-preceding81.pcm"), wheelRender(module, p, preceding, 127, source, 13, 1));
            auto legacy = p; legacy[cs80::P_sourcePerformance] = 0;
            const auto legacyActual = wheelRender(module, legacy, curves, 256, source, 13, 1);
            check(exact(legacyActual, wheelRender(module, legacy, curves, 256, source, 13, 1, true)),
                  "Legacy amount offsets equal block-start final-point flattening byte-for-byte");
            capture(out / (id + "-legacy.pcm"), legacyActual);
            auto pure = p; pure[cs80::P_parityMode] = 1;
            const auto pureActual = wheelRender(module, pure, curves, 256, source, 13, 1);
            check(exact(pureActual, wheelRender(module, pure, constant, 256, source, 13, 1)),
                  "PURE effective wheel depths remain inert");
            capture(out / (id + "-pure.pcm"), pureActual);
            if (!compat) {
                const auto scalar = wheelRender(module, p, curves, 1, source, 13, 1);
                double error = 0;
                for (int block : {64, 127, 256})
                    error = std::max(error, difference(scalar, wheelRender(module, p, curves, block, source, 13, 1)));
                check(error <= host["audio_max_abs_error"], "depth lanes match scalar reference across partitions");
                const auto fixed = wheelRender(module, p, constant, 1, source, 13, 1);
                (void) difference(scalar, fixed);
                require(scalar.size() == 8192, "complete stereo capture");
                check(std::memcmp(scalar.data(), fixed.data(), 2 * 1024 * sizeof(float)) == 0,
                      "initial1024frame hold prefix byte-exact");
                const std::vector<float> changed(scalar.begin() + 2048, scalar.begin() + 6144);
                const std::vector<float> held(fixed.begin() + 2048, fixed.begin() + 6144);
                const double effect = difference(changed, held);
                check(effect > limits["minimum_held_effect"], "positive destination effect in held frames1024to3072");
                check(exact(wheelRender(module, p, curves, 256, source, 13, 0),
                            wheelRender(module, p, constant, 256, source, 13, 0)), "zeroCC1 amount automation is PCM-inert");
                std::cout << id << " max_error=" << error << " held_effect=" << effect << '\n';
            }
            ++scenarios;
        }
        std::cout << "scenarios=" << scenarios << " renders=" << renderCount
                  << " captures=" << scenarios * 6 << " failures=" << failures
                  << " mode=" << (compat ? "compat-only" : "full") << '\n';
        return failures ? 1 : 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
