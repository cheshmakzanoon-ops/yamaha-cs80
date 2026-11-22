#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "tools/TruthInputSha256.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cs80;
namespace {
constexpr int rate = 48000, holdFrames = 6 * rate, totalFrames = 9 * rate;
constexpr double maximumPeriodErrorSeconds = .0005;
constexpr double maximumGateFractionError = .002;
int checks = 0, failures = 0;
void check(bool ok, const std::string& what) {
    ++checks; if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << what << '\n';
}
double db(double v) { return 20 * std::log10(std::max(v, 1e-15)); }
const FactoryPreset& preset(const std::string& name) {
    const auto& bank = curatedLibrary();
    auto it = std::find_if(bank.begin(), bank.end(), [&](const auto& p) { return p.name == name; });
    if (it == bank.end()) throw std::runtime_error("missing factory preset " + name);
    return *it;
}
struct Spec { const char* name; int root, mode, octaves; float hz; };
constexpr std::array<Spec, 4> specs {{{"Arp Poly",48,0,2,8}, {"Arp Bells",60,2,2,4},
    {"Arp Bass",48,0,1,6}, {"Random Arp",48,3,2,6}}};
struct Held { int count=0, note=-1, slot=-1, channel=-1; float velocity=0; bool pedal=false; };
Held observe(const Engine& e) {
    Held h;
    for (int v=0; v<8; ++v) {
        auto info=e.getVoiceInfo(v); h.pedal = h.pedal || info.pedalHeld;
        if (info.active && info.held) { ++h.count; h.note=info.note; h.slot=v; h.channel=info.channel; h.velocity=info.velocity; }
    }
    return h;
}
struct Event { int frame, note, slot, channel; float velocity; };
}

