#pragma once
// Native regression for learned render-policy boundaries. Included only by
// Vst3AutomationTests.cpp, using its actual SDK host and registered audio bound.
namespace {
void connectController(Instance& instance)
{
    require(instance.controller == nullptr, "fresh native controller");
    TUID controllerId {};
    require(instance.component->getControllerClassId(controllerId) == kResultOk, "policy controller class");
    require(instance.factory->createInstance(controllerId, IEditController::iid,
        reinterpret_cast<void**>(&instance.controller)) == kResultOk, "policy controller instance");
    require(instance.controller->initialize(nullptr) == kResultOk, "policy controller initialize");
    require(instance.component->queryInterface(IConnectionPoint::iid,
        reinterpret_cast<void**>(&instance.componentConnection)) == kResultOk, "policy component connection");
    require(instance.controller->queryInterface(IConnectionPoint::iid,
        reinterpret_cast<void**>(&instance.controllerConnection)) == kResultOk, "policy controller connection");
    require(instance.componentConnection->connect(instance.controllerConnection) == kResultOk
        && instance.controllerConnection->connect(instance.componentConnection) == kResultOk, "policy connect");
}
void installMap(Instance& instance, int parameter, bool keyCC = false, bool mixedPolicy = false)
{
    StateCapture state;
    require(instance.component->getState(&state) == kResultOk, "capture native state for map");
    const auto begin = state.bytes.find("<MIDI_LEARN");
    require(begin != std::string::npos, "state MIDI_LEARN element");
    const auto openingEnd = state.bytes.find('>', begin);
    require(openingEnd != std::string::npos, "MIDI_LEARN opening tag");
    const auto end = state.bytes[openingEnd - 1] == '/' ? openingEnd + 1
        : state.bytes.find("</MIDI_LEARN>", openingEnd) + std::strlen("</MIDI_LEARN>");
    require(end > begin && end <= state.bytes.size(), "complete MIDI_LEARN element");
    auto learned = std::string("<MIDI_LEARN version=\"1\"><MAP cc=\"20\" param=\"")
        + cs80::paramId(parameter) + "\"/>";
    if (keyCC) learned += std::string("<MAP cc=\"21\" param=\"") + cs80::paramId(cs80::P_volume) + "\"/>";
    if (mixedPolicy) learned += std::string("<MAP cc=\"22\" param=\"")
        + cs80::paramId(parameter == cs80::P_parityMode ? cs80::P_sourcePerformance : cs80::P_parityMode) + "\"/>";
    state.bytes.replace(begin, end - begin, learned + "</MIDI_LEARN>");
    require(instance.component->setState(&state) == kResultOk, "native state restore with learned policy CC");
    StateCapture restored;
    require(instance.component->getState(&restored) == kResultOk, "native restored map readback");
    require(restored.bytes.find("cc=\"20\"") != std::string::npos
        && restored.bytes.find(std::string("param=\"") + cs80::paramId(parameter) + "\"") != std::string::npos,
        "restored mapping accepted and serialized");
}
ParamID midiParameter(Instance& instance, int cc = 20, int zeroBasedChannel = 0)
{
    require(zeroBasedChannel >= 0 && zeroBasedChannel < 16, "valid native MIDI mapping channel");
    IMidiMapping* mapping = nullptr;
    require(instance.controller->queryInterface(IMidiMapping::iid,
        reinterpret_cast<void**>(&mapping)) == kResultOk, "native IMidiMapping");
    ParamID id = 0;
    const auto result = mapping->getMidiControllerAssignment(0, static_cast<int16>(zeroBasedChannel), cc, id);
    mapping->release();
    require(result == kResultOk, "native MIDI controller assignment");
    return id;
}
double readSelector(Instance& instance, int parameter)
{
    StateCapture state;
    require(instance.component->getState(&state) == kResultOk, "native policy state readback");
    require(state.seek(0, IBStream::kIBSeekSet, nullptr) == kResultOk, "policy state rewind");
    require(instance.controller->setComponentState(&state) == kResultOk, "controller policy state update");
    return instance.controller->getParamNormalized(parameterId(cs80::paramId(parameter)));
}
struct RenderResult { std::vector<float> audio; double selector, volume; };
RenderResult renderTransition(Module& module, cs80::ParamValues p, int parameter,
                              double target, bool reference, bool targetModern, bool directPolicy = false, bool keyCC = false, bool mixedPolicy = false, bool hasCurves = true)
{
    Instance instance(module, p);
    installMap(instance, parameter, keyCC, mixedPolicy);
    connectController(instance);
    const auto cc = midiParameter(instance);
    Curves curves;
    for (size_t lane = 0; lane < curves.size(); ++lane) {
        const int key = cs80::kSampleAccurateParameters[lane];
        curves[lane] = {{0, normalized(key, p[key])}, {255, 0.67 - double(lane) * 0.013}};
    }
    std::vector<float> audio;
    const int step = reference && targetModern ? 1 : 256;
    for (int sample = 0; sample < 256; sample += step) {
        Changes changes;
        if (hasCurves) for (size_t lane = 0; lane < curves.size(); ++lane) {
            const int key = cs80::kSampleAccurateParameters[lane];
            Curve points = curves[lane];
            if (reference) points = {{0, targetModern
                ? absoluteValue(curves[lane], normalized(key, p[key]), sample) : curves[lane].back().second}};
            changes.add(parameterId(cs80::paramId(key)), std::move(points));
        }
        if (sample == 0) changes.add(directPolicy ? parameterId(cs80::paramId(parameter)) : cc, {{0, target}});
        if (sample == 0 && keyCC) changes.add(midiParameter(instance, 21), {{0, 0.25}, {0, 0.75}});
        if (sample == 0 && mixedPolicy) {
            const int other = parameter == cs80::P_parityMode ? cs80::P_sourcePerformance : cs80::P_parityMode;
            const double last = normalized(other, p[other]);
            changes.add(midiParameter(instance, 22), {{0, 1.0 - last}, {0, last}});
        }
        instance.process(sample, step, &changes, audio);
    }
    instance.process(256, 1024, nullptr, audio);
    return {std::move(audio), readSelector(instance, parameter), readSelector(instance, cs80::P_volume)};
}
}

