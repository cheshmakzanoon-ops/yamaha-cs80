#define main cs80_archived_benchmark_main
#include "tools/source_factory_benchmark.cpp"
#undef main
#include <bit>
#include <cerrno>
#include <cfenv>

unsigned long long cs80_cold_attempts = 0, cs80_cold_admitted = 0;
constexpr int B = 64;
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    ScopedDenormals denormals;
    std::ofstream out(argv[1],std::ios::binary);
    auto record = [&](auto t) { out.write(reinterpret_cast<const char*>(&t),sizeof(t)); };
    for (int s=0; s<5; ++s) {
        Case c;
        c.block=B; c.isolated=false; c.sourcePerformance=(s!=4);
        c.selector=(s==4?13:0);
        const auto frame=makeFrame(c,c.selector);
        Engine engine; engine.prepare(c.rate); if (!engine.setControlFrame(frame)) return 3; engine.reset();
        std::array<float,B*2> pcm{};
        const auto step = [&](unsigned i, bool save) {
            if (!engine.setControlFrame(frame)) std::abort();
            if (s==3) engine.channelPressure(1, (i%61)/60.f);
            engine.render(pcm.data(),B);
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
        if (s!=0) for (int n:notes) engine.noteOn(n,.8f);
        if (s==2) {
            for (unsigned i=0;i<1024;++i) step(i,false);
            for (int n:notes) engine.noteOff(n);
            for (unsigned i=0;i<8192;++i) step(i,false);
        }
        cs80_cold_attempts=cs80_cold_admitted=0;
        for (unsigned i=0;i<256;++i) step(i,true);
        std::cout<<"scene "<<s<<" admitted "<<cs80_cold_admitted<<"/"<<cs80_cold_attempts<<"\n";
        if (s==0) {
            for (int n:notes) engine.noteOn(n,.8f);
            cs80_cold_attempts=cs80_cold_admitted=0;
            for(unsigned i=0;i<128;++i) step(i,true);
            std::cout<<"wake admitted "<<cs80_cold_admitted<<"/"<<cs80_cold_attempts<<"\n";
        }
    }
    return out.good()?0:4;
}
