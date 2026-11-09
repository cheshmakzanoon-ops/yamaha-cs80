#include "core/ReconstructedOscillator.h"
#define cs80 cs80_osc_profile_baseline
#include "fixtures/ReconstructedOscillatorPreProfileCache.h"
#undef cs80
#include <bit>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <string_view>
#include <type_traits>
#include <vector>
#if defined(__linux__)
#include <sched.h>
#endif

#if !defined(__GNUC__) && !defined(__clang__)
#error This complete-object cost probe requires the documented GNU/Clang materialization barrier.
#endif
namespace {
constexpr double rate=96000.;
struct Input { double frequency,threshold;bool saw,pulse; };
struct Case { const char* name;int scenario,count;bool idle; };
struct Result { std::int64_t ns=0;std::uint64_t checksum=0;bool failed=false;int cpuStart=-1,cpuEnd=-1; };
int cpu() noexcept {
#if defined(__linux__)
    return sched_getcpu();
#else
    return -1;
#endif
}
template<class Osc> std::uint64_t stateHash(const Osc& o,std::uint64_t h) {
    const auto add=[&](auto x){if constexpr(std::is_same_v<decltype(x),double>)h^=std::bit_cast<std::uint64_t>(x);else h^=std::uint64_t(x);h*=1099511628211ull;};
    const auto pins=[&](const auto& p){add(p.sawPin5);add(p.pulsePin7);add(p.sinePin2);};
    const auto& s=o.getState();add(s.charge);add(s.resetRemainingSeconds);add(s.inputBaselineVolts);add(s.triangleBaselineVolts);
    add(s.heldFrequencyHz);add(s.heldThresholdVolts);add(s.elapsedSeconds);add(s.sawEnabled);add(s.pulseEnabled);add(s.residualCursor);add(s.resetCount);
    for(const auto& r:s.residual)for(double x:r)add(x);
    const auto& d=o.getDiagnostics();add(d.invalidControl);add(d.frequencyLimited);add(d.eventCapacityExceeded);add(d.eventCount);
    add(d.rawPin7Volts);add(d.coupledInputVolts);add(d.trianglePin3Volts);add(d.shaperInputVolts);pins(d.raw);
    for(const auto& e:d.events){add(e.secondsFromSampleStart);add(e.absoluteSeconds);add(e.charge);add(e.kind);add(e.resetActive);pins(e.before);pins(e.after);}
    add(o.maximumFrequencyHz());add(o.sampleRate());return h;
}
std::vector<Input> makeInputs(const Case& c,int frames) {
    std::vector<Input> inputs;inputs.reserve((std::size_t) frames);std::uint32_t random=0x16498d7u;
    for(int i=0;i<frames;++i){const double phase=double(i)*.00317;double frequency=c.scenario==0?55.:c.scenario==2?8372.:261.6;
        double threshold=.13;bool saw=true,pulse=true;
        if(c.scenario==3){frequency=440.*std::exp2(2.*std::sin(phase*.17));threshold=1.45*std::sin(phase);}
        if(c.scenario==4){random=random*1664525u+1013904223u;frequency=60.+12000.*double(random&65535u)/65535.;threshold=1.5*std::sin(phase*13.);saw=i%7!=0;pulse=i%11!=0;}
        inputs.push_back({frequency,threshold,saw,pulse});}
    return inputs;
}
template<class Osc> Result run(const Case& c,const std::vector<Input>& input,int measured) {
    std::vector<Osc> oscillators((std::size_t)c.count);std::array<double,32> detune{};
    for(int i=0;i<c.count;++i){detune[(std::size_t)i]=1.+double(i%8)*.0007;oscillators[(std::size_t)i].reset(double(i)/double(c.count),input[0].frequency*detune[(std::size_t)i]);}
    const int warm=c.idle?1024:16384;const int steps=c.idle?measured/16:measured;
    auto process=[&](int frame,double& sum){const auto& in=input[(std::size_t)frame];
        for(int i=0;i<c.count;++i){auto& oscillator=oscillators[(std::size_t)i];
            if(c.idle){const auto value=oscillator.advanceIdle(16./rate,in.frequency*detune[(std::size_t)i],{in.threshold,in.saw,in.pulse});sum+=value;asm volatile("" : : "m"(oscillator),"m"(value) );}
            else{const auto value=oscillator.tick(in.frequency*detune[(std::size_t)i],{in.threshold,in.saw,in.pulse});sum+=value.sawPin5+.5*value.pulsePin7+.25*value.sinePin2;asm volatile("" : : "m"(oscillator),"m"(value) );}}};
    double sum=0.;for(int i=0;i<warm;++i)process(i,sum);sum=0.;
    const int cpuStart=cpu();const auto start=std::chrono::steady_clock::now();for(int i=0;i<steps;++i)process(warm+i,sum);const auto end=std::chrono::steady_clock::now();const int cpuEnd=cpu();
    bool failed=!std::isfinite(sum);for(const auto& o:oscillators)failed|=o.getDiagnostics().eventCapacityExceeded||o.getDiagnostics().invalidControl;
    std::uint64_t hash=std::bit_cast<std::uint64_t>(sum);for(const auto& o:oscillators)hash=stateHash(o,hash);
    return {std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count(),hash,failed,cpuStart,cpuEnd};
}
}
int main(int argc,char** argv){
    int frames=32768,pairs=9;
    const auto parse=[](const char* arg,int& target){std::string_view s(arg);const auto r=std::from_chars(s.data(),s.data()+s.size(),target);return r.ec==std::errc{}&&r.ptr==s.data()+s.size();};
    if(argc>3||(argc>1&&!parse(argv[1],frames))||(argc>2&&!parse(argv[2],pairs))||frames<1024||frames>1048576||frames%16||pairs<1||pairs>31)return 2;
    #if defined(__linux__)
    cpu_set_t affinity;CPU_ZERO(&affinity);const int affinityResult=sched_getaffinity(0,sizeof(affinity),&affinity);
    std::fprintf(stderr,"child affinity status=%d cpus=",affinityResult);for(int i=0;affinityResult==0&&i<CPU_SETSIZE;++i)if(CPU_ISSET(i,&affinity))std::fprintf(stderr,"%d;",i);std::fprintf(stderr,"\n");
#endif
    std::printf("scenario,oscillators,round,order,version,frames,evaluations,elapsed_ns,rtf,checksum,failed,cpu_start,cpu_end\n");
    int failures=0;
    for(int count:{16,32})for(int scenario=0;scenario<6;++scenario){
        const char* names[]={"held-low","held-middle","held-high","moving-pitch-pwm","dense-controls","idle"};
        const Case c{names[scenario],scenario==5?1:scenario,count,scenario==5};const int warm=c.idle?1024:16384,steps=c.idle?frames/16:frames;
        const auto input=makeInputs(c,warm+steps);
        for(int round=0;round<pairs;++round){Result result[2];
            for(int order=0;order<2;++order){const int version=(order+round)%2;
                result[version]=version==0?run<cs80_osc_profile_baseline::ReconstructedOscillator>(c,input,frames):run<cs80::ReconstructedOscillator>(c,input,frames);
                const auto& r=result[version];const double rtf=double(r.ns)*1e-9/(double(frames)/rate);
                std::printf("%s,%d,%d,%d,%s,%d,%d,%lld,%.12g,%llu,%d,%d,%d\n",c.name,count,round,order,version==0?"baseline":"candidate",frames,steps*count,(long long)r.ns,rtf,(unsigned long long)r.checksum,r.failed?1:0,r.cpuStart,r.cpuEnd);}
            failures+=(result[0].checksum!=result[1].checksum||result[0].failed||result[1].failed)?1:0;}}
    std::fprintf(stderr,"Oscillator profile-cache paired probe: %d failures; sizes baseline=%zu candidate=%zu\n",failures,sizeof(cs80_osc_profile_baseline::ReconstructedOscillator),sizeof(cs80::ReconstructedOscillator));
    return failures?1:0;
}
