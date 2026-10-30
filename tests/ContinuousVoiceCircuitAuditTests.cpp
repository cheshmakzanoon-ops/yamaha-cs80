// Independent audit of the electrical-input, nonproduction continuity boundary.
// Software/state-contract tests only: no calibrated capacitor or hardware claim.
#include "experimental/ContinuousVoiceCircuit.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

namespace {
using Circuit = cs80::experimental::ContinuousVoiceCircuit;
int checks = 0, failures = 0;
void check(bool ok, const char* message) {
    ++checks;
    if (!ok && ++failures <= 30) std::printf("FAIL: %s\n", message);
}
bool same(float a, float b) { return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b); }
bool same(cs80::Svf::State a, cs80::Svf::State b) { return same(a.ic1,b.ic1) && same(a.ic2,b.ic2); }
bool same(Circuit::State a, Circuit::State b) {
    if (a.prepared != b.prepared || a.samples != b.samples || !same(a.sampleRate,b.sampleRate)
        || !same(a.heldKeyVolts,b.heldKeyVolts)) return false;
    for (int r=0;r<2;++r)
        if (!same(a.ranks[r].phase,b.ranks[r].phase) || !same(a.ranks[r].hpf,b.ranks[r].hpf)
            || !same(a.ranks[r].lpf,b.ranks[r].lpf)) return false;
    return true;
}
bool same(Circuit::Output a, Circuit::Output b) {
    if (a.accepted != b.accepted || !same(a.heldKeyVolts,b.heldKeyVolts)) return false;
    for(int r=0;r<2;++r) {
        const auto& x=a.ranks[r]; const auto& y=b.ranks[r];
        if (!same(x.audio,y.audio) || !same(x.filtered,y.filtered) || !same(x.sine,y.sine)
            || !same(x.oscillatorHz,y.oscillatorHz) || !same(x.hpfHz,y.hpfHz) || !same(x.lpfHz,y.lpfHz)) return false;
    }
    return true;
}
bool finite(Circuit::Output o, Circuit::State s) {
    if (!o.accepted || !std::isfinite(o.heldKeyVolts)) return false;
    for(int r=0;r<2;++r) {
        const auto& a=o.ranks[r]; const auto& b=s.ranks[r];
        if (!std::isfinite(a.audio)||!std::isfinite(a.filtered)||!std::isfinite(a.sine)
            ||!std::isfinite(a.oscillatorHz)||!std::isfinite(a.hpfHz)||!std::isfinite(a.lpfHz)
            ||!std::isfinite(b.phase)||b.phase<0||b.phase>=1
            ||!std::isfinite(b.hpf.ic1)||!std::isfinite(b.hpf.ic2)
            ||!std::isfinite(b.lpf.ic1)||!std::isfinite(b.lpf.ic2)) return false;
    }
    return true;
}
Circuit::Controls controls(int n=0) {
    Circuit::Controls c;
    c.keyInputVolts=.25f + static_cast<float>((n/17)%7)*.125f;
    c.trHigh=(n/19)%2!=0; c.susHigh=(n/31)%2!=0;
    for(int r=0;r<2;++r) {
        auto& x=c.ranks[r];
        x.hzPerVolt=r ? 527.0f : 521.0f;
        x.vcoMultiplier=r ? 1.003f : .997f;
        x.pulseWidth=r ? .17f : .83f; x.pulseEnabled=true;
        x.noiseInput=static_cast<float>(((n*13+r*5)%37)-18)*.007f;
        x.hpfEffectiveCv=4.0f + static_cast<float>((n/11)%5)*.2f;
        x.lpfEffectiveCv=6.1f + static_cast<float>(r)*.3f;
        x.hpfQ=1.3f; x.lpfQ=2.7f;
        x.outputGain=.37f; x.filterLevel=.8f; x.sineLevel=.23f;
    }
    return c;
}
Circuit warm() {
    Circuit c; check(c.prepare(96000,.371f,{.173f,.719f}),"warm setup accepted");
    for(int n=0;n<157;++n) c.processSample(controls(n));
    return c;
}
template<class T> concept HasNoteOn = requires(T t) { t.noteOn(60,1.0f); };
template<class T> concept HasNoteOff = requires(T t) { t.noteOff(60); };
static_assert(!Circuit::productionIntegrated);
static_assert(!HasNoteOn<Circuit> && !HasNoteOff<Circuit>);
static_assert(noexcept(std::declval<Circuit&>().processSample(std::declval<const Circuit::Controls&>())));
static_assert(std::is_trivially_copyable_v<Circuit>);
auto bytes(const Circuit& c) {
    // Same-object before/after comparison, not cross-object or cross-platform
    // serialization: includes coefficients/RNG hidden by the public diagnostic.
    std::array<unsigned char,sizeof(Circuit)> result{};
    std::memcpy(result.data(),&c,sizeof(c));return result;
}

