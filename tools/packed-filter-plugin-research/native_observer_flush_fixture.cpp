#define INIT_CLASS_IID
#include "native_observer_flush.hpp"
#include <iostream>
using namespace cs80_observer;
struct Processor:IAudioProcessor {
 std::string behavior;int calls=0,audioCalls=0;
 tresult PLUGIN_API queryInterface(const TUID id,void** out)override{return observerQuery(this,id,IAudioProcessor::iid,out);}
 uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
 tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32,SpeakerArrangement*,int32)override{return kResultOk;}
 tresult PLUGIN_API getBusArrangement(BusDirection,int32,SpeakerArrangement&)override{return kResultOk;}
 tresult PLUGIN_API canProcessSampleSize(int32)override{return kResultOk;}
 uint32 PLUGIN_API getLatencySamples()override{return 0;}
 tresult PLUGIN_API setupProcessing(ProcessSetup&)override{return kResultOk;}
 tresult PLUGIN_API setProcessing(TBool)override{return kResultOk;}
 uint32 PLUGIN_API getTailSamples()override{return 0;}
 tresult PLUGIN_API process(ProcessData& d)override{
  ++calls;if(d.numSamples || d.numInputs || d.numOutputs)++audioCalls;require(!audioCalls && !d.inputs && !d.outputs && !d.inputEvents && !d.outputEvents && !d.processContext,"no audio/event/context");
  require(d.inputParameterChanges->getParameterCount()==2,"full controls");
  for(int i=0;i<2;++i){auto* q=d.inputParameterChanges->getParameterData(i);int32 offset=-1;double value=-1;require(q->getPointCount()==1 && q->getPoint(0,offset,value)==kResultOk && offset==0 && value==.25*(i+1),"complete control points");}
  int32 index;auto* q=d.outputParameterChanges->addParameterData(7,index);require(q,"output queue");q->addPoint(0,.75,index);
  if(behavior=="offset")q->addPoint(1,.5,index);
  if(behavior=="value")q->addPoint(0,2,index);
  if(behavior=="quota")for(int i=0;i<4096;++i)q->addPoint(0,.5,index);
  if(behavior=="input-mutation")d.inputParameterChanges->getParameterData(0)->addPoint(0,.5,index);
  if(behavior=="samples")d.numSamples=1;
  if(behavior=="mode")d.processMode=kOffline;
  if(behavior=="format")d.symbolicSampleSize=kSample64;
  if(behavior=="inputs")d.numInputs=1;
  if(behavior=="outputs")d.numOutputs=1;
  if(behavior=="pointers")d.outputParameterChanges=nullptr;
  static AudioBusBuffers bus{};static ProcessContext context{};static RenderEvents events;
  if(behavior=="audio-pointer")d.outputs=&bus;
  if(behavior=="event-pointer")d.inputEvents=&events;
  if(behavior=="context-pointer")d.processContext=&context;
  if(behavior=="invalid-output-id")d.outputParameterChanges->addParameterData(kNoParamId,index);
  if(behavior=="failure")return kResultFalse;
  if(behavior=="exception")throw std::runtime_error("process exception");
  return kResultOk;
 }
};
struct Session {
 struct Setup {int32 mode=kRealtime;} setup;
 Processor processor;bool processing=true,terminal=false;int cleanup=0;
 template<class F>void withProcessingOperation(F f){try{require(processing && !terminal,"phase guard");f(processor,setup);}catch(...){terminal=true;processing=false;++cleanup;throw;}}
};
int main(int argc,char** argv){try{
 require(argc==2,"case");std::string name=argv[1];Session session;FlushProtocol p;p.controls={{0,3,.25},{0,5,.5}};p.expectedIds={3,5};FlushReceipt r;bool negative=true;
 if(name=="valid" || name=="offline" || name=="retry" || name=="encode-drift" || name=="encode-output" || name=="golden")negative=false;
 else if(name=="phase")session.processing=false;
 else if(name=="missing")p.controls.pop_back();
 else if(name=="duplicate"){p.controls[1].id=3;p.expectedIds[1]=3;}
 else if(name=="wrong-id")p.controls[1].id=6;
 else if(name=="input-offset")p.controls[0].frame=1;
 else if(name=="input-value")p.controls[0].value=2;
 else if(name=="mode-mismatch")p.mode=kOffline;
 else session.processor.behavior=name;
 if(name=="offline")p.mode=session.setup.mode=kOffline;
 bool caught=false;try{flushObserved(session,p,r);}catch(...){caught=true;}
 require(caught==negative && r.attempted && r.accepted==!negative,"flush admission");require(session.processor.audioCalls==0,"never audio");
 if(negative){require(session.terminal && session.cleanup==1,"terminal cleanup");if(name=="failure" || name=="exception"){require(r.processAttempted && r.output.size()==1,"partial output retained");require(r.returned==(name=="failure"),"return/exception distinction");}return 0;}
 require(r.returned && r.processAttempted && r.output.size()==1 && !session.terminal,"completed flush");
 if(name=="retry"){caught=false;try{flushObserved(session,p,r);}catch(...){caught=true;}require(caught && session.processor.calls==1 && session.terminal && session.cleanup==1 && !r.accepted,"no duplicate native call and terminal");return 0;}
 if(name=="encode-drift")p.controls[0].value=.1;
 if(name=="encode-output")r.output[0].frame=1;
 caught=false;Bytes bytes;try{bytes=encodeFlush(p,r);}catch(...){caught=true;}
 require(caught==(name=="encode-drift" || name=="encode-output"),"encoding admission");
 if(name=="golden")for(auto v:bytes.data)std::printf("%02x",v);
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
