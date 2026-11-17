#define main initializer_fixture_unused_main
#include "native_observer_initializer_fixture.cpp"
#undef main
#include "workload_bridge.hpp"
#include <iostream>
#include <type_traits>
using namespace cs80_observer;
namespace w=packed_filter_workload;
void check(bool value){if(!value)throw std::runtime_error("bridge fixture assertion");}
template<class F>void rejects(F f){bool yes=false;try{f();}catch(const std::exception&){yes=true;}check(yes);}
ConnectedParameterRoster roster(w::Case c){ConnectedParameterRoster r;for(const auto& p:w::controls(c))r.ids.push_back(p.id);r.declaredCount=static_cast<int32>(r.ids.size());return r;}
struct NeverLoader:LoaderOps {
    int calls=0;
    void* open(const char*)override{++calls;throw std::runtime_error("no module allowed");}
    ModuleBindings resolve(void*)override{++calls;throw std::runtime_error("no module allowed");}
    int close(void*)override{++calls;throw std::runtime_error("no module allowed");}
};
int main(){
    check(kOffline==2&&kPrefetch==1);check(workloadSdkMode(w::Mode::offline)==kOffline);
    check(workloadSdkMode(w::Mode::realtime)==kRealtime);rejects([]{workloadSdkMode(w::Mode(2));});
    const auto cases=w::cases();check(cases.size()==108);
    std::set<std::vector<std::uint8_t>> protocols;
    for(auto c:cases){
        const auto r=roster(c);const auto p=workload_detail::planFromRoster(c,r);
        check(p.setup.rate==c.rate&&p.setup.block==int32(c.block));
        check(p.setup.mode==(c.mode==w::Mode::offline?kOffline:kRealtime));
        check(p.render.rate==p.setup.rate&&p.render.mode==p.setup.mode&&p.render.block==p.setup.block&&p.render.frames==4096);
        check(p.initial.mode==p.setup.mode&&p.alter.mode==p.setup.mode&&p.render.parameters.empty());
        const auto controls=w::controls(c);check(p.initial.controls.size()==controls.size());
        for(std::size_t i=0;i<controls.size();++i){const auto& q=p.initial.controls[i];check(q.frame==0&&q.id==controls[i].id&&q.value==controls[i].value&&p.initial.expectedIds[i]==q.id);}
        check(p.alter.controls.size()==2&&p.alter.expectedIds==std::vector<ParamID>({w::parameter_id("volume"),w::parameter_id("ch1_lpf_cutoff")}));
        check(p.alter.controls[0].value==double(0.22f));check(p.alter.controls[1].value==double(w::normalized(cs80::chParam(0,cs80::CP_lpfCutoff),1200.0f)));
        const auto ev=w::events(c);check(ev.size()==p.render.events.size());
        for(std::size_t i=0;i<ev.size();++i){const auto& e=p.render.events[i];const auto& q=ev[i];
            check(e.frame==q.sample&&e.value.busIndex==0&&e.value.sampleOffset==0&&e.value.ppqPosition==0&&e.value.flags==0);
            if(q.on)check(e.value.type==Event::kNoteOnEvent&&e.value.noteOn.channel==0&&e.value.noteOn.pitch==q.pitch&&e.value.noteOn.noteId==q.note_id&&e.value.noteOn.velocity==.75f&&e.value.noteOn.tuning==0&&e.value.noteOn.length==0);
            else check(e.value.type==Event::kNoteOffEvent&&e.value.noteOff.channel==0&&e.value.noteOff.pitch==q.pitch&&e.value.noteOff.noteId==q.note_id&&e.value.noteOff.velocity==0&&e.value.noteOff.tuning==0);
        }
        check(WorkloadCasePlan::segmentProjectTimeOrigin==0);
        auto encoded=encodeRenderProtocol(p.render).data;check(encoded==encodeRenderProtocol(p.render).data);
        auto initial=encodeFlushProtocol(p.initial).data;auto alter=encodeFlushProtocol(p.alter).data;check(initial!=alter);
        // Render bytes alone deliberately omit policy: policy belongs to initial.
        initial.insert(initial.end(),encoded.begin(),encoded.end());check(protocols.insert(initial).second);
        auto missing=r;missing.ids.pop_back();--missing.declaredCount;rejects([&]{workload_detail::planFromRoster(c,missing);});
        auto duplicate=r;duplicate.ids.insert(duplicate.ids.end(),{0xfffffff0u,0xfffffff0u});duplicate.declaredCount+=2;rejects([&]{workload_detail::planFromRoster(c,duplicate);});
    }
    {NeverLoader loader;InjectedModule module(loader);Handler handler;InjectedInstance owner(module,handler);
     rejects([&]{makeWorkloadCasePlan(cases.front(),owner);});check(loader.calls==0);}
    {trace.clear();failure.clear();InitFake component("component",0),controller("controller",0);InitFactory f(component,controller);factory=&f;
     Host host;FakeLoader loader;TUID cid{};std::memset(cid,5,16);InjectedModule module(loader);module.load("/sealed/fake.so",cid,host);
     Handler handler;InjectedInstance owner(module,handler);owner.initializeConnected();
     check(owner.phase()==InjectedInstance::Phase::Connected);rejects([&]{makeWorkloadCasePlan(cases.front(),owner);});
     owner.captureConnected();check(!owner.connectedMetadataReceipt().data.empty());
     // The actual fake enumeration contains only ID31, not the full workload.
     rejects([&]{makeWorkloadCasePlan(cases.front(),owner);});owner.close();module.close();
     check(loader.opens==1&&loader.closes==1);}
    std::cout<<"PASS: 108 metadata-bound descriptor mappings; no module/processing/phase driver\n";
}
