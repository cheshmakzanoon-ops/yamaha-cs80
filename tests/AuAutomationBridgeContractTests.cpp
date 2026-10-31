// Compile the exact AU class-body hooks on every platform against the minimal
// API types they use. Native macOS ABI/rendering is tested separately; this is
// a deterministic scheduling/dispatch contract fixture, not an Apple host.
#include "plugin/AuAutomationQueue.h"
#include "Ws1Tolerances.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

using UInt32 = std::uint32_t;
using OSStatus = std::int32_t;
constexpr OSStatus noErr=0,kAudioUnitErr_InvalidParameter=-10878,kAudioUnitErr_InvalidScope=-10866,
                   kAudioUnitErr_CannotDoInCurrentContext=-10863;
constexpr UInt32 kAudioUnitScope_Global=0,kParameterEvent_Immediate=1,kParameterEvent_Ramped=2;
struct AudioUnitParameterEvent
{
    UInt32 scope=0,element=0,parameter=0,eventType=0;
    union Values {
        struct { UInt32 bufferOffset; float value; } immediate;
        struct { std::int32_t startBufferOffset; UInt32 durationInFrames; float startValue,endValue; } ramp;
        Values(): immediate{} {}
    } eventValues;
};
namespace juce {
class AudioProcessorParameter
{
public:
    virtual ~AudioProcessorParameter()=default;
    float value=0.2f;
    float getValue() const {return value;}
    void setValueNotifyingHost(float next){value=next;}
};
}
using juce::AudioProcessorParameter;
bool approximatelyEqual(float a,float b){return a==b;}
#define jassert(condition) assert(condition)
struct Processor final:cs80::HostAutomationClient
{
    std::array<AudioProcessorParameter,cs80::kSampleAccurateParameters.size()+1> parameters;
    std::array<cs80::HostAutomationCurve,cs80::kSampleAccurateParameters.size()> curves;
    bool modern=true;
    bool usesHostAutomationTiming() const noexcept override{return modern;}
    int sampleAccurateParameterIndex(const AudioProcessorParameter& parameter) const noexcept override
    {for(size_t i=0;i<curves.size();++i)if(&parameters[i]==&parameter)return static_cast<int>(i);return -1;}
    double sampleAccurateParameterValue(size_t lane) const noexcept override{return parameters[lane].getValue();}
    void beginHostAutomationBlock(int) noexcept override {for(auto& curve:curves)curve={};}
    bool attachHostAutomationQueue(AudioProcessorParameter& parameter,cs80::HostAutomationQueue queue) noexcept override
    {const int lane=sampleAccurateParameterIndex(parameter);if(lane<0)return false;curves[static_cast<size_t>(lane)].begin(queue,parameter.getValue());return true;}
    void endHostAutomationBlock() noexcept override {for(auto& curve:curves)if(curve.active())curve.commit();}
};
struct Base
{
    virtual ~Base()=default;
    virtual OSStatus ScheduleParameter(const AudioUnitParameterEvent*,UInt32)=0;
};
class JuceAU:public Base
{
    std::unique_ptr<Processor> juceFilter=std::make_unique<Processor>();
    bool inParameterChangedCallback=false;
    float getMaximumParameterValue(AudioProcessorParameter*){return 1.0f;}
    AudioProcessorParameter* getParameterForAUParameterID(UInt32 parameter)
    {return parameter<juceFilter->parameters.size()?&juceFilter->parameters[parameter]:nullptr;}
    OSStatus SetParameter(UInt32 parameter,UInt32,UInt32,float value,UInt32)
    {if(auto* p=getParameterForAUParameterID(parameter)){p->setValueNotifyingHost(value);return noErr;}return kAudioUnitErr_InvalidParameter;}
    #include "plugin/AuAutomationHooks.inc"
public:
    std::vector<double> render(int frames)
    {
        cs80::HostAutomationBlock block(juceFilter.get(),frames);
        if(!cs80PrepareAuAutomation(block.get(),frames))return {};
        std::vector<double> output;
        for(int i=0;i<frames;++i)output.push_back(juceFilter->curves[0].active()?juceFilter->curves[0].valueAt(i):juceFilter->parameters[0].getValue());
        return output;
    }
    void setModern(bool value){juceFilter->modern=value;}
    void setCurrent(float value){juceFilter->parameters[0].value=value;}
    float value()const{return juceFilter->parameters[0].getValue();}
    OSStatus direct(float value,UInt32 offset){return cs80ScheduleAuImmediate(juceFilter->parameters[0],value,offset);}
};
AudioUnitParameterEvent immediate(UInt32 offset,float value)
{AudioUnitParameterEvent e;e.eventType=kParameterEvent_Immediate;e.eventValues.immediate={offset,value};return e;}
AudioUnitParameterEvent ramp(int start,UInt32 duration,float a,float b)
{AudioUnitParameterEvent e;e.eventType=kParameterEvent_Ramped;e.eventValues.ramp={start,duration,a,b};return e;}
int main(int argc,char** argv)
{
    int failures=0;const auto check=[&](bool pass,const char* text){if(!pass){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    try {
        const ws1::Tolerances limits(argc>1?argv[1]:"tests/parity/tolerances-v2/host-automation.json","host_automation");
        const auto close=[&](double a,double b){return std::abs(a-b)<=limits["normalized_curve_error"];};
        JuceAU unit;
        const std::array events {ramp(-4,8,0.1f,0.9f),immediate(9,0.3f),ramp(12,24,0.3f,0.7f)};
        check(unit.ScheduleParameter(events.data(),events.size())==noErr,"actual AU hook accepts mixed scheduled batch");
        const auto output=unit.render(16);check(output.size()==16,"AU hook produces queue views");
        if(output.size()==16)
            for(int i=0;i<16;++i)
            {
                const double expected=i<5?0.1f+(double(0.9f)-0.1f)*double(i+4)/8
                    :i<9?double(0.9f):i<12?double(0.3f):double(0.3f)+(double(0.7f)-0.3f)*double(i-12)/24;
                check(close(output[static_cast<size_t>(i)],expected),"hook preserves immediate offsets and negative-start ramps");
            }
        check(close(unit.value(),0.3f+(double(0.7f)-0.3f)*3/24),"hook publishes rendered endpoint through original setter callback");
        unit.setCurrent(0.65f);check(unit.direct(0.25f,3)==noErr,"nonzero direct offset schedules");
        const auto direct=unit.render(5);
        check(direct.size()==5&&close(direct[0],0.65)&&close(direct[2],0.65)&&close(direct[3],0.25),"current offset-zero state holds until a later direct event");
        unit.setModern(false);unit.setCurrent(0.2f);
        check(unit.ScheduleParameter(events.data(),events.size())==noErr,"protected policy accepts legacy scheduling call");
        const auto protectedOutput=unit.render(16);
        bool protectedSame=protectedOutput.size()==16;for(double value:protectedOutput)protectedSame=protectedSame&&close(value,0.3f);
        check(protectedSame,"Pure/Legacy fallback ignores ramps and applies immediate block value");
        JuceAU bounded;
        std::vector<AudioUnitParameterEvent> full(cs80::AuAutomationQueue::capacity,immediate(0,0.4f));
        check(bounded.ScheduleParameter(full.data(),full.size())==noErr,"full scheduling capacity accepted");
        const auto extra=immediate(0,0.8f);
        check(bounded.ScheduleParameter(&extra,1)==kAudioUnitErr_CannotDoInCurrentContext,"AU hook reports capacity error");
        check(bounded.render(1)==std::vector<double>{double(0.4f)},"overflow cannot modify accepted queue");
        JuceAU invalid;
        auto bad=immediate(0,0.7f);bad.parameter=999;
        const std::array partial{immediate(0,0.9f),bad};
        check(invalid.ScheduleParameter(partial.data(),partial.size())==kAudioUnitErr_InvalidParameter,"AU hook validates complete batch before accepting it");
        check(invalid.render(1)==std::vector<double>{double(0.2f)},"invalid batch has no partial side effect");
        bad=immediate(0,0.7f);bad.scope=1;
        check(invalid.ScheduleParameter(&bad,1)==kAudioUnitErr_InvalidScope,"AU hook rejects unsupported scope");
        check(invalid.ScheduleParameter(nullptr,1)==kAudioUnitErr_InvalidParameter,"AU hook rejects null event storage");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
    return failures==0?0:1;
}
