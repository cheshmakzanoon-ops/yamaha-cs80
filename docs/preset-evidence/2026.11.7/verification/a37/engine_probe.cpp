// A37 offline evidence: complete Engine, matching source snapshot and core ABI.
// No plugin wrapper or host scheduling is measured. No PCM files are written.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <sched.h>
#include <time.h>
#include <vector>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif

namespace allocation {
struct Counts { std::uint64_t calls = 0, bytes = 0, frees = 0; };
bool watching = false;
Counts counts;
void begin() noexcept { counts = {}; watching = true; }
Counts end() noexcept { watching = false; return counts; }
void record(std::size_t n) noexcept { if (watching) { ++counts.calls; counts.bytes += n; } }
void release(void* p) noexcept { if (p && watching) ++counts.frees; std::free(p); }
}
void* operator new(std::size_t n) {
    allocation::record(n);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { allocation::release(p); }
void operator delete[](void* p) noexcept { allocation::release(p); }
void operator delete(void* p, std::size_t) noexcept { allocation::release(p); }
void operator delete[](void* p, std::size_t) noexcept { allocation::release(p); }
void* operator new(std::size_t n, std::align_val_t a) {
    allocation::record(n); void* p = nullptr;
    if (posix_memalign(&p, static_cast<std::size_t>(a), n ? n : 1) == 0) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept { allocation::release(p); }
void operator delete[](void* p, std::align_val_t) noexcept { allocation::release(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { allocation::release(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { allocation::release(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    try { return ::operator new(n); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return ::operator new(n, std::nothrow); }
void operator delete(void* p, const std::nothrow_t&) noexcept { allocation::release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { allocation::release(p); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
    try { return ::operator new(n, a); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { return ::operator new(n, a, std::nothrow); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { allocation::release(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { allocation::release(p); }

constexpr int hostRate = 48000, blockSize = 256, warmFrames = 96000, timedFrames = 65536;
struct Scene { const char* id; const char* preset; std::array<int, 8> notes; int count; };
constexpr Scene scenes[] = {
    {"ah_low_one", "Choir Pad", {36}, 1},
    {"ah_low_eight", "Choir Pad", {36,38,40,41,43,45,47,48}, 8},
    {"oo_low_eight", "Dark Choir", {36,38,40,41,43,45,47,48}, 8},
    {"ah_high_eight", "Choir Pad", {72,74,76,77,79,81,83,84}, 8}
};
std::uint64_t clockNs(clockid_t id) {
    timespec t{};
    if (clock_gettime(id, &t)) std::abort();
    return static_cast<std::uint64_t>(t.tv_sec)*1000000000ull + t.tv_nsec;
}
std::uint64_t hashWords(const float* input, std::size_t count) {
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t i=0;i<count;++i) {
        std::uint32_t bits; std::memcpy(&bits,input+i,sizeof bits);
        hash ^= bits; hash *= 1099511628211ull;
    }
    return hash;
}
void printCounts(const char* name, allocation::Counts c) {
    std::printf("\"%s\":{\"calls\":%llu,\"bytes\":%llu,\"frees\":%llu}", name,
        static_cast<unsigned long long>(c.calls),static_cast<unsigned long long>(c.bytes),
        static_cast<unsigned long long>(c.frees));
}
bool trial(int index, const cs80::ParamValues& values, std::vector<float>& output) {
    const auto& scene=scenes[index];
    cs80::ControlFrame frame; frame.effectiveValues=values;
    allocation::begin();
    auto engine=std::make_unique<cs80::Engine>();
    const auto construct=allocation::end();
    allocation::begin(); engine->setControlFrame(frame); const auto configure=allocation::end();
    allocation::begin(); engine->prepare(hostRate); const auto prepare=allocation::end();
    allocation::begin();
    for(int i=0;i<scene.count;++i) engine->noteOn(scene.notes[i],.85f,1);
    const auto noteOn=allocation::end();
    allocation::begin(); engine->render(output.data(),blockSize); const auto onsetRender=allocation::end();
    allocation::begin();
    for(int at=blockSize;at<warmFrames;at+=blockSize) {
        engine->setControlFrame(frame); engine->render(output.data(),blockSize);
    }
    const auto warmRender=allocation::end();
    const auto wallStart=clockNs(CLOCK_MONOTONIC);
    const auto cpuStart=clockNs(CLOCK_PROCESS_CPUTIME_ID);
    allocation::begin();
    for(int at=0;at<timedFrames;at+=blockSize) {
        engine->setControlFrame(frame); engine->render(output.data()+2*at,blockSize);
    }
    const auto measured=allocation::end();
    const auto cpu=clockNs(CLOCK_PROCESS_CPUTIME_ID)-cpuStart;
    const auto wall=clockNs(CLOCK_MONOTONIC)-wallStart;
    const auto pcmHash=hashWords(output.data(),output.size());
    double energy=0,peak=0; std::uint64_t nonfinite=0;
    for(float x: output) {
        if (!std::isfinite(x)) ++nonfinite;
        energy+=static_cast<double>(x)*x; peak=std::max(peak,std::abs(static_cast<double>(x)));
    }
    const int voices=engine->activeVoices(), factor=engine->getOversamplingFactor();
    // Separate, untimed ordinary lifecycle observation. Rate transitions are
    // possible from processBlock; these are not included in steady render CPU.
    allocation::begin();
    auto quality=frame; quality.effectiveValues[cs80::P_oversampling]=2.f;
    engine->setControlFrame(quality); engine->render(output.data(),blockSize);
    engine->setControlFrame(frame); engine->setNonRealtime(true);
    engine->render(output.data(),blockSize); engine->setNonRealtime(false);
    const auto transitions=allocation::end();
    allocation::begin(); engine.reset(); const auto destruction=allocation::end();
    std::printf("{\"case\":\"%s\",\"preset\":\"%s\",\"voices\":%d,\"factor\":%d,\"policy\":%d,\"cpu_ns\":%llu,\"wall_ns\":%llu,\"pcm_word_fnv1a64\":\"%016llx\",\"params_word_fnv1a64\":\"%016llx\",\"nonfinite\":%llu,\"peak\":%.17g,\"rms\":%.17g,\"allocation\":{",scene.id,scene.preset,
        voices,factor,static_cast<int>(cs80::renderPolicy(values[cs80::P_parityMode],values[cs80::P_sourcePerformance])),
        static_cast<unsigned long long>(cpu),static_cast<unsigned long long>(wall),
        static_cast<unsigned long long>(pcmHash),static_cast<unsigned long long>(hashWords(values.v.data(),values.v.size())),
        static_cast<unsigned long long>(nonfinite),peak,std::sqrt(energy/output.size()));
    printCounts("construct",construct); std::printf(","); printCounts("set_frame_before_prepare",configure);
    std::printf(","); printCounts("prepare",prepare); std::printf(","); printCounts("note_on_api",noteOn);
    std::printf(","); printCounts("onset_render",onsetRender); std::printf(","); printCounts("warm_render",warmRender);
    std::printf(","); printCounts("timed_frame_and_render",measured); std::printf(","); printCounts("quality_offline_transitions",transitions);
    std::printf(","); printCounts("destruction",destruction); std::printf("}}\n"); std::fflush(stdout);
    return voices==scene.count && factor==2 && nonfinite==0;
}
int main() {
    cpu_set_t available; CPU_ZERO(&available); int pinned=-1;
    if(!sched_getaffinity(0,sizeof available,&available))
        for(int i=0;i<CPU_SETSIZE;++i) if(CPU_ISSET(i,&available)) {
            cpu_set_t one; CPU_ZERO(&one); CPU_SET(i,&one);
            if(!sched_setaffinity(0,sizeof one,&one)) pinned=i;
            break;
        }
    unsigned mxcsr=0;
#if defined(__SSE__)
    _mm_setcsr(_mm_getcsr() | 0x8040u); mxcsr=_mm_getcsr();
#endif
    // Validate ordinary and aligned hook coverage without touching any DSP.
    allocation::begin();
    void* plain=::operator new(17); void* aligned=::operator new(32,std::align_val_t(32));
    ::operator delete(plain); ::operator delete(aligned,std::align_val_t(32));
    const auto hook=allocation::end();
    if(hook.calls!=2 || hook.bytes!=49 || hook.frees!=2) return 3;
    const auto& bank=cs80::programLibrary();
    std::array<cs80::ParamValues,4> values;
    for(int i=0;i<4;++i) {
        const auto it=std::find_if(bank.begin(),bank.end(),[&](const auto& p){return p.name==scenes[i].preset;});
        if(it==bank.end()) return 4; values[i]=it->values;
    }
    std::vector<float> output(timedFrames*2);
    std::printf("{\"compiler\":\"%s\",\"pinned_cpu\":%d,\"mxcsr\":%u,\"host_rate\":%d,\"block_frames\":%d,\"warm_frames\":%d,\"timed_frames\":%d,\"sizeof_engine\":%zu,\"sizeof_body\":%zu,\"programs\":%zu,\"hook_control\":{\"calls\":%llu,\"bytes\":%llu,\"frees\":%llu}}\n",__VERSION__,pinned,mxcsr,hostRate,blockSize,warmFrames,timedFrames,
        sizeof(cs80::Engine),sizeof(cs80::body::InstrumentBody),bank.size(),
        static_cast<unsigned long long>(hook.calls),static_cast<unsigned long long>(hook.bytes),static_cast<unsigned long long>(hook.frees));
    std::fflush(stdout);
    int index=-1;
    while(std::scanf("%d",&index)==1) {
        if(index<0) return 0;
        if(index>=4 || !trial(index,values[index],output)) return 5;
    }
    return 0;
}
