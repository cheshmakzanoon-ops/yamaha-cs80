// Separately identified FP-policy diagnostic; never replaces first measurement.
#include "../BenchmarkBridge.h"
#include <immintrin.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using Clock=std::chrono::steady_clock;
constexpr unsigned policyBits=0x8040, statusBits=0x3f;
struct FpScope
{
    unsigned saved=_mm_getcsr();
    explicit FpScope(bool flush){_mm_setcsr((saved&~(policyBits|statusBits))|(flush?policyBits:0));}
    ~FpScope(){_mm_setcsr(saved);}
};
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
struct Owner
{
    BenchApi api;void* p=nullptr;bool flush;
    Owner(int arm,BenchConfig c):api(arm>=2?candidateApi():baselineApi()),flush((arm&1)!=0)
    {FpScope scope(flush);p=api.create(c);}
    ~Owner(){api.destroy(p);}
};
struct Raw {int cell,arm,callback,order;std::int64_t ns;unsigned before,after;};
struct Difference
{
    std::uint64_t samples=0,bits=0,nonzero=0,signedZero=0,subnormal[2]{};
    double maxAbs=0,squared=0;
    void add(float off,float on)
    {
        const auto a=std::bit_cast<std::uint32_t>(off),b=std::bit_cast<std::uint32_t>(on);
        const auto am=a&0x7fffffff,bm=b&0x7fffffff;
        ++samples;bits+=a!=b;
        signedZero+=a!=b&&am==0&&bm==0;
        subnormal[0]+=am>0&&am<0x00800000;
        subnormal[1]+=bm>0&&bm<0x00800000;
        const double delta=(double)off-(double)on;
        nonzero+=delta!=0;maxAbs=std::max(maxAbs,std::abs(delta));squared+=delta*delta;
    }
};
const char* scene(int s){return s==0?"source_both":s==1?"source_panel":"panel_both";}
}
int main(int argc,char** argv)
{
    const bool verify=argc==3&&std::string(argv[1])=="--verify";
    if(argc!=2&&!verify){std::fprintf(stderr,"usage: fp-policy-probe [--verify] NEW-OUTPUT-DIRECTORY\n");return 2;}
    try
    {
        const std::filesystem::path dir(argv[verify?2:1]);require(std::filesystem::create_directory(dir),"output exists");
        const unsigned entry=_mm_getcsr();
        const int callbacks=verify?4:256,warmups=verify?2:32;
        std::vector<BenchConfig> configs;
        for(double rate:{48000.,96000.})for(int block:{128,256})for(int s:{0,2})for(int condition=0;condition<4;++condition)
            configs.push_back({rate,block,s,0,condition<2?0:condition==2?1:8,condition==1});
        // First run's worst-RTF configuration, retained explicitly, not selected
        // to manufacture a new passing result. No acceptance gate is applied.
        configs.push_back({96000,128,1,1,1,false});
        std::vector<Raw> raw;raw.reserve(configs.size()*4*(size_t)callbacks);
        FILE* aggregate=std::fopen((dir/"results.csv").c_str(),"w");require(aggregate,"aggregate open");
        FILE* numeric=std::fopen((dir/"numerical-differences.csv").c_str(),"w");require(numeric,"numeric open");
        std::fprintf(aggregate,"cell,engine,fp_policy,rate,block,scene,controls,voices,idle_kind,callbacks,median_ns,p99_ns,max_ns,overruns,denormal_operand_callbacks,underflow_callbacks,audio_checksum,state_checksum,filter_eg_checksum,effective,active,kernel_samples,rejected\n");
        std::fprintf(numeric,"cell,engine,samples,bit_differences,nonzero_differences,signed_zero_differences,max_absolute_difference,rms_difference,off_subnormal_outputs,on_subnormal_outputs,state_checksum_off,state_checksum_on,filter_eg_off,filter_eg_on\n");
        for(size_t cell=0;cell<configs.size();++cell)
        {
            const auto c=configs[cell];
            std::array<std::unique_ptr<Owner>,4> e;
            std::array<std::vector<float>,4> audio;
            std::array<std::vector<std::int64_t>,4> durations;
            std::array<BenchInspection,4> before,after;
            std::array<double,4> sums{};std::array<int,4> denorm{},underflow{};
            std::array<Difference,2> differences;
            std::vector<float> external((size_t)c.block);
            for(int arm=0;arm<4;++arm)
            {
                e[arm]=std::make_unique<Owner>(arm,c);audio[arm].resize((size_t)c.block*2);durations[arm].reserve(callbacks);
                for(int b=0;b<warmups;++b){FpScope fp(e[arm]->flush);e[arm]->api.callback(e[arm]->p,b,audio[arm].data(),c.moving?external.data():nullptr);}
                FpScope observation(false);before[arm]=e[arm]->api.inspect(e[arm]->p);
                require(before[arm].active==c.voices&&before[arm].effective==(arm>=2),"precondition policy/count");
            }
            for(int b=0;b<callbacks;++b)
            {
                for(int i=0;i<c.block;++i)external[(size_t)i]=.7f*std::sin((float)((b+warmups)*c.block+i)*.013f);
                for(int order=0;order<4;++order)
                {
                    const int arm=((int)cell+b+order)%4;
                    std::int64_t ns=0;unsigned csrBefore=0,csrAfter=0;
                    {
                        FpScope fp(e[arm]->flush);csrBefore=_mm_getcsr();
                        if(verify)e[arm]->api.callback(e[arm]->p,b+warmups,audio[arm].data(),c.moving?external.data():nullptr);
                        else
                        {
                            const auto start=Clock::now();
                            e[arm]->api.callback(e[arm]->p,b+warmups,audio[arm].data(),c.moving?external.data():nullptr);
                            const auto end=Clock::now();
                            ns=std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
                        }
                        csrAfter=_mm_getcsr();
                    }
                    require((csrBefore&policyBits)==(e[arm]->flush?policyBits:0)
                         &&(csrAfter&policyBits)==(csrBefore&policyBits),"FP policy changed inside callback");
                    require((csrBefore&0x7f80)==(entry&0x7f80),"exception masks/rounding changed");
                    durations[arm].push_back(ns);raw.push_back({(int)cell,arm,b,order,ns,csrBefore,csrAfter});
                    denorm[arm]+=(csrAfter&2)!=0;underflow[arm]+=(csrAfter&16)!=0;
                    FpScope observation(false);
                    for(float x:audio[arm]){require(std::isfinite(x),"nonfinite audio");sums[arm]+=(double)x*x;}
                }
                FpScope observation(false);
                for(int engine=0;engine<2;++engine)for(size_t i=0;i<audio[0].size();++i)
                    differences[engine].add(audio[engine*2][i],audio[engine*2+1][i]);
            }
            for(int arm=0;arm<4;++arm)
            {
                FpScope observation(false);after[arm]=e[arm]->api.inspect(e[arm]->p);
                require(after[arm].finite&&after[arm].active==c.voices&&after[arm].rejected==0&&after[arm].effective==(arm>=2),"postcondition failed");
                if(arm>=2)require(after[arm].samples-before[arm].samples==(std::uint64_t)(2*callbacks*c.block),"clock count failed");
                auto sorted=durations[arm];std::sort(sorted.begin(),sorted.end());
                const double median=.5*((double)sorted[(size_t)callbacks/2-1]+(double)sorted[(size_t)callbacks/2]);
                const double budget=1e9*c.block/c.rate;
                const int overruns=(int)std::count_if(sorted.begin(),sorted.end(),[budget](auto ns){return ns>budget;});
                std::fprintf(aggregate,"%zu,%s,%s,%.0f,%d,%s,%s,%d,%s,%d,%.1f,%lld,%lld,%d,%d,%d,%.17g,%.17g,%.17g,%d,%d,%llu,%llu\n",
                    cell,arm>=2?"candidate":"baseline",arm%2?"FTZ_DAZ_ON":"FTZ_DAZ_OFF",c.rate,c.block,scene(c.scene),c.moving?"moving":"static",c.voices,c.retired?"retired":c.voices==0?"cold":"held",callbacks,
                    median,(long long)sorted[(size_t)std::ceil(.99*callbacks)-1],(long long)sorted.back(),overruns,denorm[arm],underflow[arm],sums[arm],after[arm].stateChecksum,after[arm].filterEnvelopeChecksum,after[arm].effective,after[arm].active,(unsigned long long)after[arm].samples,(unsigned long long)after[arm].rejected);
            }
            for(int engine=0;engine<2;++engine)
            {
                const auto& d=differences[engine];
                std::fprintf(numeric,"%zu,%s,%llu,%llu,%llu,%llu,%.17g,%.17g,%llu,%llu,%.17g,%.17g,%.17g,%.17g\n",cell,engine?"candidate":"baseline",
                    (unsigned long long)d.samples,(unsigned long long)d.bits,(unsigned long long)d.nonzero,(unsigned long long)d.signedZero,d.maxAbs,std::sqrt(d.squared/(double)d.samples),
                    (unsigned long long)d.subnormal[0],(unsigned long long)d.subnormal[1],after[engine*2].stateChecksum,after[engine*2+1].stateChecksum,after[engine*2].filterEnvelopeChecksum,after[engine*2+1].filterEnvelopeChecksum);
            }
            std::fflush(aggregate);std::fflush(numeric);
        }
        require(std::fclose(aggregate)==0&&std::fclose(numeric)==0,"output close");
        FILE* samples=std::fopen((dir/"callback-times.csv").c_str(),"w");require(samples,"raw open");
        std::fprintf(samples,"cell,arm,callback,order,nanoseconds,mxcsr_before,mxcsr_after\n");
        for(const auto& r:raw)std::fprintf(samples,"%d,%d,%d,%d,%lld,%u,%u\n",r.cell,r.arm,r.callback,r.order,(long long)r.ns,r.before,r.after);
        require(std::fclose(samples)==0,"raw close");
        std::printf("FP policy %s: %zu cells, %zu observations, entry MXCSR=0x%08x; no cost gate\n",verify?"UNTIMED VERIFICATION":"diagnostic",configs.size(),raw.size(),entry);
    }
    catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
    return 0;
}
