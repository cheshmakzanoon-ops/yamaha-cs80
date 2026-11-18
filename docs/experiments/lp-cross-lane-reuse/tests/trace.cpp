#define main original_benchmark_not_called
#include "tools/source_factory_benchmark.cpp"
#undef main
#include <bit>
#include <cstdint>
#include <cstdio>
#include <array>
#include <stdexcept>
#include <cerrno>

namespace cs80 {
struct Ig00156MemoCensusAccess {
    static std::array<double,12> coefficients(const Ig00156Stage& s) {
        return {s.kHalf,s.kHalfCoupling,s.gmPart,s.ffPart,s.kiGs,s.xFromFb,
            s.xFromIn,s.xFromU,s.inFromX,s.invA11,s.qSiemensValue,s.xFromBp};
    }
    static void writeState(FILE*f,const Ig00156Stage& s) {
        const double fields[]={s.net.sourceOhms,s.net.groundOhms,s.net.inputFarads,s.net.feedbackOhms,
            s.net.feedbackFarads,s.net.integratorFarads,s.net.feedforwardOhms,
            s.gs,s.gg,s.gf,s.gTotal,s.inputScale,s.feedforwardHz,
            s.kHalf,s.kHalfCoupling,s.gmPart,s.ffPart,s.qSiemensValue,
            s.kiGs,s.xFromFb,s.xFromIn,s.xFromU,s.xFromBp,s.inFromX,s.invA11,
            s.zIn,s.zFb,s.zBp,s.zLp,s.vIn,s.vFb,s.vBp,s.vLp,s.slope1,s.slope2};
        for(auto x:fields)if(std::fwrite(&x,sizeof x,1,f)!=1)std::abort();
        for(float x:{s.lastCutoff,s.lastQ,s.lastRate})if(std::fwrite(&x,sizeof x,1,f)!=1)std::abort();
        for(unsigned char x:{static_cast<unsigned char>(s.lastUnwarped),static_cast<unsigned char>(s.cutoffCoefficientsValid)})
            if(std::fwrite(&x,sizeof x,1,f)!=1)std::abort();
    }
    static std::array<double,6> derived(const Ig00156Stage& s) {
        return {s.gs,s.gg,s.gf,s.gTotal,s.inputScale,s.feedforwardHz};
    }
    struct Keys {float cutoff,q,rate;bool mode,valid;};
    static Keys keys(const Ig00156Stage& s) {return {s.lastCutoff,s.lastQ,s.lastRate,s.lastUnwarped,s.cutoffCoefficientsValid};}
 };
struct EngineMemoCensusAccess {
    static void writeState(FILE*f,const Engine&e) {
        for(const auto& voice:e.voices)for(const auto& rank:voice.ranks) {
            Ig00156MemoCensusAccess::writeState(f,rank.hpStage);
            Ig00156MemoCensusAccess::writeState(f,rank.lpStage);
        }
    }
};
}

