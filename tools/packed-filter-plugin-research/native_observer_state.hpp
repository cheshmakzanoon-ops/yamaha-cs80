#pragma once
#include "native_observer_sdk.hpp"
namespace cs80_observer {
// Policy is frozen from the exact wrapper implementation, never inferred from a
// failing call. Unsupported is an observed API result, not successful state data.
enum class ControllerStatePolicy : std::uint32_t { Supported=1, JuceUnsupported=2 };
struct StateCallReceipt {
    bool attempted=false,returned=false,streamFailed=false;
    tresult result=kInternalError;
    std::exception_ptr exception;
    std::vector<std::uint8_t> bytes;
};
struct StateObservation {
    bool attempted=false,accepted=false;
    ControllerStatePolicy policy=ControllerStatePolicy::Supported;
    StateCallReceipt component,controller;
};
inline void validateStatePolicy(ControllerStatePolicy p) {
    require(p==ControllerStatePolicy::Supported || p==ControllerStatePolicy::JuceUnsupported,"unknown controller state policy");
}
template<class Function> void observeStateCall(StateCallReceipt& r,Function&& function) {
    require(!r.attempted,"state call retry forbidden");r.attempted=true;Stream stream;
    try {r.result=function(&stream);r.returned=true;}catch(...){r.exception=std::current_exception();}
    r.streamFailed=stream.failed;r.bytes=std::move(stream.state.bytes);
}
inline void validateStateObservation(const StateObservation& r) {
    validateStatePolicy(r.policy);require(r.attempted,"missing state observation");
    const auto valid=[](const StateCallReceipt& c){return c.attempted && c.returned && !c.exception && !c.streamFailed && c.bytes.size()<=StateBuffer::limit;};
    require(valid(r.component) && r.component.result==kResultOk && !r.component.bytes.empty(),"component state required");
    require(valid(r.controller),"controller state observation failed");
    if(r.policy==ControllerStatePolicy::Supported)require(r.controller.result==kResultOk,"supported controller state required");
    else require(r.controller.result==kNotImplemented && r.controller.bytes.empty(),"exact unsupported empty controller state required");
}
// Same UI thread and initialized/connected lifecycle must be independently
// established. Receipts survive API failure/exception, but allocation failure or
// process crash still requires the external same-attempt ledger.
template<class Component,class Controller>
void observeStates(Component& component,Controller& controller,ControllerStatePolicy policy,StateObservation& out) {
    require(!out.attempted,"state observation retry forbidden");out.attempted=true;out.policy=policy;validateStatePolicy(policy);
    observeStateCall(out.component,[&](IBStream* s){return component.getState(s);});
    require(out.component.returned && !out.component.exception && !out.component.streamFailed && out.component.result==kResultOk && !out.component.bytes.empty(),"component state required");
    observeStateCall(out.controller,[&](IBStream* s){return controller.getState(s);});
    validateStateObservation(out);out.accepted=true;
}
inline Bytes encodeStates(const StateObservation& r) {
    validateStateObservation(r);require(r.accepted,"state observation not admitted");Bytes b;b.text("cs80-native-state-v2");b.u32(static_cast<std::uint32_t>(r.policy));
    for(const auto* c:{&r.component,&r.controller}){b.u32(static_cast<std::uint32_t>(c->result));b.blob(c->bytes.data(),c->bytes.size());}return b;
}
struct StateRestoreReceipt {
    bool attempted=false,accepted=false;
    StateCallReceipt component,componentSync,controller;
};
template<class Function> void restoreStateCall(StateCallReceipt& r,const std::vector<std::uint8_t>& bytes,Function&& function) {
    require(!r.attempted,"restore call retry forbidden");r.attempted=true;Stream stream;stream.state.bytes=bytes;
    try {r.result=function(&stream);r.returned=true;}catch(...){r.exception=std::current_exception();}
    r.streamFailed=stream.failed;r.bytes=std::move(stream.state.bytes);
    require(r.returned && !r.exception && !r.streamFailed && r.result==kResultOk && r.bytes==bytes,"state restore API failed or mutated input");
}
// Caller establishes stopped/deactivated state on the correct UI thread. This
// restores controls, NOT oscillator/RNG/delay-buffer history. Never equate PCM
// before/after restoration; compare matched complete histories across arms.
template<class Component,class Controller>
void restoreStates(Component& component,Controller& controller,const StateObservation& saved,StateRestoreReceipt& out) {
    require(!out.attempted,"state restore retry forbidden");out.attempted=true;validateStateObservation(saved);require(saved.accepted,"unadmitted saved state");
    restoreStateCall(out.component,saved.component.bytes,[&](IBStream* s){return component.setState(s);});
    restoreStateCall(out.componentSync,saved.component.bytes,[&](IBStream* s){return controller.setComponentState(s);});
    if(saved.policy==ControllerStatePolicy::Supported)restoreStateCall(out.controller,saved.controller.bytes,[&](IBStream* s){return controller.setState(s);});
    out.accepted=true;
}
}
