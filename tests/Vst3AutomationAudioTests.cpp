// Black-box JUCE 8.0.11 VST3 host oracle. The automation discontinuity is
// declared by the Steinberg SDK (ivstparameterchanges.h): old value at 127,
// new value at 128. Never accept a silent reference or block-partition drift.
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <pluginterfaces/vst/ivstevents.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <pluginterfaces/gui/iplugview.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Steinberg;

struct Queue final : Vst::IParamValueQueue
{
    Vst::ParamID id = 0;
    struct Point { int32 offset; Vst::ParamValue value; };
    std::vector<Point> points;
    Vst::ParamID PLUGIN_API getParameterId() override { return id; }
    int32 PLUGIN_API getPointCount() override { return (int32) points.size(); }
    tresult PLUGIN_API getPoint(int32 i, int32& offset, Vst::ParamValue& value) override
    {
        if (i < 0 || i >= (int32) points.size()) return kInvalidArgument;
        offset = points[(size_t) i].offset;
        value = points[(size_t) i].value;
        std::cerr << "VST3 host parameter point " << i << " offset=" << offset << " value=" << value << '\n';
        return kResultTrue;
    }
    tresult PLUGIN_API addPoint(int32 offset, Vst::ParamValue value, int32& index) override
    {
        points.push_back({offset, value});
        index = (int32) points.size() - 1;
        return kResultTrue;
    }
    tresult PLUGIN_API queryInterface(const TUID, void**) override { return kNoInterface; }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
};

struct Changes final : Vst::IParameterChanges
{
    Queue queue;
    bool any = false;
    int32 PLUGIN_API getParameterCount() override { return any ? 1 : 0; }
    Vst::IParamValueQueue* PLUGIN_API getParameterData(int32 i) override { return any && i == 0 ? &queue : nullptr; }
    Vst::IParamValueQueue* PLUGIN_API addParameterData(const Vst::ParamID& id, int32& index) override
    {
        queue.id = id; any = true; index = 0; return &queue;
    }
    tresult PLUGIN_API queryInterface(const TUID, void**) override { return kNoInterface; }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
};

struct Events final : Vst::IEventList
{
    std::vector<Vst::Event> events;
    int32 PLUGIN_API getEventCount() override { return (int32) events.size(); }
    tresult PLUGIN_API getEvent(int32 i, Vst::Event& event) override
    {
        if (i < 0 || i >= (int32) events.size()) return kInvalidArgument;
        event = events[(size_t) i];
        std::cerr << "VST3 host event " << i << " type " << event.type << " offset " << event.sampleOffset << '\n';
        return kResultTrue;
    }
    tresult PLUGIN_API addEvent(Vst::Event& event) override { events.push_back(event); return kResultTrue; }
    tresult PLUGIN_API queryInterface(const TUID, void**) override { return kNoInterface; }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
};

uint32_t vstHash(const char* name)
{
    uint32_t value = 0;
    for (const unsigned char* p = (const unsigned char*) name; *p; ++p)
        value = value * 31u + *p;
    // JUCE_USE_STUDIO_ONE_COMPATIBLE_PARAMETERS defaults to 1 in JUCE 8.0.11.
    return value & 0x7fffffffu;
}

// The VST3 contract expects a host IRunLoop even without an editor. An actual
// processing-only host owns handlers until each plug-in unregisters them; this
// test does not dispatch GUI/FD events while rendering offline test buffers.
struct HostContext final : Vst::IHostApplication, Linux::IRunLoop
{
    std::vector<Linux::IEventHandler*> registered;
    tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override
    {
        if (std::memcmp(iid, INLINE_UID_OF(Vst::IHostApplication), 16) == 0)
            *obj = static_cast<Vst::IHostApplication*>(this);
        else if (std::memcmp(iid, INLINE_UID_OF(Linux::IRunLoop), 16) == 0)
            *obj = static_cast<Linux::IRunLoop*>(this);
        else
        {
            *obj = nullptr;
            return kNoInterface;
        }
        addRef();
        return kResultOk;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    tresult PLUGIN_API getName(Vst::String128 name) override
    {
        constexpr char title[] = "CS80 automation host";
        for (size_t i = 0; i < sizeof(title); ++i) name[i] = title[i];
        return kResultOk;
    }
    tresult PLUGIN_API createInstance(TUID, TUID, void**) override { return kNoInterface; }
    tresult PLUGIN_API registerEventHandler(Linux::IEventHandler* h, Linux::FileDescriptor) override
    {
        registered.push_back(h);
        return kResultOk;
    }
    tresult PLUGIN_API unregisterEventHandler(Linux::IEventHandler* h) override
    {
        registered.erase(std::remove(registered.begin(), registered.end(), h), registered.end());
        return kResultOk;
    }
    tresult PLUGIN_API registerTimer(Linux::ITimerHandler*, Linux::TimerInterval) override
    {
        return kResultOk;
    }
    tresult PLUGIN_API unregisterTimer(Linux::ITimerHandler*) override { return kResultOk; }
};

struct Plugin
{
    HostContext host;
    void* module = nullptr;
    bool (*entry)(void*) = nullptr;
    bool (*exit)() = nullptr;
    IPluginFactory* factory = nullptr;
    Vst::IComponent* component = nullptr;
    Vst::IAudioProcessor* processor = nullptr;