void coldAndTruthTable() {
    Circuit c;
    const auto initial=c.getState();
    check(!initial.prepared && initial.samples==0,"unprepared state explicit");
    check(!c.processSample(controls()).accepted && same(initial,c.getState()),"unprepared process is non-mutating");
    check(c.prepare(44100,.371f,{-1.125f,2.625f}),"explicit cold initialization");
    auto s=c.getState();
    check(s.prepared && same(s.heldKeyVolts,.371f) && s.samples==0,"no implicit MIDI cold pitch");
    check(same(s.ranks[0].phase,.875f)&&same(s.ranks[1].phase,.625f),"explicit independent phases normalized");
    check(same(s.ranks[0].hpf,{})&&same(s.ranks[0].lpf,{})&&same(s.ranks[1].hpf,{})&&same(s.ranks[1].lpf,{}),"cold filter memories zero");
    for(int tr=0;tr<2;++tr) for(int sus=0;sus<2;++sus) {
        check(c.prepare(48000,.371f,{.173f,.719f}),"truth fixture setup");
        auto in=controls(); in.keyInputVolts=1.25f; in.trHigh=tr!=0; in.susHigh=sus!=0;
        const auto out=c.processSample(in);
        const float expected=(tr==1&&sus==1) ? .371f : 1.25f;
        check(out.accepted&&same(out.heldKeyVolts,expected),"all four electrical NAND combinations");
        for(int r=0;r<2;++r) {
            float phase=(r==0?.173f:.719f)+cs80::clampf(expected*in.ranks[r].hzPerVolt*in.ranks[r].vcoMultiplier/48000,0,.45f);
            if(phase>=1) phase-=std::floor(phase);
            check(same(c.getState().ranks[r].phase,phase),"gate transition advances rather than resets phase");
        }
    }
    auto in=controls(); in.trHigh=true; in.susHigh=true;
    const float held=c.getState().heldKeyVolts;
    for(int n=0;n<1000;++n) {
        in.keyInputVolts=static_cast<float>(n%17);
        check(same(c.processSample(in).heldKeyVolts,held),"HH does not silently follow latest KI or MIDI note");
    }
    in.susHigh=false; in.keyInputVolts=0;
    auto before=c.getState(); auto out=c.processSample(in);
    check(out.accepted&&same(out.heldKeyVolts,0),"low SUS tracks exact zero key voltage");
    check(same(before.ranks[0].phase,c.getState().ranks[0].phase)&&same(before.ranks[1].phase,c.getState().ranks[1].phase),"zero key is not silently clamped to an audible oscillator pitch");
    check(c.getState().samples==before.samples+1,"zero frequency still clocks circuit sample");
}

