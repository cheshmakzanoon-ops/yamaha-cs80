#define INIT_CLASS_IID
#include "native_observer_state.hpp"
#include <iostream>
using namespace cs80_observer;
static std::vector<std::string> calls;
struct Fake {
 std::string name; tresult get=kResultOk,set=kResultOk,sync=kResultOk;bool empty=false,bad=false,throws=false,mutate=false;
 tresult getState(IBStream* s){calls.push_back(name+".get");if(throws)throw std::runtime_error("get exception");char b[]{'A',0,'Z'};if(!empty)s->write(bad?nullptr:b,3,nullptr);return get;}
 tresult setState(IBStream* s){calls.push_back(name+".set");if(throws)throw std::runtime_error("set exception");char b[3]{};int32 n=0;s->read(b,3,&n);require(empty || n==3,"rewound state");if(mutate){s->seek(0,0,nullptr);char x='X';s->write(&x,1,nullptr);}if(bad)s->read(nullptr,1,nullptr);return set;}
 tresult setComponentState(IBStream* s){calls.push_back(name+".sync");char b[3]{};int32 n=0;s->read(b,3,&n);require(n==3 && b[0]=='A',"independent rewind");return sync;}
};
int main(int argc,char** argv){try{
 require(argc==2,"case");std::string name=argv[1];Fake c{"component"},d{"controller"};ControllerStatePolicy policy=ControllerStatePolicy::JuceUnsupported;d.get=kNotImplemented;d.empty=true;StateObservation observed;
 bool negative=false;
 if(name=="supported" || name=="supported-empty" || name=="restore-controller-failure" || name=="restore-controller-throw" || name=="restore-controller-mutate"){policy=ControllerStatePolicy::Supported;d.get=kResultOk;d.empty=name=="supported-empty";}
 else if(name=="component-empty"){c.empty=true;negative=true;}
 else if(name=="component-failure"){c.get=kResultFalse;negative=true;}
 else if(name=="component-throw"){c.throws=true;negative=true;}
 else if(name=="component-stream"){c.bad=true;negative=true;}
 else if(name=="controller-other-failure"){d.get=kResultFalse;negative=true;}
 else if(name=="controller-ok-unexpected"){d.get=kResultOk;negative=true;}
 else if(name=="controller-bytes"){d.empty=false;negative=true;}
 else if(name=="controller-stream"){d.empty=false;d.bad=true;negative=true;}
 else if(name=="controller-throw"){d.throws=true;negative=true;}
 else if(name=="wrong-policy"){policy=static_cast<ControllerStatePolicy>(99);negative=true;}
 bool caught=false;try{observeStates(c,d,policy,observed);}catch(...){caught=true;}
 require(caught==negative && observed.attempted && observed.accepted==!negative,"capture admission");
 if(negative){require(!observed.accepted,"negative accepted");if(name=="component-throw")require(observed.component.exception && !observed.component.returned && !observed.controller.attempted,"exception receipt");return 0;}
 if(name=="retry"){caught=false;try{observeStates(c,d,policy,observed);}catch(...){caught=true;}require(caught,"capture retry");return 0;}
 auto encoded=encodeStates(observed);require(!encoded.data.empty(),"encoding");
 if(name=="encoding-tamper"){observed.controller.bytes.push_back(1);caught=false;try{encodeStates(observed);}catch(...){caught=true;}require(caught,"tampered encoding");return 0;}
 calls.clear();bool restoreNegative=false;
 if(name=="restore-component-failure"){c.set=kResultFalse;restoreNegative=true;}
 if(name=="restore-component-throw"){c.throws=true;restoreNegative=true;}
 if(name=="restore-mutate"){c.mutate=true;restoreNegative=true;}
 if(name=="restore-stream"){c.bad=true;restoreNegative=true;}
 if(name=="restore-sync-failure"){d.sync=kResultFalse;restoreNegative=true;}
 if(name=="restore-unadmitted"){observed.accepted=false;restoreNegative=true;}
 if(name=="restore-controller-failure"){d.set=kResultFalse;restoreNegative=true;}
 if(name=="restore-controller-throw"){d.throws=true;restoreNegative=true;}
 if(name=="restore-controller-mutate"){d.mutate=true;restoreNegative=true;}
 StateRestoreReceipt r;caught=false;try{restoreStates(c,d,observed,r);}catch(...){caught=true;}
 require(caught==restoreNegative && r.accepted==!restoreNegative && r.attempted,"restore admission");
 if(restoreNegative){if(name=="restore-component-failure")require(r.component.result==kResultFalse && !r.componentSync.attempted,"stopped after failure");return 0;}
 require(calls== (policy==ControllerStatePolicy::Supported?std::vector<std::string>{"component.set","controller.sync","controller.set"}:std::vector<std::string>{"component.set","controller.sync"}),"exact restore order/no unsupported call");
 if(name=="restore-retry"){caught=false;try{restoreStates(c,d,observed,r);}catch(...){caught=true;}require(caught,"restore retry");}
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