    explicit Plugin(const std::string& path)
    {
        module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!module) throw std::runtime_error(std::string("dlopen: ") + dlerror());
        entry = (bool (*)(void*)) dlsym(module, "ModuleEntry");
        exit = (bool (*)()) dlsym(module, "ModuleExit");
        auto getFactory = (IPluginFactory* (*)()) dlsym(module, "GetPluginFactory");
        if (!entry || !exit || !getFactory || !entry(module)) throw std::runtime_error("VST3 exported entry points");
        factory = getFactory();
        if (!factory) throw std::runtime_error("VST3 factory");
        void* factory3Ptr = nullptr;
        if (factory->queryInterface(INLINE_UID_OF(IPluginFactory3), &factory3Ptr) == kResultOk)
        {
            auto* factory3 = static_cast<IPluginFactory3*>(factory3Ptr);
            if (factory3->setHostContext(static_cast<Vst::IHostApplication*>(&host)) != kResultOk)
                throw std::runtime_error("setHostContext");
            factory3->release();
        }
        PClassInfo info {};
        bool found = false;
        for (int i = 0; i < factory->countClasses(); ++i)
        {
            if (factory->getClassInfo(i, &info) != kResultOk) continue;
            if (std::strstr(info.category, "Audio Module Class") != nullptr) { found = true; break; }
        }
        if (!found) throw std::runtime_error("No VST3 audio component class");
        if (factory->createInstance(info.cid, INLINE_UID_OF(Vst::IComponent), (void**) &component) != kResultOk)
            throw std::runtime_error("createInstance IComponent");
        if (component->queryInterface(INLINE_UID_OF(Vst::IAudioProcessor), (void**) &processor) != kResultOk)
            throw std::runtime_error("query IAudioProcessor");
        if (component->initialize(static_cast<Vst::IHostApplication*>(&host)) != kResultOk)
            throw std::runtime_error("initialize");
        if (component->activateBus(Vst::kAudio, Vst::kInput, 0, true) != kResultOk)
            throw std::runtime_error("activate mono input bus");
        if (component->activateBus(Vst::kAudio, Vst::kOutput, 0, true) != kResultOk)
            throw std::runtime_error("activate stereo output bus");
        Vst::ProcessSetup setup { Vst::kRealtime, Vst::kSample32, 256, 48000.0 };
        if (processor->setupProcessing(setup) != kResultOk) throw std::runtime_error("setupProcessing");
        if (component->setActive(true) != kResultOk) throw std::runtime_error("setActive");
        if (processor->setProcessing(true) != kResultOk) throw std::runtime_error("setProcessing");
        std::cout << "VST3 buses: input=" << component->getBusCount(Vst::kAudio, Vst::kInput)
                  << " output=" << component->getBusCount(Vst::kAudio, Vst::kOutput)
                  << " MIDI=" << component->getBusCount(Vst::kEvent, Vst::kInput) << '\n';
        Vst::BusInfo bus {};
        if (component->getBusInfo(Vst::kAudio, Vst::kInput, 0, bus) == kResultOk)
            std::cout << "VST3 input channels: " << bus.channelCount << " flags " << bus.flags << '\n';
        if (component->getBusInfo(Vst::kAudio, Vst::kOutput, 0, bus) == kResultOk)
            std::cout << "VST3 output channels: " << bus.channelCount << " flags " << bus.flags << '\n';
    }
    ~Plugin()
    {
        std::cerr << "VST3 host releasing component\n";
        if (processor) { std::cerr << "setProcessing(false)\n"; processor->setProcessing(false); }
        if (component) { std::cerr << "setActive(false)\n"; component->setActive(false); }
        if (component) { std::cerr << "terminate()\n"; component->terminate(); }
        if (processor) { std::cerr << "processor->release()\n"; processor->release(); }
        if (component) { std::cerr << "component->release()\n"; component->release(); }
        std::cerr << "VST3 host releasing factory\n";
        if (factory) factory->release();
        // Keep the single VST3 module loaded across both independent audio
        // comparisons. Library unload is a distinct teardown validation gate.
    }

