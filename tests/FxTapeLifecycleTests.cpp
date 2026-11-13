#include "core/FxSlot.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr int rate = 48000;
constexpr double pi = 3.14159265358979323846;
constexpr double minimumTail = 1.0e-5;
int checks = 0, failures = 0;
void check(bool ok, const char* label, int model, int path)
{
    ++checks; if (!ok) ++failures;
    std::printf("%s model=%d path=%d %s\n", ok ? "PASS" : "FAIL", model, path, label);
}
cs80::fx::SlotControls controls(bool modern)
{
    cs80::fx::SlotControls c;
    c.type = modern ? 0 : 3; c.model = modern ? 4 : 0;
    c.mix = 0.75f; c.a = c.b = 0.5f; c.gainComp = false;
    return c;
}
cs80::fx::StereoSample tone(int frame)
{
    return {static_cast<float>(0.1 * std::sin(2.0*pi*733.0*frame/rate)),
            static_cast<float>(0.07 * std::cos(2.0*pi*997.0*frame/rate))};
}
bool finite(cs80::fx::StereoSample s) { return std::isfinite(s.l) && std::isfinite(s.r); }
double peak(cs80::fx::StereoSample s) { return std::max(std::abs(double(s.l)), std::abs(double(s.r))); }
void run(bool modern)
{
    const auto active = controls(modern);
    cs80::fx::FxSlot warmed; warmed.prepare(rate);
    // 100 ms input burst followed by 100 ms silence, shorter than either tap.
    for (int i=0; i<rate/5; ++i) {
        const auto x = i<rate/10 ? tone(i) : cs80::fx::StereoSample{};
        warmed.process(active,x.l,x.r,0.0f,rate);
    }
    auto uninterrupted = warmed;
    double oldTail = 0.0;
    bool tailFinite = true;
    for (int i=0; i<7*rate/10; ++i) {
        const auto y=uninterrupted.process(active,0,0,0.0f,rate);
        tailFinite = tailFinite && finite(y); oldTail=std::max(oldTail,peak(y));
    }
    check(tailFinite && oldTail>minimumTail,"uninterrupted old-history tail witness",active.model,-1);
    for (int path=0;path<3;++path) {
        auto actual=warmed;
        auto transition=active;
        if(path==0) transition.mix=0.0f;
        if(path==1) transition.mix=0.001f;
        if(path==2) transition=controls(!modern);
        bool transitionFinite=true, dryExact=true;
        // Ordinary input while bypassed must not be buffered for later playback.
        // On the identity path, it instead excites the other tape for 50 ms.
        for(int i=0;i<rate/20;++i) {
            const auto x=tone(i+137);
            const auto y=actual.process(transition,x.l,x.r,0.0f,rate);
            transitionFinite=transitionFinite&&finite(y);
            dryExact=dryExact&&(y.l==x.l&&y.r==x.r);
        }
        check(transitionFinite,"transition output finite",active.model,path);
        if(path<2) check(dryExact,"bypass exactly dry",active.model,path);
        cs80::fx::FxSlot fresh; fresh.prepare(rate);
        bool equal=true, allFinite=true;
        double newTail=0.0;
        // Reactivate: 100 ms silence, 100 ms new burst, then 800 ms silence.
        for(int i=0;i<rate;++i) {
            const auto x=(i>=rate/10&&i<rate/5)?tone(i+283):cs80::fx::StereoSample{};
            const auto a=actual.process(active,x.l,x.r,0.0f,rate);
            const auto b=fresh.process(active,x.l,x.r,0.0f,rate);
            equal=equal&&(a.l==b.l&&a.r==b.r);
            allFinite=allFinite&&finite(a)&&finite(b);
            if(i>=rate/5) newTail=std::max(newTail,peak(b));
        }
        check(allFinite&&equal,"reactivation exactly matches fresh silence and new probe",active.model,path);
        check(newTail>minimumTail,"fresh new-probe delayed tail witness",active.model,path);
        std::printf("METRIC model=%d path=%d old_tail_peak=%.17g new_tail_peak=%.17g\n",active.model,path,oldTail,newTail);
    }
}
}
int main()
{
    run(false); run(true);
    std::printf("RESULT checks=%d failures=%d\n",checks,failures);
    return failures?1:0;
}
