// Supported finite-input fixture only. No production DSP or automation roster changes.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include <pluginterfaces/vst/ivstmidicontrollers.h>
#include "Vst3Frozen81Roster.h"

namespace {
bool pcmBytesEqual(const std::vector<float>& a, const std::vector<float>& b)
{
    // Retain the shared helper's full finite/size validation, but compare bytes:
    // floating point equality alone would treat positive and negative zero alike.
    (void) difference(a, b);
    return std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}
// Opt-in, confined to this executable. The existing native renderer is untouched.
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
std::vector<float> renderCc1(Module& module, const cs80::ParamValues& parameters,
                            int block, bool connected, const Curve& cc1Points)
{
    Instance instance(module, parameters);
    ParamID cc1 = 0;
    if (connected) cc1 = connectCc1(instance);
    require(connected || cc1Points.empty(), "CC1 points require explicitly connected fixture");
    std::vector<float> result;
    result.reserve(8192);
    for (int start = 0; start < 4096; start += block) {
        const int count = std::min(block, 4096 - start);
        Changes changes;
        Curve points;
        for (const auto& point : cc1Points)
            if (point.first >= start && point.first < start + count)
                points.emplace_back(point.first - start, point.second);
        if (!points.empty()) changes.add(cc1, std::move(points));
        instance.process(start, count, &changes, result);
    }
    return result;
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 4, "usage: cc1-fixture MODULE RATE MODE");
        const std::string rate(argv[2]), mode(argv[3]);
        require(rate == "44100" || rate == "48000" || rate == "96000", "supported rate");
        require(mode == "realtime" || mode == "offline", "supported mode");
        testSampleRate = std::stod(rate);
        testProcessMode = mode == "offline" ? kOffline : kRealtime;
        require(cs80::kSampleAccurateParameters.size() >= 81, "at least 81 production lanes");
        require(std::equal(cs80::kFrozen81Roster.begin(), cs80::kFrozen81Roster.end(),
                           cs80::kSampleAccurateParameters.begin()), "literal 81-lane prefix unchanged");
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
        parameters[cs80::P_arpOn] = parameters[cs80::P_kasScanTiming] = 0;
        parameters[cs80::P_modWheelSub] = 0;
        parameters[cs80::P_modWheelVcf] = 1;
        parameters[cs80::P_ribbonCcEnable] = 0;
        Module module(argv[1]);
        const Curve zero {{0, 0.0}};
        const Curve held {{0, 0.0}, {1024, 1.0}, {2048, 0.0}};
        int renders = 0;
        for (int block : {1, 64, 256}) {
            const auto original = render(module, parameters, Curves {}, block); ++renders;
            const auto off = renderCc1(module, parameters, block, false, {}); ++renders;
            const auto connected = renderCc1(module, parameters, block, true, {}); ++renders;
            const auto neutral = renderCc1(module, parameters, block, true, zero); ++renders;
            const auto moved = renderCc1(module, parameters, block, true, held); ++renders;
            check(pcmBytesEqual(original, off), "default-off fixture matches unchanged renderer exactly");
            check(pcmBytesEqual(off, connected), "controller connection alone is PCM-inert");
            check(pcmBytesEqual(off, neutral), "connected CC1 zero is PCM-inert");
            const auto effect = difference(neutral, moved); // validates every sample as finite
            require(neutral.size() == 8192, "complete stereo capture");
            check(std::memcmp(neutral.data(), moved.data(), 2048 * sizeof(float)) == 0,
                  "CC1 onset prefix remains byte-exact");
            double heldEffect = 0;
            // Interleaved stereo samples for frames [1024, 2048).
            for (size_t i = 2048; i < 4096; ++i)
                heldEffect = std::max(heldEffect, std::abs(double(neutral[i]) - moved[i]));
            check(heldEffect > 1e-6, "mapped CC1 has positive effect during the held interval");
            std::cout << "rate=" << rate << " mode=" << mode << " block=" << block
                      << " cc1_effect=" << effect << " held_effect=" << heldEffect << '\n';
        }
        std::cout << "fixture_renders=" << renders << " failures=" << failures
                  << " (no mod-wheel automation capability claim)\n";
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "CC1 fixture: " << error.what() << '\n'; return 1;
    }
}