    std::vector<float> render(int length, const std::vector<Queue::Point>& changes, bool noteOn,
                              int noteOffset = 0, int notePitch = 60,
                              bool useHostContext = false, int64_t hostSamplePosition = 0,
                              const char* automatedParameter = "volume")
    {
        std::vector<float> in((size_t) length);
        std::vector<float> left((size_t) length), right((size_t) length);
        float* inPtrs[] { in.data() };
        float* outPtrs[] { left.data(), right.data() };
        Vst::AudioBusBuffers inBus {};
        inBus.numChannels = 1; inBus.channelBuffers32 = inPtrs;
        Vst::AudioBusBuffers outBus {};
        outBus.numChannels = 2; outBus.channelBuffers32 = outPtrs;
        Changes paramChanges;
        paramChanges.queue.id = vstHash(automatedParameter);
        paramChanges.queue.points = changes;
        paramChanges.any = !changes.empty();
        Events inputEvents;
        if (noteOn)
        {
            Vst::Event e {};
            e.type = Vst::Event::kNoteOnEvent;
            e.sampleOffset = noteOffset;
            e.noteOn.channel = 0;
            e.noteOn.pitch = (int16) notePitch;
            e.noteOn.velocity = 0.9f;
            e.noteOn.noteId = -1;
            inputEvents.events.push_back(e);
        }
        Vst::ProcessData data {};
        data.processMode = Vst::kRealtime;
        data.symbolicSampleSize = Vst::kSample32;
        data.numSamples = length;
        data.numInputs = 1; data.inputs = &inBus;
        data.numOutputs = 1; data.outputs = &outBus;
        data.inputParameterChanges = &paramChanges;
        data.inputEvents = &inputEvents;
        Vst::ProcessContext context {};
        if (useHostContext)
        {
            context.state = Vst::ProcessContext::kPlaying | Vst::ProcessContext::kContTimeValid
                          | Vst::ProcessContext::kTempoValid | Vst::ProcessContext::kProjectTimeMusicValid
                          | Vst::ProcessContext::kSystemTimeValid | Vst::ProcessContext::kClockValid;
            context.sampleRate = 48000.0;
            context.tempo = 120.0;
            context.projectTimeSamples = hostSamplePosition;
            context.continousTimeSamples = hostSamplePosition;
            context.projectTimeMusic = (double) hostSamplePosition / 24000.0;
            context.systemTime = (int64) std::llround ((double) hostSamplePosition * 1.0e9 / 48000.0);
            context.samplesToNextClock = 240 - (int32) (hostSamplePosition % 240);
            data.processContext = &context;
        }
        if (processor->process(data) != kResultOk) throw std::runtime_error("process");
        float hostPeak = 0.0f;
        for (int i = 0; i < length; ++i)
            hostPeak = std::max(hostPeak, std::max(std::abs(left[(size_t) i]), std::abs(right[(size_t) i])));
        std::cerr << "VST3 host output peak " << hostPeak << '\n';
        std::vector<float> result;
        result.reserve((size_t) length * 2);
        for (int i = 0; i < length; ++i) { result.push_back(left[(size_t) i]); result.push_back(right[(size_t) i]); }
        return result;
    }
};

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3)
    {
        std::cerr << "usage: automation_driver <VST3 .so> [lifecycle|empty|flat|flat_sample|jump|offset|transport|midi|ramp|ramp_brilliance|ramp_resonance]\n";
        return 2;
    }
    try
    {
        if (argc == 3 && std::string(argv[2]) == "empty")
        {
            Plugin plugin(argv[1]);
            std::cout << "empty lifecycle activated\n";
            return 0;
        }
        if (argc == 3 && std::string(argv[2]) == "lifecycle")
        {
            Plugin plugin(argv[1]);
            plugin.render(256, {}, true);
            plugin.render(256, {}, false);
            std::cout << "lifecycle without automation rendered\n";
            return 0;
        }
        const auto mode = argc == 3 ? std::string(argv[2]) : "offset";
        if (mode != "flat" && mode != "flat_sample" && mode != "jump"
            && mode != "offset" && mode != "transport" && mode != "midi"
            && mode != "ramp" && mode != "ramp_brilliance" && mode != "ramp_resonance")
            throw std::runtime_error("unknown comparison mode");

        // 'jump' is a legal VST3 discontinuity: the old value is repeated at
        // sample 127 before the new value arrives at sample 128.  Two points
        // at 0 and 128 alone describe a *linear ramp*, not a step (SDK spec).
        // The Steinberg VST3 SDK defines queue points as linear curve sections,
        // with an implicit previous value at sample -1. Thus [0:.8,128:.2]
        // is a genuine ramp, *not* the discontinuity that 'jump' tests.
        // 'ramp' compares it with the actual value at every sample in a
        // microblock reference. 'flat_sample' proves this finer host partition
        // itself changes no audio, so the ramp oracle cannot pass vacuously.
        // Truth model is committed before evaluation:
        // tests/parity/tolerances-v2/vst3-key-ramp-audio-v1.json.
        // All three continuous globals have the same normalized VST3 queue
        // law, but distinct Engine smoothing, macro and filter propagation.
        const bool rampMode = mode == "ramp" || mode == "ramp_brilliance" || mode == "ramp_resonance";
        const char* automatedParameter = mode == "ramp_brilliance" ? "brilliance"
                                       : mode == "ramp_resonance" ? "resonance" : "volume";
        const bool flatMode = mode == "flat" || mode == "flat_sample";
        const bool microblockReference = mode == "flat_sample" || rampMode;
        const std::vector<Queue::Point> fullBlockPoints = flatMode
            ? std::vector<Queue::Point> {}
            : (mode == "jump" || mode == "transport" || mode == "midi")
                ? std::vector<Queue::Point> { {0, 0.8}, {127, 0.8}, {128, 0.2} }
                : std::vector<Queue::Point> { {0, 0.8}, {128, 0.2} };
        const std::vector<Queue::Point> firstBlockPoints = flatMode
            ? std::vector<Queue::Point> {}
            : std::vector<Queue::Point> { {0, 0.8} };
        const std::vector<Queue::Point> secondBlockPoints = flatMode
            ? std::vector<Queue::Point> {}
            : std::vector<Queue::Point> { {0, 0.2} };

        const bool timedHostContext = mode == "transport";
        const bool midiAcrossSplit = mode == "midi";

        std::vector<float> one;
        Plugin block256(argv[1]);
        Plugin twoBlocks(argv[1]);
        {
            block256.render(256, {}, true, 0, 60, timedHostContext, 0);
            for (int i = 0; i < 2; ++i)
                block256.render(256, {}, false, 0, 60, timedHostContext, (i + 1) * 256);
            one = block256.render(256, fullBlockPoints, midiAcrossSplit, 140, 64,
                                  timedHostContext, 768, automatedParameter);
        }
        std::vector<float> two;
        {
            twoBlocks.render(256, {}, true, 0, 60, timedHostContext, 0);
            for (int i = 0; i < 2; ++i)
                twoBlocks.render(256, {}, false, 0, 60, timedHostContext, (i + 1) * 256);
            if (microblockReference)
            {
                for (int i = 0; i < 128; ++i)
                {
                    // The full queue is a line from (0,.8) to (128,.2).
                    // Reference values at 0..127 must use that exact slope.
                    const double value = 0.8 + (0.2 - 0.8) * (double) i / 128.0;
                    const std::vector<Queue::Point> samplePoint = rampMode
                        ? std::vector<Queue::Point> { {0, value} }
                        : std::vector<Queue::Point> {};
                    auto sample = twoBlocks.render(1, samplePoint, false, 0, 60,
                                                   false, 0, automatedParameter);
                    two.insert(two.end(), sample.begin(), sample.end());
                }
                auto tail = twoBlocks.render(128, secondBlockPoints, false, 0, 60,
                                              false, 0, automatedParameter);
                two.insert(two.end(), tail.begin(), tail.end());
            }
            else
            {
                auto first = twoBlocks.render(128, firstBlockPoints, false, 0, 60,
                                              timedHostContext, 768);
                auto second = twoBlocks.render(128, secondBlockPoints, midiAcrossSplit, 12, 64,
                                               timedHostContext, 896);
                two = std::move(first);
                two.insert(two.end(), second.begin(), second.end());
            }
        }
        double maxDelta = 0, energy = 0, deltaSquares = 0, referenceSquares = 0;
        for (size_t i = 0; i < one.size(); ++i)
        {
            const double delta = (double) one[i] - two[i];
            maxDelta = std::max(maxDelta, std::abs(delta));
            deltaSquares += delta * delta;
            referenceSquares += (double) two[i] * two[i];
            energy += std::abs((double) two[i]);
        }
        std::cout << "VST3 automation mode=" << mode
                  << (microblockReference ? " 256 vs 128x1+128 max_abs=" : " 256 vs 128+128 max_abs=")
                  << maxDelta
                  << " rms_diff=" << std::sqrt(deltaSquares / one.size())
                  << " reference_rms=" << std::sqrt(referenceSquares / two.size())
                  << " sum_abs=" << energy << '\n';
        if (energy < 0.01) return 3; // reject vacuous silence comparison
        return maxDelta < 1.0e-6 ? 0 : 1;
    }
    catch (const std::exception& e) { std::cerr << "failure: " << e.what() << '\n'; return 4; }
}
