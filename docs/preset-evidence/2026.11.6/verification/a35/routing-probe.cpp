#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "core/InstrumentBodyMaterials.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
using namespace cs80;
namespace {
int checks=0, failures=0;
double maximumPeak=0;
void check(bool v,const std::string& label){++checks;if(!v){++failures;std::printf("FAIL: %s\n",label.c_str());}}
constexpr std::array<int,10> materialModels{0,16,16,16,16,19,2,7,7,6};
ParamValues fixture(int rank,int model,int material,float mix=1.f){
    ParamValues p;
    p[P_sourcePerformance]=1;p[P_parityMode]=p[P_kasScanTiming]=0;
    p[P_mix]=float(rank);p[P_volume]=.22f;
    p[P_analogNoise]=.12f;p[P_drift]=.14f;p[P_voiceDispersion]=.16f;
    p[P_chorusOn]=0;p[P_detune]=p[P_portamento]=p[P_initBend]=0;
    p[P_subVco]=p[P_subAtVco]=p[P_subVcf]=p[P_subAtVcf]=0;
    p[bodyParam(rank,0)]=float(model);p[bodyMaterialParam(rank)]=float(material);
    p[bodyParam(rank,1)]=mix;p[bodyParam(rank,2)]=.45f;p[bodyParam(rank,3)]=1;
    p[chParam(rank,CP_vA)]=.003f;p[chParam(rank,CP_vD)]=4;
    p[chParam(rank,CP_vS)]=.75f;p[chParam(rank,CP_vR)]=.3f;
    p[chParam(rank,CP_fIl)]=p[chParam(rank,CP_fAl)]=0;
    p[chParam(rank,CP_initBrill)]=p[chParam(rank,CP_afterBrill)]=0;
    return p;
}
std::unique_ptr<Engine> engine(const ParamValues& p){auto e=std::make_unique<Engine>();e->prepare(48000);e->setParams(p);return e;}
std::vector<float> render(Engine& e,int n){
    std::vector<float> x(2*n);for(int at=0;at<n;at+=113)e.render(x.data()+2*at,std::min(113,n-at));
    for(float v:x){if(!std::isfinite(v)||std::abs(v)>=1){check(false,"finite below-full-scale output");break;}maximumPeak=std::max(maximumPeak,std::abs(double(v)));}
    return x;
}
double relative(const std::vector<float>& a,const std::vector<float>& b){double d=0,p=0;for(size_t i=0;i<a.size();++i){d+=std::pow(double(a[i])-b[i],2);p+=double(a[i])*a[i];}return std::sqrt(d/std::max(p,1.e-30));}
void zeroMix(){
    for(int rank=0;rank<2;++rank)for(int material=1;material<=9;++material){
        auto p=fixture(rank,materialModels[material],0,0),q=p;q[bodyMaterialParam(rank)]=float(material);
        auto a=engine(p),b=engine(q);
        for(int note:{48,60,72}){a->noteOn(note,.72f);b->noteOn(note,.72f);check(render(*a,2400)==render(*b,2400),"zero mix preserves circuit waveform/RNG across strikes");a->noteOff(note);b->noteOff(note);check(render(*a,480)==render(*b,480),"zero mix preserves release histories");}
    }
}
void fallback(){
    for(int rank=0;rank<2;++rank)for(int model=0;model<=body::maxModel;++model){
        const int wrong=model==body::bell?body::harp:body::gong;
        check(body::compatibleMaterial(model,wrong)==0,"fixture uses incompatible pair");
        auto p=fixture(rank,model,0,.8f),q=p;q[bodyMaterialParam(rank)]=float(wrong);
        auto a=engine(p),b=engine(q);a->noteOn(60,.8f);b->noteOn(60,.8f);
        check(render(*a,3600)==render(*b,3600),"incompatible material equals Standard in complete Engine");
    }
}
void captured(){
    struct Case{const char* name;int model,material,kind;float value;};
    for(int rank=0;rank<2;++rank)for(auto c:{
        Case{"model",16,2,0,2},Case{"decay",16,2,1,.25f},
        Case{"size",7,8,2,2.f},Case{"variant",18,0,3,1},
        Case{"material",16,0,4,2}}){
        auto p=fixture(rank,c.model,c.material),q=p;
        int id= c.kind==0?bodyParam(rank,0):c.kind==1?bodyParam(rank,3):c.kind==2?bodySizeParam(rank):c.kind==3?bodyVariantParam(rank):bodyMaterialParam(rank);
        q[id]=c.value;auto a=engine(p),b=engine(p);a->noteOn(60,.8f);b->noteOn(60,.8f);
        check(render(*a,4800)==render(*b,4800),std::string(c.name)+" common prefix");b->setParams(q);
        check(render(*a,4800)==render(*b,4800),std::string(c.name)+" leaves held note sample-identical");
        a->noteOn(67,.8f);b->noteOn(67,.8f);const double e=relative(render(*a,9600),render(*b,9600));
        check(e>1.e-4,std::string(c.name)+" changes next accepted note");
        std::printf("CAPTURE rank=%d %s next-note relative difference=%.9g\n",rank,c.name,e);
    }
}
void live(){
    for(int rank=0;rank<2;++rank)for(int control:{1,2}){
        auto p=fixture(rank,16,2,control==1?.2f:1.f),q=p;
        if(control==2)p[bodyParam(rank,2)]=q[bodyParam(rank,2)]=.2f;
        q[bodyParam(rank,control)]=.9f;
        auto a=engine(p),b=engine(p);a->noteOn(60,.8f);b->noteOn(60,.8f);
        check(render(*a,9600)==render(*b,9600),"live-control common prefix");b->setParams(q);
        const double e=relative(render(*a,9600),render(*b,9600));
        check(e>1.e-4,"mix/brightness changes held note without new onset");
        std::printf("LIVE rank=%d control=%d relative difference=%.9g\n",rank,control,e);
    }
}
void productionSwap(FactoryPreset& p){
#include "rank-swap.inc"
}
void swap(){
    FactoryPreset p;for(int rank=0;rank<2;++rank){for(int k=0;k<CP_count;++k)p.values[chParam(rank,k)]=float(100*rank+k);for(int k=0;k<4;++k)p.values[bodyParam(rank,k)]=float(100*rank+40+k);p.values[bodySizeParam(rank)]=1.f+rank;p.values[bodyVariantParam(rank)]=float(1+rank);}
    p.values[P_body1Model]=body::steel;p.values[P_body2Model]=body::nylon;p.values[P_body1Material]=body::banjo;p.values[P_body2Material]=body::harp;p.values[P_mix]=.24f;p.values[P_detune]=3.f;p.values[P_volume]=.123f;
    const auto before=p.values;productionSwap(p);
    for(int rank=0;rank<2;++rank){for(int k=0;k<CP_count;++k)check(p.values[chParam(rank,k)]==before[chParam(1-rank,k)],"original rank swap keeps panel controls paired");for(int k=0;k<4;++k)check(p.values[bodyParam(rank,k)]==before[bodyParam(1-rank,k)],"original rank swap keeps body controls paired");for(auto id:{bodySizeParam(rank),bodyVariantParam(rank),bodyMaterialParam(rank)}){const int other=id==bodySizeParam(rank)?bodySizeParam(1-rank):id==bodyVariantParam(rank)?bodyVariantParam(1-rank):bodyMaterialParam(1-rank);check(p.values[id]==before[other],"original rank swap moves nonzero body extensions");}}
    for(int rank=0;rank<2;++rank)check(body::compatibleMaterial(int(p.values[bodyParam(rank,0)]),int(p.values[bodyMaterialParam(rank)]))==int(p.values[bodyMaterialParam(rank)]),"nonzero materials remain paired with compatible model after swap");
    check(p.values[P_mix]==1.f-before[P_mix]&&p.values[P_detune]==0.f&&p.values[P_volume]==before[P_volume],"swap preserves authored mix/tuning/global contract");
}
}
int main(int argc,char**){swap();if(argc==1){zeroMix();fallback();captured();live();}std::printf("A35 material routing: %d checks, %d failures; peak %.9g\n",checks,failures,maximumPeak);return failures?1:0;}
