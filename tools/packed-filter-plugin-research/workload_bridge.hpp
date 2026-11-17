#pragma once
// Pure descriptor bridge; no SDK calls, loader, processing or phase orchestration.
#include "workload_manifest.hpp"
#include "native_observer_initializer.hpp"
#include "native_observer_flush.hpp"
#include "native_observer_render_encoding.hpp"
namespace cs80_observer {
struct WorkloadCasePlan {
    SynthSetup setup;
    FlushProtocol initial,alter;
    RenderProtocol render;
    // Existing renderObserved starts projectTimeSamples at zero for EACH call.
    // This is a segment-relative contract, not continuous transport/history.
    static constexpr std::int64_t segmentProjectTimeOrigin=0;
};
inline int32 workloadSdkMode(packed_filter_workload::Mode mode) {
    switch(mode) {
        case packed_filter_workload::Mode::realtime:return kRealtime;
        case packed_filter_workload::Mode::offline:return kOffline;
    }
    require(false,"unknown manifest mode");return kRealtime;
}
namespace workload_detail {
// Internal pure mapping seam for inert tests. This is not native provenance.
inline WorkloadCasePlan planFromRoster(packed_filter_workload::Case c,const ConnectedParameterRoster& roster) {
    namespace w=packed_filter_workload;
    w::validate(c);require(roster.declaredCount>0,"complete connected parameter count required");
    w::bind_native_ids(c,{static_cast<std::uint32_t>(roster.declaredCount),roster.ids});
    WorkloadCasePlan p;p.setup={double(c.rate),workloadSdkMode(c.mode),static_cast<int32>(c.block)};
    p.initial.mode=p.alter.mode=p.setup.mode;
    const auto generated=w::controls(c);
    for(const auto& q:generated) {p.initial.controls.push_back({0,q.id,q.value});p.initial.expectedIds.push_back(q.id);}
    const auto appendAlter=[&](int parameter,float physical) {
        const auto name=cs80::paramId(parameter);const auto id=w::parameter_id(name);
        // Alter is a frozen subset of the COMPLETE already-bound initial roster.
        require(std::count(roster.ids.begin(),roster.ids.end(),id)==1,"alter ID absent/ambiguous");
        p.alter.controls.push_back({0,id,double(w::normalized(parameter,physical))});p.alter.expectedIds.push_back(id);
    };
    appendAlter(cs80::P_volume,0.22f);
    appendAlter(cs80::chParam(0,cs80::CP_lpfCutoff),1200.0f);
    p.render.rate=p.setup.rate;p.render.mode=p.setup.mode;p.render.block=p.setup.block;p.render.frames=w::frames;
    p.render.legacyCCFixtureConvention=false;
    for(const auto& row:w::events(c)) {
        Event e{};e.busIndex=row.bus;e.type=row.on?Event::kNoteOnEvent:Event::kNoteOffEvent;
        if(row.on) {e.noteOn.channel=static_cast<int16>(row.channel);e.noteOn.pitch=static_cast<int16>(row.pitch);e.noteOn.velocity=row.velocity;e.noteOn.noteId=row.note_id;}
        else {e.noteOff.channel=static_cast<int16>(row.channel);e.noteOff.pitch=static_cast<int16>(row.pitch);e.noteOff.velocity=row.velocity;e.noteOff.noteId=row.note_id;}
        p.render.events.push_back({row.sample,e});
    }
    require(p.initial.controls.size()==cs80::P_numParams+2 && p.alter.controls.size()==2 && p.render.parameters.empty(),"complete frozen bridge rosters");
    validateFlush(p.initial);validateFlush(p.alter);validateProtocol(p.render);
    return p;
}
}
// Production integration must use this concrete owned-session entry point after
// one successful Connected metadata enumeration and BEFORE prepare/start.
inline WorkloadCasePlan makeWorkloadCasePlan(packed_filter_workload::Case c,const InjectedInstance& owner) {
    require(owner.phase()==InjectedInstance::Phase::Connected && !owner.primaryError
            && !owner.connectedMetadataReceipt().data.empty(),"successful owned Connected metadata required");
    return workload_detail::planFromRoster(c,owner.connectedParameterRoster());
}
// Reuse encodeFlushProtocol(initial/alter) and encodeRenderProtocol(render).
// No bridge-specific binary schema, native execution, E-state transfer or phase
// A/B/C/D/E driver is introduced here. Any bridge failure must be terminal in the
// eventual owning driver, before the next flush/render operation.
}
