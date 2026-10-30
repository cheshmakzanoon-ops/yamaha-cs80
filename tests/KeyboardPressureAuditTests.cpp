// Independent JUCE-free audit of actual publication tokens, synchronous capture,
// and conservative audio-owned pressure attribution. No Engine/DSP edits.
#include "plugin/KeyboardEventQueue.h"
#include "plugin/KeyboardGestureCapture.h"
#include "plugin/KeyboardPressureOwnership.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace {
std::atomic<bool> countAllocations{false};
std::atomic<unsigned> allocations{0};
void* allocate(std::size_t n){if(countAllocations.load())++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
int checks=0,failures=0;
void check(bool condition,const char* why){++checks;if(!condition){if(failures<30)std::fprintf(stderr,"FAIL: %s\n",why);++failures;}}
using Token=cs80::KeyPressureToken;
using Capture=cs80::KeyboardGestureCapture;
using Ownership=cs80::KeyboardPressureOwnership;
struct Voice{bool active=false,held=false,pedalHeld=false;int channel=1,note=-1;};
using Snapshot=std::array<Voice,8>;
template<std::size_t N>Token onset(cs80::KeyboardEventQueue<N>& q,int ch=1,int note=60,unsigned t=1){Token token;check(q.push(ch,note,.8f,true,t,&token),"onset publication accepted");check(static_cast<bool>(token),"accepted onset token nonempty");return token;}
void testPublicationAndBoundaries(){
    using Q=cs80::KeyboardEventQueue<8>;Q q,foreign;Q::Batch batch;auto a=onset(q);
    check(a.queue==&q&&a.epoch==q.requestedEpoch()&&a.channel==1&&a.note==60,"token belongs to actual queue/note/channel/epoch");
    check(q.pushPressure(a,0,2),"zero pressure is a valid event");
    check(q.push(1,60,0,false,3),"release publication");auto b=onset(q,1,60,4);
    check(a.epoch==b.epoch&&a.onset!=b.onset,"same-epoch restrike identity differs");
    check(q.pushPressure(a,.95f,5)&&q.pushPressure(b,.3f,6),"FIFO retains old/current identity for consumer validation");
    check(!foreign.pushPressure(a,.4f,7),"cross-queue token rejected");
    check(!q.pushPressure({},.4f,7),"empty token rejected");
    for(float value:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        check(!q.pushPressure(b,value,7),"nonfinite pressure rejected");
    auto bad=b;bad.channel=0;check(!q.pushPressure(bad,.2f,7),"invalid token channel rejected");bad=b;bad.note=128;check(!q.pushPressure(bad,.2f,7),"invalid token note rejected");
    check(q.take(batch)==6,"one bounded combined FIFO preserves all six events");
    check(batch[0].on&&!batch[0].pressure&&batch[0].token==a,"first onset exact identity");
    check(batch[1].pressure&&!batch[1].on&&batch[1].velocity==0&&batch[1].token==a,"zero pressure not confused with note-off");
    check(!batch[2].pressure&&!batch[2].on&&!batch[2].token,"ordinary release has no invented onset");
    check(batch[3].token==b&&batch[4].token==a&&batch[5].token==b,"same epoch late packet not rebound to latest note");
    int previous=-1;for(std::size_t i=0;i<6;++i){const int pos=Q::samplePosition(batch,6,i,64);check(pos>=previous&&pos>=0&&pos<64,"combined-event sample mapping ordered/bounded");previous=pos;}
    check(q.pushPressure(b,-2,7)&&q.pushPressure(b,2,8),"finite software range clamps");
    check(q.take(batch)==2&&batch[0].velocity==0&&batch[1].velocity==1,"pressure endpoint clamp exact");
    // A delayed old producer cannot relabel the token after reset or UI ack.
    q.requestReset();check(!q.pushPressure(b,.8f,9),"old packet during reset blocked");
    q.acknowledge(q.requestedEpoch());check(!q.pushPressure(b,.8f,10),"old packet after acknowledgment blocked");
    auto c=onset(q,1,60,11);check(c.epoch!=b.epoch&&c.onset>b.onset,"reset does not reuse onset generation");
    check(q.pushPressure(c,.6f,10),"fresh epoch pressure accepted; regressed producer timestamp normalized");
    check(q.take(batch)==2&&batch[0].token==c&&batch[1].token==c&&batch[1].milliseconds==11,"fresh token/timestamp preserved");
    Token invalid=a;check(!q.push(0,60,.8f,true,12,&invalid)&&!invalid,"rejected note clears output token");
    invalid=a;check(q.push(1,60,0,false,12,&invalid)&&!invalid,"note-off never captures earlier onset");q.take(batch);
    using Small=cs80::KeyboardEventQueue<2>;Small small;Small::Batch shortBatch;auto s=onset(small);
    check(small.pushPressure(s,.5f,2),"pressure fills shared bounded capacity");
    check(!small.push(1,60,0,false,3)&&small.hasOverflow(),"lost off after pressure flood latches overflow");
    check(small.take(shortBatch)==2&&!small.pushPressure(s,.2f,4),"drain does not silently reopen overflowed producer");
    small.requestReset();small.acknowledge(small.requestedEpoch());check(!small.pushPressure(s,.2f,5),"overflow recovery invalidates old pressure");
    auto fresh=onset(small,1,61,6);check(small.pushPressure(fresh,.2f,7)&&small.take(shortBatch)==2,"new gesture playable after overflow ack");
}
void testCapture(){
    static_assert(!std::is_copy_constructible_v<Capture>&&!std::is_move_constructible_v<Capture>);
    int processorA=0,processorB=0;cs80::KeyboardEventQueue<32> q;
    const auto a=onset(q,1,60),b=onset(q,1,60),c=onset(q,1,61);
    {Capture cap(&processorA,1,60);Capture::attempted(&processorB,1,60,b);Capture::attempted(&processorA,2,60,b);Capture::attempted(&processorA,1,61,c);
        check(!cap.result(),"foreign processor/channel/note cannot populate scope");Capture::attempted(&processorA,1,60,a);Capture::attempted(&processorA,1,60,b);
        check(cap.result()==a,"first actual matching publication wins");}
    {Capture cap(&processorA,1,60);Capture::attempted(&processorA,1,60,{});Capture::attempted(&processorA,1,60,b);
        check(!cap.result(),"first rejected attempt cannot be substituted by later accepted onset");}
    {Capture outer(&processorA,1,60);
        {Capture inner(&processorA,1,60);Capture::attempted(&processorA,1,60,a);check(inner.result()==a&&!outer.result(),"nested same-processor capture is top-only");}
        {Capture inner(&processorB,1,60);Capture::attempted(&processorA,1,60,b);check(!outer.result()&&!inner.result(),"foreign nested scope does not search outer stack");
            Capture::attempted(&processorB,1,60,b);check(inner.result()==b,"nested foreign processor owns its own result");}
        try{Capture inner(&processorB,1,61);throw std::runtime_error("unwind");}catch(const std::runtime_error&){}
        Capture::attempted(&processorA,1,60,b);check(outer.result()==b,"RAII restores outer capture after exception");}
    // This is deliberately a real second producer thread, not a current-token lookup.
    {Capture cap(&processorA,1,60);Token background;
        std::thread other([&]{q.push(1,60,.5f,true,7,&background);Capture::attempted(&processorA,1,60,background);});other.join();
        check(static_cast<bool>(background)&&!cap.result(),"same-pitch background producer cannot steal thread-local capture");
        const auto actual=onset(q,1,60,8);Capture::attempted(&processorA,1,60,actual);check(cap.result()==actual&&!(actual==background),"actual UI dispatch capture not most recent unrelated lookup");}
}
void testOwnership(){
    cs80::KeyboardEventQueue<32> q;const auto a=onset(q),b=onset(q),other=onset(q,2,60),nextNote=onset(q,1,67);
    Ownership owner;Snapshot before{},v{};v[0]={true,true,false,1,60};owner.allocated(before,v,a,1,60);
    check(owner.allows(a,v),"allocated UI onset owns pressure");check(!owner.allows(b,v)&&!owner.allows(other,v),"same note foreign onset/channel not owner");
    owner.noteOff(1,60);check(!owner.allows(a,v),"UI note-off invalidates current gesture");
    v[0].held=false;before=v;v[1]={true,true,false,1,60};owner.allocated(before,v,b,1,60);
    check(!owner.allows(a,v)&&owner.allows(b,v),"fresh restrike owns pressure while prior UI release tail remains attributable");
    // Existing DSP groups pressure by pitch. Host held/pedal/release aliases are
    // therefore all excluded conservatively, including another MIDI channel.
    for(int ch:{1,2,16})for(int state=0;state<3;++state){auto aliased=v;aliased[2]={true,state==0,state==1,ch,60};
        check(!owner.allows(b,aliased),"unattributed host alias blocks pressure in every held/release/channel case");}
    owner.hostNote(60);check(!owner.allows(b,v),"host same-note event invalidates gesture owner across channels");
    owner.clear();before={};v={};v[0]={true,true,false,1,60};owner.allocated(before,v,a,1,60);
    // All eight held/pedal-held voices mean the ninth UI onset had no allocation.
    for(int i=1;i<8;++i)v[i]={true,i%2==0,i%2!=0,i+1,60+i};
    before=v;
    owner.allocated(before,v,b,1,60);check(!owner.allows(b,v)&&!owner.allows(a,v),"refused ninth onset creates no pressure ownership and supersedes prior gesture");
    // Reusing a previously UI-tagged slot for a HOST onset must erase attribution.
    owner.clear();before={};v={};v[0]={true,true,false,1,60};owner.allocated(before,v,a,1,60);
    v[0].held=false;before=v;v[0].held=true;owner.allocated(before,v,{},1,60);
    before=v;v[1]={true,true,false,1,60};owner.allocated(before,v,b,1,60);
    check(!owner.allows(b,v),"host reuse of same slot/pitch cannot retain old UI origin");
    owner.clear();before={};v={};v[0]={true,true,false,1,60};owner.allocated(before,v,a,1,60);
    v[0].note=67;check(!owner.allows(a,v),"bound slot changed pitch is not the original held onset");
    v[0].note=60;v[0].channel=2;check(!owner.allows(a,v),"bound slot changed channel cannot retain pressure claim");
    v[0].channel=1;v[0].held=false;v[0].pedalHeld=true;check(!owner.allows(a,v),"pedal-held released gesture is not a held origin");
    v[0].held=true;v[0].pedalHeld=false;owner.clear();check(!owner.allows(a,v),"reset/arp attribution clear blocks still-active stale origin");
    before=v;v[1]={true,true,false,1,67};owner.allocated(before,v,nextNote,1,67);check(owner.allows(nextNote,v),"unrelated pitch does not block fresh UI gesture");
    owner.clear();before={};v={};v[0]={true,true,false,1,60};owner.allocated(before,v,a,1,60);
    owner.clear();before=v;v[1]={true,true,false,1,60};owner.allocated(before,v,b,1,60);
    check(!owner.allows(b,v),"clear revokes ALL origins, not only current gesture table");
}
void testConsumerAllocationBoundary(){
    cs80::KeyboardEventQueue<8> q;cs80::KeyboardEventQueue<8>::Batch batch;const auto a=onset(q);q.pushPressure(a,.4f,2);
    Ownership owner;Snapshot before{},after{};after[0]={true,true,false,1,60};
    allocations=0;countAllocations=true;
    const auto n=q.take(batch);owner.allocated(before,after,a,1,60);const bool allowed=owner.allows(a,after);owner.noteOff(1,60);owner.hostNote(60);owner.clear();q.requestReset();
    countAllocations=false;
    check(n==2&&allowed,"consumer fixture exercised publication/attribution paths");check(allocations.load()==0,"audited consumer/drain/reset/helper paths make no ordinary C++ allocation");
}
} // namespace
void* operator new(std::size_t n){return allocate(n);}void* operator new[](std::size_t n){return allocate(n);}
void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
int main(){testPublicationAndBoundaries();testCapture();testOwnership();testConsumerAllocationBoundary();
    std::printf("Keyboard pressure independent audit: %d checks, %d failures\n",checks,failures);return failures?1:0;}