namespace {
#ifdef COUNTED
extern "C" unsigned long long cs80_lane_count_hits();
extern "C" unsigned long long cs80_lane_count_calls();
#else
constexpr unsigned long long cs80_lane_count_hits(){return 0;}
constexpr unsigned long long cs80_lane_count_calls(){return 0;}
#endif
struct Counts { unsigned long long calls{},hits{}; };
std::array<Counts,2> counts{};int phase=-1;
void printCounts(const char* label,unsigned p,const Counts& c) {
    std::printf("%s,%s,%llu,%llu\n",label,p?"late":"onset",c.calls,c.hits);
}
template<class T> void bin(FILE*f,T x) {
    if(std::fwrite(&x,sizeof x,1,f)!=1) throw std::runtime_error("write");
}
template<class T,std::size_t N> void bin(FILE*f,const std::array<T,N>&a) {
    for(const auto& x:a) bin(f,x);
}
void capture(FILE*f,Engine& eng,const std::array<float,128>&pcm) {
    for(float x:pcm) {if(!std::isfinite(x))throw std::runtime_error("nonfinite");bin(f,x);}
    bin(f,int(eng.activeVoices()));
    for(int r=0;r<2;r++) {
        auto x=eng.getSourceCutoffReadout(r);
        bin(f,x.pinVolts);bin(f,x.effectiveCv);bin(f,x.loadedBusVolts);bin(f,x.saturatedVoices);
        for(int v=0;v<8;v++) {
            auto a=eng.getRankInfo(v,r);
            bin(f,a.hpfHz);bin(f,a.lpfHz);bin(f,a.filterEnvelope);bin(f,a.ampEnvelope);
            bin(f,a.oscillatorHz);bin(f,a.keyVolts);bin(f,a.hpfQ);bin(f,a.lpfQ);
            bin(f,static_cast<unsigned char>(a.hpfEnabled));bin(f,static_cast<unsigned char>(a.lpfEnabled));
        }
    }
    auto x=eng.getSourceSharedReadout();
    bin(f,x.q);bin(f,x.qVolts);bin(f,x.qBusVolts);bin(f,x.levelBusVolts);
    bin(f,x.levelPinVolts);bin(f,x.levelGain);bin(f,x.masterResVolts);bin(f,x.saturatedVoices);
    bin(f,static_cast<unsigned char>(x.masterResSaturated));
    // Per-block flags are observed without clearing or changing the environment.
    bin(f,_mm_getcsr());
}
void scene(int which,const char* prefix) {
    const char* labels[]={"factory_quiet","panel_quiet","factory_held","factory_moving",
        "panel_held","pure_held","legacy_held","release_wake","factory_moving_frame_first","panel_moving_frame_first"};
    counts={};phase=-1;
    Case c;c.rate=48000;c.block=64;c.selector=(which==1||which==4||which==9)?13:0;
    c.isolated=which==5;c.sourcePerformance=which!=5&&which!=6;const bool moving=which==3||which>=8;c.performanceProfile=moving?1:0;
    auto frame=makeFrame(c,c.selector);
    frame.effectiveValues[P_drift]=.37f;frame.effectiveValues[P_voiceDispersion]=.18f;
    frame.effectiveValues[P_driftModel]=0.f;
    Engine eng;eng.prepare(c.rate);if(!eng.setControlFrame(frame))throw std::runtime_error("frame");eng.reset();
    if(which>=2)for(int v=0;v<8;v++)eng.noteOn(notes[v],.8f,moving?v+2:1);
    std::array<float,128> pcm{};
    const auto filename=std::string(prefix)+"-"+labels[which]+".bin";
    FILE*f=std::fopen(filename.c_str(),"wb");if(!f)throw std::runtime_error("open");
    const auto stateFilename=std::string(prefix)+"-"+labels[which]+".state.bin";
    FILE*sf=std::fopen(stateFilename.c_str(),"wb");if(!sf)throw std::runtime_error("state open");
    for(int b=0;b<1536;b++) {
        phase=b<256?0:b>=1024?1:-1;
        const auto oldHits=cs80_lane_count_hits(),oldCalls=cs80_lane_count_calls();
        if(which>=8 && !eng.setControlFrame(frame))throw std::runtime_error("frame replay");
        if(moving&&b%4==0) {
            int v=(b/4)%8;float x=float((b*13)%97)/96.f;
            eng.channelPressure(v+2,x);eng.pitchBend(v+2,x*.1f);eng.controlChange(74,x,v+2);
        }
        if(which<8 && (b==128||b==384||b==1100))if(!eng.setControlFrame(frame))throw std::runtime_error("repeat");
        if(which==7) {
            if(b==192)for(auto n:notes)eng.noteOff(n);
            if(b==600)for(int v=0;v<8;v++)eng.noteOn(notes[v],.72f);
            if(b==1040) {frame.effectiveValues[P_parityMode]=1;if(!eng.setControlFrame(frame))throw std::runtime_error("Pure");}
            if(b==1080) {frame.effectiveValues[P_parityMode]=0;frame.effectiveValues[P_sourcePerformance]=0;if(!eng.setControlFrame(frame))throw std::runtime_error("Legacy");}
            if(b==1120) {frame.effectiveValues[P_sourcePerformance]=1;if(!eng.setControlFrame(frame))throw std::runtime_error("Circuit");}
            if(b==1280) {eng.prepare(44100);if(!eng.setControlFrame(frame))throw std::runtime_error("rate");eng.reset();}
            if(b==1360)for(int v=0;v<8;v++)eng.noteOn(notes[v],.72f);
        }
        eng.render(pcm.data(),64);
        if(phase>=0) {counts[phase].calls+=cs80_lane_count_calls()-oldCalls; counts[phase].hits+=cs80_lane_count_hits()-oldHits;}
        // Full PCM and readouts for both counted windows, sparse controls during warmup.
        if(phase>=0||b%32==0) {
            unsigned short cw=0,sw=0;asm volatile("fnstcw %0" : "=m"(cw));asm volatile("fnstsw %0" : "=m"(sw));
            const int savedErrno=errno;bin(sf,b);bin(sf,cw);bin(sf,sw);bin(sf,savedErrno);
            cs80::EngineMemoCensusAccess::writeState(sf,eng);
            bin(f,b);capture(f,eng,pcm);
        }
    }
    phase=-1;if(std::fclose(f))throw std::runtime_error("close");
    if(std::fclose(sf))throw std::runtime_error("state close");
    for(unsigned p=0;p<2;p++)printCounts(labels[which],p,counts[p]);
}
}
 int main(int argc,char**argv){ if(argc!=2)return 2; ScopedDenormals fp; std::puts("scene,window,calls,hits");for(int k=0;k<10;k++)scene(k,argv[1]); }
