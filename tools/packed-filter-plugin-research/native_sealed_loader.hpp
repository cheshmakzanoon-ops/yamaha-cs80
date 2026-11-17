#pragma once
#include "native_identity_pin.hpp"
#include "native_observer_dlopen.hpp"
#include "native_provider.hpp"
#include <fstream>
namespace cs80_observer {
namespace sealed_detail {
inline std::string mapsText(){
    cs80_identity::detail::Fd fd(::open("/proc/self/maps",O_RDONLY|O_CLOEXEC));std::string out;std::array<char,8192> buf{};
    for(;;){const auto n=::read(fd.n,buf.data(),buf.size());if(n<0 && errno==EINTR)continue;require(n>=0,"maps read error");if(n==0)break;
        require(out.size()+static_cast<std::size_t>(n)<=4*1024*1024,"maps snapshot bound");out.append(buf.data(),static_cast<std::size_t>(n));}
    return out;
}
inline void representable(const std::string& path){require(!path.empty() && path[0]=='/' && std::none_of(path.begin(),path.end(),[](unsigned char c){return c<32 || c==127 || c=='\\';}),"unrepresentable provider path");}
inline bool pathMatches(std::string path,const std::string& target){
    if(path.ends_with(" (deleted)"))path.resize(path.size()-10);
    if(path==target)return true;
    if(path.empty() || path[0]!='/' )return false;
    representable(path);std::error_code ec;auto canonical=std::filesystem::canonical(path,ec);return !ec && canonical.string()==target;
}
inline void rejectLoadedPath(const std::string& target,const std::string& maps,std::vector<std::string>& retained){
    // Leading maps fields already parsed; preserve path including spaces.
    std::size_t at=0;while(at<maps.size()){const auto end=maps.find('\n',at);auto line=std::string_view(maps).substr(at,end-at);std::size_t p=0;
        for(int i=0;i<5;++i){while(p<line.size() && line[p]==' ')++p;while(p<line.size() && line[p]!=' ')++p;}
        while(p<line.size() && line[p]==' ')++p;
        require(!pathMatches(std::string(line.substr(p)),target),"target pathname already mapped");at=end+1;
    }
    struct Inventory {std::vector<std::string> paths;std::exception_ptr failure;};Inventory inventory;
    const auto visitor=[](dl_phdr_info* info,std::size_t,void* data)->int {auto& state=*static_cast<Inventory*>(data);try{
        require(state.paths.size()<4096 && info->dlpi_name,"loaded object inventory bound");const auto n=::strnlen(info->dlpi_name,4097);require(n<=4096,"loaded object path bound");state.paths.emplace_back(info->dlpi_name,n);return 0;
    }catch(...){state.failure=std::current_exception();return 1;}};
    const int status=::dl_iterate_phdr(visitor,&inventory);if(inventory.failure)std::rethrow_exception(inventory.failure);require(status==0 && !inventory.paths.empty(),"loaded object inventory failed");
    retained=std::move(inventory.paths);
    for(const auto& name:retained){if(name.empty() || name.starts_with("linux-vdso.so."))continue;
        std::error_code ec;auto path=std::filesystem::canonical(name,ec);require(!ec,"unresolvable loaded object pathname");representable(path.string());require(name!=target && path.string()!=target,"target pathname already loaded");}
}
}
struct SealedProviderEvidence {
    std::string beforeMaps,loadedMaps,afterMaps;std::vector<std::string> beforeObjects,afterObjects;
    std::uintptr_t base=0;std::array<cs80_provider::U,3> exportAddresses{},exportOffsets{};
};
// Joined provider identity only. Does not execute ModuleEntry/Exit or establish
// factory/instance lifecycle, full runtime closure, HistoryOwner or ReloadToken.
class SealedLoader final:public LoaderOps {
    const std::shared_ptr<const cs80_identity::PinnedModule> pin_;
    DlopenOps loader_;void* handle_=nullptr;bool attempted_=false,resolved_=false,closed_=false,failed_=false,closeOk_=false;
    cs80_provider::Layout elf_;std::uintptr_t base_=0;std::array<cs80_provider::U,3> offsets_{};
    std::exception_ptr primary_,cleanup_;SealedProviderEvidence evidence_;
    cs80_provider::Stamp stamp()const{const auto& s=pin_->stamp();return {s.device,s.inode,s.size};}
    void recheck()const{pin_->recheckHeld();require(pin_->identity()->sameEvidence(*pin_->identity()->reverify()),"identity reverify differs");}
    void absent(std::string& text,std::vector<std::string>& objects)const{text=sealed_detail::mapsText();auto maps=cs80_provider::parseMaps(text);cs80_provider::rejectExistingInode(maps,stamp());sealed_detail::rejectLoadedPath(pin_->identity()->modulePath(),text,objects);}
    void closeOwned(){
        require(!closed_,"sealed close retry forbidden");closed_=true;auto* h=std::exchange(handle_,nullptr);
        try{require(h,"no owned sealed handle");const auto status=loader_.close(h);require(status==0,"native dlclose failed (not retried)");absent(evidence_.afterMaps,evidence_.afterObjects);recheck();closeOk_=true;}
        catch(...){failed_=true;cleanup_=std::current_exception();throw;}
    }
    [[noreturn]] void fail(){failed_=true;if(!primary_)primary_=std::current_exception();if(handle_ && !closed_){try{closeOwned();}catch(...){}}std::rethrow_exception(primary_);}
public:
    explicit SealedLoader(std::shared_ptr<const cs80_identity::PinnedModule> pin):pin_(std::move(pin)){require(bool(pin_),"verified pinned module required");}
    SealedLoader(const SealedLoader&)=delete;SealedLoader& operator=(const SealedLoader&)=delete;
    void* open(const char* exactPath)override{
        try{require(!attempted_ && !closed_ && !failed_,"sealed load retry forbidden");attempted_=true;
            require(exactPath && pin_->identity()->modulePath()==exactPath,"sealed exact module path");sealed_detail::representable(exactPath);recheck();
            require(pin_->stamp().size<=256*1024*1024,"sealed ELF byte bound");std::vector<std::uint8_t> bytes(static_cast<std::size_t>(pin_->stamp().size));std::size_t at=0;
            while(at<bytes.size()){const auto n=::pread(pin_->borrowedFd(),bytes.data()+at,bytes.size()-at,static_cast<off_t>(at));if(n<0 && errno==EINTR)continue;require(n>0,"held ELF read incomplete");at+=static_cast<std::size_t>(n);}
            elf_=cs80_provider::parseElf(bytes);pin_->recheckHeld();absent(evidence_.beforeMaps,evidence_.beforeObjects);handle_=loader_.open(exactPath);require(handle_,"sealed native open failed");
            link_map* map=nullptr;require(::dlinfo(handle_,RTLD_DI_LINKMAP,&map)==0 && map && map->l_name && std::string(map->l_name)==exactPath,"actual load link-map path differs");base_=static_cast<std::uintptr_t>(map->l_addr);evidence_.base=base_;recheck();return handle_;
        }catch(...){fail();}
    }
    ModuleBindings resolve(void* h)override{
        try{require(!failed_ && !closed_ && h && h==handle_ && !resolved_,"sealed resolution ownership/attempt");resolved_=true;
            auto result=loader_.resolve(h);std::array<cs80_provider::U,3> addresses{};const char* names[]{"ModuleEntry","GetPluginFactory","ModuleExit"};
            for(std::size_t i=0;i<3;++i){::dlerror();auto* symbol=::dlsym(h,names[i]);require(symbol && !::dlerror(),"actual export resolution");addresses[i]=reinterpret_cast<std::uintptr_t>(symbol);}
            evidence_.exportAddresses=addresses;evidence_.loadedMaps=sealed_detail::mapsText();
            offsets_=cs80_provider::checkExportOffsets(elf_,cs80_provider::parseMaps(evidence_.loadedMaps),stamp(),base_,addresses);evidence_.exportOffsets=offsets_;recheck();return result;
        }catch(...){fail();}
    }
    int close(void* h)override{try{require(!closed_ && h && h==handle_,"sealed close ownership");closeOwned();return 0;}catch(...){failed_=true;if(!primary_)primary_=std::current_exception();throw;}}
    bool providerClosedCleanly()const noexcept{return attempted_ && resolved_ && closed_ && closeOk_ && !failed_;}
    bool terminalFailure()const noexcept{return failed_;}
    unsigned nativeOpenCalls()const noexcept{return loader_.nativeOpenCalls;}
    unsigned nativeCloseCalls()const noexcept{return loader_.nativeCloseCalls;}
    const SealedProviderEvidence& providerEvidence()const noexcept{return evidence_;}
    const auto& verifiedIdentity()const noexcept{return pin_->identity();}
    const auto& exportFileOffsets()const noexcept{return offsets_;}
    const auto& primaryError()const noexcept{return primary_;}
    const auto& cleanupError()const noexcept{return cleanup_;}
    ~SealedLoader(){if(handle_ && !closed_){try{closeOwned();}catch(...){}}}
};
}
