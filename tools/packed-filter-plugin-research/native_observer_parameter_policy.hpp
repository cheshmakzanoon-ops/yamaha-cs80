#pragma once
#include "native_observer_sdk.hpp"
namespace cs80_observer {
enum class ParameterBindingPolicy : std::uint32_t {ExactNative=1,SourceAwarePanel=2};
inline void validateParameterBindingPolicy(ParameterBindingPolicy p){require(p==ParameterBindingPolicy::ExactNative || p==ParameterBindingPolicy::SourceAwarePanel,"parameter binding policy");}
inline const char* historySchema(ParameterBindingPolicy p){validateParameterBindingPolicy(p);return p==ParameterBindingPolicy::SourceAwarePanel?"cs80-instance-history-A-D-v2":"cs80-instance-history-A-D-v1";}
}
