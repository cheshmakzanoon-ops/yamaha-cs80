#pragma once
#include "native_identity.hpp"
// Preparation only. Retaining this descriptor does NOT prove dlopen used it.
namespace cs80_identity {
class PinnedModule final {
    const std::shared_ptr<const VerifiedIdentity> identity_;
    detail::Fd fd_;
    PinnedModule(std::shared_ptr<const VerifiedIdentity> identity,int fd):identity_(std::move(identity)),fd_(fd){}
    static void heldCheck(int fd,const ModuleStamp& expected){
        const int flags=::fcntl(fd,F_GETFL);check(flags>=0&&(flags&O_ACCMODE)==O_RDONLY,"held descriptor not read-only");
        auto before=detail::statFd(fd);auto actual=detail::entry("module",before);check(!actual.directory,"held module not regular");
        check(static_cast<std::uint64_t>(before.st_dev)==expected.device&&static_cast<std::uint64_t>(before.st_ino)==expected.inode&&actual.size==expected.size&&actual.mode==expected.mode&&actual.uid==expected.uid&&actual.gid==expected.gid,"held stamp mismatch");
        detail::Hash hash;std::array<char,65536> buffer{};std::uint64_t offset=0;
        check(expected.size<=static_cast<std::uint64_t>(std::numeric_limits<off_t>::max()),"held file offset unsupported");
        for(;;){auto n=::pread(fd,buffer.data(),buffer.size(),static_cast<off_t>(offset));if(n<0&&errno==EINTR)continue;check(n>=0,"held read failed");if(n==0)break;offset=detail::add(offset,static_cast<std::uint64_t>(n),expected.size);hash.update(buffer.data(),static_cast<std::size_t>(n));}
        check(offset==expected.size&&detail::stable(before,detail::statFd(fd)),"held module changed during read");actual.sha256=hash.finish();check(detail::sameStamp(detail::stamp(before,actual),expected),"held content mismatch");
    }
public:
    PinnedModule(const PinnedModule&)=delete;PinnedModule&operator=(const PinnedModule&)=delete;
    PinnedModule(PinnedModule&&)=delete;PinnedModule&operator=(PinnedModule&&)=delete;
    static std::shared_ptr<const PinnedModule> pin(const std::shared_ptr<const VerifiedIdentity>& identity){
        check(bool(identity),"missing verified identity");auto fresh=identity->reverify();
        detail::Fd parent(detail::openRoot(fresh->expected_.bundle.path));detail::rootCheck(parent.n,fresh->expected_.bundle);
        const auto& path=fresh->expected_.module;std::size_t p=0;
        for(;;){auto q=path.find('/',p);if(q==std::string::npos)break;auto part=path.substr(p,q-p);int next=::openat(parent.n,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);check(next>=0,"pin intermediate symlink/missing");::close(parent.n);parent.n=next;p=q+1;}
        detail::Fd held(::openat(parent.n,path.substr(p).c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));
        heldCheck(held.n,fresh->moduleStamp());fresh=fresh->reverify();heldCheck(held.n,fresh->moduleStamp());
        // Keep the guard owning the fd until object allocation/construction succeeds.
        auto* result=new PinnedModule(std::move(fresh),held.n);held.n=-1;
        return std::shared_ptr<const PinnedModule>(result);
    }
    const std::shared_ptr<const VerifiedIdentity>& identity()const{return identity_;}
    const ModuleStamp& stamp()const{return identity_->moduleStamp();}
    // Borrow only. Do not close/dup2/reassign; no lifetime extension beyond this object.
    int borrowedFd()const{return fd_.n;}
    void recheckHeld()const{heldCheck(fd_.n,stamp());}
};
} // cs80_identity
