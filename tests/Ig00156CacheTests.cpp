// Pure header-only regression against the unmodified b5e0f1f stage.
// Run --benchmark explicitly for advisory timings; timing is never a test gate.
#include "Ig00156StageBaseline.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
using cs80::Ig00156Stage;
using cs80::Ig00156StageBaseline;
std::uint64_t comparisons = 0, samples = 0, exceptionalNanPairs = 0;
// Invalid controls can produce NaNs whose sign/payload is compiler-dependent.
// This exception is enabled ONLY in explicit invalid-control cases. Finite,
// infinite and signed-zero results still require identical bits in those cases.
// Floating exception flags and changing rounding modes are not a cache contract.
bool exceptionalControls = false;
int failures = 0;
const char* context = "";
template<class T> void same(T a, T b, const char* field)
{
    if (exceptionalControls && std::isnan(a) && std::isnan(b))
    { ++exceptionalNanPairs; return; }
    ++comparisons;
    using Bits = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
    if (std::bit_cast<Bits>(a) != std::bit_cast<Bits>(b))
    {
        if (failures++ < 12) std::cerr << context << ": " << field << " bit mismatch at sample " << samples << '\n';
    }
}
template<class A, class B> void stateSame(const A& a, const B& b)
{
    same(a.input,b.input,"input"); same(a.feedback,b.feedback,"feedback");
    same(a.bp,b.bp,"bp"); same(a.lp,b.lp,"lp");
}
void observe(const Ig00156Stage& a, const Ig00156StageBaseline& b)
{
    stateSame(a.getState(),b.getState()); stateSame(a.getNodes(),b.getNodes());
    const auto am=a.getMemory(); const auto bm=b.getMemory();
    stateSame(am.z,bm.z); stateSame(am.v,bm.v);
    same(am.slope1,bm.slope1,"slope1"); same(am.slope2,bm.slope2,"slope2");
    same(am.halfIntegrators,bm.halfIntegrators,"memory integrators");
    same(am.halfCouplings,bm.halfCouplings,"memory couplings");
    same(a.halfSteps().integrators,b.halfSteps().integrators,"integrators");
    same(a.halfSteps().couplings,b.halfSteps().couplings,"couplings");
    same(a.qSiemens(),b.qSiemens(),"qSiemens");
    same(a.passbandGain(),b.passbandGain(),"gain");
    same(a.feedforwardCutoffHz(),b.feedforwardCutoffHz(),"feedforward");
}
// Keep set/process calls in separate optimization contexts. With FMA enabled,
// GCC can contract or constant-fold adjacent inlined calls differently even
// when BOTH classes are the unchanged baseline. This wrapper leaves each
// implementation and its floating-point flags unchanged.
#if defined(_MSC_VER)
#define CS80_CACHE_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define CS80_CACHE_NOINLINE __attribute__((noinline))
#else
#define CS80_CACHE_NOINLINE
#endif
template<class Stage> CS80_CACHE_NOINLINE auto processIndependently(Stage& s,float input)
{ return s.process(input); }
template<class Stage> CS80_CACHE_NOINLINE void setIndependently(Stage& s,float fc,float q,float rate,bool unwarped)
{ s.set(fc,q,rate,unwarped); }
#undef CS80_CACHE_NOINLINE

