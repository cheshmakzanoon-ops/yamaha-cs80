// Native Audio Unit v2 host fixture, built and run on macOS. Calls the built
// component's actual AudioUnitScheduleParameters and AudioUnitRender APIs.
#include "core/Params.h"
#include "plugin/HostAutomationBridge.h"
#include "Ws1Tolerances.h"
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Point = std::pair<int, double>;
using Curve = std::vector<Point>;
using Curves = std::array<Curve, cs80::kSampleAccurateParameters.size()>;
int failures = 0;
double curveTolerance = 0.0000001;
void check(bool pass, const char* message)
{ if (!pass) { ++failures; if (failures <= 32) std::cerr << "FAIL: " << message << '\n'; } }
void require(bool pass, const char* message)
{ if (!pass) throw std::runtime_error(message); }
AudioUnitParameterID parameterId(const std::string& text)
{
    std::uint32_t hash = 0;
    for (const auto byte : text) hash = hash * 31u + static_cast<unsigned char>(byte);
    return hash & 0x7fffffffu;
}
float parameterMaximum(AudioUnit unit, AudioUnitParameterID parameter)
{
    AudioUnitParameterInfo info {};
    UInt32 bytes = sizeof(info);
    require(AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global,
        parameter, &info, &bytes) == noErr, "AU parameter range");
    const auto maximum = info.maxValue;
    if ((info.flags & kAudioUnitParameterFlag_CFNameRelease) != 0 && info.cfNameString != nullptr)
        CFRelease(info.cfNameString);
    return maximum;
}
float normalized(int parameter, float physical)
{
    const auto& definition = cs80::paramDef(parameter);
    float proportion = std::clamp((physical - definition.min) / (definition.max - definition.min), 0.0f, 1.0f);
    if (definition.skew != 1.0f) proportion = std::pow(proportion, definition.skew);
    return proportion;
}
double absoluteValue(const Curve& curve, double initial, int sample, bool immediate)
{
    Point previous {-1, initial};
    for (const auto& point : curve)
    {
        if (sample < point.first)
        {
            if (immediate) return previous.second;
            const double right = double(sample - previous.first) / double(point.first - previous.first);
            return previous.second * (1.0 - right) + point.second * right;
        }
        previous = point;
    }
    return previous.second;
}
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
        // AudioUnitParameterEvent endpoints are Float32. The independent oracle
        // starts from those exact transmitted endpoints before interpolation.
        for (auto& point : curves[i]) point.second = static_cast<float>(point.second);
    }
    return curves;
}
struct Module
{
    CFBundleRef bundle = nullptr;
    AudioComponent component = nullptr;
    explicit Module(const char* path)
    {
        auto url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(path), static_cast<CFIndex>(std::strlen(path)), true);
        require(url != nullptr, "AU bundle URL");
        bundle = CFBundleCreate(kCFAllocatorDefault, url);
        CFRelease(url);
        require(bundle != nullptr && CFBundleLoadExecutable(bundle), "load built AU component bundle");
        auto factory = reinterpret_cast<AudioComponentFactoryFunction>(CFBundleGetFunctionPointerForName(bundle, CFSTR("JuceAUFactory")));
        require(factory != nullptr, "AU component factory symbol");
        AudioComponentDescription description {};
        description.componentType = kAudioUnitType_MusicDevice;
        description.componentSubType = 0x43733830u;      // 'Cs80', CMake PLUGIN_CODE
        description.componentManufacturer = 0x52656649u; // 'RefI', CMake manufacturer
        component = AudioComponentRegister(&description, CFSTR("CS-80 Automation Fixture"), 0x00010000, factory);
        require(component != nullptr, "register exact built AU factory");
    }
    ~Module()
    {
        // AudioComponentRegister retains its factory for this process. Keep the
        // executable loaded until process exit; unload stress is a separate gate.
        if (bundle != nullptr) CFRelease(bundle);
    }
};
struct Instance
{
    AudioUnit unit = nullptr;
    explicit Instance(Module& module, const cs80::ParamValues& parameters, int source)
    {
        require(AudioComponentInstanceNew(module.component, &unit) == noErr, "create AU instance");
        AudioStreamBasicDescription format {};
        format.mSampleRate = 48000.0;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked
                            | kAudioFormatFlagsNativeEndian;
        format.mBytesPerPacket = format.mBytesPerFrame = sizeof(float);
        format.mFramesPerPacket = 1;
        format.mChannelsPerFrame = 2;
        format.mBitsPerChannel = 32;
        require(AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0,
                                      &format, sizeof(format)) == noErr, "AU stereo float format");
        UInt32 maximum = 4096;
        require(AudioUnitSetProperty(unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0,
                                      &maximum, sizeof(maximum)) == noErr, "AU maximum block");
        for (int parameter = 0; parameter < cs80::P_numParams; ++parameter)
        {
            const auto id = parameterId(cs80::paramId(parameter));
            // AU exposes integer ranges for JUCE choice parameters. The host
            // queries that range instead of sending VST3 normalized values.
            require(AudioUnitSetParameter(unit, id, kAudioUnitScope_Global, 0,
                normalized(parameter, parameters[parameter]) * parameterMaximum(unit, id), 0) == noErr,
                "AU initial parameter");
        }
        for (int rank = 0; rank < 2; ++rank)
        {
            const auto id = parameterId("hardware_source_" + std::to_string(rank + 1));
            require(AudioUnitSetParameter(unit, id, kAudioUnitScope_Global, 0,
                float(source) / 13.0f * parameterMaximum(unit, id), 0) == noErr, "AU programmer source");
        }
        require(AudioUnitInitialize(unit) == noErr, "initialize AU");
        for (int parameter : cs80::kSampleAccurateParameters)
        {
            AudioUnitParameterInfo info {}; UInt32 bytes=sizeof(info);
            require(AudioUnitGetProperty(unit,kAudioUnitProperty_ParameterInfo,kAudioUnitScope_Global,
                parameterId(cs80::paramId(parameter)),&info,&bytes)==noErr,"AU parameter metadata");
            check((info.flags & kAudioUnitParameterFlag_CanRamp)!=0,"key AU parameter advertises ramps");
            if ((info.flags & kAudioUnitParameterFlag_CFNameRelease)!=0 && info.cfNameString!=nullptr) CFRelease(info.cfNameString);
        }
        AudioUnitParameterInfo info {};UInt32 bytes=sizeof(info);
        require(AudioUnitGetProperty(unit,kAudioUnitProperty_ParameterInfo,kAudioUnitScope_Global,
            parameterId(cs80::paramId(cs80::P_subSpeed)),&info,&bytes)==noErr,"non-key AU parameter metadata");
        check((info.flags & kAudioUnitParameterFlag_CanRamp)==0,"non-key AU parameter does not advertise unsupported ramps");
        if ((info.flags & kAudioUnitParameterFlag_CFNameRelease)!=0 && info.cfNameString!=nullptr) CFRelease(info.cfNameString);
    }
    ~Instance()
    { if (unit != nullptr) { AudioUnitUninitialize(unit); AudioComponentInstanceDispose(unit); } }
    void process(int start, int count, const std::vector<AudioUnitParameterEvent>& automation,
                 std::vector<float>& output, bool sixteen, bool directOffset = false)
    {
        if (!automation.empty())
        {
            if (directOffset)
            {
                for (const auto& event : automation)
                    require(AudioUnitSetParameter(unit, event.parameter, event.scope, event.element,
                        event.eventValues.immediate.value, event.eventValues.immediate.bufferOffset) == noErr,
                        "AU direct buffer-offset setter");
            }
            else require(AudioUnitScheduleParameters(unit, automation.data(), static_cast<UInt32>(automation.size())) == noErr,
                         "AU schedules actual host events");
        }
        std::vector<std::array<int,3>> notes;
        for (int key = 0; key < (sixteen ? 16 : 2); ++key)
        {
            if (17+key >= start && 17+key < start+count) notes.push_back({17+key-start,48+key,1});
            if (3072+key >= start && 3072+key < start+count) notes.push_back({3072+key-start,48+key,0});
        }
        std::sort(notes.begin(),notes.end());
        for(const auto& note:notes)
            require(MusicDeviceMIDIEvent(unit,note[2] != 0 ? 0x90u : 0x80u,static_cast<UInt32>(note[1]),
                note[2] != 0 ? 96u : 0u,static_cast<UInt32>(note[0])) == noErr,"AU MIDI at sample offset");
        std::vector<float> left(static_cast<size_t>(count)),right(static_cast<size_t>(count));
        struct StereoBuffers { UInt32 count; AudioBuffer channels[2]; } buffers {
            2, {{1,static_cast<UInt32>(count*sizeof(float)),left.data()},
                {1,static_cast<UInt32>(count*sizeof(float)),right.data()}} };
        AudioTimeStamp timestamp {}; timestamp.mSampleTime=start;timestamp.mFlags=kAudioTimeStampSampleTimeValid;
        AudioUnitRenderActionFlags flags=0;
        require(AudioUnitRender(unit,&flags,&timestamp,0,static_cast<UInt32>(count),
            reinterpret_cast<AudioBufferList*>(&buffers))==noErr,"AU render");
        for(int i=0;i<count;++i){output.push_back(left[static_cast<size_t>(i)]);output.push_back(right[static_cast<size_t>(i)]);}
    }
};
AudioUnitParameterEvent immediateEvent(int parameter,int offset,double value)
{
    AudioUnitParameterEvent event {};
    event.scope=kAudioUnitScope_Global;event.element=0;event.parameter=parameterId(cs80::paramId(parameter));
    event.eventType=kParameterEvent_Immediate;
    event.eventValues.immediate.bufferOffset=static_cast<UInt32>(offset);event.eventValues.immediate.value=static_cast<float>(value);
    return event;
}
enum class Kind { ramp, immediate, scalarRamp, scalarImmediate, flatImmediate, off, directImmediate };
std::vector<float> render(Module& module,const cs80::ParamValues& parameters,const Curves& curves,int block,
                          Kind kind,int source=13)
{
    Instance instance(module,parameters,source);
    std::vector<float> output;output.reserve(8192);
    for(int start=0;start<4096;start+=block)
    {
        const int count=std::min(block,4096-start);
        std::vector<AudioUnitParameterEvent> events;
        for(size_t lane=0;lane<curves.size();++lane)
        {
            if(kind==Kind::off || curves[lane].empty()) continue;
            const int parameter=cs80::kSampleAccurateParameters[lane];
            const double initial=normalized(parameter,parameters[parameter]);
            if(kind==Kind::scalarRamp || kind==Kind::scalarImmediate || kind==Kind::flatImmediate)
            {
                events.push_back(immediateEvent(parameter,0,absoluteValue(curves[lane],initial,
                    start+count-1,kind!=Kind::scalarRamp)));
                continue;
            }
            if(kind==Kind::immediate || kind==Kind::directImmediate)
            {
                for(const auto& point:curves[lane])
                    if(point.first>=start && point.first<start+count)
                        events.push_back(immediateEvent(parameter,point.first-start,point.second));
                continue;
            }
            Point previous {-1,initial};
            for(const auto& point:curves[lane])
            {
                if(point.first>=start && previous.first<start+count)
                {
                    AudioUnitParameterEvent event {};
                    event.scope=kAudioUnitScope_Global;event.element=0;event.parameter=parameterId(cs80::paramId(parameter));
                    event.eventType=kParameterEvent_Ramped;
                    event.eventValues.ramp.startBufferOffset=previous.first-start;
                    event.eventValues.ramp.durationInFrames=static_cast<UInt32>(point.first-previous.first);
                    event.eventValues.ramp.startValue=static_cast<float>(previous.second);
                    event.eventValues.ramp.endValue=static_cast<float>(point.second);
                    events.push_back(event);
                }
                previous=point;
            }
        }
        instance.process(start,count,events,output,parameters[cs80::P_voiceMode]>0.5f,kind==Kind::directImmediate);
        if(kind==Kind::ramp && parameters[cs80::P_parityMode]<0.5f && parameters[cs80::P_sourcePerformance]>=0.5f)
            for(size_t lane=0;lane<curves.size();++lane)
            {
                if(curves[lane].empty())continue;
                const int parameter=cs80::kSampleAccurateParameters[lane];
                AudioUnitParameterValue value=0;
                require(AudioUnitGetParameter(instance.unit,parameterId(cs80::paramId(parameter)),
                    kAudioUnitScope_Global,0,&value)==noErr,"AU final parameter readback");
                check(std::abs(double(value)-absoluteValue(curves[lane],normalized(parameter,parameters[parameter]),
                    start+count-1,false))<=curveTolerance,"AU publishes the current buffer endpoint, not a future ramp endpoint");
            }
    }
    return output;
}
double difference(const std::vector<float>& a,const std::vector<float>& b)
{
    require(a.size()==b.size(),"AU matching render sizes");double maximum=0;
    for(size_t i=0;i<a.size();++i){require(std::isfinite(a[i])&&std::isfinite(b[i]),"AU finite audio");maximum=std::max(maximum,std::abs(double(a[i])-b[i]));}
    return maximum;
}
} // namespace
int main(int argc,char** argv)
{
    try {
        require(argc==3,"usage: cs80_au_automation_host_tests COMPONENT TOLERANCES");
        const ws1::Tolerances limits(argv[2],"host_automation");
        curveTolerance=limits["normalized_curve_error"];
        Module module(argv[1]);
        auto parameters=cs80::defaultValues();parameters[cs80::P_sourcePerformance]=1;parameters[cs80::P_parityMode]=0;
        parameters[cs80::P_drift]=parameters[cs80::P_voiceDispersion]=parameters[cs80::P_analogNoise]=0;
        const auto curves=curvesFor(parameters);
        const auto scalarRamp=render(module,parameters,curves,1,Kind::scalarRamp);
        const auto scalarStep=render(module,parameters,curves,1,Kind::scalarImmediate);
        double rampError=0,stepError=0;
        for(int block:{7,64,256,1024,4096})
        {
            rampError=std::max(rampError,difference(scalarRamp,render(module,parameters,curves,block,Kind::ramp)));
            stepError=std::max(stepError,difference(scalarStep,render(module,parameters,curves,block,Kind::immediate)));
        }
        check(rampError<=limits["audio_max_abs_error"],"native AU ramps match independent per-sample controls across partitions");
        check(stepError<=limits["audio_max_abs_error"],"native AU immediate offsets match independent per-sample controls");
        check(difference(scalarStep,render(module,parameters,curves,256,Kind::directImmediate))<=limits["audio_max_abs_error"],
              "AudioUnitSetParameter retains nonzero buffer offsets");
        const auto neutral=render(module,parameters,curves,256,Kind::off);
        check(difference(scalarRamp,neutral)>0.00001,"AU automation is audible (positive control)");
        for(bool pure:{false,true})
        {
            auto protectedParameters=parameters;
            if(pure)protectedParameters[cs80::P_parityMode]=1;else protectedParameters[cs80::P_sourcePerformance]=0;
            check(render(module,protectedParameters,curves,256,Kind::ramp)==render(module,protectedParameters,curves,256,Kind::off),
                  "AU Pure/Legacy preserve the former ignored-ramp behavior");
            check(render(module,protectedParameters,curves,256,Kind::immediate)==render(module,protectedParameters,curves,256,Kind::flatImmediate),
                  "AU Pure/Legacy preserve immediate block-start behavior");
        }
        Curves rankOnly=curves;
        for(size_t i=0;i<rankOnly.size();++i)if(cs80::kSampleAccurateParameters[i]<cs80::P_ch1Start)rankOnly[i].clear();
        for(int source:{0,11,12})check(render(module,parameters,rankOnly,256,Kind::ramp,source)==render(module,parameters,rankOnly,256,Kind::off,source),
                                     "AU panel curves preserve factory/memory ownership");
        auto sixteen=parameters;sixteen[cs80::P_voiceMode]=1;
        const double sixteenError=difference(render(module,sixteen,curves,1,Kind::scalarRamp),render(module,sixteen,curves,256,Kind::ramp));
        check(sixteenError<=limits["audio_max_abs_error"],"AU automation reaches both engines");
        std::cout<<"native AU ramp error="<<rampError<<" immediate error="<<stepError<<" sixteen-voice error="<<sixteenError<<'\n';
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
    return failures==0?0:1;
}
