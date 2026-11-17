#include "native_observer_worker.hpp"
#include "native_observer_support.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <latch>
#include <new>
using namespace cs80_observer::worker;
using cs80_observer::require;
using namespace std::chrono_literals;
static std::atomic<long> failAfter{-1};
void* operator new(std::size_t n){auto f=failAfter.load();if(f>=0){if(!f)throw std::bad_alloc{};--failAfter;}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc{};}
void operator delete(void* p) noexcept{std::free(p);}void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete[](void* p) noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
WaitPolicy policy(){return {Clock::now()+2s,10000,1ms};}
template<class F> bool fails(F f){try{f();}catch(...){return true;}return false;}
void run(const std::string& name){
    if(name=="startup-oom"){failAfter=0;bool rejected=false;try{ProcessingWorker worker;}catch(const std::bad_alloc&){rejected=true;}failAfter=-1;require(rejected,"thread creation allocation failure");return;}
    if(name=="hang" || name=="destructor-hang"){
        ProcessingWorker worker;std::latch entered{1};auto seq=worker.submit(Kind::Render,[&]{entered.count_down();for(;;)std::this_thread::yield();});
        require(fails([&]{worker.await(seq,{Clock::now()+20ms,100,1ms},[]{});}),"wait timeout");entered.wait();
        if(name=="hang")worker.close();
        return; // destructor also must block, never detach
    }
    if(name=="destructor-joins"){
        std::atomic<bool> done=false;{ProcessingWorker worker;auto seq=worker.submit(Kind::Render,[&]{std::this_thread::sleep_for(30ms);done=true;});require(fails([&]{worker.await(seq,{Clock::now()+2ms,100,1ms},[]{});}),"short timeout");}
        require(done,"destructor waited for task not detached");return;
    }
    ProcessingWorker worker;const auto ui=std::this_thread::get_id();std::atomic<int> calls=0;
    if(name=="basic"){
        std::thread::id observed;
        for(auto kind:{Kind::Render,Kind::Flush}){auto seq=worker.submit(kind,[&]{++calls;observed=std::this_thread::get_id();});require(calls==int(seq-1),"submit queues only");auto receipt=worker.await(seq,policy(),[]{});require(receipt.returned() && receipt.kind()==kind && receipt.sequence()==seq && receipt.thread()==observed && observed!=ui,"authoritative task receipt");}
        auto join=worker.close();require(worker.authorizesCleanup(join) && worker.authorizesAdmission(join) && join.submitted()==2 && join.consumed()==2 && join.attempted()==2,"healthy joined");return;
    }
    if(name=="cross-receipt"){
        auto join=worker.close();ProcessingWorker other;auto otherJoin=other.close();require(!worker.authorizesCleanup(otherJoin) && !other.authorizesAdmission(join) && worker.authorizesAdmission(join),"instance identity receipt");return;
    }
    if(name=="close-before-await"){
        worker.submit(Kind::Render,[&]{++calls;});auto join=worker.close();require(!calls && join.canceled()==1 && !join.attempted() && worker.authorizesCleanup(join) && !worker.authorizesAdmission(join),"canceled unattempted cleanup only");return;
    }
    if(name=="submit-oom"){
        std::array<char,4096> capture{};auto callable=[capture,&calls]{calls+=capture[0];};failAfter=0;const bool rejected=fails([&]{worker.submit(Kind::Render,callable);});failAfter=-1;
        require(rejected && worker.failure()==Failure::Allocation && !calls,"task construction OOM terminal");auto join=worker.close();require(worker.authorizesCleanup(join) && !worker.authorizesAdmission(join) && !join.submitted(),"OOM join");return;
    }
    if(name=="copy-throw"){
        struct Copy {Copy()=default;Copy(const Copy&){throw std::runtime_error("copy error");}void operator()()const{}} callable;
        require(fails([&]{worker.submit(Kind::Render,callable);}),"copy throws");auto join=worker.close();require(!join.submitted() && join.failure()==Failure::Construction && worker.exception(),"construction error distinct from OOM");return;
    }
    if(name=="invalid-kind"){
        require(fails([&]{worker.submit(static_cast<Kind>(99),[&]{++calls;});}) && !calls,"invalid kind rejected");auto join=worker.close();require(!join.submitted() && !worker.authorizesAdmission(join),"no invalid publication");return;
    }
    if(name=="task-destructor-lock" || name=="cancel-destructor-lock"){
        std::atomic<int> destructions=0;
        struct Callable {ProcessingWorker* owner;std::atomic<int>* destructions;std::atomic<int>* calls;
            void operator()()const{++*calls;}
            ~Callable(){(void)owner->failure();++*destructions;}
        } callable{&worker,&destructions,&calls};
        auto sequence=worker.submit(Kind::Render,callable);
        if(name=="task-destructor-lock")worker.await(sequence,policy(),[]{});
        auto join=worker.close();require(destructions>=1,"callable destroyed outside lock");
        require(worker.authorizesCleanup(join) && worker.authorizesAdmission(join)==(name=="task-destructor-lock"),"destructor path receipt");return;
    }
    if(name=="wrong-submit"){
        std::thread foreign([&]{require(fails([&]{worker.submit(Kind::Render,[&]{++calls;});}),"wrong submit");});foreign.join();auto join=worker.close();require(!calls && join.failure()==Failure::WrongThread && worker.authorizesCleanup(join),"wrong thread untouched");return;
    }
    if(name=="after-close"){
        auto join=worker.close();require(fails([&]{worker.submit(Kind::Render,[]{});}) && !worker.authorizesAdmission(join) && worker.authorizesCleanup(join),"postclose invalidates admission not join");return;
    }
    if(name=="nested-construct"){
        struct Copy {ProcessingWorker* worker;Copy(ProcessingWorker* p):worker(p){}Copy(const Copy& x):worker(x.worker){require(fails([&]{worker->close();}),"nested construct close");}void operator()()const{}} callable{&worker};
        require(fails([&]{worker.submit(Kind::Render,callable);}),"copy reentry aborts publish");auto join=worker.close();require(!join.submitted() && join.failure()==Failure::Reentrant,"no outer callable publication");return;
    }
    std::latch entered{1},allow{1};
    auto seq=worker.submit(Kind::Render,[&]{
        ++calls;entered.count_down();
        if(name=="callback-throw")throw std::runtime_error("callback exception retained");
        if(name=="worker-submit")require(fails([&]{worker.submit(Kind::Flush,[]{});}),"worker submit rejected");
        if(name=="worker-close")require(fails([&]{worker.close();}),"worker close rejected");
        if(name=="pump-throw" || name=="pump-close" || name=="pump-submit" || name=="pump-await" || name=="pump-budget" || name=="on-time-observed-late")allow.wait();
        if(name=="timeout-late" || name=="late-observed")std::this_thread::sleep_for(30ms);
    });
    if(name=="double-submit")require(fails([&]{worker.submit(Kind::Flush,[]{});}) && !calls,"one queued slot");
    else if(name=="wrong-sequence")require(fails([&]{worker.await(seq+1,policy(),[]{});}) && !calls,"wrong sequence cannot dispatch");
    else if(name=="expired")require(fails([&]{worker.await(seq,{Clock::now()-1ms,10,1ms},[]{});}) && !calls,"expired cannot dispatch");
    else if(name=="invalid-budget")require(fails([&]{worker.await(seq,{Clock::now()+1s,0,1ms},[]{});}) && !calls,"budget cannot dispatch");
    else if(name=="wrong-await") {std::thread foreign([&]{require(fails([&]{worker.await(seq,policy(),[]{});}),"wrong await");});foreign.join();require(!calls,"wrong await cannot dispatch");}
    else if(name=="callback-throw" || name=="worker-submit" || name=="worker-close")require(fails([&]{worker.await(seq,policy(),[]{});}),"callback terminal");
    else if(name=="timeout-late"){
        require(fails([&]{worker.await(seq,{Clock::now()+5ms,100,1ms},[]{});}),"deadline failure");
        require(fails([&]{worker.submit(Kind::Flush,[]{});}),"no retry after timeout");
    }else if(name=="late-observed"){
        require(fails([&]{worker.await(seq,{Clock::now()+5ms,100,1ms},[&]{entered.wait();std::this_thread::sleep_for(50ms);});}),"late completed result must fail");
    }else if(name=="on-time-observed-late"){
        auto receipt=worker.await(seq,{Clock::now()+200ms,100,1ms},[&]{entered.wait();allow.count_down();std::this_thread::sleep_for(250ms);});require(receipt.returned(),"completion timestamp not UI observation");
    }else if(name=="pump-throw" || name=="pump-close" || name=="pump-submit" || name=="pump-await" || name=="pump-budget"){
        bool first=true;std::uint32_t pumps=0;
        const bool rejected=fails([&]{worker.await(seq,{Clock::now()+1s,name=="pump-budget"?1u:100u,1ms},[&]{
            ++pumps;entered.wait();require(std::this_thread::get_id()==ui,"pump UI thread");
            if(first){first=false;
                if(name=="pump-throw")throw std::runtime_error("pump exception retained");
                if(name=="pump-close")worker.close();
                if(name=="pump-submit")worker.submit(Kind::Flush,[]{});
                if(name=="pump-await")worker.await(seq,policy(),[]{});
            }
        });});
        allow.count_down();require(rejected && pumps>=1,"pump failure terminal");
    }else if(name=="repeat-await"){
        worker.await(seq,policy(),[]{});require(fails([&]{worker.await(seq,policy(),[]{});}),"cannot consume twice");
    }else require(false,"unknown case");
    auto join=worker.close();require(worker.authorizesCleanup(join),"actual join authorizes cleanup");
    if(name=="on-time-observed-late")require(worker.authorizesAdmission(join),"healthy timestamp receipt");
    else require(!worker.authorizesAdmission(join),"failure no admission");
    if(name=="timeout-late" || name=="late-observed")require(join.completed()==1 && join.consumed()==0 && join.failure()==Failure::Deadline,"late success cannot erase timeout");
    if(name=="callback-throw")require(worker.lastTask()->exception() && worker.exception() && join.completed()==1 && !join.consumed(),"exception retained");
}
int main(int argc,char** argv){try{require(argc==2,"case");run(argv[1]);std::cout<<"worker_control_passed=1\n";return 0;}catch(const std::exception& e){failAfter=-1;std::cerr<<e.what()<<'\n';return 1;}}
