#include "BenchBridge.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <time.h>
#if !defined(__SSE__) && !defined(_M_X64)
#error This registered measurement requires an x86 FTZ/DAZ guard; preregister another architecture separately
#endif
#include <xmmintrin.h>
namespace {
using Clock=std::chrono::steady_clock;
namespace fs=std::filesystem;
constexpr unsigned seed=167350139, fpControlMask=0xffc0u;
constexpr int measuredCallbacks=512, warmups=64;
struct Raw { int pair,caseId,callback,arm,order; std::int64_t ns; };
struct Case { int id; OffConfig config; };
struct Owner {
    OffApi api; void* p;
    Owner(OffApi a,OffConfig c):api(a),p(a.create(c)){}
    ~Owner(){api.destroy(p);}
};
struct FpGuard {
    unsigned initial=_mm_getcsr(), controls=(initial|0x8040u)&fpControlMask;
    FpGuard(){_mm_setcsr(initial|0x8040u);}
    ~FpGuard(){_mm_setcsr(initial);}
    bool valid()const{return (_mm_getcsr()&fpControlMask)==controls;}
};
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
double threadCpu(){timespec t{}; if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)!=0)throw std::runtime_error("thread CPU clock unavailable"); return static_cast<double>(t.tv_sec)+1.e-9*t.tv_nsec;}
std::ofstream file(const fs::path& path){std::ofstream f;f.exceptions(std::ios::failbit|std::ios::badbit);f.open(path);f<<std::setprecision(17);return f;}
const char* policyName(int p){return std::array{"legacy_off","isolation_off","source_performance_off","legacy_masked","isolation_masked"}[p];}
const char* conditionName(int c){return std::array{"cold_idle","retired_idle","eight_held"}[c];}
void configColumns(std::ostream& out,const Case& c){const auto& x=c.config;out<<c.id<<','<<x.rate<<','<<x.block<<','<<(x.scene==0?"source_both":"panel_both")<<','<<conditionName(x.condition)<<','<<(x.moving?"moving":"static")<<','<<policyName(x.policy);}
void inspect(const OffInspection& a,const OffConfig& c){const int expectedPolicy=c.policy==3?0:c.policy==4?1:c.policy;require(a.active==(c.condition==2?8:0)&&a.policy==expectedPolicy&&!a.effective&&a.samples==0&&a.rejected==0&&a.finite,"active/policy/effective/state invariant failed");}
std::uint64_t hashAudio(std::uint64_t hash,const std::vector<float>& audio){for(float x:audio){const auto bits=std::bit_cast<std::uint32_t>(x);for(int i=0;i<4;++i){hash^=(bits>>(i*8))&255u;hash*=1099511628211ull;}}return hash;}
void rawOutput(const fs::path& dir,const std::vector<Raw>& raw){auto f=file(dir/"callback-times.csv");f<<"pair,case_id,callback,arm,order,nanoseconds\n";for(const auto& r:raw)f<<r.pair<<','<<r.caseId<<','<<r.callback<<','<<r.arm<<','<<r.order<<','<<r.ns<<'\n';}
}
int main(int argc,char** argv){
    const bool verify=argc==3&&std::string(argv[1])=="--verify";
    if(argc!=2&&!verify){std::cerr<<"usage: continuity-off-probe [--verify] NEW-DIRECTORY\n";return 2;}
    std::vector<Raw> raw;
    fs::path directory;
    bool created=false;
    int completedPairs=0;
    std::string failure;
    try{
        directory=argv[verify?2:1];
        require(fs::create_directory(directory),"output directory already exists or cannot be created");created=true;
        FpGuard fp;
        const int callbacks=verify?4:measuredCallbacks;
        raw.reserve(240*2*callbacks);
        std::vector<Case> cases;
        int id=0;
        for(int rate:{44100,48000})for(int block:{128,256})for(int scene:{0,1})
        for(int condition:{0,1,2})for(int moving:{0,1})for(int policy=0;policy<5;++policy)
            cases.push_back({id++,{rate,block,scene,condition,moving,policy}});
        std::mt19937 random(seed);
        for(std::size_t n=cases.size();n>1;--n)std::swap(cases[n-1],cases[static_cast<std::size_t>(random())%n]);
        auto matrix=file(directory/"matrix.csv");matrix<<"pair,case_id,rate,block,scene,condition,controls,policy\n";
        for(std::size_t pair=0;pair<cases.size();++pair){matrix<<pair<<',';configColumns(matrix,cases[pair]);matrix<<'\n';}matrix.close();
        auto metadata=file(directory/"metadata.json");
        metadata<<"{\"fixture\":\"continuity-default-off-matched-v1\",\"verification_only\":"<<(verify?"true":"false")<<",\"warmups\":"<<warmups<<",\"callbacks_per_arm\":"<<callbacks<<",\"pairs\":240,\"order_seed\":"<<seed<<",\"initial_mxcsr\":"<<fp.initial<<",\"timed_mxcsr_control_bits\":"<<fp.controls<<",\"fp_scope\":\"both arms setup warmup callbacks FTZ DAZ ON; original restored\",\"cpu_scope\":\"whole paired callback phase including both arms and untimed input/checks\",\"compiler\":\""<<__VERSION__<<"\"}\n";metadata.close();
        auto summary=file(directory/"results.csv");
        summary<<"pair,case_id,rate,block,scene,condition,controls,policy,arm,callbacks,mean_ns,median_ns,p99_ns,max_ns,overruns,audio_hash,compared_float_values,peak,state_checksum,pair_process_cpu_s_including_checks,pair_thread_cpu_s_including_checks\n";
        for(std::size_t pair=0;pair<cases.size();++pair){
            const auto& c=cases[pair];const auto& cfg=c.config;
            std::array<std::unique_ptr<Owner>,2> engines;
            for(int order=0;order<2;++order){const int arm=(static_cast<int>(pair)+order)&1;engines[arm]=std::make_unique<Owner>(arm?offCandidateApi():offBaselineApi(),cfg);}
            std::array<std::vector<float>,2> audio{std::vector<float>(2*cfg.block),std::vector<float>(2*cfg.block)};
            std::vector<float> external(cfg.block);
            auto prepare=[&](int callback){const float wave=std::sin(static_cast<float>(callback)*.071f);for(int i=0;i<cfg.block;++i)external[i]=.5f*std::sin(static_cast<float>(callback*cfg.block+i)*.013f);for(auto& e:engines)e->api.prepareInput(e->p,{wave,callback});};
            auto compare=[&]{require(std::memcmp(audio[0].data(),audio[1].data(),audio[0].size()*sizeof(float))==0,"paired audio bits differ");for(float x:audio[0])require(std::isfinite(x),"nonfinite paired output");};
            for(int b=0;b<warmups;++b){prepare(b);for(int order=0;order<2;++order){const int arm=(static_cast<int>(pair)+b+order)&1;auto& e=engines[arm];require(fp.valid(),"FP controls changed before warmup");require(e->api.callback(e->p,audio[arm].data(),cfg.moving?external.data():nullptr),"warmup frame rejected");require(fp.valid(),"FP controls changed during warmup");}compare();}
            for(const auto& e:engines)inspect(e->api.inspect(e->p),cfg);
            std::array<std::vector<std::int64_t>,2> durations;
            for(auto& d:durations)d.reserve(callbacks);
            std::array<std::uint64_t,2> hashes{14695981039346656037ull,14695981039346656037ull};
            double peak=0;
            const auto processStart=verify?0:std::clock();const double threadStart=verify?0:threadCpu();
            for(int b=0;b<callbacks;++b){
                prepare(b+warmups);
                for(int order=0;order<2;++order){
                    const int arm=(static_cast<int>(pair)+b+order)&1;auto& e=engines[arm];
                    require(fp.valid(),"FP controls changed before callback");
                    bool accepted=false;std::int64_t ns=0;
                    if(verify)accepted=e->api.callback(e->p,audio[arm].data(),cfg.moving?external.data():nullptr);
                    else{const auto before=Clock::now();accepted=e->api.callback(e->p,audio[arm].data(),cfg.moving?external.data():nullptr);const auto after=Clock::now();ns=std::chrono::duration_cast<std::chrono::nanoseconds>(after-before).count();}
                    raw.push_back({static_cast<int>(pair),c.id,b,arm,order,ns});durations[arm].push_back(ns);
                    require(accepted,"measured frame rejected");require(fp.valid(),"FP controls changed during callback");
                }
                compare();
                for(int arm=0;arm<2;++arm)hashes[arm]=hashAudio(hashes[arm],audio[arm]);
                for(float x:audio[0])peak=std::max(peak,std::abs(static_cast<double>(x)));
            }
            const double threadElapsed=verify?0:threadCpu()-threadStart;
            const double processElapsed=verify?0:static_cast<double>(std::clock()-processStart)/CLOCKS_PER_SEC;
            for(int arm=0;arm<2;++arm){
                const auto info=engines[arm]->api.inspect(engines[arm]->p);inspect(info,cfg);
                auto sorted=durations[arm];std::sort(sorted.begin(),sorted.end());
                const double median=.5*(static_cast<double>(sorted[callbacks/2-1])+static_cast<double>(sorted[callbacks/2]));
                double sum=0;for(auto d:sorted)sum+=static_cast<double>(d);
                const auto p99=sorted[static_cast<std::size_t>(std::ceil(.99*callbacks))-1];
                const double budget=1.e9*cfg.block/cfg.rate;
                const auto overruns=std::count_if(sorted.begin(),sorted.end(),[&](auto n){return n>budget;});
                summary<<pair<<',';configColumns(summary,c);summary<<','<<(arm?"candidate_off_or_masked":"baseline_16735ee")<<','<<callbacks<<','<<sum/callbacks<<','<<median<<','<<p99<<','<<sorted.back()<<','<<overruns<<','<<hashes[arm]<<','<<static_cast<std::uint64_t>(callbacks)*cfg.block*2<<','<<peak<<','<<info.stateChecksum<<','<<processElapsed<<','<<threadElapsed<<'\n';
            }
            summary.flush();++completedPairs;
            std::cout<<(verify?"verify":"timing")<<" pair "<<completedPairs<<"/240, "<<policyName(cfg.policy)<<", "<<cfg.rate<<'/'<<cfg.block<<'\n'<<std::flush;
        }
        summary.close();
    }catch(const std::exception& error){failure=error.what();}
    if(created){try{rawOutput(directory,raw);auto status=file(directory/"status.json");status<<"{\"completed\":"<<(failure.empty()?"true":"false")<<",\"pairs_complete\":"<<completedPairs<<",\"raw_observations\":"<<raw.size()<<"}\n";if(!failure.empty()){auto error=file(directory/"error.txt");error<<failure<<'\n';}}catch(const std::exception& error){std::cerr<<"Evidence write failed: "<<error.what()<<'\n';return 1;}}
    if(!failure.empty()){std::cerr<<"FAIL: "<<failure<<'\n';return 1;}
    return 0;
}
