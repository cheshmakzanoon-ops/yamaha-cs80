#pragma once
#include "native_observer_module_history.hpp"
#include <cstdio>
namespace cs80_observer {
// Only public ProcessLifetime execution path. Dedicated supervised child only.
// No return (including exceptions), no native-owner destruction after execution.
class ProcessLifetimeChild final {
    static constexpr std::size_t payloadCap=64u*1024u*1024u,receiptCap=96u*1024u*1024u;
    static void outputIdentity(int fd){struct stat s{};require(::fstat(fd,&s)==0 && S_ISREG(s.st_mode) && s.st_uid==::geteuid() && s.st_nlink==1 && s.st_size==0 && (s.st_mode&0777)==0600 && ::lseek(fd,0,SEEK_CUR)==0,"exclusive empty owned receipt file");}
    static std::string digest(const auto& b){cs80_identity::detail::Hash hash;hash.update(b.data(),b.size());return hash.finish();}
public:
    ProcessLifetimeChild()=delete;
    [[noreturn]] static void run(std::shared_ptr<const cs80_identity::PinnedModule> pin,const TUID cid,packed_filter_workload::Case c,ControllerStatePolicy policy,worker::WaitPolicy wait,const std::string& nonce,const std::string& output){
        int fd=-1;ModuleHistoryOwner* owner=nullptr;const char* step="request/output-setup";
        try{
            require(nonce.size()==64 && nonce.find_first_not_of("0123456789abcdef")==std::string::npos,"explicit supervisor 32-byte nonce");
            require(bool(pin),"verified module pin");cs80_identity::detail::absolute(output,{4096,4096,4096,4096});
            // Anchor every parent directory without symlinks, create final name exclusively.
            const auto slash=output.rfind('/');const auto parent=output.substr(0,slash);require(!parent.empty(),"receipt parent directory");
            cs80_identity::detail::Fd directory(cs80_identity::detail::openRoot(parent));
            fd=::openat(directory.n,output.substr(slash+1).c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);require(fd>=0,"exclusive receipt create failed");outputIdentity(fd);
            step="graph-create";owner=new ModuleHistoryOwner(pin,ModuleLifetimeProfile::ProcessLifetime);
            step="module-history";owner->run(cid,c,policy,wait);require(owner->admitted(),"process history not admitted");
            step="receipt-encode";auto& graph=*owner->graph_;const auto& sealed=graph.loader.evidence();const auto& history=owner->history();const auto& provider=sealed.providerEvidence();
            require(sealed.lifecycleClosedCleanly() && sealed.nativeCloseReturnedZero() && (sealed.disposition()==ProviderDisposition::Removed || sealed.disposition()==ProviderDisposition::Retained),"checked process-lifetime completion");
            require(history.canonicalPayload.data.size()<=payloadCap && !history.canonicalPayload.data.empty(),"bounded complete A-D payload");
            // Identity checks include all declared runtime/input files, not unproven runtime closure.
            pin->recheckHeld();require(pin->identity()->sameEvidence(*pin->identity()->reverify()),"post-teardown identity mismatch");
            // Account for every variable field before assembling the bounded receipt.
            std::size_t encodedBound=4096;const auto account=[&](std::size_t n){require(n<=receiptCap-encodedBound,"terminal receipt preallocation bound");encodedBound+=n;};
            account(history.canonicalPayload.data.size());account(history.caseIdentity.size());account(pin->identity()->arm().size());
            for(const auto* maps:{&provider.beforeMaps,&provider.loadedMaps,&provider.afterMaps})account(maps->size()+8);
            for(const auto* objects:{&provider.beforeObjects,&provider.afterObjects})for(const auto& object:*objects)account(object.size()+8);
            account(graph.baseline.size()*16);account(graph.after.size()*16);
            for(std::size_t i=0;i<graph.module.calls.count;++i){require(graph.module.calls.rows[i].name,"module call name");account(std::strlen(graph.module.calls.rows[i].name)+16);}
            Bytes receipt;receipt.data.reserve(encodedBound);receipt.text("cs80-process-lifetime-child-v1");receipt.text("process-lifetime");receipt.text(nonce);receipt.u64(static_cast<std::uint64_t>(::getpid()));
            receipt.text(pin->identity()->arm());receipt.text(pin->identity()->expectedManifestDigest());receipt.text(pin->identity()->evidenceDigest());receipt.text(pin->stamp().sha256);
            receipt.text("cs80-instance-history-A-D-v1");receipt.blob(history.caseIdentity.data(),history.caseIdentity.size());receipt.u32(1);receipt.text(sealed.disposition()==ProviderDisposition::Removed?"provider_removed":"provider_retained");
            receipt.text(digest(history.canonicalPayload.data));receipt.blob(history.canonicalPayload.data.data(),history.canonicalPayload.data.size());
            for(const auto* maps:{&provider.beforeMaps,&provider.loadedMaps,&provider.afterMaps})receipt.text(*maps);
            for(const auto* objects:{&provider.beforeObjects,&provider.afterObjects}){receipt.u64(objects->size());for(const auto& object:*objects)receipt.text(object);}
            receipt.u64(provider.base);for(auto address:provider.exportAddresses)receipt.u64(address);for(auto offset:provider.exportOffsets)receipt.u64(offset);
            for(const auto* threads:{&graph.baseline,&graph.after}){receipt.u64(threads->size());for(const auto& [tid,start]:*threads){receipt.u64(tid);receipt.u64(start);}}
            receipt.u64(graph.module.calls.count);for(std::size_t i=0;i<graph.module.calls.count;++i){const auto& call=graph.module.calls.rows[i];require(call.normal && call.ok && !call.error,"successful module ledger");receipt.text(call.name);receipt.u64(static_cast<std::uint64_t>(call.returned));}
            require(receipt.data.size()<=receiptCap,"terminal receipt size bound");
            const auto bodyHash=digest(receipt.data);Bytes framed;framed.u64(receipt.data.size());framed.raw(receipt.data.data(),receipt.data.size());framed.text(bodyHash);
            step="receipt-write";outputIdentity(fd);std::size_t at=0;while(at<framed.data.size()){const auto n=::write(fd,framed.data.data()+at,framed.data.size()-at);if(n<0 && errno==EINTR)continue;require(n>0,"terminal receipt write failed");at+=static_cast<std::size_t>(n);}
            step="receipt-fsync";require(::fsync(fd)==0,"terminal receipt flush failed");step="receipt-close";const int closing=std::exchange(fd,-1);require(::close(closing)==0,"terminal receipt close failed");
            // Even Removed keeps the whole graph intact. Only process termination follows.
            std::_Exit(0);
        }catch(...){
            // Never retry native cleanup or destroy the graph. Invalid/incomplete output
            // can exist, but a supervisor MUST reject every nonzero child exit.
            // Bounded best-effort non-admitting stderr diagnostics. Formatting performs
            // no graph cleanup, and diagnostics failure cannot change exit24.
            const char* primary="non-standard exception";
            try{throw;}catch(const std::exception& e){primary=e.what();}catch(...){}
            if(owner && owner->graph_)owner->quarantine();
            ModuleHistoryFailure failure;if(owner)failure=owner->failure_;
            unsigned badCalls=0;for(std::size_t i=0;i<failure.module.count;++i)if(!failure.module.rows[i].normal || !failure.module.rows[i].ok || failure.module.rows[i].error)++badCalls;
            char diagnostic[1024];const int count=std::snprintf(diagnostic,sizeof(diagnostic),"CS80_PROCESS_LIFETIME_FAILURE v1 stage=%.64s module_step=%.64s primary=%.256s opens=%u closes=%u provider_absent=%u host_refs=%u registrations=%zu failed_module_calls=%u cleanup_retried=0 exit=24\n",step,failure.attempted,primary?primary:"unknown",failure.opens,failure.closes,failure.providerAbsent?1u:0u,failure.hostReferences,failure.registrations,badCalls);
            if(count>0){const auto size=std::min(static_cast<std::size_t>(count),sizeof(diagnostic)-1);(void)::write(STDERR_FILENO,diagnostic,size);}
            if(fd>=0){const int closing=std::exchange(fd,-1);(void)::close(closing);}
            std::_Exit(24);
        }
    }
};
}
