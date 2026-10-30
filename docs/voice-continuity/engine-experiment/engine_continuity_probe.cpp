#include "BenchmarkBridge.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock=std::chrono::steady_clock;
struct Row { int pair,arm,callback,order; std::int64_t ns; };
struct Owner
{
    BenchApi api; void* p;
    Owner(BenchApi a,BenchConfig c):api(a),p(a.create(c)){}
    ~Owner(){api.destroy(p);}
};
void require(bool condition,const char* message)
{if(!condition)throw std::runtime_error(message);}
const char* sceneName(int scene){return scene==0?"source_both":scene==1?"source_panel":"panel_both";}
const char* armName(int arm){return arm?"candidate_enabled":"baseline_428";}
}
int main(int argc,char** argv)
{
    const bool verify=argc==3&&std::string(argv[1])=="--verify";
    if(argc!=2&&!verify){std::fprintf(stderr,"usage: engine-continuity-probe [--verify] NEW-OUTPUT-DIRECTORY\n");return 2;}
    try
    {
        const std::filesystem::path directory(argv[verify?2:1]);
        require(std::filesystem::create_directory(directory),"output directory exists");
        FILE* summary=std::fopen((directory/"results.csv").c_str(),"w");
        require(summary!=nullptr,"cannot open aggregate output");
        std::fprintf(summary,"pair,arm,rate,block,scene,controls,voices,idle_kind,callbacks,median_ns,p99_ns,max_ns,overruns,audio_checksum,state_checksum,filter_eg_before,filter_eg_after,effective,active,kernel_samples,rejected\n");
        const int callbacks=verify?4:256,warmups=verify?2:32;
        std::vector<Row> raw; raw.reserve(144*2*callbacks);
        int pair=0,totalOverruns=0;
        for(double rate:{44100.,48000.,96000.}) for(int block:{128,256})
        for(int scene=0;scene<3;++scene) for(int moving:{0,1}) for(int condition=0;condition<4;++condition)
        {
            const int voices=condition<2?0:condition==2?1:8;
            const bool retired=condition==1;
            BenchConfig config{rate,block,scene,moving,voices,retired};
            Owner baseline(baselineApi(),config),candidate(candidateApi(),config);
            std::array<Owner*,2> engines{&baseline,&candidate};
            std::array<std::vector<float>,2> audio{std::vector<float>(2*block),std::vector<float>(2*block)};
            std::vector<float> external(block);
            std::array<std::vector<std::int64_t>,2> durations;
            for(auto& v:durations)v.reserve(callbacks);
            std::array<double,2> checksum{};
            for(int b=0;b<warmups;++b)
                for(auto* e:engines)e->api.callback(e->p,b,audio[0].data(),moving?external.data():nullptr);
            const std::array<BenchInspection,2> before{baseline.api.inspect(baseline.p),candidate.api.inspect(candidate.p)};
            require(before[0].active==voices&&before[1].active==voices,"wrong premeasurement active count");
            require(!before[0].effective&&before[1].effective,"wrong effective policy");
            for(int b=0;b<callbacks;++b)
            {
                for(int i=0;i<block;++i)external[(size_t)i]=.7f*std::sin((float)((b+warmups)*block+i)*.013f);
                // Alternate both callback and pair order; collect/check OUTSIDE timers.
                for(int order=0;order<2;++order)
                {
                    const int arm=(pair+b+order)&1; auto* e=engines[(size_t)arm];
                    std::int64_t ns=0;
                    if(verify) e->api.callback(e->p,b+warmups,audio[(size_t)arm].data(),moving?external.data():nullptr);
                    else
                    {
                        const auto start=Clock::now();
                        e->api.callback(e->p,b+warmups,audio[(size_t)arm].data(),moving?external.data():nullptr);
                        const auto end=Clock::now();
                        ns=std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
                    }
                    durations[(size_t)arm].push_back(ns);
                    raw.push_back({pair,arm,b,order,ns});
                    for(float x:audio[(size_t)arm])
                    {require(std::isfinite(x),"nonfinite measured audio");checksum[(size_t)arm]+=(double)x*x;}
                }
            }
            for(int arm=0;arm<2;++arm)
            {
                const auto e=engines[(size_t)arm]; const auto info=e->api.inspect(e->p);
                require(info.active==voices&&info.finite&&info.rejected==0,"postmeasurement invariant failed");
                require(info.effective==(arm==1),"effective policy changed");
                if(arm==1)require(info.samples-before[1].samples==(std::uint64_t)(callbacks*block*2),"continuous clock count mismatch");
                if(arm==1&&retired&&scene==2)
                    require(info.filterEnvelopeChecksum!=before[1].filterEnvelopeChecksum,"retired PANEL EG did not relax");
                auto sorted=durations[(size_t)arm];std::sort(sorted.begin(),sorted.end());
                const double median=.5*((double)sorted[(size_t)callbacks/2-1]+(double)sorted[(size_t)callbacks/2]);
                const auto p99=sorted[(size_t)std::ceil(.99*callbacks)-1];
                const double budget=1.e9*block/rate;
                const int overruns=(int)std::count_if(sorted.begin(),sorted.end(),[budget](auto ns){return ns>budget;});
                totalOverruns+=overruns;
                std::fprintf(summary,"%d,%s,%.0f,%d,%s,%s,%d,%s,%d,%.1f,%lld,%lld,%d,%.17g,%.17g,%.17g,%.17g,%d,%d,%llu,%llu\n",
                    pair,armName(arm),rate,block,sceneName(scene),moving?"moving":"static",voices,
                    retired?"retired":voices==0?"cold":"held",callbacks,median,(long long)p99,(long long)sorted.back(),overruns,
                    checksum[(size_t)arm],info.stateChecksum,before[(size_t)arm].filterEnvelopeChecksum,info.filterEnvelopeChecksum,
                    info.effective,info.active,(unsigned long long)info.samples,(unsigned long long)info.rejected);
            }
            std::fflush(summary); ++pair;
        }
        require(std::fclose(summary)==0,"aggregate close failed");
        FILE* samples=std::fopen((directory/"callback-times.csv").c_str(),"w");
        require(samples!=nullptr,"cannot write raw observations");
        std::fprintf(samples,"pair,arm,callback,order,nanoseconds\n");
        for(const auto& r:raw)std::fprintf(samples,"%d,%d,%d,%d,%lld\n",r.pair,r.arm,r.callback,r.order,(long long)r.ns);
        require(std::fclose(samples)==0,"raw close failed");
        std::printf("Engine %s: %d pairs, %zu callbacks, %d retained overruns\n",verify?"UNTIMED VERIFICATION":"benchmark",pair,raw.size(),totalOverruns);
    }
    catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
    return 0;
}