// Independent routing oracle built only from the unchanged production
// primitives, NOT another ContinuousVoiceCircuit. It checks clock/order/routing,
// not the primitives' physical accuracy or discretization error.
struct Oracle {
    std::array<cs80::Oscillator,2> osc;
    std::array<cs80::Svf,2> hp,lp;
    cs80::Rng rng{99};
    float key=.371f,rate=96000;
    std::uint64_t count=0;
    Oracle() {osc[0].reset(.173f);osc[1].reset(.719f);}
    Circuit::Output tick(const Circuit::Controls& c) {
        if(!c.trHigh || !c.susHigh) key=c.keyInputVolts;
        Circuit::Output out;out.accepted=true;out.heldKeyVolts=key;
        for(int r=0;r<2;++r) {
            const auto& x=c.ranks[r]; const float hz=key*x.hzPerVolt*x.vcoMultiplier;
            const float h=cs80::nominalFilterCutoff(key,x.hpfEffectiveCv),l=cs80::nominalFilterCutoff(key,x.lpfEffectiveCv);
            const auto o=osc[r].tick(hz,rate,x.pulseWidth,x.sawEnabled,x.pulseEnabled,0,rng);
            float signal=(o.saw+o.pulse+x.noiseInput)*.9f;
            hp[r].set(h,x.hpfQ,rate); const float high=hp[r].process(signal).hp;
            if(x.hpfEnabled) signal=high;
            lp[r].set(l,x.lpfQ,rate); const float low=lp[r].process(signal).lp;
            if(x.lpfEnabled) signal=low;
            out.ranks[r]={(signal*x.filterLevel+o.sine*x.sineLevel)*x.outputGain,signal,o.sine,hz,h,l};
        }
        ++count;return out;
    }
    Circuit::State state() const {
        Circuit::State s{true,rate,key,count,{}};
        for(int r=0;r<2;++r)s.ranks[r]={osc[r].getPhase(),hp[r].getState(),lp[r].getState()};
        return s;
    }
};
void routingMuteBypassAndExternalOwnership() {
    Circuit c; c.prepare(96000,.371f,{.173f,.719f}); Oracle oracle;
    for(int n=0;n<8192;++n) {
        auto in=controls(n);
        for(int r=0;r<2;++r) {
            const int mask=(n/47+r)%4;
            in.ranks[r].hpfEnabled=(mask&1)!=0;in.ranks[r].lpfEnabled=(mask&2)!=0;
            if(n%13<7)in.ranks[r].outputGain=0;
            if(n%29<5)in.ranks[r].sawEnabled=in.ranks[r].pulseEnabled=false;
        }
        check(same(c.processSample(in),oracle.tick(in)),"independent primitive routing/gain oracle");
        check(same(c.getState(),oracle.state()),"two oscillator/four filter clocks include bypass and zero gain");
    }
    Circuit muted=warm(),audible=muted;
    bool changedHp[2]{},changedLp[2]{},changedPhase[2]{};
    const auto before=muted.getState();
    for(int n=0;n<4096;++n) {
        auto a=controls(n),b=a;
        for(int r=0;r<2;++r){a.ranks[r].outputGain=0;a.ranks[r].hpfEnabled=a.ranks[r].lpfEnabled=false;b.ranks[r].hpfEnabled=b.ranks[r].lpfEnabled=false;}
        const auto silence=muted.processSample(a);audible.processSample(b);
        check(same(muted.getState(),audible.getState()),"gain changes do not affect hidden circuit state");
        for(int r=0;r<2;++r){check(silence.ranks[r].audio==0,"zero gain is silent without retiring circuit");
            const auto now=muted.getState().ranks[r];
            changedHp[r]|=!same(now.hpf,before.ranks[r].hpf);changedLp[r]|=!same(now.lpf,before.ranks[r].lpf);changedPhase[r]|=!same(now.phase,before.ranks[r].phase);}
    }
    for(int r=0;r<2;++r)check(changedHp[r]&&changedLp[r]&&changedPhase[r],"both fully bypassed/muted rank histories evolve");
    // There are no EG references/callbacks in the API. A caller advances its own
    // control once and supplies the scalar; calling the kernel cannot advance it.
    cs80::Adsr eg,reference;eg.noteOn(.01f,.1f,.5f,.2f);reference=eg;
    int externalClocks=0;
    for(int n=0;n<1024;++n){auto in=controls(n);const float value=eg.tick(96000,true,true);++externalClocks;
        const float expected=reference.tick(96000,true,true);in.ranks[0].outputGain=value;
        auto twin=muted;const auto one=muted.processSample(in);const auto two=twin.processSample(in);
        check(same(value,expected)&&same(one,two),"caller-owned EG scalar has one external clock and deterministic consumption");}
    check(externalClocks==1024,"kernel API owns no extra EG ticks");
}

