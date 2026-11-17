#pragma once
#include "native_observer_render.hpp"
#include <set>
namespace cs80_observer {
struct FlushProtocol {int32 mode=kRealtime;std::vector<TimedParameter> controls;std::vector<ParamID> expectedIds;};
struct FlushReceipt {
 bool attempted=false,processAttempted=false,returned=false,accepted=false,descriptorsValid=false,queuesValid=false;
 tresult result=kInternalError;std::exception_ptr exception;std::vector<TimedParameter> output;std::vector<std::uint8_t> protocol;
};
inline void validateFlush(const FlushProtocol& p){
 require(p.mode==kRealtime || p.mode==kOffline,"flush mode");require(!p.controls.empty() && p.controls.size()<=4096 && p.controls.size()==p.expectedIds.size(),"complete bounded flush roster");std::set<ParamID> ids;
 for(std::size_t i=0;i<p.controls.size();++i){const auto& q=p.controls[i];require(q.frame==0 && q.id!=kNoParamId && q.id==p.expectedIds[i] && ids.insert(q.id).second && normalizedValue(q.value),"flush input identity/offset/value");}
}
inline Bytes encodeFlushProtocol(const FlushProtocol& p){validateFlush(p);Bytes b;b.text("cs80-control-input-v1");b.u32(static_cast<std::uint32_t>(p.mode));b.u64(p.controls.size());for(const auto& q:p.controls){b.u32(q.id);b.f64(q.value);}return b;}
// Session must implement the owned phase-checked wrapper. A caller-provided enum
// is not enough: wrapper catches errors, latches terminal and performs cleanup.
template<class Session> void flushObserved(Session& session,const FlushProtocol& p,FlushReceipt& receipt){
 const bool retry=receipt.attempted;receipt.attempted=true;
 try {session.withProcessingOperation([&](IAudioProcessor& processor,const auto& setup){
  require(!retry,"flush retry forbidden");validateFlush(p);receipt.protocol=encodeFlushProtocol(p).data;require(p.mode==setup.mode,"flush setup mode mismatch");
  // Queue offset zero is a CONTROL boundary, not one audio frame. ProcessData
  // remains all-zero audio dimensions; no sample or audio buffer is allocated.
  RenderChanges input(false,1),output(true,1);
  for(const auto& q:p.controls){int32 index;input.appendQueue(q.id,index)->points.emplace_back(0,q.value);}
  ProcessData data{};data.processMode=p.mode;data.symbolicSampleSize=kSample32;data.inputParameterChanges=&input;data.outputParameterChanges=&output;
  receipt.processAttempted=true;
  try{receipt.result=processor.process(data);receipt.returned=true;}catch(...){receipt.exception=std::current_exception();}
  receipt.descriptorsValid=data.processMode==p.mode && data.symbolicSampleSize==kSample32 && data.numSamples==0 && data.numInputs==0 && data.numOutputs==0 && !data.inputs && !data.outputs && !data.inputEvents && !data.outputEvents && !data.processContext && data.inputParameterChanges==&input && data.outputParameterChanges==&output;
  receipt.queuesValid=input.valid() && output.valid();
  for(const auto& q:output.queues)for(const auto& v:q->points)receipt.output.push_back({v.first,q->id,v.second});
  if(receipt.exception)std::rethrow_exception(receipt.exception);
  require(receipt.returned && receipt.result==kResultOk,"flush process failed");require(receipt.descriptorsValid && receipt.queuesValid,"flush descriptors/queues invalid");receipt.accepted=true;
 });}catch(...){receipt.accepted=false;if(!receipt.exception)receipt.exception=std::current_exception();throw;}
}
inline Bytes encodeFlush(const FlushProtocol& p,const FlushReceipt& r){
 validateFlush(p);require(r.attempted && r.processAttempted && r.returned && r.accepted && !r.exception && r.result==kResultOk && r.descriptorsValid && r.queuesValid,"unadmitted flush receipt");require(r.output.size()<=4096,"flush output bound");
 require(r.protocol==encodeFlushProtocol(p).data,"flush protocol identity drift");Bytes b;b.text("cs80-control-flush-v1");b.blob(r.protocol.data(),r.protocol.size());b.u32(static_cast<std::uint32_t>(p.mode));b.u32(0);b.u32(0);b.u32(0); // exact audio dimensions
 b.u64(p.controls.size());for(const auto& q:p.controls){b.u32(q.id);b.f64(q.value);}b.u32(static_cast<std::uint32_t>(r.result));b.u64(r.output.size());
 std::set<ParamID> groups;ParamID last=kNoParamId;
 for(const auto& q:r.output){require(q.frame==0 && q.id!=kNoParamId && normalizedValue(q.value),"invalid flush output point");if(q.id!=last){require(groups.insert(q.id).second,"flush output group reentry");last=q.id;}b.u32(q.id);b.f64(q.value);}return b;
}
}
