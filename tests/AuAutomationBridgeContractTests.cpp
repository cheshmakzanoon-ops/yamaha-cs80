// Compile the exact AU class-body hooks on every platform against the minimal
// API types they use. Native macOS ABI/rendering is tested separately; this is
// a deterministic scheduling/dispatch contract fixture, not an Apple host.
#include "plugin/AuAutomationQueue.h"
#include "Ws1Tolerances.h"
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>
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
    float value=0.2f, projectionOffset=0.0f;
    void* observerContext=nullptr;
    void (*duringSet)(void*)=nullptr;
    float getValue() const {return value+projectionOffset;}
    void setValueNotifyingHost(float next){value=next;if(duringSet!=nullptr)duringSet(observerContext);}
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
    // Keep the original reviewed JUCE GetParameter expression as the fallback
    // so the same witness can run before publication support exists. There is
    // no skipped assertion: that original path must fail the exact knot gate.
    template<class Unit> static float readPublished(Unit& unit,AudioProcessorParameter& parameter)
    {
        if constexpr(requires {unit.cs80ReadAuParameter(parameter);})
            return unit.cs80ReadAuParameter(parameter);
        return parameter.getValue();
    }
    template<class Unit> static void notifyExternal(Unit& unit,AudioProcessorParameter& parameter)
    {if constexpr(requires {unit.cs80InvalidateAuParameter(parameter);})unit.cs80InvalidateAuParameter(parameter);}
    template<class Unit> static void resetPublication(Unit& unit)
    {if constexpr(requires {unit.cs80ResetAuPublications();})unit.cs80ResetAuPublications();}
    template<class Unit> static float readPair(Unit& unit,float current)
    {
        if constexpr(requires {unit.cs80AuPublications[0].read(current);})
            return unit.cs80AuPublications[0].read(current);
        return current;
    }
    template<class Unit> static void publishPair(Unit& unit,float normalized,float current)
    {
        if constexpr(requires {unit.cs80AuPublications[0].begin();})
        {
            auto& publication=unit.cs80AuPublications[0];
            const auto token=publication.begin();publication.publish(token,normalized,current);
        }
    }
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
    float hostValue(){return readPublished(*this,juceFilter->parameters[0]);}
    void projectionOffset(float value){juceFilter->parameters[0].projectionOffset=value;}
    void notifyExternal(){notifyExternal(*this,juceFilter->parameters[0]);}
    void resetPublication(){resetPublication(*this);}
    float readPair(float current){return readPair(*this,current);}
    void publishPair(float normalized,float current){publishPair(*this,normalized,current);}
    void invalidateDuringCommit()
    {
        auto& parameter=juceFilter->parameters[0];parameter.observerContext=this;
        parameter.duringSet=[](void* context){static_cast<JuceAU*>(context)->notifyExternal();};
    }
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
        const ws1::Tolerances publicationLimits((std::filesystem::path(argc>1?argv[1]:
            "tests/parity/tolerances-v2/host-automation.json").parent_path()/"au-normalized-publication.json").string(),
            "au_normalized_publication");
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

        // Model the independently proven nonidentity storage round trip without
        // depending on this machine's floating-point contraction or libm. The
        // real Intel AU witness supplies the physical-range evidence separately.
        JuceAU publication;
        publication.projectionOffset(0x1p-23f);
        const auto same=[&](float a,float b,const char* limit){return std::abs(double(a)-double(b))<=publicationLimits[limit];};
        check(same(publication.hostValue(),publication.value(),"external_readback_error"),
              "unpublished AU parameter retains its original storage getter");
        constexpr float accepted=0.62851631641387939f;
        const auto publish=[&] {
            const auto event=immediate(0,accepted);
            check(publication.ScheduleParameter(&event,1)==noErr,"AU publication witness schedules an accepted Float32");
            check(publication.render(1).size()==1,"AU publication witness renders its complete event");
        };
        publish();
        check(publication.value()!=accepted,"storage-projection witness has a nonzero independent positive control");
        check(same(publication.hostValue(),accepted,"scheduled_value_error"),
              "AU hook preserves its accepted normalized Float32 despite physical storage projection");
        publication.setModern(false);
        check(same(publication.hostValue(),publication.value(),"protected_readback_error"),
              "Pure/Legacy retain the original storage getter after a modern publication");
        publication.setModern(true);
        publication.setCurrent(0.4f);
        check(same(publication.hostValue(),publication.value(),"external_readback_error"),
              "changed underlying storage supersedes the preceding AU publication");
        publish();
        publication.notifyExternal();
        check(same(publication.hostValue(),publication.value(),"external_readback_error"),
              "external notification invalidates AU ownership even with unchanged storage readback");
        publish();
        publication.resetPublication();
        check(same(publication.hostValue(),publication.value(),"external_readback_error"),
              "AU lifecycle reset discards preceding publication ownership");
        publication.invalidateDuringCommit();
        publish();
        check(same(publication.hostValue(),publication.value(),"external_readback_error"),
              "an external invalidation during commit cannot be overwritten by the older AU publication");
        JuceAU concurrent;
        constexpr float first=0.25f,second=0.75f,firstReadback=first+0x1p-23f,secondReadback=second+0x1p-23f;
        std::atomic<bool> finished=false;
        std::uint64_t incoherent=0;
        std::thread reader([&] {
            do {
                const auto a=concurrent.readPair(firstReadback),b=concurrent.readPair(secondReadback);
                if(a!=first&&a!=firstReadback)++incoherent;
                if(b!=second&&b!=secondReadback)++incoherent;
            } while(!finished.load());
        });
        for(int repeat=0;repeat<16384;++repeat)
        {
            concurrent.publishPair(first,firstReadback);
            concurrent.publishPair(second,secondReadback);
        }
        finished.store(true);reader.join();
        check(incoherent<=publicationLimits["incoherent_read_count"],
              "concurrent AU readers never combine an endpoint with another publication's readback");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
    return failures==0?0:1;
}
