#define main cs80_archived_benchmark_main
#include "tools/source_factory_benchmark.cpp"
#undef main
#include <bit>
#include <cerrno>
#include <cfenv>
#include <valgrind/callgrind.h>

unsigned long long cs80_cold_attempts = 0, cs80_cold_admitted = 0;
constexpr int B = 64;
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    const int task=std::stoi(argv[2]), window=std::stoi(argv[3]);
    if (task<0||task>7||window<0||window>2) return 2;
    ScopedDenormals denormals;
    std::ofstream out(argv[1],std::ios::binary);
    auto record = [&](auto t) { out.write(reinterpret_cast<const char*>(&t),sizeof(t)); };
    for (int s=task; s<=task; ++s) {
        int traceIndex=0;
        Case c;
        c.block=B; c.isolated=false; c.sourcePerformance=(s!=7);
        c.selector=(s==7?13:0);
        auto frame=makeFrame(c,c.selector);
        frame.effectiveValues[P_drift]=.37f;
        frame.effectiveValues[P_voiceDispersion]=.18f;
        frame.effectiveValues[P_driftModel]=0.f;
        Engine engine; engine.prepare(c.rate); if (!engine.setControlFrame(frame)) return 3; engine.reset();
        std::array<float,B*2> pcm{};
        const auto step = [&](unsigned i, bool save) {
            if (!engine.setControlFrame(frame)) std::abort();
            if (s==3) engine.channelPressure(1, (i%61)/60.f);
            if (s==6) engine.polyPressure(notes[i%8], (i%61)/60.f, int(i%8+2));
            const int measured = save ? traceIndex++ : -1;
            const bool sample=measured>=0 && ((window!=2 && measured<4) || (window!=0 && measured==4));
            if (measured==0) { CALLGRIND_START_INSTRUMENTATION; }
            if (sample) CALLGRIND_TOGGLE_COLLECT;
            engine.render(pcm.data(),B);
            if (sample) CALLGRIND_TOGGLE_COLLECT;
            if (measured==4) { CALLGRIND_DUMP_STATS; CALLGRIND_STOP_INSTRUMENTATION; }
            if (!save) return;
            for (float sample : pcm) {
                if (!std::isfinite(sample)) std::abort();
                record(std::bit_cast<std::uint32_t>(sample));
            }
            record(engine.activeVoices());
            for (int rank=0;rank<2;++rank) {
                const auto r=engine.getSourceCutoffReadout(rank);
                record(r.saturatedVoices);
                for (const auto& channels:r.pinVolts) for (double v:channels) record(v);
                for (const auto& channels:r.effectiveCv) for (double v:channels) record(v);
                for (double v:r.loadedBusVolts) record(v);
            }
            unsigned short x87state,x87control;
            asm volatile("fnstsw %0; fnstcw %1":"=m"(x87state),"=m"(x87control)::"memory");
            record(_mm_getcsr()); record(x87state); record(x87control); record(errno);
        };
        for (unsigned i=0;i<1024;++i) step(i,false);
        if (s!=0) for (unsigned lane=0;lane<notes.size();++lane) engine.noteOn(notes[lane],s>=4?float(64+lane*7)/127.f:102.f/127.f, s==6?int(lane+2):1);
        if (s==2 || s==5) {
            for (unsigned i=0;i<1024;++i) step(i,false);
            for (int n:notes) engine.noteOff(n);
            for (unsigned i=0;i<8192;++i) step(i,false);
        }
        cs80_cold_attempts=cs80_cold_admitted=0;
        for (unsigned i=0;i<5;++i) step(i,true);
        std::cout<<"scene "<<s<<" admitted "<<cs80_cold_admitted<<"/"<<cs80_cold_attempts<<"\n";
        if (s==0) {
            for (unsigned lane=0;lane<notes.size();++lane) engine.noteOn(notes[lane],s>=4?float(64+lane*7)/127.f:102.f/127.f, s==6?int(lane+2):1);
            cs80_cold_attempts=cs80_cold_admitted=0;
            for(unsigned i=0;i<5;++i) step(i,true);
            std::cout<<"wake admitted "<<cs80_cold_admitted<<"/"<<cs80_cold_attempts<<"\n";
        }
    }
    return out.good()?0:4;
}
