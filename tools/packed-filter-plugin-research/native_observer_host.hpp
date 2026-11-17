#pragma once
// Reuse pinned official SDK services; no local IMessage/IAttributeList rewrite.
#include "public.sdk/source/vst/hosting/hostclasses.h"
namespace cs80_observer {
class OfficialHostContext {
    Steinberg::Vst::HostApplication host;
public:
    struct DefectControlOnly {};
    static constexpr bool admissibleForObserver=false;
    OfficialHostContext()=delete;
    explicit OfficialHostContext(DefectControlOnly) {}
    OfficialHostContext(const OfficialHostContext&)=delete;
    OfficialHostContext& operator=(const OfficialHostContext&)=delete;
    Steinberg::Vst::IHostApplication& application() noexcept {return host;}
    // The official helper's addRef/release are non-owning. This C++ owner must
    // outlive module/instance/session references and all plugin callbacks.
};
}
