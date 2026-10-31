#include "core/Smoothing.h"
#include "core/ExpressionOptics.h"
#include "fixtures/stationary_controls/SmoothingBefore.h"
#include "fixtures/stationary_controls/ExpressionOpticsBefore.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

// Preregistered in stationary-controls-exact.json before implementation/evaluation.
// The frozen baseline is independent of the production recurrences. Every exposed
// output and stored-state readout must retain its bits, including signed zero.
namespace {
int checks=0, failures=0;
std::uint64_t controllerSamples=0, expressionSamples=0;
constexpr std::array<double,6> rates {88200,96000,176400,192000,352800,384000};
bool same(double a,double b) { return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b); }
void check(bool ok,const char* what,double rate=0,int variant=-1)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s rate=%.0f variant=%d\n",what,rate,variant); }
}
struct ControllerPair {
    cs80::ControlSmoother actual;
    cs80::stationary_before::ControlSmoother before;
    void snap(double value) { actual.snap(value); before.snap(value); }
    bool advance(double target,double coefficient)
    {
        ++controllerSamples;
        const auto a=actual.advance(target,coefficient), b=before.advance(target,coefficient);
        return same(a,b)&&same(actual.value(),before.value())
            && actual.atTarget(target)==before.atTarget(target);
    }
};
struct ExpressionPair {
    cs80::ExpressionOpticalCell actual;
    cs80::stationary_before::ExpressionOpticalCell before;
    void prepare(double rate,const cs80::PraWahCircuit::Profile& profile)
    { actual.prepare(rate,profile); before.prepare(rate,profile); }
    void reset(double pedal) { actual.reset(pedal); before.reset(pedal); }
    void setPedal(double pedal) { actual.setPedal(pedal); before.setPedal(pedal); }
    bool readouts() const
    { return same(actual.position(),before.position())&&same(actual.opticalResistance(),before.opticalResistance()); }
    bool tick()
    {
        ++expressionSamples;
        const auto a=actual.tick(), b=before.tick();
        return same(a,b)&&readouts();
    }
};

void controlZeros()
{
    constexpr std::array<double,2> zeros {0.0,-0.0};
    constexpr std::array<double,9> coefficients {0.0,-0.0,0.125,0.5,1.0,-0.125,-1.0,2.0,-2.0};
    int variant=0;
    for (const auto initial:zeros) for (const auto target:zeros) for (const auto k:coefficients)
    {
        ControllerPair pair; pair.snap(initial);
        bool equal=pair.advance(target,k);
        // A subsequent move exposes any changed stored value, not just its return.
        for (int i=0;i<128;++i) equal&=pair.advance(0.75,0.03125);
        check(equal,"controller signed-zero equality and subsequent movement",0,variant++);
    }
}

void controlStationaryFinite()
{
    const double tiny=std::numeric_limits<double>::denorm_min();
    const double huge=std::numeric_limits<double>::max();
    const std::array<double,15> targets {-huge,-1e100,-1e-100,-tiny,-0.0,0.0,tiny,
        std::numeric_limits<double>::min(),1e-100,0.125,1.0,100.0,1e100,huge,-1.0};
    const std::array<double,12> coefficients {-huge,-2.0,-1.0,-0.0,0.0,tiny,1e-5,0.001,0.5,1.0,2.0,huge};
    int variant=0;
    for (const auto target:targets) for (const auto k:coefficients)
    {
        ControllerPair pair; pair.snap(target);
        bool equal=true;
        for (int i=0;i<16;++i) equal&=pair.advance(target,k);
        check(equal,"stationary finite controller retains target bits",0,variant++);
    }
}

void controlMoving(double rate)
{
    ControllerPair pair; pair.snap(0.4);
    constexpr std::array<double,8> targets {0.0,0.7,-0.3,1.0,0.1,-0.0,-1.0,0.375};
    constexpr std::array<double,4> seconds {0.005,0.05,0.0,0.0001};
    bool moving=true,settled=true,wake=true;
    const int count=(int)(0.5*rate);
    for (int i=0;i<count;++i)
    {
        if (i>0 && i%20011==0) pair.snap((i/20011)%2 ? -0.0 : 0.0);
        const double target=targets[(i/4096)%targets.size()];
        const double k=cs80::smoothingCoefficient(seconds[(i/997)%seconds.size()],rate);
        moving&=pair.advance(target,k);
    }
    pair.snap(-0.0);
    for (int i=0;i<4096;++i) settled&=pair.advance(i%2 ? -0.0 : 0.0,0.001);
    pair.snap(0.375);
    for (int i=0;i<4096;++i) settled&=pair.advance(0.375,0.001);
    for (int i=0;i<count;++i) wake&=pair.advance(-0.625,cs80::smoothingCoefficient(0.005,rate));
    check(moving,"moving controller coefficients, targets and snaps",rate);
    check(settled,"long stationary controller spans preserve signed target",rate);
    check(wake,"controller first and subsequent moves after stationarity",rate);
}

