// Independent JUCE-free integration witness and whole-core timing harness.
// Compile every variant with its own headers and Engine.cpp; no mixed layouts.
#include "core/Engine.h"
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <type_traits>
#include <vector>

using namespace cs80;
using namespace cs80::modern;
struct Scenario { const char* name; int features; int voices; int policy; int quality; double rate; bool automate; };
constexpr std::array<Scenario, 8> scenarios {{
    {"held-44100",0,8,1,1,44100,false},
    {"pitch-automated-44100",0,8,1,1,44100,true},
    {"held-48000",0,8,1,1,48000,false},
    {"pitch-automated-48000",0,8,1,1,48000,true},
    {"held-96000",0,8,1,1,96000,false},
    {"pitch-automated-96000",0,8,1,1,96000,true},
    {"held-192000",0,8,1,1,192000,false},
    {"pitch-automated-192000",0,8,1,1,192000,true}
}};

struct Writer {
    FILE* file = nullptr; std::uint64_t hash = 1469598103934665603ull, values = 0; bool valid = true;
    template<class T> void put(T value) {
        if constexpr(std::is_floating_point_v<T>) valid = std::isfinite(value) && valid;
        const auto bytes = std::bit_cast<std::array<unsigned char, sizeof(T)>>(value);
        for(auto byte : bytes) hash = (hash ^ byte) * 1099511628211ull;
        if(file) valid = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size() && valid;
        ++values;
    }
};

void configure(Engine& engine, ParamValues& p, const Scenario& s) {
    engine.prepare(s.rate);
    p[P_sourcePerformance] = s.policy == 0 ? 0.0f : 1.0f;
    p[P_parityMode] = s.policy == 2 ? 1.0f : 0.0f;
    p[P_oversampling] = (float)s.quality;
    p[P_fx1Model] = (s.features & 1) ? 5.0f : 0.0f;
    p[P_fx1Mix] = (s.features & 1) ? 0.7f : 0.0f;
    p[P_fx1ParamA] = 0.63f; p[P_fx1ParamB] = 0.71f;
    p[P_fx2Model] = (s.features & 4) ? 4.0f : 0.0f;
    p[P_fx2Mix] = (s.features & 4) ? 0.6f : 0.0f;
    p[P_fx2ParamA] = 0.32f; p[P_fx2ParamB] = 0.68f;
    if(s.features & 2) {
        p[P_subOriginalMode]=1.0f; p[P_subOriginalFunction]=5.0f; p[P_subExtLevel]=0.6f;
        p[P_subVco]=0.1f; p[P_subVca]=0.15f; p[P_subVcf]=0.0f;
    }
    ModernConfig cfg;
    if(s.features & 8) {
        p[P_lfo1Rate]=7.3f; p[P_lfo2Rate]=11.1f; p[P_lfo1Wave]=1.0f; p[P_lfo2Wave]=2.0f;
        p[P_mod1Amount]=0.8f; p[P_mod2Amount]=-0.7f;
        cfg.slots[0] = {ModSource::lfo1,ModSource::none,ModDestination::hpResonance,ModTarget::both};
        cfg.slots[1] = {ModSource::lfo2,ModSource::none,ModDestination::lpResonance,ModTarget::both};
        for(int rank=0;rank<2;++rank) { p[chParam(rank,CP_fIl)]=0; p[chParam(rank,CP_fAl)]=0; }
    }
    engine.setModernConfig(cfg); engine.setParams(p);
    constexpr int notes[]{36,43,48,55,60,64,67,72};
    for(int n=0;n<s.voices;++n) engine.noteOn(notes[n],0.65f+(float)n*0.025f);
}

void snapshot(Writer& w, const Engine& e) {
    for(auto p:e.getParams().v) w.put(p);
    w.put(e.getOversamplingFactor()); w.put(e.getLatencySamples());
    w.put((int)e.getRenderPolicy()); w.put(e.activeVoices());
    for(int v=0;v<8;++v) {
        const auto vi=e.getVoiceInfo(v);
        w.put(vi.active);w.put(vi.note);w.put(vi.channel);w.put(vi.velocity);w.put(vi.pressure);
        w.put(vi.held);w.put(vi.pedalHeld);
        for(int r=0;r<2;++r) {
            const auto a=e.getRankAudioState(v,r);
            w.put(a.phase);w.put(a.hpf.ic1);w.put(a.hpf.ic2);w.put(a.lpf.ic1);w.put(a.lpf.ic2);
            for(const auto& f:{a.hpCircuit,a.lpCircuit}) {w.put(f.input);w.put(f.feedback);w.put(f.bp);w.put(f.lp);}
            const auto en=e.getRankEnvelopeState(v,r);
            w.put((int)en.amplitude.stage);w.put(en.amplitude.value);w.put(en.amplitude.elapsed);
            w.put((int)en.filter.stage);w.put(en.filter.value);w.put(en.filter.elapsed);
            const auto& o=e.getRankOscillatorState(v,r);
            w.put(o.charge);w.put(o.resetRemainingSeconds);w.put(o.inputBaselineVolts);w.put(o.triangleBaselineVolts);
            w.put(o.heldFrequencyHz);w.put(o.heldThresholdVolts);w.put(o.elapsedSeconds);
            w.put(o.sawEnabled);w.put(o.pulseEnabled);w.put(o.residualCursor);w.put(o.resetCount);
            for(const auto& channel:o.residual)for(double value:channel)w.put(value);
        }
    }
}

