#include "core/Engine.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace {
constexpr int hostRate=48000, factor=2, totalFrames=28800;
constexpr int frameTolerance=2;
struct Event { int frame, note; };
struct Case { const char* name; float before, after; int editFrame; bool overdue; };
constexpr std::array<Case,3> cases {{{"decrease",16,8,1000,false},
    {"early-increase",8,16,1000,false},{"overdue-increase",8,16,4000,true}}};
int checks=0, failures=0;
void check(bool ok,const char* what,const char* name)
{ ++checks; if(!ok)++failures; std::printf("%s %s: %s\n",ok?"PASS":"FAIL",name,what); }

// Small scheduling oracle, not an audio model. Public policy has float clocks;
// retain float accumulation so comparison does not confuse long-interval roundoff
// with a rate-edit regression. No Engine helpers or private state are used.
std::vector<Event> expected(const Case& c)
{
    constexpr float dt=1.0f/(hostRate*factor);
    constexpr std::array<int,3> chord {60,64,67};
    float elapsed=1.0f/c.before, gateRemaining=0;
    int sequence=0, held=-1, previous=-1;
    std::vector<Event> events;
    for(int frame=0;frame<totalFrames;++frame) {
        const float interval=1.0f/(frame<c.editFrame?c.before:c.after);
        for(int sub=0;sub<factor;++sub) {
            elapsed+=dt;
            if(held>=0) {
                gateRemaining-=dt;
                if(gateRemaining<=0)held=-1;
            }
            if(elapsed>=interval) {
                // Keep elapsed phase, dropping complete skipped intervals; a
                // rate edit does not rewind chord order or issue multiple notes.
                elapsed-=interval;
                if(elapsed>=interval)elapsed=std::fmod(elapsed,interval);
                held=chord[sequence++%3]; gateRemaining=.58f*interval;
            }
        }
        if(held!=previous) { events.push_back({frame,held}); previous=held; }
    }
    return events;
}
void run(const Case& c)
{
    auto e=std::make_unique<cs80::Engine>();
    cs80::ParamValues p;
    p[cs80::P_parityMode]=0; p[cs80::P_sourcePerformance]=0;
    p[cs80::P_kasScanTiming]=0; p[cs80::P_oversampling]=0;
    p[cs80::P_voiceDispersion]=p[cs80::P_drift]=p[cs80::P_analogNoise]=0;
    p[cs80::P_arpOn]=1; p[cs80::P_arpMode]=0; p[cs80::P_arpOctaves]=1;
    p[cs80::P_arpRate]=c.before;
    e->prepare(hostRate); e->setParams(p);
    check(e->getOversamplingFactor()==factor&&e->getInternalSampleRate()==96000,
          "fixed Legacy 2x internal clock",c.name);
    for(int note:{60,64,67})e->noteOn(note,.7f,1);
    std::vector<Event> actual;
    int previous=-1;
    bool finite=true,single=true;
    for(int frame=0;frame<totalFrames;++frame) {
        if(frame==c.editFrame) {p[cs80::P_arpRate]=c.after;e->setParams(p);}
        float out[2]{};e->render(out,1);
        finite=finite&&std::isfinite(out[0])&&std::isfinite(out[1]);
        int held=-1,count=0;
        for(int voice=0;voice<8;++voice) {
            const auto v=e->getVoiceInfo(voice);
            if(v.active&&v.held) {held=v.note;++count;}
        }
        single=single&&count<=1;
        if(held!=previous) {actual.push_back({frame,held});previous=held;}
    }
    const auto reference=expected(c);
    bool same=actual.size()==reference.size();int maxError=0;
    if(same)for(size_t i=0;i<actual.size();++i) {
        maxError=std::max(maxError,std::abs(actual[i].frame-reference[i].frame));
        same=same&&actual[i].note==reference[i].note;
    }
    check(finite&&single,"finite audio and at most one held generated note",c.name);
    check(same&&maxError<=frameTolerance,"complete held-note transition sequence and clock within two host frames",c.name);
    bool editObserved=false;
    for(const auto& event:actual)
        if(std::abs(event.frame-c.editFrame)<=frameTolerance&&event.note==64)editObserved=true;
    check(editObserved==c.overdue,"only overdue edit schedules the next note immediately",c.name);
    check(!actual.empty()&&actual.front().note==60&&actual.front().frame<=frameTolerance,
          "initial chord begins immediately at its lowest note",c.name);
    for(const auto& event:actual)std::printf("EVENT %s frame=%d note=%d\n",c.name,event.frame,event.note);
    std::printf("METRIC %s actual_events=%zu expected_events=%zu max_frame_error=%d\n",c.name,actual.size(),reference.size(),maxError);
}
}
int main(){for(const auto& c:cases)run(c);std::printf("RESULT checks=%d failures=%d\n",checks,failures);return failures?1:0;}
