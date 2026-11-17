#define INIT_CLASS_IID
#include "native_observer_render.hpp"
#include <iostream>
#include <limits>
using namespace cs80_observer;
struct Fake final:IAudioProcessor {
    std::string mode; int calls=0;std::vector<TimedEvent> seen;std::vector<TimedParameter> params;
    tresult PLUGIN_API queryInterface(const TUID id,void** out)override{return observerQuery(this,id,IAudioProcessor::iid,out);}
    uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32,SpeakerArrangement*,int32)override{return kResultOk;}
    tresult PLUGIN_API getBusArrangement(BusDirection,int32,SpeakerArrangement&)override{return kResultOk;}
    tresult PLUGIN_API canProcessSampleSize(int32)override{return kResultOk;}
    uint32 PLUGIN_API getLatencySamples()override{return 0;}
    tresult PLUGIN_API setupProcessing(ProcessSetup&)override{return kResultOk;}
    tresult PLUGIN_API setProcessing(TBool)override{return kResultOk;}
    uint32 PLUGIN_API getTailSamples()override{return 0;}
    tresult PLUGIN_API process(ProcessData& d)override {
        ++calls;require(d.numInputs==0 && d.inputs==nullptr && d.numOutputs==1 && d.outputs && d.outputs->numChannels==2 && d.symbolicSampleSize==kSample32 && !d.outputEvents,"topology");
        require(d.processContext && d.processContext->state==0 && d.processContext->sampleRate==48000 && d.processMode==kRealtime,"context");
        auto start=d.processContext->projectTimeSamples;
        for(int i=0;i<d.inputEvents->getEventCount();++i){Event e{};require(d.inputEvents->getEvent(i,e)==kResultOk,"get event");seen.push_back({start+e.sampleOffset,e});}
        for(int i=0;i<d.inputParameterChanges->getParameterCount();++i){auto* q=d.inputParameterChanges->getParameterData(i);for(int j=0;j<q->getPointCount();++j){int32 offset;double value;require(q->getPoint(j,offset,value)==kResultOk,"get point");params.push_back({start+offset,q->getParameterId(),value});}}
        for(int i=0;i<d.numSamples;++i){d.outputs->channelBuffers32[0][i]=float(start+i);d.outputs->channelBuffers32[1][i]=-float(start+i);}
        if(mode=="process-failure")return kResultFalse;
        if(mode=="process-throw")throw std::runtime_error("fake process throw");
        if(mode=="context-mutation")d.processContext->tempo=120;
        if(mode=="interface-mutation")d.inputEvents=nullptr;
        if(mode=="sample-size-mutation")d.symbolicSampleSize=kSample64;
        if(mode=="descriptor")d.numSamples=0;
        if(mode=="nan")d.outputs->channelBuffers32[0][0]=std::numeric_limits<float>::quiet_NaN();
        if(mode=="silence")d.outputs->silenceFlags=4;
        if(mode=="input-event-mutation"){Event e{};d.inputEvents->addEvent(e);}
        if(mode=="input-param-mutation"){int32 index;d.inputParameterChanges->addParameterData(9,index);}
        if(mode=="input-point-mutation"){int32 index;d.inputParameterChanges->getParameterData(0)->addPoint(0,.5,index);}
        if(mode.rfind("output",0)==0){int32 index;auto* q=d.outputParameterChanges->addParameterData(9,index);require(q,"output queue");
            q->addPoint(mode=="output-offset"?d.numSamples:0,mode=="output-value"?2:.25,index);
            if(mode=="output-order"){q->addPoint(2,.5,index);q->addPoint(1,.5,index);}
            if(mode=="output-quota")for(int i=0;i<4097;++i)q->addPoint(0,.5,index);
        }
        return kResultOk;
    }
};
Event note(int pitch=60){Event e{};e.type=Event::kNoteOnEvent;e.noteOn.pitch=static_cast<int16>(pitch);e.noteOn.velocity=.5;e.noteOn.noteId=-1;return e;}
int main(int argc,char** argv){try {
    require(argc==2,"case argument");std::string name=argv[1];
    if(name=="interface-bounds"){
        RenderEvents e;Event value{};require(e.getEvent(-1,value)!=kResultOk && e.getEvent(0,value)!=kResultOk,"event invalid get");
        RenderChanges c(true,4);require(!c.getParameterData(-1) && !c.getParameterData(0),"queue invalid get");
        void* out=nullptr;require(e.queryInterface(IEventList::iid,&out)==kResultOk && out==&e,"event query");require(e.queryInterface(IEventList::iid,nullptr)==kInvalidArgument,"null query");
        require(c.queryInterface(IParameterChanges::iid,&out)==kResultOk && out==&c,"changes query");
        TUID unknown{};require(c.queryInterface(unknown,&out)==kNoInterface && !out,"unknown query");
        int32 index=-1,offset=-1;double v=-1;auto* q=c.addParameterData(7,index);require(q && index==0,"first queue");
        require(c.addParameterData(7,index)==q && index==0,"duplicate queue");
        require(q->queryInterface(IParamValueQueue::iid,&out)==kResultOk && out==q,"queue query");
        require(q->getPoint(-1,offset,v)!=kResultOk && q->getPoint(0,offset,v)!=kResultOk,"point invalid get");
        require(q->addPoint(0,.2,index)==kResultOk && index==0 && q->addPoint(0,.3,index)==kResultOk && index==1,"same-frame point order");
        require(q->getPoint(1,offset,v)==kResultOk && offset==0 && v==.3,"exact duplicate frame point");
        require(q->getPoint(2,offset,v)!=kResultOk,"past end point");return 0;
    }
    if(name=="output-invalid-id" || name=="output-negative" || name=="output-infinite" || name=="queue-quota"){
        RenderChanges c(true,4);int32 index;
        if(name=="output-invalid-id")require(!c.addParameterData(kNoParamId,index) && index==-1,"invalid ID");
        else if(name=="queue-quota"){for(ParamID i=0;i<4096;++i)require(c.addParameterData(i,index),"bounded queue creation");require(!c.addParameterData(4096,index),"queue bound");}
        else {auto* q=c.addParameterData(1,index);require(q->addPoint(name=="output-negative"?-1:0,name=="output-infinite"?std::numeric_limits<double>::infinity():.5,index)!=kResultOk && index==-1,"invalid output point");}
        require(!c.valid(),"invalid output must latch");return 0;
    }
    Fake f;RenderProtocol p;p.block=4;p.frames=10;
    p.events={{0,note(60)},{3,note(61)},{4,note(62)},{4,note(63)},{9,note(64)}};
    p.parameters={{0,1,.1},{3,2,.2},{3,1,.3},{4,1,.4},{9,2,.5}};
    bool bad=true;
    if(name=="note-off"){p.events[0].value={};p.events[0].value.type=Event::kNoteOffEvent;p.events[0].value.noteOff.pitch=60;p.events[0].value.noteOff.velocity=.5;p.events[0].value.noteOff.noteId=-1;}
    if(name=="cc" || name=="cc-no-opt-in" || name=="cc-invalid"){p.events[0].value={};p.events[0].value.type=Event::kLegacyMIDICCOutEvent;p.events[0].value.midiCCOut.controlNumber=1;p.events[0].value.midiCCOut.value=64;p.legacyCCFixtureConvention=name!="cc-no-opt-in";if(name=="cc-invalid")p.events[0].value.midiCCOut.value=-1;}
    if(name=="boundaries" || name=="output" || name=="empty" || name=="cc" || name=="note-off"){bad=false;f.mode=name;if(name=="empty"){p.events.clear();p.parameters.clear();}}
    else if(name=="cc-no-opt-in" || name=="cc-invalid"){}
    else if(name=="event-count")p.events.resize(65537,{0,note()});
    else if(name=="param-count")p.parameters.resize(4097,{0,1,.5});
    else if(name=="rate")p.rate=0;
    else if(name=="mode")p.mode=99;
    else if(name=="block")p.block=0;
    else if(name=="frames")p.frames=1048577;
    else if(name=="event-order")p.events[1].frame=-1;
    else if(name=="event-end")p.events.back().frame=10;
    else if(name=="event-type")p.events[0].value.type=Event::kDataEvent;
    else if(name=="event-ppq")p.events[0].value.ppqPosition=1;
    else if(name=="event-flags")p.events[0].value.flags=2;
    else if(name=="event-bus")p.events[0].value.busIndex=1;
    else if(name=="event-offset")p.events[0].value.sampleOffset=1;
    else if(name=="note-length")p.events[0].value.noteOn.length=-1;
    else if(name=="note-channel")p.events[0].value.noteOn.channel=16;
    else if(name=="note-id")p.events[0].value.noteOn.noteId=-2;
    else if(name=="note-tuning")p.events[0].value.noteOn.tuning=std::numeric_limits<float>::quiet_NaN();
    else if(name=="note-pitch")p.events[0].value.noteOn.pitch=128;
    else if(name=="note-value")p.events[0].value.noteOn.velocity=std::numeric_limits<float>::infinity();
    else if(name=="param-order")p.parameters[1].frame=-1;
    else if(name=="param-end")p.parameters.back().frame=10;
    else if(name=="param-id")p.parameters[0].id=kNoParamId;
    else if(name=="param-value")p.parameters[0].value=std::numeric_limits<double>::quiet_NaN();
    else f.mode=name;
    bool rejected=false;RenderObservation out;
    try{out=renderObserved(f,p);}catch(const std::exception&){rejected=true;}
    require(rejected==bad,"expected acceptance/rejection");
    if(bad){require(f.calls<=1,"failure must stop first process or before");}
    else {
        require(f.calls==3 && out.callbacks.size()==3 && out.callbacks.back().count==2 && out.interleaved.size()==20,"complete callback/PCM inventory");
        for(int i=0;i<10;++i)require(out.interleaved[2*i]==float(i) && out.interleaved[2*i+1]==-float(i),"PCM order");
        require(f.seen.size()==p.events.size() && f.params.size()==p.parameters.size(),"complete inputs");
        for(std::size_t i=0;i<p.events.size();++i)require(f.seen[i].frame==p.events[i].frame && (p.events[i].value.type==Event::kLegacyMIDICCOutEvent?f.seen[i].value.midiCCOut.value==64:f.seen[i].value.noteOn.pitch==p.events[i].value.noteOn.pitch),"event exact order");
        if(name!="empty")require(f.params[0].id==1 && f.params[1].id==1 && f.params[1].frame==3 && f.params[2].id==2,"per-ID point order and queue first-occurrence");
        if(name=="output")require(out.outputParameters.size()==3 && out.outputParameters[2].frame==8 && out.outputParameters[0].value==.25,"output absolute mapping");
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
