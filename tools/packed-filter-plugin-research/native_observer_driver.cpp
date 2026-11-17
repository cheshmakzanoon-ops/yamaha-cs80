// Dedicated supervised process only. No fixture includes, interposition or test hooks.
#define INIT_CLASS_IID
#include "native_observer_request.hpp"
#include <cstdio>
int main(int argc,char** argv){
 try{
  cs80_observer::require(argc==5,"usage: observer request request-sha256 nonce receipt");
  auto q=cs80_observer::request::read(argv[1],argv[2]);
  auto identity=cs80_identity::VerifiedIdentity::verify(q.identity,q.caps);
  auto pin=cs80_identity::PinnedModule::pin(identity);
  cs80_observer::ProcessLifetimeChild::run(pin,q.cid.data(),q.workload,q.state,
   {cs80_observer::worker::Clock::now()+std::chrono::milliseconds(q.timeoutMs),1000000,std::chrono::milliseconds(1)},argv[3],argv[4],q.query,true,q.topology);
 }catch(const std::exception& e){std::fprintf(stderr,"CS80_REQUEST_REJECTED %.256s\n",e.what());return 2;}
 catch(...){std::fputs("CS80_REQUEST_REJECTED non-standard exception\n",stderr);return 2;}
}