struct Pair {
    Ig00156Stage a;
    Ig00156StageBaseline b;
    double energy=0.;
    Pair() = default;
    explicit Pair(const cs80::Ig00156Network& n):a(n),b(n){}
    void set(float fc,float q,float rate,bool unwarped=false)
    { setIndependently(a,fc,q,rate,unwarped); setIndependently(b,fc,q,rate,unwarped); observe(a,b); }
    void tick(float input,bool finite=true)
    {
        const auto x=processIndependently(a,input); const auto y=processIndependently(b,input); ++samples;
        same(x.hp,y.hp,"output hp");same(x.lp,y.lp,"output lp");same(x.bp,y.bp,"output bp");
        observe(a,b);
        if (finite && !(std::isfinite(x.hp)&&std::isfinite(x.lp)&&std::isfinite(x.bp)))
        { if(failures++<12)std::cerr<<context<<": nonfinite normal sample\n"; }
        if(finite)energy+=double(x.hp)*x.hp+double(x.lp)*x.lp+double(x.bp)*x.bp;
    }
};
float signal(int i) { return float((i*37)%211-105)*.013f + .17f*std::sin(float(i)*.071f); }
const std::array<cs80::Ig00156Network,3> networks {{
    cs80::kIg00156HighPassNetwork,cs80::kIg00156LowPassNetwork,
    {91000.,39000.,.82e-6,1.1e6,220e-12,1.8e-9,5.1e6}
}};
void trajectories()
{
    for(const auto& net:networks)for(int mode=0;mode<8;++mode)
    {
        context="trajectories"; Pair p(net);
        for(int i=0;i<14000;++i)
        {
            const float fc=(mode==2||mode==3)?1200.f+1100.f*std::sin(i*.0031f):1200.f;
            const float q=(mode==1||mode==3||mode>=4)?.35f+8.65f*(float(i%997)/996.f):2.f;
            const float rate=mode==4?std::array<float,4>{88200,96000,192000,384000}[(i/137)%4]:96000.f;
            p.set(fc,q,rate,mode==5 && (i/97)%2);
            if(mode==6&&i%131==0){p.a.reset();p.b.reset();observe(p.a,p.b);}
            if(mode==7&&i%149==0)
            {
                const auto a=p.a.getMemory();const auto b=p.b.getMemory();
                p.set(5000.f,8.f,192000.f,true);p.tick(signal(i));
                p.a.restoreMemory(a);p.b.restoreMemory(b);observe(p.a,p.b);
                p.set(fc,q,rate);
            }
            p.tick(signal(i));
        }
        if(!(p.energy>1.)) {++failures;std::cerr<<"trivial trajectory\n";}
    }
}
void boundaries()
{
    context="boundary controls";
    const std::array<float,10> cutoffs{-1.f,-0.f,0.f,18.f,200.f,20000.f,43200.f,86400.f,1e10f,1e30f};
    const std::array<float,11> qs{-1e30f,-1.f,-0.f,0.f,.35f,std::nextafter(.35f,0.f),1.f,9.f,std::nextafter(9.f,10.f),10.f,1e30f};
    for(const auto& net:networks) for(float fc:cutoffs)
    {
        Pair p(net);
        for(float q:qs)for(int repeat=0;repeat<8;++repeat)
        {p.set(fc,q,96000.f,repeat>=4);p.tick(signal(repeat));}
    }
    context="adjacent Float32 controls";
    for(const auto& net:networks) for(bool unwarped:{false,true})
        for(int mode=0;mode<3;++mode)
        {
            Pair p(net);
            for(int i=0;i<1000;++i)
            {
                const float fc=mode!=1&&i%2?std::nextafter(1200.f,2000.f):1200.f;
                const float rate=mode!=0&&i%2?std::nextafter(96000.f,192000.f):96000.f;
                const float q=i%2?std::nextafter(2.f,3.f):2.f;
                p.set(fc,q,rate,unwarped);p.tick(signal(i));
            }
        }
    context="signed zero transitions";
    for(const auto& net:networks) for(bool unwarped:{false,true})
        for(float initialZero:{0.f,-0.f})
        {
            Pair p(net);
            exceptionalControls=true;
            for(int i=0;i<12;++i)
            {
                const float rate=i%2? -initialZero:initialZero;
                p.set(1000.f,1.f+float(i%3),rate,unwarped);
                p.tick(signal(i),false);
            }
            exceptionalControls=false;
            Pair cutoff(net);
            for(int i=0;i<12;++i)
            {
                cutoff.set(i%2?-initialZero:initialZero,1.f+float(i%3),96000.f,unwarped);
                cutoff.tick(signal(i));
            }
        }
    context="exceptional controls";
    exceptionalControls=true;
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    // Exceptional controls retain original IEEE behavior, including NaNs.
    // Use fresh instances per case, then test subsequent cache hits/misses too.
    for(const auto& net:networks)for(float fc:{-1.f,0.f,1000.f,inf,-inf,nan})
        for(float q:{-inf,.35f,2.f,inf,nan})for(float rate:{-1.f,-0.f,0.f,96000.f,inf,nan})
        {
            Pair p(net);
            for(int i=0;i<6;++i){p.set(fc,i%2?q:1.f,rate,i>=3);p.tick(signal(i),false);}
        }
    exceptionalControls=false;
    context="invalid to valid recovery";
    for(const auto& net:networks)
    {
        Pair p(net);p.set(1000.f,2.f,96000.f);p.tick(.3f);
        const auto am=p.a.getMemory();const auto bm=p.b.getMemory();
        for(bool restore:{false,true})
        {
            exceptionalControls=true;
            p.set(nan,nan,nan);p.tick(.2f,false);
            exceptionalControls=false;
            if(restore){p.a.restoreMemory(am);p.b.restoreMemory(bm);}
            else {p.a.reset();p.b.reset();}
            p.set(1000.f,2.f,96000.f);
            for(int i=0;i<32;++i){p.set(1000.f,1.f+float(i%3),96000.f);p.tick(signal(i));}
        }
    }
}
void lifecycle()
{
    context="lifecycle";
    Pair defaults; observe(defaults.a,defaults.b);defaults.tick(.2f);
    // First set with both original sentinel fields must still calculate.
    exceptionalControls=true;
    defaults.set(-1.f,2.f,-1.f);defaults.tick(.1f,false);
    exceptionalControls=false;
    for(const auto& net:networks)
    {
        Pair p(net), independent(net);
        for(int i=0;i<4000;++i)
        {
            if(i%43==0){const auto ah=p.a.halfSteps();const auto bh=p.b.halfSteps();
                p.set(1000.f,2.f,i%86?3000.f:96000.f,i%86!=0);
                p.a.reReference(ah);p.b.reReference(bh);observe(p.a,p.b);}
            p.set(1000.f,.5f+float(i%70)*.1f,i%86>=43?3000.f:96000.f,i%86>=43);
            if(i%31==0){const auto az=p.a.getState();const auto bz=p.b.getState();
                const auto av=p.a.getNodes();const auto bv=p.b.getNodes();
                p.tick(.7f);p.a.setState(az,av);p.b.setState(bz,bv);observe(p.a,p.b);}
            if(i%67==0){Pair copied=p;copied.set(1000.f,6.f,96000.f);copied.tick(.3f);
                observe(p.a,p.b);independent=copied;}
            if(i%79==0){auto am=p.a.getMemory();auto bm=p.b.getMemory();
                p.a.reset();p.b.reset();p.a.restoreMemory(am);p.b.restoreMemory(bm);
                p.set(1000.f,4.f,96000.f);}
            independent.set(2200.f,float(i%80)*.1f+.4f,88200.f);independent.tick(-signal(i));
            p.tick(signal(i));
        }
    }
}
struct Frame{float fc,q,input;};
struct Result{double ns;std::uint64_t checksum;};
std::uint64_t hash(std::uint64_t h,float x){return (h^std::bit_cast<std::uint32_t>(x))*1099511628211ULL;}
template<class Stage> Result timed(const std::vector<Frame>& frames)
{
    Stage s(cs80::kIg00156LowPassNetwork);
    std::uint64_t checksum=1469598103934665603ULL;
    const auto start=std::chrono::steady_clock::now();
    for(const auto& f:frames){s.set(f.fc,f.q,96000.f);const auto o=s.process(f.input);
        checksum=hash(hash(hash(checksum,o.hp),o.lp),o.bp);}
    const auto finish=std::chrono::steady_clock::now();
    return {std::chrono::duration<double,std::nano>(finish-start).count()/frames.size(),checksum};
}
void benchmark()
{
    std::cout<<"Advisory set+process timings, median of 7, 300000 samples; no timing gates.\n";
    for(int mode=0;mode<7;++mode)
    {
        const char* name=std::array<const char*,7>{"fixed","Q only","cutoff only","both","Q + cutoff every 2","Q + cutoff every 4","Q + cutoff every 16"}[mode];
        std::vector<Frame> frames;frames.reserve(300000);
        for(int i=0;i<300000;++i){const int stride=mode==4?2:mode==5?4:mode==6?16:1;
            const float fc=(mode>=2)?1200.f+1000.f*std::sin(float(i/stride)*.0021f):1200.f;
            const float q=(mode==0||mode==2)?2.f:.5f+8.f*float(i%997)/996.f;
            frames.push_back({fc,q,signal(i)});}
        std::array<double,7> oldTimes{},newTimes{};std::uint64_t checksum=0;
        for(int trial=0;trial<7;++trial){Result a,b;
            if(trial%2){a=timed<Ig00156StageBaseline>(frames);b=timed<Ig00156Stage>(frames);}
            else{b=timed<Ig00156Stage>(frames);a=timed<Ig00156StageBaseline>(frames);}
            if(a.checksum!=b.checksum||(trial&&checksum!=a.checksum)){++failures;std::cerr<<"benchmark checksum mismatch\n";}
            checksum=a.checksum;oldTimes[trial]=a.ns;newTimes[trial]=b.ns;}
        std::sort(oldTimes.begin(),oldTimes.end());std::sort(newTimes.begin(),newTimes.end());
        std::cout<<name<<": old "<<oldTimes[3]<<" ns new "<<newTimes[3]<<" ns ratio "<<newTimes[3]/oldTimes[3]<<" checksum "<<checksum<<'\n';
    }
}
} // namespace
int main(int argc,char** argv)
{
    trajectories();boundaries();lifecycle();
    if(argc==2&&std::string_view(argv[1])=="--benchmark")benchmark();
    std::cout<<comparisons<<" exact scalar comparisons across "<<samples<<" samples, "<<exceptionalNanPairs<<" exceptional both-NaN checks; "<<failures<<" failures\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
