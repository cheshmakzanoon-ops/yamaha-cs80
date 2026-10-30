#include "BenchBridge.h"
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>
#ifndef CS80_OFF_API
#error Compile separately against immutable baseline/candidate trees and distinct cs80 namespaces
#endif
namespace {
using namespace cs80;
struct Instance {
    Engine engine;
    ControlFrame frame;
    OffConfig config;
    OffInput input {};
    explicit Instance(OffConfig c) : config(c) {
        auto& p=frame.effectiveValues;
        p[P_parityMode]=(c.policy==1 || c.policy==4) ? 1.0f : 0.0f;
        p[P_sourcePerformance]=c.policy==2 ? 1.0f : 0.0f;
        frame.hardwareSources=c.scene==0 ? std::array<int,2>{1,3} : std::array<int,2>{13,13};
        p[P_volume]=.12f; p[P_mix]=.5f;
        p[P_analogNoise]=p[P_voiceDispersion]=p[P_drift]=p[P_detune]=0;
        p[P_keyRelease]=p[P_masterSustain]=p[P_sustainMode]=0;
        p[P_chorusOn]=p[P_ringAmount]=0;
        p[P_fx1Type]=p[P_fx2Type]=p[P_fx3Type]=0;
        p[P_fx1Mix]=p[P_fx2Mix]=p[P_fx3Mix]=0;
        p[P_lfo1Depth]=p[P_lfo2Depth]=p[P_lfo3Depth]=0;
        p[P_mpeOn]=p[P_arpOn]=0;
        p[P_subOriginalMode]=1; p[P_subOriginalFunction]=5;
        p[P_subExtLevel]=c.moving ? .35f : 0;
        p[P_subVco]=c.moving ? .07f : 0;
        p[P_subVcf]=c.moving ? .10f : 0;
        p[P_subVca]=0; p[P_footMode]=c.moving ? 1.0f : 0.0f;
        for(int rank=0;rank<2;++rank) {
            if(c.scene==0) {
                ParamValues tone; applyHardwareTone(tone,rank,frame.hardwareSources[rank]);
                for(int local=0;local<CP_count;++local)
                    if(local!=CP_feet && local!=CP_pan && local!=CP_hpfOn && local!=CP_lpfOn)
                        p[chParam(rank,local)]=tone[chParam(rank,local)];
            } else {
                p[chParam(rank,CP_saw)]=p[chParam(rank,CP_pulse)]=1;
                p[chParam(rank,CP_vA)]=p[chParam(rank,CP_vD)]=p[chParam(rank,CP_vR)]=.01f;
                p[chParam(rank,CP_vS)]=.7f; p[chParam(rank,CP_fR)]=2;
            }
            p[chParam(rank,CP_noise)]=0;
            p[chParam(rank,CP_hpfOn)]=p[chParam(rank,CP_lpfOn)]=1;
            p[chParam(rank,CP_feet)]=1; p[chParam(rank,CP_pan)]=0;
        }
        prepareInput({0,0});
        engine.prepare(c.rate);
        if(!engine.setControlFrame(frame)) throw std::runtime_error("initial ControlFrame rejected");
        engine.reset();
        engine.setExpression(.8f);
        if(c.condition!=0) for(int v=0;v<8;++v) engine.noteOn(36+v*5,.7f,1+v);
        if(c.condition==1) {
            std::vector<float> audio(512);
            for(int b=0;b<32;++b) engine.render(audio.data(),256);
            for(int v=0;v<8;++v) engine.noteOff(36+v*5,1+v);
            const int limit=static_cast<int>(std::ceil(40.0*c.rate/256));
            for(int b=0;b<limit && engine.activeVoices()!=0;++b) engine.render(audio.data(),256);
            if(engine.activeVoices()!=0) throw std::runtime_error("retirement fixture failed");
        }
    }
    void prepareInput(OffInput in) {
        input=in;
        auto& p=frame.effectiveValues;
#if defined(CS80_OFF_CANDIDATE)
        const bool request=config.policy>=3 && (!config.moving || (in.callback/32)%2==0);
        p[P_sourceContinuityV1]=request ? 1.0f : 0.0f;
#endif
        if(config.moving) {
            p[P_masterTune]=.25f*in.wave; p[P_brilliance]=.3f*in.wave;
            p[P_resonance]=.3f+.1f*in.wave; p[P_macro1]=.5f+.1f*in.wave;
            for(int rank=0;rank<2;++rank) {
                p[chParam(rank,CP_pw)]=.5f+.15f*in.wave;
                p[chParam(rank,CP_hpfCutoff)]=80+25*in.wave;
                p[chParam(rank,CP_lpfCutoff)]=3600+900*in.wave;
            }
        }
    }
    bool callback(float* audio,const float* ext) {
        const bool accepted=engine.setControlFrame(frame);
        if(config.moving) {
            engine.pitchBend(1,.1f*input.wave);
            engine.controlChange(11,.6f+.2f*input.wave,1);
            engine.channelPressure(1,.4f+.2f*input.wave);
        }
        engine.render(audio,config.block,ext);
        return accepted;
    }
    OffInspection inspect() const {
        OffInspection result;
        result.active=engine.activeVoices();
        result.policy=engine.getRenderPolicy()==RenderPolicy::legacy ? 0
                     : engine.getRenderPolicy()==RenderPolicy::isolation ? 1 : 2;
#if defined(CS80_OFF_CANDIDATE)
        result.effective=engine.getRenderModel().continuousCircuits;
        result.rejected=engine.getSourceContinuityRejectedSamples();
        for(int voice=0;voice<8;++voice) result.samples+=engine.getSourceVoiceCircuitState(voice).samples;
#endif
        for(int v=0;v<8;++v) for(int r=0;r<2;++r) {
            const auto s=engine.getRankAudioState(v,r);
            const auto e=engine.getRankEnvelopeState(v,r);
            for(float x:{s.phase,s.hpf.ic1,s.hpf.ic2,s.lpf.ic1,s.lpf.ic2,e.filter.value,e.amplitude.value}) {
                result.finite=result.finite && std::isfinite(x);
                result.stateChecksum+=(1+v*2+r)*x;
            }
        }
        return result;
    }
};
void* create(OffConfig c) { return new Instance(c); }
void destroy(void* p) { delete static_cast<Instance*>(p); }
void prepareInput(void* p,OffInput input) { static_cast<Instance*>(p)->prepareInput(input); }
bool callback(void* p,float* audio,const float* ext) { return static_cast<Instance*>(p)->callback(audio,ext); }
OffInspection inspect(void* p) { return static_cast<Instance*>(p)->inspect(); }
}
OffApi CS80_OFF_API() { return {create,destroy,prepareInput,callback,inspect}; }