void futureUnchanged(Circuit& tested,Circuit reference) {
    for(int n=0;n<17;++n)check(same(tested.processSample(controls(n+991)),reference.processSample(controls(n+991))),"rejection preserves future trajectory including hidden coefficients");
    check(same(tested.getState(),reference.getState()),"rejection preserves complete future visible state");
}
void reject(const Circuit::Controls& invalid) {
    auto c=warm(); const auto old=c.getState();const auto oldBytes=bytes(c);auto reference=c;
    check(!c.processSample(invalid).accepted,"invalid transaction rejected");
    check(same(c.getState(),old),"invalid transaction preserves key/ranks/rate/count");
    check(bytes(c)==oldBytes,"invalid transaction preserves all object bytes including hidden coefficients and RNG");
    futureUnchanged(c,reference);
}
void invalidTransactions() {
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    using Member=float Circuit::RankControls::*;
    struct Domain{Member member;float low,high;};
    const std::array<Domain,11> fields{{
        {&Circuit::RankControls::hzPerVolt,0,4096},{&Circuit::RankControls::vcoMultiplier,0,4096},
        {&Circuit::RankControls::pulseWidth,.05f,.95f},{&Circuit::RankControls::noiseInput,-64,64},
        {&Circuit::RankControls::hpfEffectiveCv,-64,64},{&Circuit::RankControls::lpfEffectiveCv,-64,64},
        {&Circuit::RankControls::hpfQ,.35f,9},{&Circuit::RankControls::lpfQ,.35f,9},
        {&Circuit::RankControls::filterLevel,0,64},{&Circuit::RankControls::sineLevel,0,64},
        {&Circuit::RankControls::outputGain,0,64}}};
    for(int r=0;r<2;++r)for(const auto& f:fields)for(float bad:{nan,inf,-inf,std::nextafter(f.low,-inf),std::nextafter(f.high,inf)}) {
        auto in=controls(9);in.keyInputVolts=3.5f;in.ranks[r].*(f.member)=bad;reject(in);
    }
    for(int r=0;r<2;++r)for(const auto& f:fields)for(float edge:{f.low,f.high}) {
        auto c=warm();auto in=controls(9);in.ranks[r].*(f.member)=edge;
        const auto out=c.processSample(in);
        check(finite(out,c.getState()),"each declared finite input-domain endpoint accepted");
    }
    for(float bad:{nan,inf,-inf,-.001f,1024.001f}){auto in=controls();in.keyInputVolts=bad;reject(in);in.trHigh=in.susHigh=true;reject(in);}
    auto preparation=[&](double rate,float key,std::array<float,2> phases){auto c=warm();auto reference=c;const auto old=c.getState();const auto oldBytes=bytes(c);
        check(!c.prepare(rate,key,phases),"invalid prepare rejected");check(same(old,c.getState()),"invalid prepare is atomic/non-mutating");
        check(bytes(c)==oldBytes,"invalid prepare preserves every stored byte");futureUnchanged(c,reference);};
    for(double rate:{0.,15999.,1536001.,-48000.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})preparation(rate,.5f,{.2f,.7f});
    for(float key:{nan,inf,-inf,-.01f,1025.f})preparation(48000,key,{.2f,.7f});
    for(int r=0;r<2;++r)for(float phase:{nan,inf,-inf}){std::array<float,2> p{.2f,.7f};p[r]=phase;preparation(48000,.5f,p);}
    Circuit widePhase;
    const float largest=std::numeric_limits<float>::max();
    check(widePhase.prepare(48000,1024,{largest,-largest}),"all finite explicit cold phases normalize safely");
    const auto wideOutput=widePhase.processSample(controls());
    check(finite(wideOutput,widePhase.getState()),"finite extreme cold phases produce finite state");
}
void longRunAndRateBounds() {
    for(double rate:{16000.,44100.,48000.,96000.,192000.,1536000.}) {
        Circuit a,b;a.prepare(rate,.5f,{.1f,.6f});b.prepare(rate,.5f,{.1f,.6f});
        bool allFinite=true,deterministic=true;
        for(int n=0;n<65536;++n) {
            auto in=controls(n);in.keyInputVolts=n%67==0?1024.f:(n%101==0?0.f:in.keyInputVolts);
            for(int r=0;r<2;++r){auto& x=in.ranks[r];x.hpfQ=x.lpfQ=9;x.hpfEffectiveCv=(n/73)%2?64.f:-64.f;
                x.lpfEffectiveCv=(n/97)%2?64.f:-64.f;x.noiseInput=(n%2?64.f:-64.f);
                x.hzPerVolt=x.vcoMultiplier=n%67==0?4096.f:x.hzPerVolt;
                x.filterLevel=x.sineLevel=x.outputGain=64.f;}
            const auto x=a.processSample(in),y=b.processSample(in);
            allFinite&=finite(x,a.getState());deterministic&=same(x,y);
        }
        check(allFinite,"wide-domain long-run outputs and all numerical memories finite");
        check(deterministic&&same(a.getState(),b.getState()),"same explicit sample sequence deterministic at each rate");
        check(a.getState().samples==65536,"one accepted call is exactly one internal sample");
        auto reinitialized=a;check(reinitialized.prepare(rate,1.25f,{.33f,.91f}),"explicit valid reprepare accepted");
        Circuit fresh;fresh.prepare(rate,1.25f,{.33f,.91f});
        futureUnchanged(reinitialized,fresh);
    }
    // 60 seconds at a declared 48 kHz internal rate: no audible-allocation
    // shortcut, periodic prepare/reset, or denormal-dependent termination.
    Circuit minute;minute.prepare(48000,.5f,{.1f,.6f});bool allFinite=true;
    auto in=controls();in.trHigh=in.susHigh=true;
    for(auto& r:in.ranks){r.hpfQ=r.lpfQ=9;r.outputGain=0;r.hpfEnabled=r.lpfEnabled=false;}
    for(int n=0;n<2880000;++n){in.ranks[0].noiseInput=n%2?.01f:-.01f;in.ranks[1].noiseInput=-in.ranks[0].noiseInput;
        const auto out=minute.processSample(in);allFinite&=finite(out,minute.getState());}
    check(allFinite&&minute.getState().samples==2880000,"sixty-second silent/bypassed high-Q circuit run remains finite and clocked");
}
} // namespace
int main(){coldAndTruthTable();routingMuteBypassAndExternalOwnership();invalidTransactions();longRunAndRateBounds();
    std::printf("Continuous voice circuit audit: %d checks, %d failures\n",checks,failures);return failures?1:0;}
