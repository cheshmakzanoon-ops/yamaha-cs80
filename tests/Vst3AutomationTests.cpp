// Real built-module VST3 protocol test. This is a small host, not a direct
// APVTS setter: every change travels through IAudioProcessor::process and
// IParameterChanges. No JUCE host wrapper can flatten the supplied points.
#include "core/Params.h"
#include "plugin/HostAutomationBridge.h"
#include "Ws1Tolerances.h"
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstevents.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
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
    bool active = false;
    explicit Instance(Module& module, cs80::ParamValues parameters, int source = 13)
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
        if (component != nullptr) component->terminate();
        if (audio != nullptr) audio->release();
        if (component != nullptr) component->release();
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
} // namespace

int main(int argc, char** argv)
{
    try
    {
        require(argc == 3, "usage: cs80_vst3_automation_tests MODULE TOLERANCES");
        const ws1::Tolerances limits(argv[2], "host_automation");
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
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
    return failures == 0 ? 0 : 1;
}