int main(int argc, char** argv) { try {
    if (argc != 2) throw std::runtime_error("expected existing output directory");
    const std::string out=argv[1];
    std::ofstream transitions(out+"/transitions.csv"), results(out+"/results.csv");
    transitions << std::setprecision(17) << "program,frame,seconds,note,slot,channel,velocity\n";
    results << std::setprecision(17) << "program,mode,octaves,authored_hz,factor,onsets,unique_notes,first_onset_frame,period_min_seconds,period_max_seconds,mean_period_seconds,observed_hz,max_period_error_seconds,gate_fraction_min,gate_fraction_max,max_gate_fraction_error,peak_dbfs,nonfinite,samples_over_full_scale,max_active_voices,final_active_voices,retired_seconds,final_half_second_rms_dbfs,audio_sha256,checks,failures\n";
    for (const auto& s:specs) {
        int initialChecks=checks, initialFailures=failures;
        const auto& p=preset(s.name);
        auto e=std::make_unique<Engine>(); e->prepare(rate); e->setParams(p.values);
        const std::string name=s.name;
        check(e->getParams().v == p.values.v, name+": complete factory values preserved");
        check(e->getRenderPolicy()==RenderPolicy::sourcePerformance && e->isArpeggiatorEnabled(), name+": ordinary Circuit with active arp");
        check(p.values[P_arpMode]==s.mode && p.values[P_arpOctaves]==s.octaves && p.values[P_arpRate]==s.hz, name+": authored mode/rate/octave contract");
        const std::array<int,3> chord{s.root,s.root+4,s.root+7};
        const std::array<float,3> velocities{.45f,.7f,.9f};
        // Intentionally enter a non-pitch order. All arrive before audio starts.
        for (int index:{2,0,1}) e->noteOn(chord[index],velocities[index],1);
        std::vector<Event> events;
        std::vector<float> audio(static_cast<size_t>(totalFrames)*2);
        Held previous;
        bool single=true, pedalClear=true, sourceVelocity=true, stopped=true, releaseImmediate=false;
        long nonfinite=0, over=0; int maxActive=0; double peak=0, tailEnergy=0, retired=-1;
        for (int frame=0; frame<totalFrames; ++frame) {
            if (frame==int(4.5*rate)) e->controlChange(64,1.0f,1);
            if (frame==holdFrames) {
                for (int note:chord) e->noteOff(note,1);
                releaseImmediate=observe(*e).count==0;
            }
            if (frame==int(7.5*rate)) e->controlChange(64,0.0f,1);
            auto* samples=audio.data()+2*frame;
            e->render(samples,1);
            const auto h=observe(*e);
            single=single && h.count<=1; pedalClear=pedalClear && !h.pedal;
            if (frame>=holdFrames) stopped=stopped && h.count==0;
            if (h.count) {
                int pitchClass=(h.note-s.root)%12;
                int index=pitchClass==0?0:pitchClass==4?1:pitchClass==7?2:-1;
                sourceVelocity=sourceVelocity && index>=0 && h.channel==1
                    && (index<0 || std::abs(h.velocity-velocities[index])<1e-7f);
            }
            if (h.note!=previous.note || h.slot!=previous.slot) {
                events.push_back({frame,h.note,h.slot,h.channel,h.velocity});
                transitions << name << ',' << frame << ',' << double(frame)/rate << ',' << h.note << ',' << h.slot << ',' << h.channel << ',' << h.velocity << '\n';
            }
            previous=h;
            int active=e->activeVoices(); maxActive=std::max(maxActive,active);
            if (frame>=holdFrames && active==0 && retired<0) retired=double(frame+1)/rate;
            for (int c=0;c<2;++c) {
                double value=samples[c]; nonfinite+=!std::isfinite(value); over+=std::abs(value)>1.0;
                peak=std::max(peak,std::abs(value));
                if (frame>=totalFrames-rate/2) tailEnergy+=value*value;
            }
        }
        std::vector<Event> onsets;
        std::vector<double> gates, periods;
        for (size_t i=0;i<events.size();++i) if (events[i].note>=0) {
            onsets.push_back(events[i]);
            if (i+1<events.size() && events[i+1].note==-1 && events[i+1].frame<holdFrames)
                gates.push_back(double(events[i+1].frame-events[i].frame)/rate);
        }
        for (size_t i=1;i<onsets.size();++i) periods.push_back(double(onsets[i].frame-onsets[i-1].frame)/rate);
        check(!onsets.empty() && onsets.front().frame<=1,name+": first generated gate begins within one host frame");
        check(std::abs(int(onsets.size())-int(6*s.hz))<=1,name+": six-second onset count is consistent with rate and initial immediate note");
        check(single,name+": at most one generated held gate at a time");
        check(sourceVelocity,name+": source pitch-class velocity and MIDI channel retained");
        check(pedalClear,name+": generated gates do not become pedal-held tails");
        check(releaseImmediate && stopped,name+": all source releases stop generation even with pedal down; pedal-up does not revive it");
        std::vector<int> sequence;
        for (int oct=0;oct<s.octaves;++oct) for(int n:chord) sequence.push_back(n+12*oct);
        std::set<int> allowed(sequence.begin(),sequence.end()), observed;
        bool members=true;
        for(const auto& event:onsets){members=members && allowed.count(event.note);observed.insert(event.note);}
        check(members,name+": every generated note belongs to the authored chord/octave range");
        if (s.mode==2) for(int i=int(sequence.size())-2;i>0;--i) sequence.push_back(sequence[static_cast<size_t>(i)]);
        bool ordered=true;
        if(s.mode!=3) for(size_t i=0;i<onsets.size();++i) ordered=ordered && onsets[i].note==sequence[i%sequence.size()];
        if(s.mode==3) {
            bool differsFromAscending=false;
            for(size_t i=0;i<onsets.size();++i) differsFromAscending |= onsets[i].note!=sequence[i%sequence.size()];
            check(observed.size()>=4 && differsFromAscending,name+": random ordering varies over at least four valid pitches");
        } else check(ordered,name+": exact ascending/up-down cycle matches authored mode");
        double meanPeriod=0, periodError=0, gateError=0;
        for(double duration:periods){meanPeriod+=duration;periodError=std::max(periodError,std::abs(duration-1.0/s.hz));}
        meanPeriod/=std::max<size_t>(1,periods.size());
        const double nominalGate=s.mode==3?.45:.58;
        for(double gate:gates) gateError=std::max(gateError,std::abs(gate*s.hz-nominalGate));
        check(periods.size()>=20 && periodError<=maximumPeriodErrorSeconds,name+": note intervals remain within0.5ms of authored free-running rate");
        check(gates.size()>=20 && gateError<=maximumGateFractionError,name+": gate fraction matches58% or random45% within0.2percentage points");
        check(nonfinite==0 && over==0,name+": finite audio below full scale");
        check(retired>=6 && retired<=9 && e->activeVoices()==0,name+": all active voices retire during the three-second release");
        check(e->getParams().v==p.values.v,name+": performance events leave factory parameters intact");
        const std::string audioHash=cs80::tools::inputSha256(std::string_view(reinterpret_cast<const char*>(audio.data()),audio.size()*sizeof(float)));
        results << name << ',' << s.mode << ',' << s.octaves << ',' << s.hz << ',' << e->getOversamplingFactor() << ',' << onsets.size() << ',' << observed.size() << ',' << (onsets.empty()?-1:onsets.front().frame) << ','
                << *std::min_element(periods.begin(),periods.end()) << ',' << *std::max_element(periods.begin(),periods.end()) << ',' << meanPeriod << ',' << 1/meanPeriod << ',' << periodError << ','
                << *std::min_element(gates.begin(),gates.end())*s.hz << ',' << *std::max_element(gates.begin(),gates.end())*s.hz << ',' << gateError << ','
                << db(peak) << ',' << nonfinite << ',' << over << ',' << maxActive << ',' << e->activeVoices() << ',' << retired << ',' << db(std::sqrt(tailEnergy/rate)) << ',' << audioHash << ','
                << checks-initialChecks << ',' << failures-initialFailures << '\n';
        results.flush();transitions.flush();
    }
    std::cout << "Factory arpeggiator roles: " << checks-failures << '/' << checks << " checks, 4 captures / 36 seconds.\n";
    return failures?1:0;
} catch(const std::exception& e) { std::cerr << "ERROR " << e.what() << '\n';return 2; } }