int main(int argc,char** argv) {
    if(argc<3)return 2;
    const bool witness=std::string(argv[1])=="witness";
    if(witness) {
        FILE* file=std::fopen(argv[2],"wb");if(!file)return 3;
        Writer w{file}; std::uint64_t frames=0; int cases=0;
        // Both modern effect orders, lifecycle transitions, held and stepped
        // parameters, full tape-buffer duration, connected/reconnected EXT.
        for(int order=0;order<2;++order) {
            Scenario s={"witness",15,8,1,1,48000,true}; Engine e;ParamValues p;configure(e,p,s);
            if(order) {std::swap(p[P_fx1Model],p[P_fx2Model]);e.setParams(p);}
            std::array<float,257> input{};std::array<float,514> output{};
            double energy=0; int offset=0;
            for(int block=0;block<1152;++block) {
                const int count=std::array<int,6>{1,17,64,113,256,257}[block%6];
                for(int n=0;n<count;++n) input[n]=(float)(0.6*std::sin(0.017*(offset+n)));
                if(block==256) {p[P_fx1ParamA]=0.21f;p[P_fx2ParamA]=0.52f;e.setParams(p);}
                if(block==384) {p[P_fx1ParamB]=0.33f;p[P_fx2ParamB]=0.81f;e.setParams(p);}
                if(block==512) {e.allNotesOff(true);}
                if(block==640) {e.noteOn(62,0.8f);e.noteOn(69,0.6f);}
                if(block==704) {p[P_oversampling]=2;e.setParams(p);e.noteOn(64,0.7f);}
                if(block==736) {p[P_oversampling]=0;e.setParams(p);e.noteOn(60,0.7f);}
                if(block==800) {e.reset();e.noteOn(55,0.75f);}
                if(block==864) {e.prepare(44100);e.noteOn(60,0.8f);}
                if(block==928) {p[P_fx1Mix]=0;p[P_fx2Mix]=0;e.setParams(p);}
                if(block==960) {p[P_fx1Mix]=0.7f;p[P_fx2Mix]=0.6f;e.setParams(p);}
                if(block==1024) {p[P_parityMode]=1;e.setParams(p);e.noteOn(60,0.8f);}
                if(block==1088) {p[P_parityMode]=0;e.setParams(p);e.noteOn(60,0.8f);}
                e.render(output.data(),count,(block>=400&&block<432)?nullptr:input.data());
                for(int n=0;n<count*2;++n){w.put(output[n]);energy+=(double)output[n]*output[n];}
                if(block%16==0)snapshot(w,e);
                frames+=count;offset+=count;
            }
            w.valid=energy>1e-10&&w.valid;++cases;
        }
        w.valid=std::fclose(file)==0&&w.valid;
        std::printf("cases=%d frames=%llu values=%llu hash=%016llx valid=%d\n",cases,(unsigned long long)frames,(unsigned long long)w.values,(unsigned long long)w.hash,w.valid);
        return w.valid?0:1;
    }
    const int index=std::atoi(argv[2]);if(index<0||index>=(int)scenarios.size())return 4;
    const auto s=scenarios[index];Engine e;ParamValues p;configure(e,p,s);
    constexpr int warmup=16384, total=65536, blockSize=256;
    std::vector<float> input(total+warmup),output(2*(total+warmup));
    for(int n=0;n<total+warmup;++n)input[n]=(float)(0.6*std::sin(0.017*n));
    for(int n=0;n<warmup;n+=blockSize)e.render(output.data()+2*n,blockSize,(s.features&2)?input.data()+n:nullptr);
    const auto start=std::chrono::steady_clock::now();const auto cpuStart=std::clock();
    for(int n=warmup;n<total+warmup;n+=blockSize) {
        if(s.automate) {e.pitchBend(1, 0.9f * std::sin(float(n) * 0.00017f)); p[P_masterTune] = 0.3f * std::sin(float(n) * 0.00023f); p[P_tuningRef] = ((n / blockSize) % 3) == 0 ? 432.0f : 444.0f; const float t=(float)((n/blockSize)%127)/126.0f;p[P_fx1ParamA]=0.1f+0.8f*t;p[P_fx1ParamB]=0.9f-0.7f*t;p[P_fx2ParamA]=0.15f+0.65f*t;p[P_fx2ParamB]=0.7f-0.4f*t;e.setParams(p);}
        e.render(output.data()+2*n,blockSize,(s.features&2)?input.data()+n:nullptr);
    }
    const double cpu=(double)(std::clock()-cpuStart)/CLOCKS_PER_SEC;
    const double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    Writer w;double energy=0;for(float sample:output){w.put(sample);energy+=(double)sample*sample;}snapshot(w,e);
    const bool valid=w.valid&&(s.voices==0||energy>1e-10);
    std::printf("{\"name\":\"%s\",\"frames\":%d,\"rate\":%.0f,\"cpu\":%.9f,\"wall\":%.9f,\"realtime_cpu\":%.9f,\"hash\":\"%016llx\",\"valid\":%s}\n",s.name,total,s.rate,cpu,wall,cpu/(total/s.rate),(unsigned long long)w.hash,valid?"true":"false");
    return valid?0:1;
}
