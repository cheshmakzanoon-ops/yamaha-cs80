// Real built-module VST3 protocol test. This is a small host, not a direct
// APVTS setter: every change travels through IAudioProcessor::process and
// IParameterChanges. No JUCE host wrapper can flatten the supplied points.
#include "core/Params.h"
#include "plugin/HostAutomationBridge.h"
#include "Ws1Tolerances.h"
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/base/ibstream.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstevents.h>
#include <pluginterfaces/vst/ivstmessage.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace Steinberg;
using namespace Steinberg::Vst;
using Point = std::pair<int, double>;
using Curve = std::vector<Point>;
int failures = 0;
void check(bool pass, const char* message)
{
    if (!pass) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void require(bool pass, const char* message)
{ if (!pass) throw std::runtime_error(message); }

ParamID parameterId(const std::string& text)
{
    // JUCE 8.0.11 VST3ClientExtensions::convertJuceParameterId, ASCII IDs.
    std::uint32_t hash = 0;
    for (const auto byte : text) hash = hash * 31u + static_cast<unsigned char>(byte);
    return hash & 0x7fffffffu;
}
float normalized(int parameter, float physical)
{
    const auto& definition = cs80::paramDef(parameter);
    float proportion = std::clamp((physical - definition.min) / (definition.max - definition.min), 0.0f, 1.0f);
    if (definition.skew != 1.0f) proportion = std::pow(proportion, definition.skew);
    return proportion;
}

struct Queue final : IParamValueQueue
{
    ParamID id = 0;
    Curve points;
    tresult PLUGIN_API queryInterface(const TUID target, void** object) override
    {
        if (std::memcmp(target, IParamValueQueue::iid, sizeof(TUID)) == 0
            || std::memcmp(target, FUnknown::iid, sizeof(TUID)) == 0)
        { *object = this; return kResultOk; }
        *object = nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    ParamID PLUGIN_API getParameterId() override { return id; }
    int32 PLUGIN_API getPointCount() override { return static_cast<int32>(points.size()); }
    tresult PLUGIN_API getPoint(int32 index, int32& offset, ParamValue& value) override
    {
        if (index < 0 || static_cast<size_t>(index) >= points.size()) return kInvalidArgument;
        offset = points[static_cast<size_t>(index)].first;
        value = points[static_cast<size_t>(index)].second;
        return kResultTrue;
    }
    tresult PLUGIN_API addPoint(int32 offset, ParamValue value, int32& index) override
    { index = getPointCount(); points.emplace_back(offset, value); return kResultTrue; }
};
struct Changes final : IParameterChanges
{
    std::vector<std::unique_ptr<Queue>> queues;
    void add(ParamID id, Curve points)
    { auto queue = std::make_unique<Queue>(); queue->id = id; queue->points = std::move(points); queues.push_back(std::move(queue)); }
    tresult PLUGIN_API queryInterface(const TUID target, void** object) override
    {
        if (std::memcmp(target, IParameterChanges::iid, sizeof(TUID)) == 0
            || std::memcmp(target, FUnknown::iid, sizeof(TUID)) == 0)
        { *object = this; return kResultOk; }
        *object = nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    int32 PLUGIN_API getParameterCount() override { return static_cast<int32>(queues.size()); }
    IParamValueQueue* PLUGIN_API getParameterData(int32 index) override
    { return index >= 0 && static_cast<size_t>(index) < queues.size() ? queues[static_cast<size_t>(index)].get() : nullptr; }
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID& id, int32& index) override
    { index = getParameterCount(); add(id, {}); return queues.back().get(); }
};
struct Events final : IEventList
{
    std::vector<Event> events;
    tresult PLUGIN_API queryInterface(const TUID target, void** object) override
    {
        if (std::memcmp(target, IEventList::iid, sizeof(TUID)) == 0
            || std::memcmp(target, FUnknown::iid, sizeof(TUID)) == 0)
        { *object = this; return kResultOk; }
        *object = nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    int32 PLUGIN_API getEventCount() override { return static_cast<int32>(events.size()); }
    tresult PLUGIN_API getEvent(int32 index, Event& event) override
    {
        if (index < 0 || static_cast<size_t>(index) >= events.size()) return kInvalidArgument;
        event = events[static_cast<size_t>(index)]; return kResultTrue;
    }
    tresult PLUGIN_API addEvent(Event& event) override { events.push_back(event); return kResultTrue; }
};

struct StateCapture final : IBStream
{
    std::string bytes;
    size_t position = 0;
    tresult PLUGIN_API queryInterface(const TUID target, void** object) override
    {
        if (std::memcmp(target, IBStream::iid, sizeof(TUID)) == 0
            || std::memcmp(target, FUnknown::iid, sizeof(TUID)) == 0)
        { *object = this; return kResultOk; }
        *object = nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    tresult PLUGIN_API read(void* destination, int32 count, int32* readBytes) override
    {
        if (count < 0 || (destination == nullptr && count != 0)) return kInvalidArgument;
        const auto available = std::min(static_cast<size_t>(count), bytes.size() - position);
        if (available != 0) std::memcpy(destination, bytes.data() + position, available);
        position += available;
        if (readBytes != nullptr) *readBytes = static_cast<int32>(available);
        return kResultOk;
    }
    tresult PLUGIN_API write(void* source, int32 count, int32* written) override
    {
        if (count < 0 || (source == nullptr && count != 0)) return kInvalidArgument;
        const auto size = static_cast<size_t>(count);
        bytes.resize(std::max(bytes.size(), position + size));
        if (size != 0) std::memcpy(bytes.data() + position, source, size);
        position += size;
        if (written != nullptr) *written = count;
        return kResultOk;
    }
    tresult PLUGIN_API seek(int64 offset, int32 mode, int64* result) override
    {
        const auto base = mode == kIBSeekSet ? 0 : mode == kIBSeekCur ? static_cast<int64>(position)
            : mode == kIBSeekEnd ? static_cast<int64>(bytes.size()) : -1;
        if (base < 0 || offset < -base || offset > static_cast<int64>(bytes.size()) - base)
            return kInvalidArgument;
        position = static_cast<size_t>(base + offset);
        if (result != nullptr) *result = static_cast<int64>(position);
        return kResultOk;
    }
    tresult PLUGIN_API tell(int64* position) override
    { if (position == nullptr) return kInvalidArgument; *position = static_cast<int64>(this->position); return kResultOk; }
};

struct Module
{
    void* handle = nullptr;
    IPluginFactory* factory = nullptr;
    bool (*exit)() = nullptr;
    TUID componentId {};
    explicit Module(const char* path)
    {
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        require(handle != nullptr, dlerror());
        auto entry = reinterpret_cast<bool (*)(void*)>(dlsym(handle, "ModuleEntry"));
        exit = reinterpret_cast<bool (*)()>(dlsym(handle, "ModuleExit"));
        auto getFactory = reinterpret_cast<IPluginFactory* (*)()>(dlsym(handle, "GetPluginFactory"));
        require(entry != nullptr && exit != nullptr && getFactory != nullptr, "VST3 module entry points");
        require(entry(handle), "VST3 ModuleEntry");
        factory = getFactory();
        require(factory != nullptr, "VST3 factory");
        bool found = false;
        for (int i = 0; i < factory->countClasses(); ++i)
        {
            PClassInfo info {};
            if (factory->getClassInfo(i, &info) == kResultOk && std::strcmp(info.category, kVstAudioEffectClass) == 0)
            { std::memcpy(componentId, info.cid, sizeof(TUID)); found = true; break; }
        }
        require(found, "VST3 audio component class");
    }
    ~Module()
    {
        if (factory != nullptr) factory->release();
        if (exit != nullptr) exit();
        if (handle != nullptr) dlclose(handle);
    }
};
struct Instance
{
    IComponent* component = nullptr;
    IAudioProcessor* audio = nullptr;
    IEditController* controller = nullptr;
    IConnectionPoint* componentConnection = nullptr;
    IConnectionPoint* controllerConnection = nullptr;
    IPluginFactory* factory = nullptr;
    bool active = false;
    explicit Instance(Module& module, cs80::ParamValues parameters, int source = 13) : factory(module.factory)
    {
        require(module.factory->createInstance(module.componentId, IComponent::iid,
                    reinterpret_cast<void**>(&component)) == kResultOk, "VST3 createInstance");
        require(component->initialize(nullptr) == kResultOk, "VST3 initialize");
        require(component->queryInterface(IAudioProcessor::iid, reinterpret_cast<void**>(&audio)) == kResultOk,
                "VST3 IAudioProcessor");
        ProcessSetup setup { kRealtime, kSample32, 4096, 48000.0 };
        require(audio->setupProcessing(setup) == kResultOk, "VST3 setupProcessing");
        component->activateBus(kAudio, kOutput, 0, true);
        component->activateBus(kEvent, kInput, 0, true);
        Changes initial;
        for (int parameter = 0; parameter < cs80::P_numParams; ++parameter)
            initial.add(parameterId(cs80::paramId(parameter)), {{0, normalized(parameter, parameters[parameter])}});
        for (int rank = 0; rank < 2; ++rank)
            initial.add(parameterId("hardware_source_" + std::to_string(rank + 1)), {{0, double(source) / 13.0}});
        ProcessData flush {};
        flush.symbolicSampleSize = kSample32;
        flush.processMode = kRealtime;
        flush.inputParameterChanges = &initial;
        require(audio->process(flush) == kResultOk, "VST3 parameter-only flush");
        require(component->setActive(true) == kResultOk, "VST3 activate");
        active = true;
        require(audio->setProcessing(true) == kResultOk, "VST3 start processing");
    }
    ~Instance()
    {
        if (audio != nullptr) audio->setProcessing(false);
        if (component != nullptr && active) component->setActive(false);
        releaseReadbackController();
        if (component != nullptr) component->terminate();
        if (audio != nullptr) audio->release();
        if (component != nullptr) component->release();
    }
    void releaseReadbackController() noexcept
    {
        if (componentConnection != nullptr && controllerConnection != nullptr)
        {
            componentConnection->disconnect(controllerConnection);
            controllerConnection->disconnect(componentConnection);
        }
        if (controller != nullptr) controller->terminate();
        if (controllerConnection != nullptr) controllerConnection->release();
        if (componentConnection != nullptr) componentConnection->release();
        if (controller != nullptr) controller->release();
        controllerConnection = componentConnection = nullptr;
        controller = nullptr;
    }
    void flush(Changes& changes)
    {
        ProcessData data {};
        data.symbolicSampleSize = kSample32;
        data.processMode = kRealtime;
        data.inputParameterChanges = &changes;
        require(audio->process(data) == kResultOk, "active VST3 zero-frame parameter flush");
    }
    std::array<double, cs80::kSampleAccurateParameters.size()> normalizedState()
    {
        if (controller == nullptr)
        {
            TUID controllerId {};
            require(component->getControllerClassId(controllerId) == kResultOk, "VST3 controller class");
            require(factory->createInstance(controllerId, IEditController::iid,
                reinterpret_cast<void**>(&controller)) == kResultOk, "VST3 controller instance");
            require(controller->initialize(nullptr) == kResultOk, "VST3 controller initialize");
            require(component->queryInterface(IConnectionPoint::iid, reinterpret_cast<void**>(&componentConnection))
                == kResultOk, "VST3 component connection point");
            require(controller->queryInterface(IConnectionPoint::iid, reinterpret_cast<void**>(&controllerConnection))
                == kResultOk, "VST3 controller connection point");
            require(componentConnection->connect(controllerConnection) == kResultOk
                && controllerConnection->connect(componentConnection) == kResultOk, "VST3 processor/controller connection");
        }
        StateCapture state;
        // Use the native state-transfer protocol, on this fixture's UI thread.
        // Pinned JUCE setComponentState refreshes its controller from the actual
        // processor parameter's getValue(). APVTS state alone includes another
        // physical projection, so parsing that XML would not be exact readback.
        require(component->getState(&state) == kResultOk, "VST3 current processor state");
        require(state.seek(0, IBStream::kIBSeekSet, nullptr) == kResultOk, "VST3 state stream rewind");
        require(controller->setComponentState(&state) == kResultOk, "VST3 controller receives current processor state");
        std::vector<ParamID> ids;
        for (int32 index = 0; index < controller->getParameterCount(); ++index)
        {
            ParameterInfo info {};
            require(controller->getParameterInfo(index, info) == kResultOk, "VST3 controller parameter metadata");
            ids.push_back(info.id);
        }
        std::array<double, cs80::kSampleAccurateParameters.size()> values {};
        for (size_t lane = 0; lane < values.size(); ++lane)
        {
            const auto id = parameterId(cs80::paramId(cs80::kSampleAccurateParameters[lane]));
            require(std::find(ids.begin(), ids.end(), id) != ids.end(), "VST3 controller exposes the named key");
            values[lane] = controller->getParamNormalized(id);
            require(std::isfinite(values[lane]) && values[lane] >= 0.0 && values[lane] <= 1.0,
                    "VST3 flushed normalized readback is finite and in range");
        }
        // Resume audio with the same component-only host configuration. The
        // separate with/without-reader gate proves this observation is inert.
        releaseReadbackController();
        return values;
    }
    void process(int start, int count, Changes* changes, std::vector<float>& output, bool sixteen = false)
    {
        std::vector<float> left(static_cast<size_t>(count)), right(static_cast<size_t>(count));
        float* channels[] { left.data(), right.data() };
        AudioBusBuffers bus {};
        bus.numChannels = 2;
        bus.channelBuffers32 = channels;
        Events events;
        const auto note = [&](int absolute, int pitch, bool on) {
            if (absolute < start || absolute >= start + count) return;
            Event event {};
            event.busIndex = 0;
            event.sampleOffset = absolute - start;
            event.type = on ? Event::kNoteOnEvent : Event::kNoteOffEvent;
            if (on) { event.noteOn.channel = 0; event.noteOn.pitch = static_cast<int16>(pitch); event.noteOn.velocity = 0.75f; event.noteOn.noteId = pitch; }
            else { event.noteOff.channel = 0; event.noteOff.pitch = static_cast<int16>(pitch); event.noteOff.velocity = 0.0f; event.noteOff.noteId = pitch; }
            events.events.push_back(event);
        };
        for (int key = 0; key < (sixteen ? 16 : 2); ++key)
        { note(17 + key, 48 + key, true); note(3072 + key, 48 + key, false); }
        std::stable_sort(events.events.begin(), events.events.end(),
                         [](const Event& a, const Event& b) { return a.sampleOffset < b.sampleOffset; });
        ProcessContext context {};
        context.sampleRate = 48000.0;
        context.projectTimeSamples = start;
        ProcessData data {};
        data.processMode = kRealtime;
        data.symbolicSampleSize = kSample32;
        data.numSamples = count;
        data.numOutputs = 1;
        data.outputs = &bus;
        data.inputEvents = &events;
        data.inputParameterChanges = changes;
        data.processContext = &context;
        require(audio->process(data) == kResultOk, "VST3 audio processing");
        for (int i = 0; i < count; ++i)
        { output.push_back(left[static_cast<size_t>(i)]); output.push_back(right[static_cast<size_t>(i)]); }
    }
};

double absoluteValue(const Curve& curve, double initial, int sample)
{
    Point previous {-1, initial};
    for (const auto& point : curve)
    {
        if (sample < point.first)
        {
            const double right = double(sample - previous.first) / double(point.first - previous.first);
            return previous.second * (1.0 - right) + point.second * right;
        }
        previous = point;
    }
    return previous.second;
}
using Curves = std::array<Curve, cs80::kSampleAccurateParameters.size()>;
Curves curvesFor(const cs80::ParamValues& parameters)
{
    Curves curves;
    for (size_t i = 0; i < curves.size(); ++i)
    {
        const float initial = normalized(cs80::kSampleAccurateParameters[i], parameters[cs80::kSampleAccurateParameters[i]]);
        const double lane = static_cast<double>(i);
        curves[i] = {{0, initial}, {255, 0.22 + lane * 0.01}, {256, 0.72 - lane * 0.008},
                     {1023, 0.41 + lane * 0.005}, {2047, 0.64 - lane * 0.007},
                     {3071, 0.18 + lane * 0.01}, {4095, 0.50 + lane * 0.006}};
    }
    return curves;
}
std::vector<float> render(Module& module, const cs80::ParamValues& parameters, const Curves& curves,
                          int block, bool automate = true, bool flatten = false, int source = 13, bool nullChanges = false)
{
    Instance instance(module, parameters, source);
    std::vector<float> output;
    output.reserve(8192);
    for (int start = 0; start < 4096; start += block)
    {
        const int count = std::min(block, 4096 - start);
        Changes changes;
        if (automate)
            for (size_t lane = 0; lane < curves.size(); ++lane)
            {
                if (curves[lane].empty()) continue;
                const int parameter = cs80::kSampleAccurateParameters[lane];
                const double initial = normalized(parameter, parameters[parameter]);
                Curve points;
                if (!flatten)
                    for (const auto& point : curves[lane])
                        if (point.first >= start && point.first < start + count)
                            points.emplace_back(point.first - start, point.second);
                if (points.empty() || points.back().first != count - 1)
                    points.emplace_back(flatten ? 0 : count - 1, absoluteValue(curves[lane], initial, start + count - 1));
                changes.add(parameterId(cs80::paramId(parameter)), std::move(points));
            }
        instance.process(start, count, nullChanges ? nullptr : &changes, output, parameters[cs80::P_voiceMode] > 0.5f);
    }
    return output;
}
double difference(const std::vector<float>& a, const std::vector<float>& b)
{
    require(a.size() == b.size(), "matching render size");
    double result = 0;
    for (size_t i = 0; i < a.size(); ++i)
    { require(std::isfinite(a[i]) && std::isfinite(b[i]), "finite host render"); result = std::max(result, std::abs(double(a[i]) - b[i])); }
    return result;
}

// The existing partition fixture starts at an explicit point zero. A host may
// instead change parameters in a zero-frame process call between real blocks,
// then supply a queue whose first point is later. That new state owns sample -1.
std::vector<float> renderAfterFlush(Module& module, const cs80::ParamValues& parameters,
                                    int block, int firstPoint, bool priorCurve, bool scalar, bool readState = true)
{
    Instance instance(module, parameters);
    std::vector<float> output;
    Changes prior;
    if (priorCurve)
        for (size_t lane = 0; lane < cs80::kSampleAccurateParameters.size(); ++lane)
        {
            const int parameter = cs80::kSampleAccurateParameters[lane];
            prior.add(parameterId(cs80::paramId(parameter)),
                {{0, normalized(parameter, parameters[parameter])}, {255, 0.23 + double(lane) * 0.007}});
        }
    instance.process(0, 256, priorCurve ? &prior : nullptr, output);
    Changes flush;
    for (size_t lane = 0; lane < cs80::kSampleAccurateParameters.size(); ++lane)
        flush.add(parameterId(cs80::paramId(cs80::kSampleAccurateParameters[lane])),
                  {{0, double(0.69f - float(lane) * 0.009f)}});
    instance.flush(flush);
    std::array<double, cs80::kSampleAccurateParameters.size()> flushed {};
    if (readState) flushed = instance.normalizedState();
    else require(!scalar, "the scalar oracle requires actual native readback");
    for (int sample = 0; sample < block; sample += scalar ? 1 : block)
    {
        Changes changes;
        for (size_t lane = 0; lane < cs80::kSampleAccurateParameters.size(); ++lane)
        {
            const double initial = flushed[lane];
            const double target = 0.31 + double(lane) * 0.008;
            const double fraction = std::min(1.0, double(sample + 1) / double(firstPoint + 1));
            const double value = scalar ? initial * (1.0 - fraction) + target * fraction : target;
            changes.add(parameterId(cs80::paramId(cs80::kSampleAccurateParameters[lane])),
                        {{scalar ? 0 : firstPoint, value}});
        }
        instance.process(256 + sample, scalar ? 1 : block, &changes, output);
    }
    instance.process(256 + block, 1024, nullptr, output);
    return output;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        require(argc == 3, "usage: cs80_vst3_automation_tests MODULE TOLERANCES");
        const ws1::Tolerances limits(argv[2], "host_automation");
        const ws1::Tolerances boundaryLimits((std::filesystem::path(argv[2]).parent_path()
            / "host-automation-state-boundary.json").string(), "host_automation_state_boundary");
        Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1.0f;
        parameters[cs80::P_parityMode] = 0.0f;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0.0f;
        const auto curves = curvesFor(parameters);
        const auto scalar = render(module, parameters, curves, 1);
        double maxError = 0;
        for (int block : {7, 64, 256, 1024, 4096})
            maxError = std::max(maxError, difference(scalar, render(module, parameters, curves, block)));
        check(maxError <= limits["audio_max_abs_error"], "real VST3 queues match independent per-sample control frames at every partition");
        const auto neutral = render(module, parameters, curves, 256, false);
        check(neutral == render(module, parameters, curves, 256, false, false, 13, true),
              "empty queue and original no-queue path are byte-identical");
        check(difference(scalar, neutral) > 0.00001, "host automation changes audible output (positive control)");
        auto pure = parameters; pure[cs80::P_parityMode] = 1.0f;
        check(render(module, pure, curves, 256) == render(module, pure, curves, 256, true, true),
              "PURE CS-80 retains final-point-at-block-start behavior byte-for-byte");
        auto legacy = parameters; legacy[cs80::P_sourcePerformance] = 0.0f;
        check(render(module, legacy, curves, 256) == render(module, legacy, curves, 256, true, true),
              "Legacy retains final-point-at-block-start behavior byte-for-byte");
        Curves rankOnly = curves;
        for (size_t i = 0; i < rankOnly.size(); ++i)
            if (cs80::kSampleAccurateParameters[i] < cs80::P_ch1Start) rankOnly[i].clear();
        for (int source : {0, 11, 12})
            check(render(module, parameters, rankOnly, 256, true, false, source)
                    == render(module, parameters, rankOnly, 256, false, false, source),
                  "panel automation cannot override a selected factory or memory source");
        auto sixteen = parameters; sixteen[cs80::P_voiceMode] = 1.0f;
        const auto sixteenError = difference(render(module, sixteen, curves, 1), render(module, sixteen, curves, 256));
        check(sixteenError <= limits["audio_max_abs_error"], "automation reaches both engines in sixteen-voice mode");
        std::cout << "real VST3 maximum partition error=" << maxError << " sixteen-voice error=" << sixteenError << '\n';
        double boundaryError = 0.0, readerError = 0.0;
        for (int block : {7, 64, 256})
            for (int firstPoint : {1, block - 1})
                for (bool priorCurve : {false, true})
                {
                    const auto actual = renderAfterFlush(module, parameters, block, firstPoint, priorCurve, false);
                    const auto withoutReader = renderAfterFlush(module, parameters, block, firstPoint, priorCurve, false, false);
                    const auto observationError = difference(actual, withoutReader);
                    readerError = std::max(readerError, observationError);
                    check(observationError <= boundaryLimits["native_reader_audio_error"]
                        && std::memcmp(actual.data(), withoutReader.data(), actual.size() * sizeof(float)) == 0,
                        "native normalized-state reader preserves the complete audio render byte-for-byte");
                    const auto error = difference(actual,
                        renderAfterFlush(module, parameters, block, firstPoint, priorCurve, true));
                    boundaryError = std::max(boundaryError, error);
                    check(error <= boundaryLimits["audio_max_abs_error"],
                          "native zero-frame flush supersedes the old start of a delayed VST3 curve");
                }
        std::cout << "real VST3 zero-frame state-boundary maximum audio error=" << boundaryError
                  << " reader observation error=" << readerError << '\n';
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
    return failures == 0 ? 0 : 1;
}
