#pragma once
#include "native_observer_render.hpp"
#include <set>

namespace cs80_observer {
static_assert(sizeof(float)==4 && sizeof(double)==8 && std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,"v1 encoding requires IEEE binary32/binary64");
// Version 1 (zero input) stays byte-identical. Version 2 adds explicit input
// topology immediately after the version field. No ABI structs/pointers/padding.
inline void renderSigned32(Bytes& b,std::int32_t v){b.u32(std::bit_cast<std::uint32_t>(v));}
inline void renderSigned64(Bytes& b,std::int64_t v){b.u64(std::bit_cast<std::uint64_t>(v));}
inline Bytes encodeRenderProtocol(const RenderProtocol& p){
    validateProtocol(p);Bytes b;b.raw("CS80RPRT",8);
    const bool topologyV2=p.topology!=AudioInputTopology::ZeroAudioInput;
    b.u32(topologyV2?2:1);if(topologyV2)b.u32(static_cast<std::uint32_t>(p.topology));
    b.f64(p.rate);renderSigned32(b,p.mode);renderSigned32(b,p.block);renderSigned64(b,p.frames);
    b.u32(p.legacyCCFixtureConvention?1:0);b.u64(p.events.size());
    for(const auto& row:p.events){const auto& e=row.value;
        renderSigned64(b,row.frame);renderSigned32(b,e.busIndex);renderSigned32(b,e.sampleOffset);b.f64(e.ppqPosition);b.u32(e.flags);b.u32(e.type);
        if(e.type==Event::kNoteOnEvent){renderSigned32(b,e.noteOn.channel);renderSigned32(b,e.noteOn.pitch);b.pcm(e.noteOn.tuning);b.pcm(e.noteOn.velocity);renderSigned32(b,e.noteOn.length);renderSigned32(b,e.noteOn.noteId);}
        else if(e.type==Event::kNoteOffEvent){renderSigned32(b,e.noteOff.channel);renderSigned32(b,e.noteOff.pitch);b.pcm(e.noteOff.tuning);b.pcm(e.noteOff.velocity);renderSigned32(b,e.noteOff.noteId);}
        else {b.u32(e.midiCCOut.controlNumber);renderSigned32(b,e.midiCCOut.channel);renderSigned32(b,e.midiCCOut.value);renderSigned32(b,e.midiCCOut.value2);}
    }
    b.u64(p.parameters.size());for(const auto& q:p.parameters){renderSigned64(b,q.frame);b.u32(q.id);b.f64(q.value);}
    return b;
}
inline void validateRenderObservation(const RenderProtocol& p,const RenderObservation& o){
    validateProtocol(p);
    const auto callbacks=static_cast<std::size_t>((p.frames+p.block-1)/p.block);
    require(o.callbacks.size()==callbacks,"complete callback count required");
    for(std::size_t i=0;i<callbacks;++i){const auto start=static_cast<std::int64_t>(i)*p.block;const auto& c=o.callbacks[i];
        require(c.start==start && c.count==std::min<std::int64_t>(p.block,p.frames-start) && c.returned==kResultOk && (c.silenceFlags&~uint64{3})==0,"invalid completed callback partition/result/mask");}
    require(o.interleaved.size()==static_cast<std::size_t>(p.frames)*2,"complete stereo PCM required");
    for(float v:o.interleaved)require(std::isfinite(v),"nonfinite encoded PCM");
    require(o.outputParameters.size()<=65536,"encoded output point total bound");
    std::int64_t block=-1,last=-1;ParamID id=kNoParamId;std::size_t count=0;std::set<ParamID> ids;
    for(const auto& q:o.outputParameters){
        require(q.frame>=0 && q.frame<p.frames && q.id!=kNoParamId && normalizedValue(q.value),"encoded output point frame/id/value");
        const auto next=q.frame/p.block;require(next>=block,"output callback order");
        if(next!=block){block=next;count=0;ids.clear();id=kNoParamId;last=-1;}
        require(++count<=4096,"encoded output per-block bound");
        if(q.id!=id){require(ids.insert(q.id).second,"noncontiguous output ID queue");id=q.id;last=-1;}
        require(q.frame>=last,"output queue point order");last=q.frame;
    }
}
inline Bytes encodeRenderObservation(const RenderProtocol& p,const RenderObservation& o){
    validateRenderObservation(p,o);const auto protocol=encodeRenderProtocol(p);
    Bytes b;b.raw("CS80ROBS",8);b.u32(1);b.blob(protocol.data.data(),protocol.data.size());
    b.u64(o.callbacks.size());for(const auto& c:o.callbacks){renderSigned64(b,c.start);renderSigned32(b,c.count);renderSigned32(b,c.returned);b.u64(c.silenceFlags);}
    b.u64(o.interleaved.size());for(float v:o.interleaved)b.pcm(v);
    b.u64(o.outputParameters.size());for(const auto& q:o.outputParameters){renderSigned64(b,q.frame);b.u32(q.id);b.f64(q.value);}
    return b;
}
} // namespace cs80_observer
