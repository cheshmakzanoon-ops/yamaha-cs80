#pragma once
// Native regression for retired render-policy writes. Included only by
// Vst3AutomationTests.cpp, using its actual SDK host and registered audio bound.
// Parameter IDs remain addressable, while every playback path stays circuit.
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
struct RenderResult { std::vector<float> audio; double source, parity, volume; };
RenderResult renderTransition(Module& module, cs80::ParamValues p, int parameter,
                              double target, bool reference, bool directPolicy = false,
                              bool keyCC = false, bool mixedPolicy = false,
                              bool hasCurves = true, bool submitPolicy = true)
{
    Instance instance(module, p);
    installMap(instance, parameter, keyCC, mixedPolicy);
    connectController(instance);
    const auto cc = midiParameter(instance);
    Curves curves;
    for (size_t lane = 0; lane < curves.size(); ++lane) {
        const int key = cs80::kSampleAccurateParameters[lane];
        curves[lane] = {{0, normalized(key, p[key])}, {255, validGeneratedNormalized(0.67 - double(lane) * 0.013)}};
    }
    for (const auto& curve : curves) checkGeneratedCurve(curve);
    std::vector<float> audio;
    // Put the notes on before the obsolete write. Equality with the omitted
    // write therefore catches voice/envelope/controller resets, not just a
    // selector that happens to serialize its fixed value after a reset.
    instance.process(0, 256, nullptr, audio);
    const int step = reference ? 1 : 256;
    for (int sample = 0; sample < 256; sample += step) {
        Changes changes;
        if (hasCurves) for (size_t lane = 0; lane < curves.size(); ++lane) {
            const int key = cs80::kSampleAccurateParameters[lane];
            Curve points = curves[lane];
            if (reference) points = {{0, absoluteValue(curves[lane], normalized(key, p[key]), sample)}};
            changes.add(parameterId(cs80::paramId(key)), std::move(points));
        }
        if (sample == 0 && submitPolicy)
            changes.add(directPolicy ? parameterId(cs80::paramId(parameter)) : cc, {{0, target}});
        if (sample == 0 && keyCC) changes.add(midiParameter(instance, 21), {{0, 0.25}, {0, 0.75}});
        if (sample == 0 && mixedPolicy && submitPolicy) {
            const int other = parameter == cs80::P_parityMode ? cs80::P_sourcePerformance : cs80::P_parityMode;
            const double last = normalized(other, p[other]);
            changes.add(midiParameter(instance, 22), {{0, 1.0 - last}, {0, last}});
        }
        instance.process(256 + sample, step, &changes, audio);
    }
    instance.process(512, 1024, nullptr, audio);
    return {std::move(audio), readSelector(instance, cs80::P_sourcePerformance),
            readSelector(instance, cs80::P_parityMode), readSelector(instance, cs80::P_volume)};
}
bool hasCircuitPolicy(const RenderResult& render)
{
    return render.source == 1.0 && render.parity == 0.0;
}
}

namespace {
void testVst3PolicyBoundaries(Module& module, double audioTolerance)
{
    auto p = cs80::defaultValues();
    p[cs80::P_parityMode] = 0.0f; p[cs80::P_sourcePerformance] = 1.0f;
    p[cs80::P_drift] = p[cs80::P_voiceDispersion] = p[cs80::P_analogNoise] = 0.0f;
    {
        Instance instance(module, p);
        connectController(instance);
        for (const int selector : {cs80::P_parityMode, cs80::P_sourcePerformance}) {
            bool found = false;
            for (int32 index = 0; index < instance.controller->getParameterCount(); ++index) {
                ParameterInfo info {};
                require(instance.controller->getParameterInfo(index, info) == kResultOk, "native circuit-policy parameter metadata");
                if (info.id != parameterId(cs80::paramId(selector))) continue;
                found = true;
                check((info.flags & ParameterInfo::kCanAutomate) == 0,
                      "retired policy ID remains present but is not advertised as automatable");
            }
            check(found, "retired circuit-policy parameter ID remains in the native host table");
        }
    }
    for (const int selector : {cs80::P_parityMode, cs80::P_sourcePerformance}) {
        for (const double target : {0.0, 1.0}) {
            auto stale = p;
            stale[cs80::P_sourcePerformance] = 0.0f;
            stale[cs80::P_parityMode] = 1.0f;
            const auto actual = renderTransition(module, stale, selector, target, false);
            const auto reference = renderTransition(module, stale, selector, target, true);
            const auto omitted = renderTransition(module, p, selector, target, false, false, false, false, true, false);
            const auto error = difference(actual.audio, reference.audio);
            std::cout << "CIRCUIT_POLICY id=" << cs80::paramId(selector) << " requested=" << target
                      << " source=" << actual.source << " parity=" << actual.parity
                      << " max_audio_error=" << error << std::endl;
            check(hasCircuitPolicy(actual) && hasCircuitPolicy(reference),
                  "obsolete learned policy writes retain canonical circuit state");
            check(actual.audio == omitted.audio,
                  "old initial policy and learned policy writes preserve held audio byte-for-byte");
            check(error <= audioTolerance,
                  "obsolete learned policy writes preserve independent per-sample automation timing");

            const auto mixedActual = renderTransition(module, stale, selector, target, false, false, true, true);
            const auto mixedReference = renderTransition(module, stale, selector, target, true, false, true, true);
            const auto mixedOmitted = renderTransition(module, p, selector, target, false, false, true, true, true, false);
            const auto mixedError = difference(mixedActual.audio, mixedReference.audio);
            std::cout << "CIRCUIT_MIXED id=" << cs80::paramId(selector) << " requested=" << target
                      << " volume=" << mixedActual.volume << " max_audio_error=" << mixedError << std::endl;
            check(hasCircuitPolicy(mixedActual) && hasCircuitPolicy(mixedReference),
                  "mixed repeated policy CCs cannot restore either retired render path");
            check(mixedActual.audio == mixedOmitted.audio,
                  "mixed obsolete policy CCs leave ordinary learned controls and held audio unchanged");
            check(mixedError <= audioTolerance,
                  "mixed policies and repeated learned keys preserve sample-accurate audio timing");
            check(std::abs(mixedActual.volume - double(0.67f)) <= 0.000001
                      && std::abs(mixedReference.volume - double(0.67f)) <= 0.000001,
                  "active host curve retains its independently specified endpoint after learned key writes");

            const auto unqueued = renderTransition(module, stale, selector, target, false, false, true, true, false);
            const auto unqueuedOmitted = renderTransition(module, p, selector, target, false, false, true, true, false, false);
            check(hasCircuitPolicy(unqueued) && unqueued.audio == unqueuedOmitted.audio,
                  "obsolete policy writes preserve no-queue held audio and learned controls byte-for-byte");
            check(std::abs(unqueued.volume - double(96.0f / 127.0f)) <= 0.000001,
                  "without a host curve the last repeated ordinary learned CC remains live");
            check(difference(mixedActual.audio, unqueued.audio) > 0.00001,
                  "host curves remain audibly active after rejected policy writes");

            const auto direct = renderTransition(module, stale, selector, target, false, true);
            const auto directReference = renderTransition(module, stale, selector, target, true, true);
            check(hasCircuitPolicy(direct) && hasCircuitPolicy(directReference),
                  "direct native policy writes retain the same fixed circuit values");
            check(direct.audio == omitted.audio,
                  "direct native policy writes preserve held voices and audio byte-for-byte");
            check(difference(direct.audio, directReference.audio) <= audioTolerance,
                  "direct native policy writes cannot flatten sample-accurate host curves");
        }
    }
}
}
