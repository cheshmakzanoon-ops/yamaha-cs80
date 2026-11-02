#include "plugin/AuAutomationQueue.h"
#include "Ws1Tolerances.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {
using Event = cs80::AuAutomationQueue::Event;
double scalar(const std::vector<Event>& events, int lane, int sample, double initial)
{
    const Event* latest = nullptr;
    for (const auto& event : events)
        if (event.lane == lane && event.start <= sample && (latest == nullptr || event.start >= latest->start)) latest = &event;
    if (latest == nullptr) return initial;
    if (!latest->ramp || latest->duration == 0) return latest->last;
    const double elapsed = std::clamp(double(sample - latest->start) / latest->duration, 0.0, 1.0);
    return latest->first * (1.0 - elapsed) + latest->last * elapsed;
}
}
int main(int argc, char** argv)
{
    int failures = 0;
    const auto check = [&](bool pass, const char* message) {
        if (!pass) { ++failures; if (failures <= 20) std::cerr << "FAIL: " << message << '\n'; }
    };
    try {
        const ws1::Tolerances limits(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/host-automation.json", "host_automation");
        std::vector<Event> events {
            {0,-5,10,0.1,0.9,true}, {0,10,0,0.3,0.3,false}, {0,20,40,0.3,0.7,true},
            {0,30,10,0.8,0.1,true}, {0,63,0,0.9,0.9,false}, {0,64,2000,0.9,0.1,true},
            {1,0,1024,0.2,0.8,true}, {1,1023,0,0.3,0.3,false}, {1,2047,0,0.9,0.9,false}
        };
        double maximum = 0;
        for (int block : {1,7,64,256,1024})
            for (int start = 0; start < 2048; start += block)
            {
                const int frames = std::min(block,2048-start);
                cs80::AuAutomationQueue storage;
                std::array<double, cs80::AuAutomationQueue::lanes> initial;
                initial.fill(0.2);
                for (int lane = 0; lane < 2; ++lane)
                {
                    initial[static_cast<size_t>(lane)] = scalar(events,lane,start-1,0.2);
                    const Event* running = nullptr;
                    for (const auto& event : events)
                        if (event.lane == lane && event.start < start && (running == nullptr || event.start >= running->start)) running = &event;
                    if (running != nullptr && running->ramp && running->start + running->duration >= start)
                    { auto relative=*running;relative.start-=start;check(storage.add(relative),"continued ramp accepted"); }
                }
                for (const auto& event : events)
                    if (event.start >= start && event.start < start+frames)
                    { auto relative=event;relative.start-=start;check(storage.add(relative),"current render event accepted"); }
                check(storage.prepare(initial,frames),"valid render prepares");
                for (int lane = 0; lane < 2; ++lane)
                {
                    cs80::HostAutomationCurve curve;curve.begin(storage.queue(static_cast<size_t>(lane)),initial[static_cast<size_t>(lane)]);
                    for(int sample=0;sample<frames;++sample)
                    {
                        const double error=std::abs(curve.valueAt(sample)-scalar(events,lane,start+sample,0.2));
                        maximum=std::max(maximum,error);
                        if(error>limits["normalized_curve_error"] && failures<20) std::cerr<<"block="<<block<<" start="<<start<<" sample="<<sample<<" lane="<<lane<<" actual="<<curve.valueAt(sample)<<" expected="<<scalar(events,lane,start+sample,0.2)<<"\n";
                        check(error<=limits["normalized_curve_error"],"AU immediate/ramp matches independent scalar timeline");
                    }
                    if(curve.active()) check(std::abs(curve.finalValue()-scalar(events,lane,start+frames-1,0.2))<=limits["normalized_curve_error"],"AU publishes current render endpoint");
                }
            }
        cs80::AuAutomationQueue currentState;
        std::array<double, cs80::AuAutomationQueue::lanes> current;current.fill(0.65);
        check(currentState.add({0,7,0,0.25,0.25,false}) && currentState.prepare(current,9), "current AU state plus delayed immediate event prepares");
        cs80::HostAutomationCurve currentCurve;currentCurve.begin(currentState.queue(0),0.2);
        check(std::abs(currentCurve.valueAt(0)-0.65)<=limits["normalized_curve_error"]
              && std::abs(currentCurve.valueAt(6)-0.65)<=limits["normalized_curve_error"],
              "offset-zero state replaces a stale previous-render value immediately");
        // A malformed previous-render readback must not convert a delayed
        // *immediate* AU event into a ramp. The first seven frames hold a
        // finite fallback; the exact event at sample 7 remains a hard step.
        for (double malformed : {
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity()})
        {
            cs80::AuAutomationQueue invalidInitial;
            std::array<double, cs80::AuAutomationQueue::lanes> prior;
            prior.fill(0.2); prior[0] = malformed;
            check(invalidInitial.add({0,7,0,0.25,0.25,false}) && invalidInitial.prepare(prior,9),
                  "AU delayed immediate accepts finite event despite malformed prior readback");
            cs80::HostAutomationCurve guarded;
            guarded.begin(invalidInitial.queue(0), malformed);
            bool held = true;
            for (int sample = 0; sample < 7; ++sample)
                held = held && guarded.valueAt(sample) == 0.0;
            check(held && guarded.valueAt(7) == 0.25 && guarded.valueAt(8) == 0.25,
                  "malformed AU initial cannot move an immediate event before sample 7");
            check(guarded.finalValue() == 0.25,
                  "AU malformed-initial recovery retains the host endpoint");
        }
        cs80::AuAutomationQueue bounded;
        for(size_t i=0;i<bounded.capacity;++i) check(bounded.add({0,0,0,0.4,0.4,false}),"full documented scheduling capacity accepted");
        check(!bounded.add({0,0,0,0.5,0.5,false}),"overflow reported without accepting partial event");
        std::array<double, cs80::AuAutomationQueue::lanes> initial;initial.fill(0.2);
        check(bounded.prepare(initial,1),"full-capacity queue prepares");
        cs80::HostAutomationCurve curve;curve.begin(bounded.queue(0),0.2);
        check(curve.valueAt(0)==0.4,"capacity error preserved accepted last value");
        check(!bounded.add({0,0,-1,0.2,0.3,true}),"negative duration rejected");
        check(!bounded.add({0,-1,0,0.2,0.3,false}),"negative immediate offset rejected");
        check(!bounded.add({0,0,1,std::numeric_limits<double>::quiet_NaN(),0.3,true}),"nonfinite event rejected");
        check(!bounded.add({0,std::numeric_limits<int>::max(),1,0.2,0.3,true}),"time overflow rejected");
        check(bounded.add({0,7,0,0.2,0.3,false}),"future event retained for render validation");
        check(!bounded.prepare(initial,7),"outside-render event rejected instead of silently dropped");
        std::cout<<"AU normalized curve maximum error="<<maximum<<'\n';
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
    return failures==0?0:1;
}
