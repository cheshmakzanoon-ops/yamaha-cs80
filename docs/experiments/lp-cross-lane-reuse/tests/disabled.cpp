#include "core/Ig00156Batch.h"
#include <array>
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <type_traits>
#include <xmmintrin.h>

using cs80::Ig00156Stage;
using cs80::Ig00156LpCoefficientMemo;
using cs80::Ig00156LpStockRefresh;
static_assert(std::is_trivially_copyable_v<Ig00156Stage>);
static_assert(sizeof(Ig00156Stage)==296);
namespace {
struct Owned {
    alignas(Ig00156Stage) std::array<unsigned char,sizeof(Ig00156Stage)> storage{};
    Ig00156Stage* p;
    explicit Owned(const cs80::Ig00156Network& net=cs80::kIg00156LowPassNetwork)
        :p(std::construct_at(reinterpret_cast<Ig00156Stage*>(storage.data()),net)) {}
    ~Owned(){std::destroy_at(p);}
    void copy(const Owned& other){std::memcpy(p,other.p,sizeof *p);}
};
struct Status {unsigned csr;unsigned short cw,sw;int error;};
unsigned failures=0;std::uint64_t checks=0,hits=0,misses=0,donors=0,negativeCaught=0;
const char* context="";
void environment(unsigned csr) {
    unsigned short cw=static_cast<unsigned short>(0x037fU|(((csr>>13)&3U)<<10));
    asm volatile("fnclex\n\tfldcw %0"::"m"(cw));_mm_setcsr(csr);errno=29;
}
Status status() {
    Status s{};s.csr=_mm_getcsr();asm volatile("fnstcw %0":"=m"(s.cw));
    asm volatile("fnstsw %0":"=m"(s.sw));s.error=errno;return s;
}
void require(bool pass,const char* what) {
    ++checks;if(!pass&&failures++<16)std::fprintf(stderr,"%s: %s\n",context,what);
}
void sameStatus(Status a,Status b) {
    require(a.csr==b.csr,"MXCSR");require(a.cw==b.cw,"x87 control");
    require(a.sw==b.sw,"x87 status");require(a.error==b.error,"errno");
}
void sameState(const Owned&a,const Owned&b) {
    // Identically zeroed placement storage and memcpy cloning initialize all
    // padding alike. This compares all fields as well as unchanged padding.
    require(std::memcmp(a.p,b.p,sizeof(Ig00156Stage))==0,"stage representation");
}
__attribute__((noinline)) void stockSet(Ig00156Stage* s,float cutoff,float q,float rate) {
    s->set(cutoff,q,rate,false);
}
void initialize(Owned& s,float rate,float q=1.2f,float cutoffRatio=.2f) {
    environment(0x1f80U);stockSet(s.p,rate*cutoffRatio,q,rate);
    for(float x:{.21f,-.37f,.15f})s.p->process(x);
}
void makeDonor(Owned& donor,Ig00156LpCoefficientMemo& memo,float cutoff,float q,float rate,unsigned csr) {
    Owned reference;reference.copy(donor);
    environment(csr);stockSet(reference.p,cutoff,q,rate);auto expected=status();
    environment(csr);Ig00156LpStockRefresh token;
    const bool hit=cs80::tryReuseIg00156LpCoefficients(*donor.p,cutoff,q,rate,rate,memo,token);
    require(!hit,"initial donor unexpectedly reused");
    stockSet(donor.p,cutoff,q,rate);
    cs80::captureIg00156LpStockRefresh(*donor.p,token,memo);auto actual=status();
    sameState(reference,donor);sameStatus(expected,actual);donors+=memo.valid;
}
bool compareTarget(Owned& a,Owned& b,Ig00156LpCoefficientMemo& memo,float cutoff,float q,
                   float rate,float clock,unsigned csr,int expectedHit=-1) {
    environment(csr);stockSet(a.p,cutoff,q,rate);auto sa=status();
    environment(csr);Ig00156LpStockRefresh token;
    bool hit=cs80::tryReuseIg00156LpCoefficients(*b.p,cutoff,q,rate,clock,memo,token);
    stockSet(b.p,cutoff,q,rate); // V2 retains this genuine original call even on a hit.
    cs80::captureIg00156LpStockRefresh(*b.p,token,memo);auto sb=status();
    sameState(a,b);sameStatus(sa,sb);
    if(expectedHit>=0)require(hit==(expectedHit!=0),"admission route");
    hits+=hit;misses+=!hit;
    return hit;
}
void continuation(Owned& a,Owned& b,unsigned csr) {
    for(float input:{0.f,.1f,-.25f,.75f}) {
        environment(csr);auto x=a.p->process(input);auto sa=status();
        environment(csr);auto y=b.p->process(input);auto sb=status();
        require(std::bit_cast<unsigned>(x.lp)==std::bit_cast<unsigned>(y.lp),"LP output");
        require(std::bit_cast<unsigned>(x.hp)==std::bit_cast<unsigned>(y.hp),"HP output");
        require(std::bit_cast<unsigned>(x.bp)==std::bit_cast<unsigned>(y.bp),"BP output");
        sameState(a,b);sameStatus(sa,sb);
    }
}
void ordinaryGrid() {
    context="ordinary grid";
    for(float rate:{8000.f,44100.f,88200.f,96000.f,192000.f,1536000.f})
    for(float q:{.1f,.35f,.7f,1.5f,5.f,9.f,12.f})
    for(unsigned rounding=0;rounding<4;rounding++)
    for(unsigned denormal=0;denormal<4;denormal++)
    for(unsigned flags:{0U,0x20U,0x3fU}) {
        const unsigned csr=0x1f80U|(rounding<<13)|((denormal&1)?0x40U:0)|((denormal&2)?0x8000U:0)|flags;
        environment(0x1f80U);Owned d,a,b;initialize(d,rate,.9f);initialize(a,rate);b.copy(a);
        Ig00156LpCoefficientMemo memo;makeDonor(d,memo,rate*2.f,.71f,rate,csr);
        const bool expected=memo.valid&&((csr&memo.fpFlags)==memo.fpFlags);
        compareTarget(a,b,memo,rate*3.f,q,rate,rate,csr,expected);
        continuation(a,b,csr);
    }
}
void boundariesAndLifecycle() {
    context="boundaries/lifecycle";constexpr float rate=96000.f;const unsigned csr=0x1fa0U;
    for(int kind=0;kind<16;kind++) {
        environment(0x1f80U);Owned d,a,b;initialize(d,rate);initialize(a,rate);b.copy(a);
        Ig00156LpCoefficientMemo memo;makeDonor(d,memo,200000.f,.71f,rate,csr);
        float cutoff=300000.f,q=1.7f,r=rate,clock=rate;unsigned env=csr;int want=1;
        if(kind==0){cutoff=std::nextafter(48000.f,0.f);want=0;}
        if(kind==1)cutoff=48000.f;
        if(kind==2){q=0.f;want=0;}
        if(kind==3){q=-1.f;want=0;}
        if(kind==4){q=std::bit_cast<float>(0x7f800123U);want=0;}
        if(kind==5){cutoff=std::bit_cast<float>(0x7f800000U);want=0;}
        if(kind==6){r=0.f;want=0;}
        if(kind==7){r=6000.f;want=0;}
        if(kind==8){clock=6000.f;want=0;}
        if(kind==9){env=0x1f80U;want=0;}
        if(kind==10){env=csr|0x8040U;want=0;}
        if(kind==11){a.p->restoreMemory(a.p->getMemory());b.p->restoreMemory(b.p->getMemory());want=0;}
        if(kind==12){a.p->reset();b.p->reset();}
        if(kind==13){cutoff=rate*.2f;want=0;}
        if(kind==14){memo=Ig00156LpCoefficientMemo{};want=0;}
        if(kind==15){
            // Valid-representation test-only perturbation of actual gs, not the nominal network.
            auto perturb=[](Owned& x){std::uint64_t v;std::memcpy(&v,x.storage.data()+56,8);++v;std::memcpy(x.storage.data()+56,&v,8);};
            perturb(a);perturb(b);want=0;
        }
        compareTarget(a,b,memo,cutoff,q,r,clock,env,want);
    }
    context="same pointer";
    environment(0x1f80U);Owned donor,reference;initialize(donor,rate);
    Ig00156LpCoefficientMemo memo;makeDonor(donor,memo,200000.f,.71f,rate,csr);reference.copy(donor);
    compareTarget(reference,donor,memo,300000.f,1.7f,rate,rate,csr,0);
    context="wrong network";
    environment(0x1f80U);Owned d,a(cs80::kIg00156HighPassNetwork),b(cs80::kIg00156HighPassNetwork);
    initialize(d,rate);initialize(a,rate);b.copy(a);Ig00156LpCoefficientMemo m;
    makeDonor(d,m,200000.f,.71f,rate,csr);compareTarget(a,b,m,300000.f,1.7f,rate,rate,csr,0);
}
void staleAndNegative() {
    context="stale raw/Q-only donor";constexpr float rate=96000.f;
    for(bool qOnly:{false,true}) {
        environment(0x5f80U);Owned d;
        stockSet(d.p,200000.f,2.f,rate);
        environment(0x1fa0U);Ig00156LpCoefficientMemo memo;Ig00156LpStockRefresh token;
        bool hit=cs80::tryReuseIg00156LpCoefficients(*d.p,200000.f,qOnly?1.5f:2.f,rate,rate,memo,token);
        require(!hit&&!token.stage,"raw/Q-only must not certify donor");
        stockSet(d.p,200000.f,qOnly?1.5f:2.f,rate);cs80::captureIg00156LpStockRefresh(*d.p,token,memo);
        require(!memo.valid,"raw/Q-only populated memo");
    }
    context="negative wrong-Q whole copy";
    environment(0x1f80U);Owned d,a,b;initialize(d,rate);initialize(a,rate);b.copy(a);
    Ig00156LpCoefficientMemo memo;makeDonor(d,memo,200000.f,.71f,rate,0x1fa0U);
    require(compareTarget(a,b,memo,300000.f,1.7f,rate,rate,0x1fa0U,1),"negative setup hit");
    // This deliberately wrong alternative copies the two Q-dependent doubles.
    std::memcpy(b.storage.data()+136,d.storage.data()+136,8);
    std::memcpy(b.storage.data()+176,d.storage.data()+176,8);
    bool caught=std::memcmp(a.p,b.p,sizeof(Ig00156Stage))!=0;require(caught,"wrong-Q control undetected");negativeCaught+=caught;
    context="negative poisoned shared field";
    initialize(a,rate);b.copy(a);auto poisoned=memo;poisoned.coefficient[0]=std::nextafter(poisoned.coefficient[0],1.);
    environment(0x1fa0U);stockSet(a.p,300000.f,1.7f,rate);
    environment(0x1fa0U);Ig00156LpStockRefresh token;
    require(cs80::tryReuseIg00156LpCoefficients(*b.p,300000.f,1.7f,rate,rate,poisoned,token),"poison setup hit");
    stockSet(b.p,300000.f,1.7f,rate);
    caught=std::memcmp(a.p,b.p,sizeof(Ig00156Stage))!=0;require(caught,"poisoned shared control undetected");negativeCaught+=caught;
}
void unmaskedRejects() {
    context="unmasked try-helper rejection";environment(0x1f80U);Owned d,a,b;
    initialize(d,96000.f);initialize(a,96000.f);b.copy(a);Ig00156LpCoefficientMemo memo;
    makeDonor(d,memo,200000.f,.71f,96000.f,0x1fa0U);
    for(unsigned mask=7;mask<=12;mask++) {
        unsigned csr=0x1f80U&~(1U<<mask);environment(csr);auto before=status();Ig00156LpStockRefresh token;
        bool hit=cs80::tryReuseIg00156LpCoefficients(*b.p,300000.f,1.7f,96000.f,96000.f,memo,token);auto after=status();
        _mm_setcsr(0x1f80U);require(!hit&&!token.stage,"unmasked helper admitted or certified");
        sameState(a,b);sameStatus(before,after);
    }
}
}
int main() {
    auto old=_mm_getcsr();ordinaryGrid();
    environment(0x1f80U);std::printf("checks=%llu failures=%u hits=%llu misses=%llu donors=%llu negative_controls_caught=%llu\n",
        (unsigned long long)checks,failures,(unsigned long long)hits,(unsigned long long)misses,
        (unsigned long long)donors,(unsigned long long)negativeCaught);_mm_setcsr(old);return failures?1:0;
}
