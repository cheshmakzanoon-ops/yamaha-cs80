// Exact numerical regression, not a hardware-model or CPU qualification.
// Fixed floating environment per run: the cache does not promise to reproduce
// recomputation after external rounding/FTZ/DAZ changes between process calls.
// Native builds deliberately retain the compiler's normal FMA contraction.
// PRA_BASELINE_SELF_CHECK compares independent frozen instances through the
// same noinline boundaries to diagnose compiler-context mismatches.
#include "core/PraWahCircuit.h"
#include "PraWahCircuitBaseline.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

#if defined(_MSC_VER)
#define PRA_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define PRA_NOINLINE __attribute__((noinline))
#else
#define PRA_NOINLINE
#endif

namespace {
using Baseline = cs80_pra_baseline::PraWahCircuit;
#ifdef PRA_BASELINE_SELF_CHECK
using Candidate = Baseline;
#else
using Candidate = cs80::PraWahCircuit;
#endif
using Profile = Candidate::Profile;
constexpr double kInfinity=std::numeric_limits<double>::infinity();
constexpr double kNaN=std::numeric_limits<double>::quiet_NaN();
std::uint64_t frames = 0, checks = 0;
double energy = 0.0;
const char* context = "startup";
int sample = 0;

void require(bool condition, const char* what)
{
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL %s sample=%d: %s\n", context, sample, what);
        std::exit(1);
    }
}
void exact(double actual, double expected, const char* what)
{
    ++checks;
    if (std::bit_cast<std::uint64_t>(actual) != std::bit_cast<std::uint64_t>(expected)) {
        std::fprintf(stderr, "FAIL %s sample=%d %s actual=%a expected=%a\n",
                     context, sample, what, actual, expected);
        std::exit(1);
    }
}
PRA_NOINLINE Candidate::Nodes candidateTick(Candidate& a, double input) { return a.process(input); }
PRA_NOINLINE Baseline::Nodes baselineTick(Baseline& b, double input) { return b.process(input); }

template<class P> std::array<double,9> profileValues(const P& p)
{
    return {p.parallelOhms,p.vr1Ohms,p.vr2Ohms,p.sourceOhms,p.dryLoadSiemens,
            p.heelOhms,p.toeOhms,p.opticalTauSeconds,p.opticalDecaySeconds};
}
Baseline::Profile baselineProfile(const Profile& p)
{
    Baseline::Profile q;
    q.parallelOhms=p.parallelOhms; q.vr1Ohms=p.vr1Ohms; q.vr2Ohms=p.vr2Ohms;
    q.sourceOhms=p.sourceOhms; q.dryLoadSiemens=p.dryLoadSiemens;
    q.heelOhms=p.heelOhms; q.toeOhms=p.toeOhms;
    q.opticalTauSeconds=p.opticalTauSeconds; q.opticalDecaySeconds=p.opticalDecaySeconds;
    return q;
}
template<class Circuit> std::array<double,20> observableState(const Circuit& c)
{
    std::array<double,20> s{};
    for (std::size_t i=0;i<4;++i) {
        s[2*i]=c.capacitorState()[i].voltage;
        s[2*i+1]=c.capacitorState()[i].current;
    }
    s[8]=c.opticalResistance(); s[9]=c.targetOpticalResistance(); s[10]=c.sampleRate();
    const auto p=profileValues(c.getProfile());
    for(std::size_t i=0;i<p.size();++i) s[11+i]=p[i];
    return s;
}
struct Pair {
    Candidate a;
    Baseline b;
    void compareState() const
    {
        const auto actual=observableState(a), expected=observableState(b);
        for(std::size_t i=0;i<actual.size();++i) exact(actual[i],expected[i],"state");
    }
    void prepare(double rate, const Profile& p, double pedal)
    {
        require(a.prepare(rate,p,pedal)==b.prepare(rate,baselineProfile(p),pedal),"prepare return");
        compareState();
    }
    void pedal(double x) {a.setPedal(x);b.setPedal(x);compareState();}
    void resistance(double x) {a.setOpticalResistance(x);b.setOpticalResistance(x);compareState();}
    void reset() {a.reset();b.reset();compareState();}
    void reset(double x) {a.reset(x);b.reset(x);compareState();}
    void tick(double input)
    {
        const auto x=candidateTick(a,input);
        const auto y=baselineTick(b,input);
        const std::array<double,9> actual{x.x,x.n,x.a,x.p,x.b,x.q,x.c,x.d,x.y};
        const std::array<double,9> expected{y.x,y.n,y.a,y.p,y.b,y.q,y.c,y.d,y.y};
        for(std::size_t i=0;i<actual.size();++i) {
            require(std::isfinite(actual[i]),"finite node");
            exact(actual[i],expected[i],"node");
        }
        compareState();
        energy+=x.x*x.x+x.y*x.y;
        ++frames;
    }
};
double signal(int i) {return .3*std::sin(i*.073)+.1*std::cos(i*.013);}

