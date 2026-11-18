#define main original_benchmark_main_not_run
#include "tools/source_factory_benchmark.cpp"
#undef main
#include <bit>
#include <cstring>
#include <cstdio>
#include <stdexcept>
using D = cs80::source_factory::BothRankDrives;
using B = std::array<cs80::source_factory::RankBoundary,2>;
namespace {
struct Counts { unsigned long long samples{}, resets{}, rank_calls[2]{}, rank_hits[2]{}, shared_hits{}, shared_touch_dep_hits{}, shared_ctrl_hits{}, thermal_calls{}, thermal_equal{}; };
Counts co; bool ready=false,on=false; D oldD{}; B oldB{}; double oldRes{}; bool enabled[2]{};
template<class T> bool same(T x,T y){ if constexpr(sizeof(T)==4)return std::bit_cast<unsigned>(x)==std::bit_cast<unsigned>(y); else return std::bit_cast<unsigned long long>(x)==std::bit_cast<unsigned long long>(y); }
bool sb(const B&a,const B&b){ for(unsigned i=0;i<2;i++){auto A=a[i],B=b[i];if(!same(A.qHp.volts,B.qHp.volts)||!same(A.qHp.ohms,B.qHp.ohms)||!same(A.qLp.volts,B.qLp.volts)||!same(A.qLp.ohms,B.qLp.ohms)||!same(A.level.volts,B.level.volts)||!same(A.level.ohms,B.level.ohms)||!same(A.initialLevelVolts,B.initialLevelVolts)||!same(A.afterLevelVolts,B.afterLevelVolts))return false;}return true;}
}
extern "C" void census_clear(){ready=false;if(on)++co.resets;}
extern "C" void census_thermal(float a,float b){if(on){++co.thermal_calls;co.thermal_equal+=same(a,b);}}
extern "C" void census_source(const D& d,const B& b,double res,bool a0,bool a1){
    bool active[]={a0,a1};bool common=ready&&enabled[0]==a0&&enabled[1]==a1&&same(res,oldRes)&&sb(b,oldB);
    bool shared=common,sharedTouch=common;
    if(on){++co.samples;co.shared_ctrl_hits+=common;}
    for(int r=0;r<2;r++){bool eq=ready&&enabled[r]==active[r], touch=eq;
        for(int v=0;v<8;v++){const auto& x=d[r][v];const auto& y=oldD[r][v];auto tp=same(x.velocity,y.velocity)&&same(x.pressure,y.pressure);eq=eq&&tp&&same(x.filterEnvelopeZ,y.filterEnvelopeZ);touch=touch&&tp;}
        if(on&&active[r]){++co.rank_calls[r];co.rank_hits[r]+=eq;}
        shared=shared&&eq;sharedTouch=sharedTouch&&touch;enabled[r]=active[r];
    }
    if(on){co.shared_hits+=shared;co.shared_touch_dep_hits+=sharedTouch;}
    oldD=d;oldB=b;oldRes=res;ready=true;
}
template<class T> void bin(FILE* f,T x){if(std::fwrite(&x,sizeof(x),1,f)!=1)throw std::runtime_error("write");}
template<class T,std::size_t N> void bin(FILE*f,const std::array<T,N>&v){for(const auto& x:v)bin(f,x);}
void render_scene(int scene,const char* prefix){
 const char* labels[]={"factory_quiet","factory_held","factory_moving","factory_release_wake","panel_held","pure_held"};Case c;c.rate=48000;c.block=64;c.selector=scene==4?13:0;c.isolated=scene==5;c.sourcePerformance=scene!=5;c.performanceProfile=scene==2?1:0;auto frame=makeFrame(c,c.selector); frame.effectiveValues[P_drift]=0.37f;frame.effectiveValues[P_voiceDispersion]=0.18f;frame.effectiveValues[P_driftModel]=0.f;
 Engine eng;eng.prepare(c.rate);if(!eng.setControlFrame(frame))throw std::runtime_error("frame");eng.reset(); if(scene!=0)for(int i=0;i<8;i++)eng.noteOn(notes[i],0.8f,c.performanceProfile?i+2:1);
 std::array<float,128> pcm{};std::string fpath=std::string(prefix)+"-"+labels[scene]+".bin";FILE*f=std::fopen(fpath.c_str(),"wb");if(!f)throw std::runtime_error("open");
 for(int b=-64;b<512;b++){
    if(b==0){co={};on=true;}
    if(scene==3&&b==90){frame.effectiveValues[P_parityMode]=1; if(!eng.setControlFrame(frame))throw std::runtime_error("Pure");}
    if(scene==3&&b==112){frame.effectiveValues[P_parityMode]=0;frame.effectiveValues[P_sourcePerformance]=0; if(!eng.setControlFrame(frame))throw std::runtime_error("Legacy");}
    if(scene==3&&b==142){frame.effectiveValues[P_sourcePerformance]=1; if(!eng.setControlFrame(frame))throw std::runtime_error("Circuit");}
    if(scene==3&&b==256){eng.prepare(44100); if(!eng.setControlFrame(frame))throw std::runtime_error("rate+prepare");eng.reset();}
    if(b==128||b==384){if(!eng.setControlFrame(frame))throw std::runtime_error("repeat");}
    if(scene==2 && b>=0&&b%4==0){int i=(b/4)%8;float t=float((b*13)%97)/96.f;eng.channelPressure(i+2,t);eng.pitchBend(i+2,t*.1f);eng.controlChange(74,t,i+2);}
    if(scene==3&&b==48)for(auto n:notes)eng.noteOff(n);if(scene==3&&b==320)for(int i=0;i<8;i++)eng.noteOn(notes[i],.72f);
    eng.render(pcm.data(),64);if(b<0)continue;
    for(auto x:pcm){if(!std::isfinite(x))throw std::runtime_error("nonfinite");bin(f,x);}
    bin(f,int(eng.activeVoices()));for(int r=0;r<2;r++){const auto x=eng.getSourceCutoffReadout(r);bin(f,x.pinVolts);bin(f,x.effectiveCv);bin(f,x.loadedBusVolts);bin(f,x.saturatedVoices);
        for(int v=0;v<8;v++){auto a=eng.getRankInfo(v,r);bin(f,a.hpfHz);bin(f,a.lpfHz);bin(f,a.filterEnvelope);bin(f,a.ampEnvelope);bin(f,a.oscillatorHz);bin(f,a.keyVolts);bin(f,a.hpfQ);bin(f,a.lpfQ);bin(f,static_cast<unsigned char>(a.hpfEnabled));bin(f,static_cast<unsigned char>(a.lpfEnabled));}}
    const auto x=eng.getSourceSharedReadout();bin(f,x.q);bin(f,x.qVolts);bin(f,x.qBusVolts);bin(f,x.levelBusVolts);bin(f,x.levelPinVolts);bin(f,x.levelGain);bin(f,x.masterResVolts);bin(f,x.saturatedVoices);bin(f,static_cast<unsigned char>(x.masterResSaturated));
 }
 on=false;ready=false;if(std::fclose(f))throw std::runtime_error("close");
 std::printf("%s,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n", labels[scene],co.samples,co.resets,co.rank_calls[0],co.rank_hits[0],co.rank_calls[1],co.rank_hits[1],co.shared_hits,co.shared_touch_dep_hits,co.shared_ctrl_hits,co.thermal_calls,co.thermal_equal);
}
int main(int argc,char**argv){if(argc!=2)return 2;ScopedDenormals denormals;std::puts("scene,readout_calls,invalidations,rank0_calls,rank0_exact_repeats,rank1_calls,rank1_exact_repeats,both_rank_all_input_repeats,shared_without_z_repeats_not_qualified,control_boundary_repeats,thermal_rank_ticks,thermal_float_unchanged");for(int i=0;i<6;i++)render_scene(i,argv[1]);}
