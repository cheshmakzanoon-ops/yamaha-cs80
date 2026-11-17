#pragma once
// Linux/glibc implementation only. No CLI, auto-load, or synth invocation.
#include "native_observer_initializer.hpp"
#include <dlfcn.h>
#include <link.h>
#include <elf.h>
#include <filesystem>

namespace cs80_observer {
class DlopenOps final : public LoaderOps {
    friend class SealedLoader;
    const bool deferOpenFailureCleanup_;
    void* live=nullptr;
    std::uintptr_t base=0;
    bool opened=false,resolved=false,closeAttempted=false;
    template<class F> F function(const char* name) {
        ::dlerror();void* symbol=::dlsym(live,name);const char* error=::dlerror();
        if(error || !symbol){lastError=std::string("required export ")+name+": "+(error?error:"null symbol");throw std::runtime_error(lastError);}
        Dl_info info{};void* extra=nullptr;
        require(::dladdr1(symbol,&info,&extra,RTLD_DL_SYMENT)!=0 && extra,
                "export symbol provenance unavailable");
        const auto* native=static_cast<const ElfW(Sym)*>(extra);
        require(ELF64_ST_TYPE(native->st_info)==STT_FUNC &&
                reinterpret_cast<std::uintptr_t>(info.dli_fbase)==base,
                "export must be a function owned by this exact module");
        // POSIX specifies dlsym function-pointer conversion; memcpy avoids a C++
        // object/function reinterpret_cast diagnostic without changing the ABI.
        static_assert(sizeof(F)==sizeof(symbol));F result{};std::memcpy(&result,&symbol,sizeof(result));return result;
    }
public:
    static_assert(sizeof(void*)==8,"this loader supports only 64-bit Linux ELF");
    std::string lastError;
    unsigned nativeOpenCalls=0,nativeCloseCalls=0;
    bool resolutionSucceeded=false;
    int closeStatus=0;
    explicit DlopenOps(bool deferOpenFailureCleanup=false):deferOpenFailureCleanup_(deferOpenFailureCleanup){}DlopenOps(const DlopenOps&)=delete;DlopenOps& operator=(const DlopenOps&)=delete;
    void* open(const char* exactPath) override {
        require(!opened && !closeAttempted,"loader open retry forbidden");opened=true;
        require(exactPath && *exactPath,"module path required");
        const std::filesystem::path path(exactPath);
        require(path.is_absolute() && path.lexically_normal()==path && std::filesystem::is_regular_file(path)
                && std::filesystem::canonical(path)==path,"canonical regular non-symlink module required");
        ++nativeOpenCalls;live=::dlopen(exactPath,RTLD_NOW|RTLD_LOCAL);
        if(!live){const char* error=::dlerror();lastError=error?error:"dlopen failed without diagnostic";return nullptr;}
        try {
            link_map* map=nullptr;
            require(::dlinfo(live,RTLD_DI_LINKMAP,&map)==0 && map,"module link-map unavailable");
            base=static_cast<std::uintptr_t>(map->l_addr);return live;
        }catch(...) {
            // No handle escaped open(), so the loader owns this failure cleanup.
            if(deferOpenFailureCleanup_)throw;
            auto* h=std::exchange(live,nullptr);closeAttempted=true;++nativeCloseCalls;closeStatus=::dlclose(h);throw;
        }
    }
    ModuleBindings resolve(void* handle) override {
        require(live && handle==live && !closeAttempted && !resolved,"single resolution of owned live handle required");resolved=true;
        ModuleBindings bindings;
        bindings.entry=function<bool(*)(void*)>("ModuleEntry");
        bindings.factory=function<IPluginFactory*(*)()>("GetPluginFactory");
        bindings.exit=function<bool(*)()>("ModuleExit");
        resolutionSucceeded=true;return bindings;
    }
    int close(void* handle) override {
        require(live && handle==live && !closeAttempted,"single close of owned handle required");
        closeAttempted=true;auto* h=std::exchange(live,nullptr);++nativeCloseCalls;closeStatus=::dlclose(h);return closeStatus;
    }
    // No implicit dlclose here: only ModuleSession knows whether objects/entry
    // are still live. That owner must explicitly close before this object dies.
};
} // namespace cs80_observer
