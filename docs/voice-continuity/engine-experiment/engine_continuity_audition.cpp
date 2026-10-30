// Noise-free diagnostic audition. IEEE-float WAV, no limiter/normalization.
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <vector>
using namespace cs80;
namespace {
void need(bool b,const char* what){if(!b)throw std::runtime_error(what);}
template<class T>void write(FILE* f,T x){need(std::fwrite(&x,sizeof x,1,f)==1,"write failed");}
void tag(FILE* f,const char* s){need(std::fwrite(s,1,4,f)==4,"tag failed");}
void wav(const std::filesystem::path& path,const std::vector<float>& audio)
{
    static_assert(std::endian::native==std::endian::little);
    FILE* f=std::fopen(path.c_str(),"wb");need(f!=nullptr,"cannot write WAV");
    tag(f,"RIFF");write<std::uint32_t>(f,36+audio.size()*4);tag(f,"WAVE");tag(f,"fmt ");
    write<std::uint32_t>(f,16);write<std::uint16_t>(f,3);write<std::uint16_t>(f,2);
    write<std::uint32_t>(f,48000);write<std::uint32_t>(f,48000*8);write<std::uint16_t>(f,8);write<std::uint16_t>(f,32);
    tag(f,"data");write<std::uint32_t>(f,audio.size()*4);
    need(std::fwrite(audio.data(),4,audio.size(),f)==audio.size(),"audio write failed");need(std::fclose(f)==0,"WAV close failed");
}
}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    try
    {
        const std::filesystem::path dir(argv[1]);need(std::filesystem::create_directory(dir),"output exists");
        for(int scene=0;scene<2;++scene)
        {
            Engine e;ControlFrame f;f.hardwareSources=scene==0?std::array<int,2>{1,3}:std::array<int,2>{13,13};
            auto& p=f.effectiveValues;p[P_sourcePerformance]=1;
            p[P_analogNoise]=p[P_drift]=p[P_voiceDispersion]=p[P_detune]=p[P_keyRelease]=p[P_masterSustain]=0;
            for(int r=0;r<2;++r){p[chParam(r,CP_noise)]=0;p[chParam(r,CP_vR)]=.001f;p[chParam(r,CP_fR)]=2;}
            e.prepare(48000);need(e.setControlFrame(f),"frame rejected");
#if defined(CS80_BENCH_CANDIDATE)
            experimental::ExperimentalVoiceContinuityProfile profile;profile.enabled=true;
            // Match the first sixteen baseline assignment phase draws. This is
            // an audition alignment convention, not an identified startup law.
            profile.phaseSeed=0x51c80u;need(e.setExperimentalVoiceContinuity(profile),"profile rejected");
#endif
            const char* name=scene==0?"source_both":"panel_both";
            FILE* trace=std::fopen((dir/(std::string(name)+".csv")).c_str(),"w");need(trace!=nullptr,"trace open");
            std::fprintf(trace,"elapsed_host_samples,active,phase0,hp_ic1,hp_ic2,lp_ic1,lp_ic2,filter_eg,amp_eg,key_K,oscillator_Hz\n");
            std::vector<float> audio(96000*2);
            for(int block=0;block<750;++block)
            {
                if(block==0)for(int v=0;v<8;++v)e.noteOn(48+v*3,.65f,1+v);
                if(block==94)for(int v=0;v<8;++v)e.noteOff(48+v*3,1+v);
                if(block==313||block==500)
                {
                    for(int r=0;r<2;++r)p[chParam(r,CP_hpfOn)]=p[chParam(r,CP_lpfOn)]=block==500?1:0;
                    need(e.setControlFrame(f),"bypass frame rejected");
                }
                if(block==563)e.noteOn(72,.65f,2);
                if(block==657)e.noteOff(72,2);
                e.render(audio.data()+block*256,128);
                const auto state=e.getRankAudioState(0,0);const auto env=e.getRankEnvelopeState(0,0);const auto info=e.getRankInfo(0,0);
                std::fprintf(trace,"%d,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",(block+1)*128,e.activeVoices(),state.phase,state.hpf.ic1,state.hpf.ic2,state.lpf.ic1,state.lpf.ic2,env.filter.value,env.amplitude.value,info.keyVolts,info.oscillatorHz);
            }
            need(std::fclose(trace)==0,"trace close");
            float peak=0;for(float x:audio){need(std::isfinite(x),"nonfinite audio");peak=std::max(peak,std::abs(x));}
#if defined(CS80_BENCH_CANDIDATE)
            need(e.getExperimentalRejectedSamples()==0,"kernel rejection");
#endif
            wav(dir/(std::string(name)+".wav"),audio);
            std::printf("%s: 96000 frames, float WAV sample_peak=%.9g (not normalized)\n",name,peak);
        }
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
    return 0;
}