void controlNonfinite()
{
    const double inf=std::numeric_limits<double>::infinity(), nan=std::numeric_limits<double>::quiet_NaN();
    const std::array<double,6> targets {0.0,-0.0,0.375,inf,-inf,nan};
    const std::array<double,6> coefficients {0.0,0.125,1.0,inf,-inf,nan};
    int variant=0;
    for (const auto target:targets) for (const auto k:coefficients)
    {
        ControllerPair pair; pair.snap(target);
        bool equal=true;
        for (int i=0;i<4;++i) equal&=pair.advance(target,k);
        check(equal,"nonfinite controller arithmetic follows the original fallback",0,variant++);
    }
}

cs80::PraWahCircuit::Profile profileFor(int index)
{
    cs80::PraWahCircuit::Profile p;
    if (index==1) p.opticalTauSeconds=p.opticalDecaySeconds=0.0;
    if (index==2) {
        p.opticalTauSeconds=1.0; p.opticalDecaySeconds=5.0;
        p.heelOhms=cs80::PraWahCircuit::maximumOpticalOhms;
        p.toeOhms=cs80::PraWahCircuit::minimumOpticalOhms;
    }
    if (index==3) p.heelOhms=p.toeOhms=100000.0;
    return p;
}

void expression(double rate,int profileIndex)
{
    ExpressionPair pair;
    const auto profile=profileFor(profileIndex);
    pair.prepare(rate,profile);
    const double inf=std::numeric_limits<double>::infinity(), nan=std::numeric_limits<double>::quiet_NaN();
    const std::array<double,12> pedals {-1.0,-0.0,0.0,std::numeric_limits<double>::denorm_min(),
        0.03,0.125,0.5,0.999,1.0,2.0,nan,inf};
    bool stationary=true,firstMove=true,moving=true,reset=true;
    for (std::size_t variant=0;variant<pedals.size();++variant)
    {
        pair.reset(pedals[variant]); reset&=pair.readouts();
        for (int i=0;i<257;++i) stationary&=pair.tick();
        pair.setPedal(variant%2 ? 0.25 : 0.75);
        firstMove&=pair.tick();
        for (int i=0;i<(int)(0.015*rate);++i) moving&=pair.tick();
    }
    pair.reset(0.625);
    for (int i=0;i<4096;++i) stationary&=pair.tick();
    // Full-rate optical movement and repeated invalid/control writes. No signal
    // threshold or engine silence condition can justify skipping these states.
    for (int stage=0;stage<4;++stage)
    {
        pair.setPedal(stage%2 ? 0.15 : 0.85);
        firstMove&=pair.tick();
        for (int i=0;i<(int)(0.125*rate);++i)
        {
            if (i%1021==0) pair.setPedal(i%2042 ? nan : inf);
            moving&=pair.tick();
        }
    }
    // Default cell reaches its existing exact snap during this interval; slower
    // valid profiles remain moving. Both must retain the baseline continuation.
    pair.setPedal(0.625);
    for (int i=0;i<(int)(2.0*rate);++i) moving&=pair.tick();
    pair.setPedal(0.4);
    firstMove&=pair.tick();
    for (int i=0;i<1024;++i) moving&=pair.tick();
    pair.prepare(rate,profile);
    reset&=pair.readouts();
    for (int i=0;i<1024;++i) reset&=pair.tick();
    check(stationary,"expression exactly stationary spans and signed pedal zeros",rate,profileIndex);
    check(firstMove,"expression first move after stationary optical state",rate,profileIndex);
    check(moving,"expression optical motion and original settling recurrence",rate,profileIndex);
    check(reset,"expression reset and prepare preserve exact continuation",rate,profileIndex);
}

void expressionNonfinite()
{
    const double inf=std::numeric_limits<double>::infinity(), nan=std::numeric_limits<double>::quiet_NaN();
    for (int variant=0;variant<7;++variant)
    {
        cs80::PraWahCircuit::Profile p;
        if (variant==0) p.opticalDecaySeconds=nan;
        if (variant==1) p.opticalTauSeconds=nan;
        if (variant==2) p.heelOhms=inf;
        if (variant==3) p.toeOhms=nan;
        if (variant==4) p.heelOhms=0.0;
        if (variant==5) p.opticalDecaySeconds=inf;
        if (variant==6) p.opticalDecaySeconds=-std::numeric_limits<double>::denorm_min();
        ExpressionPair pair; pair.prepare(96000,p);
        bool equal=pair.readouts();
        for (int i=0;i<16;++i) equal&=pair.tick();
        pair.setPedal(0.25);
        for (int i=0;i<16;++i) equal&=pair.tick();
        check(equal,"invalid expression profile preserves original arithmetic fallback",96000,variant);
    }
}
}

int main()
{
    controlZeros(); controlStationaryFinite(); controlNonfinite(); expressionNonfinite();
    for (const auto rate:rates) {
        controlMoving(rate);
        for (int profile=0;profile<4;++profile) expression(rate,profile);
    }
    std::printf("stationary controls exact: %d checks, %d failures; %llu controller and %llu expression samples\n",
        checks,failures,(unsigned long long)controllerSamples,(unsigned long long)expressionSamples);
    return failures==0 ? 0 : 1;
}
