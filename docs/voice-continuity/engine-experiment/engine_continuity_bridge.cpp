#include "BenchmarkBridge.h"
#include "core/Engine.h"
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>
#ifndef CS80_BENCH_API
#error Compile with a distinct CS80_BENCH_API and cs80 namespace per immutable tree
#endif
namespace {
using namespace cs80;
struct Instance
{
    Engine engine;
    ControlFrame frame;
    BenchConfig config;
    explicit Instance(BenchConfig c): config(c)
    {
        auto& p = frame.effectiveValues;
        p[P_sourcePerformance]=1;
        frame.hardwareSources = c.scene==0 ? std::array<int,2>{1,3}
                              : c.scene==1 ? std::array<int,2>{1,13} : std::array<int,2>{13,13};
        p[P_analogNoise]=p[P_drift]=p[P_voiceDispersion]=p[P_detune]=0;
        p[P_keyRelease]=p[P_masterSustain]=0;
        p[P_subOriginalFunction]=5; p[P_subExtLevel]=c.moving ? .5f : 0;
        p[P_subVco]=c.moving ? .2f : 0; p[P_subVcf]=c.moving ? .13f : 0;
        for(int r=0;r<2;++r)
        {
            p[chParam(r,CP_noise)]=0; p[chParam(r,CP_saw)]=1;
            p[chParam(r,CP_hpfOn)]=p[chParam(r,CP_lpfOn)]=1;
            p[chParam(r,CP_vA)]=p[chParam(r,CP_vD)]=p[chParam(r,CP_vR)]=.001f;
            p[chParam(r,CP_vS)]=.7f; p[chParam(r,CP_fR)]=2;
        }
        engine.prepare(c.rate);
        if(!engine.setControlFrame(frame)) throw std::runtime_error("initial frame rejected");
#if defined(CS80_BENCH_CANDIDATE)
        experimental::ExperimentalVoiceContinuityProfile profile; profile.enabled=true;
        if(!engine.setExperimentalVoiceContinuity(profile)) throw std::runtime_error("profile rejected");
#endif
        const int onset=c.retired ? 8 : c.voices;
        for(int v=0;v<onset;++v) engine.noteOn(45+v*4,.65f,1+v);
        if(c.retired)
        {
            std::vector<float> buffer(512);
            for(int b=0;b<16;++b) engine.render(buffer.data(),256);
            for(int v=0;v<8;++v) engine.noteOff(45+v*4,1+v);
            for(int b=0;b<1024 && engine.activeVoices();++b) engine.render(buffer.data(),256);
            if(engine.activeVoices()!=0) throw std::runtime_error("retirement precondition failed");
        }
    }
    void callback(int blockIndex,float* audio,const float* external)
    {
        // Timed in BOTH arms, including parameter validation/compilation lookup.
        // No source-selection changes; no benchmark instrumentation in callback.
        if(config.moving)
        {
            const float wave=std::sin((float)blockIndex*.071f);
            auto& p=frame.effectiveValues;
            p[P_masterTune]=wave*.7f; p[P_brilliance]=wave*.5f;
            p[P_resonance]=.4f+.2f*wave;
            p[P_macro1]=.5f+.15f*wave;
            for(int r=0;r<2;++r)
            {
                p[chParam(r,CP_pw)]=.5f+.2f*wave;
                p[chParam(r,CP_hpfCutoff)]=50+20*wave;
                p[chParam(r,CP_lpfCutoff)]=4000+1200*wave;
            }
            if(!engine.setControlFrame(frame)) throw std::runtime_error("moving frame rejected");
        }
        engine.render(audio,config.block,external);
    }
    BenchInspection inspect() const
    {
        BenchInspection result; result.active=engine.activeVoices();
#if defined(CS80_BENCH_CANDIDATE)
        result.effective=engine.isExperimentalVoiceContinuityActive();
        result.rejected=engine.getExperimentalRejectedSamples();
        result.samples=engine.getExperimentalVoiceCircuitState(0).samples;
#endif
        for(int v=0;v<8;++v) for(int r=0;r<2;++r)
        {
            const auto s=engine.getRankAudioState(v,r);
            const auto e=engine.getRankEnvelopeState(v,r);
            for(float x:{s.phase,s.hpf.ic1,s.hpf.ic2,s.lpf.ic1,s.lpf.ic2,e.filter.value})
            { result.finite=result.finite&&std::isfinite(x); result.stateChecksum+=(1+v*2+r)*x; }
            result.filterEnvelopeChecksum+=e.filter.value;
        }
        return result;
    }
};
void* create(BenchConfig c){return new Instance(c);}
void destroy(void* p){delete static_cast<Instance*>(p);}
void callback(void* p,int b,float* a,const float* x){static_cast<Instance*>(p)->callback(b,a,x);}
BenchInspection inspect(void* p){return static_cast<Instance*>(p)->inspect();}
}
BenchApi CS80_BENCH_API(){return {create,destroy,callback,inspect};}
