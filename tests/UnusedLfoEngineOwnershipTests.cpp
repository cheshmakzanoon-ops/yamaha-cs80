#include "core/Engine.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// Independent routing identities preregistered in unused-lfo-readout-exact.json.
// The state/phase oracle is separate so its mutations need not rebuild Engine.
namespace {
int checks=0,failures=0;
void check(bool ok,const char* what,float rate=0,int variant=-1)
{ ++checks; if(!ok){++failures;std::printf("FAIL: %s rate=%.0f variant=%d\n",what,rate,variant);} }
constexpr std::array<int,3> depthIds {cs80::P_lfo1Depth,cs80::P_lfo2Depth,cs80::P_lfo3Depth};
constexpr std::array<int,3> destIds {cs80::P_lfo1Dest,cs80::P_lfo2Dest,cs80::P_lfo3Dest};
constexpr std::array<int,3> rateIds {cs80::P_lfo1Rate,cs80::P_lfo2Rate,cs80::P_lfo3Rate};
constexpr std::array<int,3> waveIds {cs80::P_lfo1Wave,cs80::P_lfo2Wave,cs80::P_lfo3Wave};
constexpr std::array<cs80::modern::ModSource,3> lfoSources {cs80::modern::ModSource::lfo1,cs80::modern::ModSource::lfo2,cs80::modern::ModSource::lfo3};
cs80::ParamValues parameters(bool pure=false)
{
    cs80::ParamValues p;
    p[cs80::P_sourcePerformance]=1.0f;
    p[cs80::P_parityMode]=pure?1.0f:0.0f;
    p[cs80::P_analogNoise]=0.0f;
    p[cs80::P_drift]=0.0f;
    p[cs80::P_fx1Type]=p[cs80::P_fx2Type]=p[cs80::P_fx3Type]=0.0f;
    p[cs80::P_fx1Model]=p[cs80::P_fx2Model]=p[cs80::P_fx3Model]=0.0f;
    for(int i=0;i<3;++i){p[depthIds[i]]=0.0f;p[destIds[i]]=0.0f;p[rateIds[i]]=7.25f+i*2.5f;p[waveIds[i]]=i==0?0.0f:i==1?5.0f:6.0f;}
    return p;
}
cs80::modern::ModernConfig route(cs80::modern::ModSource source,cs80::modern::ModDestination destination)
{
    cs80::modern::ModernConfig c;
    c.slots[0]={source,cs80::modern::ModSource::none,destination,cs80::modern::ModTarget::both};
    return c;
}
std::vector<float> render(const cs80::ParamValues& p,const cs80::modern::ModernConfig& config={})
{
    cs80::Engine e;e.prepare(48000);e.setParams(p);e.setModernConfig(config);e.reset();e.noteOn(60,0.8f);
    std::vector<float> out(8192*2);e.render(out.data(),8192);return out;
}
bool audioSame(const std::vector<float>& a,const std::vector<float>& b)
{return a.size()==b.size()&&std::memcmp(a.data(),b.data(),a.size()*sizeof(float))==0;}
bool hasSignal(const std::vector<float>& a)
{for(float v:a)if(std::isfinite(v)&&v!=0.0f)return true;return false;}

void engineOwnership()
{
    for(int i=0;i<3;++i)
    {
        auto dry=parameters();dry[destIds[i]]=1.0f;
        auto wet=dry;wet[depthIds[i]]=0.4f;
        const auto a=render(dry),b=render(wet);
        check(hasSignal(a)&&hasSignal(b)&&!audioSame(a,b),"source Engine direct depth owns LFO readout",48000,i);
        auto zero=parameters();zero[cs80::P_mod1Amount]=0.2f;
        auto nonzero=zero;nonzero[depthIds[i]]=0.4f; // direct destination stays unconnected.
        const auto matrix=route(lfoSources[i],cs80::modern::ModDestination::pitch);
        const auto x=render(zero,matrix),y=render(nonzero,matrix);
        check(hasSignal(x)&&audioSame(x,y),"active matrix owns LFO readout even at zero direct depth",48000,i);
    }
}

void engineUnusedWake()
{
    auto p=parameters();p[cs80::P_mod1Amount]=0.37f;
    cs80::Engine candidate,reference;
    candidate.prepare(48000);reference.prepare(48000);
    candidate.setParams(p);reference.setParams(p);
    // With every FX type/model off, this modern route changes no audio, but keeps
    // the reference matrix active so all LFO outputs are needed by its contract.
    reference.setModernConfig(route(cs80::modern::ModSource::constant,cs80::modern::ModDestination::fx1Mix));
    candidate.reset();reference.reset();candidate.noteOn(60,0.8f);reference.noteOn(60,0.8f);
    std::array<float,256> a{},b{};
    bool unused=true,wake=true;
    for(int stage=0;stage<4;++stage)for(int block=0;block<96;++block)
    {
        for(int i=0;i<3;++i)
        {
            p[depthIds[i]]=stage%2?0.3f:0.0f;
            p[destIds[i]]=1.0f+i;
            p[waveIds[i]]=(float)((block/13+stage+i)%cs80::kLfoWaveCount);
            p[rateIds[i]]=1.5f+(float)((block/7+3*i)%18);
        }
        candidate.setParams(p);reference.setParams(p);
        candidate.render(a.data(),128);reference.render(b.data(),128);
        const bool equal=std::memcmp(a.data(),b.data(),sizeof(a))==0;
        if(stage%2)wake&=equal;else unused&=equal;
    }
    check(unused,"unused source LFO interval matches forced-readout reference Engine");
    check(wake,"source LFO activation after disabled intervals is bit-identical");
    auto pure=parameters(true),changed=pure;
    changed[cs80::P_mod1Amount]=0.6f;
    for(int i=0;i<3;++i){changed[depthIds[i]]=0.9f;changed[destIds[i]]=1.0f;changed[waveIds[i]]=6.0f;}
    check(audioSame(render(pure),render(changed,route(cs80::modern::ModSource::lfo1,cs80::modern::ModDestination::pitch))),
        "PURE stays byte-inert for modern LFO readout routing");
}
}
int main()
{
    engineOwnership();engineUnusedWake();
    std::printf("unused LFO engine ownership: %d checks, %d failures\n",checks,failures);
    return failures==0?0:1;
}
