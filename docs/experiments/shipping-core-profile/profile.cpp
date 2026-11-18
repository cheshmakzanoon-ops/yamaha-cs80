// Own-process statistical PC sampling only. Production DSP is an unchanged archive.
#define main source_fixture_main_not_run
#include "tools/source_factory_benchmark.cpp"
#undef main
#include <csignal>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>
#include <sched.h>
#include <cstring>
#include <bit>
#include <cstdio>

namespace {
constexpr unsigned capacity = 65536;
std::uint64_t pcSamples[capacity] {};
volatile sig_atomic_t sampleCount=0, sampleOverflow=0, inCore=0, outsideSamples=0;
void record(int, siginfo_t*, void* context) noexcept {
    if (!inCore) { outsideSamples=outsideSamples+1; return; }
    const auto* u=static_cast<ucontext_t*>(context);
    const auto n=sampleCount;
    if (n>=static_cast<sig_atomic_t>(capacity)) { sampleOverflow=1; return; }
    pcSamples[n]=static_cast<std::uint64_t>(u->uc_mcontext.gregs[REG_RIP]);
    sampleCount=n+1;
}
std::uint64_t processNs() {
    timespec t{}; if(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t))std::abort();
    return static_cast<std::uint64_t>(t.tv_sec)*1000000000ull+t.tv_nsec;
}
void timer(unsigned us) {
    itimerval v{}; v.it_value.tv_sec=us/1000000;v.it_value.tv_usec=us%1000000;
    v.it_interval=v.it_value;if(setitimer(ITIMER_PROF,&v,nullptr))std::abort();
}
[[gnu::noinline]] std::uint64_t calibrationLoop(std::uint64_t x) {
    for(unsigned i=0;i<250000;i++) { x=x*6364136223846793005ull+1;asm volatile("" : "+r"(x)); }
    return x;
}
}
int main(int argc,char**argv) {
    if(argc!=5)return 2;
    const int scene=std::atoi(argv[1]); const unsigned period=std::strtoul(argv[2],nullptr,10);
    const unsigned seconds=std::strtoul(argv[3],nullptr,10); const std::string out=argv[4];
    if(scene<0||scene>2||(period!=1999&&period!=5003)||seconds!=20)return 2;
    const char* labels[]={"factory-held","factory-moving","panel-moving"};
    struct sigaction action{};action.sa_sigaction=record;action.sa_flags=SA_SIGINFO;
    sigemptyset(&action.sa_mask);if(sigaction(SIGPROF,&action,nullptr))return 3;
    const auto c0=processNs(); std::uint64_t spin=1;
    inCore=1;timer(period);
    while(processNs()-c0<150000000ull)spin=calibrationLoop(spin);
    timer(0);inCore=0;const unsigned calibrated=sampleCount;
    if(calibrated<8||sampleOverflow)return 4;
    {
        FILE* f=std::fopen((out+".calibration-pcs").c_str(),"wb");if(!f)return 5;
        std::fwrite(pcSamples,sizeof(pcSamples[0]),calibrated,f);std::fclose(f);
    }
    sampleCount=0;outsideSamples=0;
    ScopedDenormals fp;
    Case c;c.rate=48000;c.block=64;c.selector=scene==2?13:0;c.isolated=false;c.sourcePerformance=true;
    const bool moving=scene!=0;c.performanceProfile=moving?1:0;
    auto frame=makeFrame(c,c.selector);frame.effectiveValues[P_drift]=.37f;
    frame.effectiveValues[P_voiceDispersion]=.18f;frame.effectiveValues[P_driftModel]=0.f;
    Engine eng;eng.prepare(c.rate);if(!eng.setControlFrame(frame))return 6;eng.reset();
    for(int v=0;v<8;v++)eng.noteOn(notes[v],.8f,moving?v+2:1);
    for(int rank=0;rank<2;rank++) {
        auto s=eng.getSourceToneInfo(rank);
        if(s.selector!=c.selector||s.route!=source_factory::route(rank,c.selector,true))return 7;
    }
    std::array<float,128> pcm{};
    const auto step=[&](unsigned block) {
        if(moving&&block%4==0) {
            const int v=(block/4)%8;const float x=float((block*13)%97)/96.f;
            eng.channelPressure(v+2,x);eng.pitchBend(v+2,x*.1f);eng.controlChange(74,x,v+2);
        }
        // Real processor reapplies controls at every callback, not sparse fixture repeats.
        if(!eng.setControlFrame(frame))std::abort();
        eng.render(pcm.data(),64);
    };
    constexpr unsigned warm=1024;
    for(unsigned b=0;b<warm;b++)step(b);
    std::ofstream maps(out+".maps");maps<<std::ifstream("/proc/self/maps").rdbuf();maps.close();
    const int firstCpu=sched_getcpu();const unsigned initialCsr=_mm_getcsr();
    const auto started=processNs();unsigned blocks=0;std::uint64_t nonzero=0,voiceChecks=0;
    std::uint64_t hash=1469598103934665603ull,prefixHash=hash;double sumSquares=0;
    timer(period);
    do {
        inCore=1;step(warm+blocks);inCore=0;
        if(eng.activeVoices()!=8)return 8;
        for(int v=0;v<8;v++) {
            const auto info=eng.getVoiceInfo(v);
            if(!info.active||!info.held||info.note!=notes[v]||info.channel!=(moving?v+2:1))return 9;
            ++voiceChecks;
        }
        for(float x:pcm) {
            if(!std::isfinite(x))return 10;
            nonzero+=x!=0;sumSquares+=double(x)*x;
            hash^=std::bit_cast<std::uint32_t>(x);hash*=1099511628211ull;
        }
        ++blocks;if(blocks==4096)prefixHash=hash;
    } while(processNs()-started<std::uint64_t(seconds)*1000000000ull);
    timer(0);inCore=0;
    const auto elapsed=processNs()-started;
    if(blocks<4096||sampleCount<1000||sampleOverflow||!nonzero||!sumSquares||firstCpu!=sched_getcpu())return 11;
    FILE* f=std::fopen((out+".pcs").c_str(),"wb");if(!f)return 12;
    std::fwrite(pcSamples,sizeof(pcSamples[0]),sampleCount,f);std::fclose(f);
    std::printf("{\"scene\":\"%s\",\"period_us\":%u,\"calibration_samples\":%u,\"calibration_result\":\"%016llx\",\"samples\":%d,\"outside_core_samples\":%d,\"overflow\":%d,\"blocks\":%u,\"frames\":%llu,\"cpu_ns\":%llu,\"cpu\":%d,\"nonzero\":%llu,\"voice_checks\":%llu,\"pcm_hash\":\"%016llx\",\"prefix4096_hash\":\"%016llx\",\"sum_squares\":%.17g,\"mxcsr_start\":%u,\"mxcsr_end\":%u,\"routes\":[%d,%d],\"oversampling\":%d}\n",
      labels[scene],period,calibrated,(unsigned long long)spin,sampleCount,outsideSamples,sampleOverflow,blocks,
      (unsigned long long)blocks*64,(unsigned long long)elapsed,firstCpu,(unsigned long long)nonzero,
      (unsigned long long)voiceChecks,(unsigned long long)hash,(unsigned long long)prefixHash,sumSquares,
      initialCsr,_mm_getcsr(),int(eng.getSourceToneInfo(0).route),int(eng.getSourceToneInfo(1).route),eng.getOversamplingFactor());
}
