#include "core/Engine.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
namespace {
int checks=0, failures=0;
void check(bool ok,const std::string& reason) {
    ++checks; if(!ok) { ++failures; std::cerr << "FAIL: " << reason << '\n'; }
}
void advance(cs80::Engine& e) { std::array<float,512> out{}; for(int i=0;i<4;++i) e.render(out.data(),256); }
int voice(const cs80::Engine& e,int note) {
    for(int i=0;i<16;++i) { const auto v=e.getVoiceInfo(i); if(v.active&&v.note==note) return i; } return -1;
}
struct Fixture { const char* name; int sub; bool realtime; double shift; };
std::vector<std::uint8_t> message(const Fixture& f) {
    std::vector<std::uint8_t> d{0xF0,(std::uint8_t)(f.realtime?0x7F:0x7E),0x7F,0x08,(std::uint8_t)f.sub};
    if(f.sub==0x01) {
        d.push_back(0); for(int i=0;i<16;++i) d.push_back(' ');
        for(int n=0;n<128;++n) {
            // Keep the top key in range with the standard no-change marker.
            if(n==127) d.insert(d.end(),{0x7F,0x7F,0x7F});
            else d.insert(d.end(),{(std::uint8_t)(n+1),0,0});
        }
        std::uint8_t checksum=0; for(std::size_t i=1;i<d.size();++i) checksum^=d[i];
        d.push_back(checksum&0x7F);
    } else if(f.sub==0x02||f.sub==0x07) {
        if(f.sub==0x07) d.push_back(0);
        d.insert(d.end(),{0,2,69,70,0,0,70,71,0,0});
    } else {
        d.insert(d.end(),{3,0x7F,0x7F}); // all sixteen channels
        for(int pc=0;pc<12;++pc) {
            if(f.sub==0x08) d.push_back(114); // +50 cents
            else d.insert(d.end(),{0x60,0}); // +50 cents
        }
    }
    d.push_back(0xF7); return d;
}
void caseAt(double rate,bool circuit,const Fixture& f) {
    const std::string label=std::string(f.name)+(circuit?" Circuit ":" Legacy ")+std::to_string((int)rate);
    cs80::ParamValues p;
    p[cs80::P_sourcePerformance]=circuit?1.0f:0.0f; p[cs80::P_parityMode]=0;
    p[cs80::P_tuningMode]=2;
    p[cs80::P_voiceDispersion]=p[cs80::P_drift]=p[cs80::P_analogNoise]=0;
    p[cs80::P_detune]=p[cs80::P_portamento]=p[cs80::P_initBend]=0;
    cs80::Engine e; e.prepare(rate); e.setParams(p); e.noteOn(69,0.8f); advance(e);
    const int held=voice(e,69); check(held>=0,label+" held note exists"); if(held<0)return;
    const auto identity=e.getVoiceInfo(held).assignmentSerial;
    const float before=e.getRankInfo(held,0).oscillatorHz;
    const auto d=message(f); check(e.handleSysEx(d.data(),(int)d.size()),label+" complete MTS accepted");
    advance(e);
    const double heldShift=12.0*std::log2((double)e.getRankInfo(held,0).oscillatorHz/before);
    const double wanted=f.realtime?f.shift:0.0;
    check(std::isfinite(heldShift)&&std::abs(heldShift-wanted)<=0.001,
          label+" held-note semitone shift expected "+std::to_string(wanted)+" observed "+std::to_string(heldShift));
    check(e.activeVoices()==1&&e.getVoiceInfo(held).held&&e.getVoiceInfo(held).assignmentSerial==identity,
          label+" existing voice ownership preserved");
    e.noteOn(70,0.8f); advance(e);
    const int future=voice(e,70); check(future>=0,label+" subsequent note exists"); if(future<0)return;
    const double futureShift=12.0*std::log2((double)e.getRankInfo(future,0).oscillatorHz/before)-1.0;
    check(std::isfinite(futureShift)&&std::abs(futureShift-f.shift)<=0.001,
          label+" future-note tuning expected "+std::to_string(f.shift)+" observed "+std::to_string(futureShift));
}
// Supplementary preservation controls: the live table setter is not NRT SysEx,
// and PURE CS-80 must continue to isolate both entry points from sounding pitch.
void preservationAt(double rate,bool circuit,bool pure,bool setter) {
    const std::string label=std::string(setter?"live-setter":"real-sysex")
        +(pure?" pure ":" modern ")+(circuit?"Circuit ":"Legacy ")+std::to_string((int)rate);
    cs80::ParamValues p;
    p[cs80::P_sourcePerformance]=circuit?1.0f:0.0f; p[cs80::P_parityMode]=pure?1.0f:0.0f;
    p[cs80::P_tuningMode]=2;
    p[cs80::P_voiceDispersion]=p[cs80::P_drift]=p[cs80::P_analogNoise]=0;
    p[cs80::P_detune]=p[cs80::P_portamento]=p[cs80::P_initBend]=0;
    cs80::Engine e; e.prepare(rate); e.setParams(p); e.noteOn(69,0.8f); advance(e);
    const int held=voice(e,69); check(held>=0,label+" held note exists"); if(held<0)return;
    const auto identity=e.getVoiceInfo(held).assignmentSerial;
    const float before=e.getRankInfo(held,0).oscillatorHz;
    bool accepted=true;
    if(setter) {
        cs80::tuning::TuningTable table; table.pitch[69]=70; table.pitch[70]=71;
        e.setMtsTuning(table);
    } else {
        const auto d=message({"single",0x02,true,1});
        accepted=e.handleSysEx(d.data(),(int)d.size());
    }
    check(accepted&&e.getMtsTuning().pitch[69]==70&&e.getMtsTuning().pitch[70]==71,
          label+" accepted table preserved even in pure mode");
    advance(e);
    const double wanted=pure?0.0:1.0;
    const double heldShift=12.0*std::log2((double)e.getRankInfo(held,0).oscillatorHz/before);
    check(std::isfinite(heldShift)&&std::abs(heldShift-wanted)<=0.001,label+" held tuning preserved");
    check(e.activeVoices()==1&&e.getVoiceInfo(held).held&&e.getVoiceInfo(held).assignmentSerial==identity,
          label+" existing voice ownership preserved");
    e.noteOn(70,0.8f); advance(e);
    const int future=voice(e,70); check(future>=0,label+" subsequent note exists"); if(future<0)return;
    const double futureShift=12.0*std::log2((double)e.getRankInfo(future,0).oscillatorHz/before)-1.0;
    check(std::isfinite(futureShift)&&std::abs(futureShift-wanted)<=0.001,label+" future tuning preserved");
}
}
int main() {
    const Fixture fixtures[]{
        {"single",0x02,true,1},{"bank-real",0x07,true,1},{"bank-nonreal",0x07,false,1},
        {"octave1-real",0x08,true,.5},{"octave1-nonreal",0x08,false,.5},
        {"octave2-real",0x09,true,.5},{"octave2-nonreal",0x09,false,.5},{"bulk-nonreal",0x01,false,1}};
    for(double rate:{44100.0,48000.0,96000.0}) for(bool circuit:{false,true})
        for(const auto& f:fixtures) caseAt(rate,circuit,f);
    for(double rate:{44100.0,48000.0,96000.0}) for(bool circuit:{false,true})
        for(bool pure:{false,true}) for(bool setter:{false,true}) preservationAt(rate,circuit,pure,setter);
    std::cout<<checks<<" MTS realtime semantic checks, "<<failures<<" failures\n";
    return failures==0?0:1;
}