void mainMatrix()
{
    for(double rate:{8000.,44100.,48000.,96000.,192000.,1536000.}) {
        for(int variant=0;variant<3;++variant) {
            Pair pair;
            Profile p;
            if(variant==1) {
                p.heelOhms=100.;p.toeOhms=10000000.;
                p.opticalTauSeconds=p.opticalDecaySeconds=0.;
            }
            if(variant==2) {
                p.sourceOhms=100000.;p.dryLoadSiemens=.001;
                p.vr1Ohms=25000.;p.parallelOhms=10000.;
            }
            char description[80];
            std::snprintf(description,sizeof(description),"matrix rate=%.0f profile=%d",rate,variant);
            context=description;
            pair.prepare(rate,p,.3);
            for(sample=0;sample<160000;++sample) {
                if(sample==1000) pair.pedal(.9);
                if(sample==30000) pair.pedal(.1);
                if(sample==70000) pair.pedal(.75);
                if(sample>=90000&&sample<91000) pair.pedal(std::sin(sample*.08)*1.5);
                if(sample==40000) {pair.reset();pair.reset();}
                if(sample==60000) pair.reset(.45);
                if(sample==95000) pair.resistance(30000.);
                if(sample==110000) pair.prepare(rate*.5,p,.8);
                if(sample==115000) {pair.prepare(-1.,p,.8);pair.pedal(kNaN);}
                // Same optical key with changed clock/profile must invalidate.
                if(sample==120000) {p.vr1Ohms=4321.;pair.prepare(rate,p,.8);}
                if(sample==125000) {p.parallelOhms=345678.;pair.prepare(rate,p,.8);}
                if(sample==130000) pair.prepare(rate,p,.2);
                double input=signal(sample);
                if(sample%997==0) input=kInfinity;
                if(sample%991==0) input=kNaN;
                pair.tick(input);
            }
        }
    }
}
void constructorAndAdjacent()
{
    context="constructor and repeated reset";
    Pair pair;
    for(sample=0;sample<10000;++sample) pair.tick(signal(sample));
    pair.reset();pair.reset();pair.reset(-0.);pair.reset(0.);
    context="adjacent double pedal/resistance";
    for(double base:{-0.,0.,.1,.5,1.}) {
        for(double pedal:{std::nextafter(base,-kInfinity),base,std::nextafter(base,kInfinity)}) {
            pair.pedal(pedal);
            for(sample=0;sample<1000;++sample) pair.tick(signal(sample));
            pair.reset(pedal);
            for(sample=0;sample<100;++sample) pair.tick(signal(sample));
        }
    }
    for(double base:{100.,2000.,30000.,10000000.}) {
        for(double resistance:{std::nextafter(base,-kInfinity),base,std::nextafter(base,kInfinity)}) {
            pair.resistance(resistance);pair.reset();
            for(sample=0;sample<1000;++sample) pair.tick(signal(sample));
        }
    }
}
void invalidInputs()
{
    context="invalid prepare preserves observable and hidden state";
    Pair pair;
    pair.prepare(48000.,Profile{},.4);
    for(sample=0;sample<1000;++sample) pair.tick(signal(sample));
    const auto invalidPrepare=[&](double rate,const Profile& p,double pedal) {
        const auto beforeA=observableState(pair.a), beforeB=observableState(pair.b);
        require(!pair.a.prepare(rate,p,pedal),"candidate rejects invalid prepare");
        require(!pair.b.prepare(rate,baselineProfile(p),pedal),"baseline rejects invalid prepare");
        const auto afterA=observableState(pair.a), afterB=observableState(pair.b);
        for(std::size_t i=0;i<beforeA.size();++i) {
            exact(afterA[i],beforeA[i],"candidate invalid prepare unchanged");
            exact(afterB[i],beforeB[i],"baseline invalid prepare unchanged");
        }
        for(sample=0;sample<128;++sample) pair.tick(signal(sample));
    };
    for(double rate:{-1.,0.,7999.,1536001.,kInfinity,-kInfinity,kNaN}) invalidPrepare(rate,Profile{},.4);
    for(double pedal:{kInfinity,-kInfinity,kNaN}) invalidPrepare(48000.,Profile{},pedal);
    constexpr std::array<double Profile::*,9> members{
        &Profile::parallelOhms,&Profile::vr1Ohms,&Profile::vr2Ohms,&Profile::sourceOhms,
        &Profile::dryLoadSiemens,&Profile::heelOhms,&Profile::toeOhms,
        &Profile::opticalTauSeconds,&Profile::opticalDecaySeconds};
    for(auto member:members) for(double bad:{-1.,1e100,kNaN,kInfinity}) {
        Profile p;p.*member=bad;invalidPrepare(48000.,p,.4);
    }
    context="invalid setters ignored";
    for(double bad:{kNaN,kInfinity,-kInfinity}) {
        const auto before=observableState(pair.a);
        pair.pedal(bad);pair.resistance(bad);
        const auto after=observableState(pair.a);
        for(std::size_t i=0;i<before.size();++i) exact(after[i],before[i],"invalid setter unchanged");
    }
    pair.resistance(99.);pair.resistance(10000001.);
    for(sample=0;sample<1000;++sample) pair.tick(signal(sample));
}
void independentInstancesAndSettling()
{
    context="distinct interleaved instances";
    Pair left,right;
    Profile p;p.parallelOhms=123456.;p.vr1Ohms=777.;
    left.prepare(44100.,Profile{},.2);right.prepare(192000.,p,.8);
    for(sample=0;sample<20000;++sample) {
        if(sample%211==0) left.pedal((sample%101)/100.);
        if(sample%317==0) right.reset((sample%97)/96.);
        left.tick(signal(sample));right.tick(-signal(sample));
    }
    context="long max-rate optical settling and held coefficients";
    Pair pair;pair.prepare(1536000.,Profile{},0.);pair.pedal(1.);
    for(sample=0;sample<3500000;++sample) {
        if(sample==1000000) pair.pedal(0.);
        pair.tick(signal(sample));
    }
    require(std::abs(pair.a.opticalResistance()-pair.a.targetOpticalResistance())<1e-3,
            "long optical transition approaches target without a new snap");
}
void identicalKeyRateChanges()
{
    context="identical optical key across every rate-only reprepare pair";
    constexpr std::array<double,6> rates{8000.,44100.,48000.,96000.,192000.,1536000.};
    for(double before:rates) for(double after:rates) {
        Pair pair;
        pair.prepare(before,Profile{},.4);
        pair.tick(.2); // Populate exactly the same optical key used after prepare.
        const double optical=pair.a.opticalResistance();
        pair.prepare(after,Profile{},.4);
        exact(pair.a.opticalResistance(),optical,"rate-only prepare retains optical key");
        for(sample=0;sample<64;++sample) pair.tick(signal(sample));
    }
}
}
int main()
{
    mainMatrix();constructorAndAdjacent();invalidInputs();independentInstancesAndSettling();identicalKeyRateChanges();
    require(energy>0.,"non-silent exercise");
    std::printf("PASS frames=%llu checks=%llu energy=%.17g\n",
                (unsigned long long)frames,(unsigned long long)checks,energy);
}
