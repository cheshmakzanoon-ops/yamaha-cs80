#pragma once
// Bounded correctness observer only. No loader, CLI, audio device or CPU clock.
#include "native_observer_sdk.hpp"
#include "native_observer_topology.hpp"
#include <pluginterfaces/vst/ivstevents.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>
#include <memory>

namespace cs80_observer {
struct TimedEvent { std::int64_t frame; Event value; };
struct TimedParameter { std::int64_t frame;ParamID id;double value; };
struct RenderProtocol {
    double rate=48000;int32 mode=kRealtime,block=64;std::int64_t frames=4096;
    bool legacyCCFixtureConvention=false; // SDK defines this event as output-only. Explicit research opt-in.
    std::vector<TimedEvent> events;
    std::vector<TimedParameter> parameters;
    AudioInputTopology topology=AudioInputTopology::ZeroAudioInput;
};
inline bool normalizedValue(double v){return std::isfinite(v) && v>=0 && v<=1;}
inline void validateEvent(const Event& e) {
    require(e.busIndex==0 && e.sampleOffset==0 && e.ppqPosition==0,"canonical event header");
    require((e.flags & ~(Event::kIsLive|Event::kUserReserved1|Event::kUserReserved2))==0,"unsupported event flags");
    const auto note=[](int channel,int pitch,double velocity,double tuning,int id){require(channel>=0 && channel<16 && pitch>=0 && pitch<=127 && normalizedValue(velocity) && std::isfinite(tuning) && id>=-1,"invalid note event");};
    if(e.type==Event::kNoteOnEvent){note(e.noteOn.channel,e.noteOn.pitch,e.noteOn.velocity,e.noteOn.tuning,e.noteOn.noteId);require(e.noteOn.length>=0,"negative note length");}
    else if(e.type==Event::kNoteOffEvent)note(e.noteOff.channel,e.noteOff.pitch,e.noteOff.velocity,e.noteOff.tuning,e.noteOff.noteId);
    else if(e.type==Event::kLegacyMIDICCOutEvent){require(e.midiCCOut.controlNumber<=127 && e.midiCCOut.channel>=0 && e.midiCCOut.channel<16 && e.midiCCOut.value>=0 && e.midiCCOut.value2>=0,"invalid legacy CC fixture event");}
    else require(false,"unsupported event type");
}
inline void validateProtocol(const RenderProtocol& p) {
    validateAudioInputTopology(p.topology);
    require((p.rate==44100 || p.rate==48000 || p.rate==96000) && (p.mode==kRealtime || p.mode==kOffline),"unsupported rate/mode");
    require(p.block>0 && p.block<=4096 && p.frames>0 && p.frames<=1048576,"bounded positive render dimensions");
    require(p.events.size()<=65536 && p.parameters.size()<=4096,"bounded input stream");
    std::int64_t last=-1;
    for(const auto& e:p.events){require(e.frame>=last && e.frame>=0 && e.frame<p.frames,"event order/frame");validateEvent(e.value);require(e.value.type!=Event::kLegacyMIDICCOutEvent || p.legacyCCFixtureConvention,"legacy CC input requires explicit fixture convention");last=e.frame;}
    last=-1;
    for(const auto& q:p.parameters){require(q.frame>=last && q.frame>=0 && q.frame<p.frames && q.id!=kNoParamId && normalizedValue(q.value),"parameter order/frame/value");last=q.frame;}
}

template<class Interface> inline tresult observerQuery(Interface* self,const TUID requested,const FUID& iid,void** out){
    if(!out)return kInvalidArgument;
    *out=nullptr;
    if(std::memcmp(requested,iid,sizeof(TUID)) && std::memcmp(requested,FUnknown::iid,sizeof(TUID)))return kNoInterface;
    *out=self;self->addRef();return kResultOk;
}
class RenderEvents final:public IEventList {
public:
    std::vector<Event> rows;bool mutationAttempted=false;
    tresult PLUGIN_API queryInterface(const TUID id,void** out)override{return observerQuery(this,id,IEventList::iid,out);}
    uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
    int32 PLUGIN_API getEventCount()override{return static_cast<int32>(rows.size());}
    tresult PLUGIN_API getEvent(int32 i,Event& event)override{if(i<0 || static_cast<std::size_t>(i)>=rows.size())return kInvalidArgument;event=rows[static_cast<std::size_t>(i)];return kResultOk;}
    tresult PLUGIN_API addEvent(Event&)override{mutationAttempted=true;return kNotImplemented;}
};
class RenderQueue final:public IParamValueQueue {
public:
    ParamID id;bool writable,failed=false;int32 frames;std::size_t* total;
    std::vector<std::pair<int32,ParamValue>> points;
    RenderQueue(ParamID i,bool write,int32 n,std::size_t* count):id(i),writable(write),frames(n),total(count){}
    tresult PLUGIN_API queryInterface(const TUID iid,void** out)override{return observerQuery(this,iid,IParamValueQueue::iid,out);}
    uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
    ParamID PLUGIN_API getParameterId()override{return id;}
    int32 PLUGIN_API getPointCount()override{return static_cast<int32>(points.size());}
    tresult PLUGIN_API getPoint(int32 i,int32& offset,ParamValue& value)override{if(i<0 || static_cast<std::size_t>(i)>=points.size())return kInvalidArgument;offset=points[static_cast<std::size_t>(i)].first;value=points[static_cast<std::size_t>(i)].second;return kResultOk;}
    tresult PLUGIN_API addPoint(int32 offset,ParamValue value,int32& index)override{
        index=-1;
        if(!writable || offset<0 || offset>=frames || !normalizedValue(value) || (!points.empty() && offset<points.back().first) || *total>=4096){failed=true;return kInvalidArgument;}
        try{points.emplace_back(offset,value);++*total;index=static_cast<int32>(points.size()-1);return kResultOk;}catch(...){failed=true;return kInternalError;}
    }
};
class RenderChanges final:public IParameterChanges {
public:
    bool writable,failed=false;int32 frames;std::size_t total=0;
    std::vector<std::unique_ptr<RenderQueue>> queues;
    RenderChanges(bool write,int32 n):writable(write),frames(n){}
    tresult PLUGIN_API queryInterface(const TUID id,void** out)override{return observerQuery(this,id,IParameterChanges::iid,out);}
    uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
    int32 PLUGIN_API getParameterCount()override{return static_cast<int32>(queues.size());}
    IParamValueQueue* PLUGIN_API getParameterData(int32 i)override{return i>=0 && static_cast<std::size_t>(i)<queues.size()?queues[static_cast<std::size_t>(i)].get():nullptr;}
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID& id,int32& index)override{
        index=-1;if(!writable || id==kNoParamId){failed=true;return nullptr;}
        try{return appendQueue(id,index);}catch(...){failed=true;return nullptr;}
    }
    RenderQueue* appendQueue(ParamID id,int32& index){
        for(std::size_t i=0;i<queues.size();++i)if(queues[i]->id==id){index=static_cast<int32>(i);return queues[i].get();}
        require(queues.size()<4096,"parameter queue bound");queues.push_back(std::make_unique<RenderQueue>(id,writable,frames,&total));index=static_cast<int32>(queues.size()-1);return queues.back().get();
    }
    bool valid()const{return !failed && std::none_of(queues.begin(),queues.end(),[](const auto& q){return q->failed;});}
};
struct RenderCallback {std::int64_t start;int32 count;tresult returned;uint64 silenceFlags;};
struct RenderObservation {
    std::vector<float> interleaved;
    std::vector<RenderCallback> callbacks;
    std::vector<TimedParameter> outputParameters;
};
// Caller must have initialized/setup/activated/started this processor with the
// SAME rate/mode/max block, explicitly selected input topology and complete frozen state. This helper does not do so.
inline RenderObservation renderObserved(IAudioProcessor& processor,const RenderProtocol& p) {
    validateProtocol(p);RenderObservation out;
    out.interleaved.reserve(static_cast<std::size_t>(p.frames)*2);
    std::size_t event=0,parameter=0;
    for(std::int64_t start=0;start<p.frames;start+=p.block){
        const auto count=static_cast<int32>(std::min<std::int64_t>(p.block,p.frames-start));
        RenderEvents inputEvents;RenderChanges input(false,count),output(true,count);
        while(event<p.events.size() && p.events[event].frame<start+count){auto e=p.events[event].value;e.sampleOffset=static_cast<int32>(p.events[event].frame-start);inputEvents.rows.push_back(e);++event;}
        while(parameter<p.parameters.size() && p.parameters[parameter].frame<start+count){const auto& q=p.parameters[parameter++];int32 index=0;auto* queue=input.appendQueue(q.id,index);queue->points.emplace_back(static_cast<int32>(q.frame-start),q.value);}
        std::vector<float> left(static_cast<std::size_t>(count),0),right(static_cast<std::size_t>(count),0);
        float* channels[]{left.data(),right.data()};AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;
        // VST3 requires an AudioBusBuffers descriptor and channel-pointer array
        // for inactive buses too. The inactive mono sample pointer may be null.
        // This profile is legal only after the owner has disabled the input bus.
        float* inputChannels[]{nullptr};AudioBusBuffers inputBus{};
        inputBus.numChannels=1;inputBus.channelBuffers32=inputChannels;
        const bool disabledInput=p.topology==AudioInputTopology::DisabledMonoExternalInput;
        ProcessContext context{};context.sampleRate=p.rate;context.projectTimeSamples=start;
        ProcessData data{};data.processMode=p.mode;data.symbolicSampleSize=kSample32;data.numSamples=count;data.numOutputs=1;data.outputs=&bus;
        data.numInputs=disabledInput?1:0;data.inputs=disabledInput?&inputBus:nullptr;
        data.inputEvents=&inputEvents;data.inputParameterChanges=&input;data.outputParameterChanges=&output;data.processContext=&context;
        const auto result=processor.process(data);out.callbacks.push_back({start,count,result,bus.silenceFlags});
        require(result==kResultOk,"native process failed");
        require(data.numSamples==count && data.numInputs==(disabledInput?1:0) && data.inputs==(disabledInput?&inputBus:nullptr) && (!disabledInput || (inputBus.numChannels==1 && inputBus.channelBuffers32==inputChannels && inputChannels[0]==nullptr && inputBus.silenceFlags==0)) && data.numOutputs==1 && data.outputs==&bus && bus.numChannels==2 && bus.channelBuffers32==channels && channels[0]==left.data() && channels[1]==right.data(),"process buffer descriptor mutation");require(!inputEvents.mutationAttempted && input.valid() && output.valid(),"invalid host queue mutation/output");
        require(data.processMode==p.mode && data.symbolicSampleSize==kSample32 && data.inputEvents==&inputEvents && data.outputEvents==nullptr && data.inputParameterChanges==&input && data.outputParameterChanges==&output && data.processContext==&context,"process interface descriptor mutation");
        require(context.state==0 && context.sampleRate==p.rate && context.projectTimeSamples==start && context.systemTime==0 && context.continousTimeSamples==0 && context.projectTimeMusic==0 && context.barPositionMusic==0 && context.cycleStartMusic==0 && context.cycleEndMusic==0 && context.tempo==0 && context.timeSigNumerator==0 && context.timeSigDenominator==0 && context.chord.keyNote==0 && context.chord.rootNote==0 && context.chord.chordMask==0 && context.smpteOffsetSubframes==0 && context.frameRate.framesPerSecond==0 && context.frameRate.flags==0 && context.samplesToNextClock==0,"process context mutation");
        require((bus.silenceFlags & ~uint64{3})==0,"invalid stereo silence mask");
        for(int32 i=0;i<count;++i){const auto l=left[static_cast<std::size_t>(i)],r=right[static_cast<std::size_t>(i)];require(std::isfinite(l) && std::isfinite(r),"nonfinite rendered PCM");out.interleaved.push_back(l);out.interleaved.push_back(r);}
        for(const auto& q:output.queues)for(const auto& point:q->points){require(out.outputParameters.size()<65536,"total output parameter bound");out.outputParameters.push_back({start+point.first,q->id,point.second});}
    }
    require(event==p.events.size() && parameter==p.parameters.size() && out.interleaved.size()==static_cast<std::size_t>(p.frames)*2,"incomplete rendered input/output inventory");
    return out;
}
} // namespace cs80_observer
