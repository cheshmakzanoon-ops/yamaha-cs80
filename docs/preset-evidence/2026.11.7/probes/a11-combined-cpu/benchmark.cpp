// Build with three namespace-isolated header snapshots (see prepare.py).
#include "old/InstrumentBody.h"
#include "pre/InstrumentBody.h"
#include "new/InstrumentBody.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <sched.h>
#include <time.h>
#include <vector>

static bool allocationScope = false;
static std::size_t allocations = 0;
void* operator new(std::size_t n) {
    if (allocationScope) ++allocations;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

struct Case { const char* name; int model, variant, note; double rate; bool bend; };
constexpr Case cases[] = {
    {"ah36",15,3,36,96000,false}, {"ah48",15,3,48,96000,false},
    {"ah60",15,3,60,96000,false}, {"ah84",15,3,84,96000,false},
    {"oo36",15,4,36,96000,false}, {"oo60",15,4,60,96000,false},
    {"ah60_384k",15,3,60,384000,false},
    {"ah36_bend",15,3,36,96000,true}, {"ah60_bend",15,3,60,96000,true},
    {"nylon60",6,0,60,96000,false}, {"steel60",7,0,60,96000,false},
    {"bass48",8,0,48,96000,false}, {"clav60",4,0,60,96000,false},
    {"harpsi60",5,0,60,96000,false},
    {"nylon60_bend",6,0,60,96000,true}, {"steel60_bend",7,0,60,96000,true},
    {"bass48_bend",8,0,48,96000,true}, {"clav60_bend",4,0,60,96000,true},
    {"harpsi60_bend",5,0,60,96000,true},
    {"piano60",1,0,60,96000,false}, {"flute60",10,0,60,96000,false},
    {"organ60",15,0,60,96000,false},
    {"flute60_384k",10,0,60,384000,false}, {"organ60_384k",15,0,60,384000,false}
};
constexpr int samples = 131072, warm = 32768;
struct Result { double cpu, wall; std::uint64_t hash; std::size_t alloc; int modes; };
double processNs() {
    timespec t{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t)) std::abort();
    return static_cast<double>(t.tv_sec) * 1e9 + t.tv_nsec;
}
template<class Body> __attribute__((noinline))
Result run(const Case& c, const std::vector<float>& frequency, std::vector<float>& output) {
    Body body;
    body.prepare(c.rate,48000);
    body.noteOn(c.model,static_cast<float>(c.note),.85f,.6f,1,0x237165e,1,c.variant);
    for(int i=0;i<warm;++i) output[i] = body.tick(frequency[i],.6f,.2f);
    const auto beginWall = std::chrono::steady_clock::now();
    const double beginCpu = processNs();
    allocations = 0;
    allocationScope = true;
    for(int i=0;i<samples;++i) output[i] = body.tick(frequency[i+warm],.6f,.2f);
    allocationScope = false;
    const double cpu = (processNs()-beginCpu)/samples;
    const double wall = std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-beginWall).count()/samples;
    std::uint64_t hash = 14695981039346656037ull;
    for(float f: output) {
        std::uint32_t bits;
        std::memcpy(&bits,&f,sizeof bits);
        hash ^= bits;
        hash *= 1099511628211ull;
    }
    return {cpu,wall,hash,allocations,body.probeModeCount()};
}

int main() {
    cpu_set_t available;
    CPU_ZERO(&available);
    int pinned=-1;
    if(!sched_getaffinity(0,sizeof available,&available))
        for(int i=0;i<CPU_SETSIZE;++i) if(CPU_ISSET(i,&available)) {
            cpu_set_t one; CPU_ZERO(&one); CPU_SET(i,&one);
            if(!sched_setaffinity(0,sizeof one,&one)) pinned=i;
            break;
        }
    std::fprintf(stderr,"compiler=%s pinned_cpu=%d samples=%d warm=%d sizes(old,pre,new)=%zu,%zu,%zu\n",
        __VERSION__,pinned,samples,warm,sizeof(cs80old::body::InstrumentBody),
        sizeof(cs80pre::body::InstrumentBody),sizeof(cs80new::body::InstrumentBody));
    std::printf("case,comparison,block,slot,version,modes,cpu_ns,wall_ns,allocations,pcm_hash\n");
    std::vector<float> frequencies(samples+warm), output(samples);
    std::uint32_t random=0x9913ab17;
    std::uint64_t receipt=0;
    for(const auto& c: cases) {
        const double f0=440.*std::exp2((c.note-69.)/12.);
        for(std::size_t i=0;i<frequencies.size();++i)
            frequencies[i]=static_cast<float>(f0*(c.bend ? std::exp2(2.*std::sin(6.283185307179586*4.*i/c.rate)/12.) : 1.));
        // Equal first-touch and instruction-cache warm-up for all three copies.
        run<cs80old::body::InstrumentBody>(c,frequencies,output);
        run<cs80pre::body::InstrumentBody>(c,frequencies,output);
        run<cs80new::body::InstrumentBody>(c,frequencies,output);
        for(int comparison=0;comparison<2;++comparison)
            for(int block=0;block<5;++block) {
                random ^= random<<13; random ^= random>>17; random ^= random<<5;
                const int invert=random&1;
                for(int slot=0;slot<4;++slot) {
                    const bool useNew=((slot==1 || slot==2) ^ invert);
                    Result r;
                    const char* version;
                    if(useNew) { r=run<cs80new::body::InstrumentBody>(c,frequencies,output); version="new"; }
                    else if(comparison==0) { r=run<cs80old::body::InstrumentBody>(c,frequencies,output); version="old"; }
                    else { r=run<cs80pre::body::InstrumentBody>(c,frequencies,output); version="pre"; }
                    receipt ^= r.hash;
                    std::printf("%s,%s,%d,%d,%s,%d,%.6f,%.6f,%zu,%016llx\n",c.name,
                        comparison==0 ? "old_new" : "pre_new",block,slot,version,r.modes,r.cpu,r.wall,r.alloc,
                        static_cast<unsigned long long>(r.hash));
                }
                std::fflush(stdout);
            }
        std::fprintf(stderr,"completed=%s\n",c.name);
    }
    std::fprintf(stderr,"receipt=%016llx\n",static_cast<unsigned long long>(receipt));
}