namespace {
void testVst3PolicyBoundaries(Module& module, double audioTolerance)
{
        auto p = cs80::defaultValues();
        p[cs80::P_parityMode] = 0.0f; p[cs80::P_sourcePerformance] = 1.0f;
        p[cs80::P_drift] = p[cs80::P_voiceDispersion] = p[cs80::P_analogNoise] = 0.0f;
        for (int mode = 0; mode < 3; ++mode) {
            auto q = p;
            const int selector = mode == 0 ? cs80::P_parityMode : cs80::P_sourcePerformance;
            const double target = mode == 1 ? 0.0 : 1.0;
            if (mode == 2) q[cs80::P_sourcePerformance] = 0.0f;
            const auto unqueued = renderTransition(module, q, selector, target, false, mode == 2, false, true, true, false);
            std::uint64_t unqueuedHash = 14695981039346656037ull;
            const auto* bytes = reinterpret_cast<const unsigned char*>(unqueued.audio.data());
            for (size_t i = 0; i < unqueued.audio.size() * sizeof(float); ++i)
                unqueuedHash = (unqueuedHash ^ bytes[i]) * 1099511628211ull;
            std::cout << "NO_CURVES mode=" << mode << " pcm_fnv1a=" << unqueuedHash
                << " selector=" << unqueued.selector << " volume=" << unqueued.volume << std::endl;
            const auto actual = renderTransition(module, q, selector, target, false, mode == 2);
            const auto reference = renderTransition(module, q, selector, target, true, mode == 2);
            const auto error = difference(actual.audio, reference.audio);
            std::cout << "POLICY transition=" << (mode == 0 ? "modern_to_pure" : mode == 1 ? "modern_to_legacy" : "legacy_to_modern")
                << " actual_selector=" << actual.selector << " reference_selector=" << reference.selector
                << " target=" << target << " max_audio_error=" << error << std::endl;
            check(actual.selector == target && reference.selector == target, "learned native CC performs the requested policy transition");
            check(error <= audioTolerance, "policy transition preserves protected flattening or modern sample timing");
            const auto mixedActual = renderTransition(module, q, selector, target, false, mode == 2, false, true, true);
            const auto mixedReference = renderTransition(module, q, selector, target, true, mode == 2, false, true, true);
            const auto mixedError = difference(mixedActual.audio, mixedReference.audio);
            std::cout << "MIXED transition=" << (mode == 0 ? "modern_to_pure" : mode == 1 ? "modern_to_legacy" : "legacy_to_modern")
                << " actual_selector=" << mixedActual.selector << " reference_selector=" << mixedReference.selector
                << " actual_volume=" << mixedActual.volume << " reference_volume=" << mixedReference.volume
                << " max_audio_error=" << mixedError << std::endl;
            check(mixedActual.selector == target && mixedReference.selector == target, "mixed policy CCs reach final target");
            check(mixedError <= audioTolerance, "mixed policies and repeated learned keys preserve audio timing");
            if (mode != 2)
                check(std::abs(mixedActual.volume - double(96.0f / 127.0f)) <= 0.000001
                    && std::abs(mixedReference.volume - double(96.0f / 127.0f)) <= 0.000001,
                    "protected repeated learned key wins over host endpoint, including after entering PURE");
            for (bool direct : {true, false}) {
                auto stable = q;
                if (!direct) stable[selector] = static_cast<float>(target);
                const auto controlActual = renderTransition(module, stable, selector, target, false, mode == 2, direct);
                const auto controlReference = renderTransition(module, stable, selector, target, true, mode == 2, direct);
                const auto controlError = difference(controlActual.audio, controlReference.audio);
                std::cout << "CONTROL transition=" << (mode == 0 ? "modern_to_pure" : mode == 1 ? "modern_to_legacy" : "legacy_to_modern")
                    << " kind=" << (direct ? "native_selector" : "same_policy_CC")
                    << " actual_selector=" << controlActual.selector << " reference_selector=" << controlReference.selector
                    << " max_audio_error=" << controlError << std::endl;
                check(controlActual.selector == target && controlReference.selector == target,
                    "control selector reaches the requested policy");
                check(controlError <= audioTolerance, "direct selector and same-policy controls obey timing contract");
            }
        }
}
}
