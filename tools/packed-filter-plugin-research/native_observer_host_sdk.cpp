// Amalgamates exact pinned official SDK helpers without editing/vendor-copying
// their implementation. One TU owns interface IDs, avoiding duplicate globals.
#define INIT_CLASS_IID
#include "native_observer_initializer.hpp"
#include "native_observer_host.hpp"
#include "pluginterfaces/base/funknown.cpp"
#include "public.sdk/source/common/commonstringconvert.cpp"
#include "public.sdk/source/vst/utility/stringconvert.cpp"
#include "public.sdk/source/vst/hosting/pluginterfacesupport.cpp"
#include "public.sdk/source/vst/hosting/hostclasses.cpp"
