#include "core/Engine.h"
#include <cstdlib>
#include <new>
#include <iostream>
#include <memory>
#include <array>

namespace probe {
thread_local bool armed=false;
thread_local unsigned calls=0, frees=0;
void release() noexcept { if(armed) ++frees; }
void hit() noexcept { if(armed) ++calls; }
}
extern "C" {
void __real_free(void*);
void __wrap_free(void* p) { if(p) probe::release(); __real_free(p); }
void* __real_malloc(size_t);
void* __real_calloc(size_t,size_t);
void* __real_realloc(void*,size_t);
void* __real_aligned_alloc(size_t,size_t);
int __real_posix_memalign(void**,size_t,size_t);
void* __wrap_malloc(size_t n) { probe::hit(); return __real_malloc(n); }
void* __wrap_calloc(size_t n,size_t s) { probe::hit(); return __real_calloc(n,s); }
void* __wrap_realloc(void* p,size_t n) { probe::hit(); return __real_realloc(p,n); }
void* __wrap_aligned_alloc(size_t a,size_t n) { probe::hit(); return __real_aligned_alloc(a,n); }
int __wrap_posix_memalign(void** p,size_t a,size_t n) { probe::hit(); return __real_posix_memalign(p,a,n); }
}
void* operator new(size_t n) { probe::hit(); if(auto* p=__real_malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if(p) probe::release(); __real_free(p); }
void operator delete[](void* p) noexcept { if(p) probe::release(); __real_free(p); }
void operator delete(void* p,size_t) noexcept { if(p) probe::release(); __real_free(p); }
void operator delete[](void* p,size_t) noexcept { if(p) probe::release(); __real_free(p); }
void* operator new(size_t n,std::align_val_t a) { probe::hit(); void* p=nullptr; if(__real_posix_memalign(&p,size_t(a),n ? n : 1)==0) return p; throw std::bad_alloc(); }
void* operator new[](size_t n,std::align_val_t a) { return ::operator new(n,a); }
void operator delete(void* p,std::align_val_t) noexcept { if(p) probe::release(); __real_free(p); }
void operator delete[](void* p,std::align_val_t) noexcept { if(p) probe::release(); __real_free(p); }
void operator delete(void* p,size_t,std::align_val_t) noexcept { if(p) probe::release(); __real_free(p); }
void operator delete[](void* p,size_t,std::align_val_t) noexcept { if(p) probe::release(); __real_free(p); }
void* operator new(size_t n,const std::nothrow_t&) noexcept { try { return ::operator new(n); } catch(...) { return nullptr; } }
void* operator new[](size_t n,const std::nothrow_t&) noexcept { return ::operator new(n,std::nothrow); }
void operator delete(void* p,const std::nothrow_t&) noexcept { if(p) probe::release(); __real_free(p); }
void operator delete[](void* p,const std::nothrow_t&) noexcept { if(p) probe::release(); __real_free(p); }
void* operator new(size_t n,std::align_val_t a,const std::nothrow_t&) noexcept { try { return ::operator new(n,a); } catch(...) { return nullptr; } }
void* operator new[](size_t n,std::align_val_t a,const std::nothrow_t&) noexcept { return ::operator new(n,a,std::nothrow); }
void operator delete(void* p,std::align_val_t,const std::nothrow_t&) noexcept { if(p) probe::release(); __real_free(p); }
void operator delete[](void* p,std::align_val_t,const std::nothrow_t&) noexcept { if(p) probe::release(); __real_free(p); }


#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

// This harness deliberately compiles twice against matching headers and core
// archives. It never reads the changing factory bank. The first 198 controls
// are identical; appended current-version size fields stay at neutral 1.
using namespace cs80;
constexpr int hostRate = 48000;
constexpr int blockFrames = 256;
constexpr int measuredFrames = hostRate;
constexpr int warmupFrames = 8192;

struct Scenario { const char* name; int model; int firstNote; int engines; };
constexpr Scenario scenarios[] = {
    { "body_off_8", body::off, 48, 1 },
    { "piano_low_8", body::piano, 36, 1 },
    { "piano_mid_8", body::piano, 60, 1 },
    { "tine_8", body::tine, 48, 1 },
    { "bowed_8", body::bowed, 48, 1 },
    { "brass_8", body::brass, 48, 1 },
    { "piano_low_16_proxy", body::piano, 36, 2 }
};
constexpr int chord[] = { 0, 4, 7, 11, 12, 16, 19, 23 };

ParamValues patch(int model)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f;
    p[P_parityMode] = p[P_kasScanTiming] = 0.0f;
    p[P_oversampling] = 1.0f; // Standard, reported effective factor must be 2.
    p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = 0.0f;
    p[P_mix] = 0.4f; p[P_detune] = 3.2f; p[P_volume] = 0.4f;
    p[P_subVco] = p[P_subVcf] = p[P_subAtVco] = p[P_subAtVcf] = 0.0f;
    p[P_chorusOn] = 1.0f; p[P_chorusDepth] = 0.2f; p[P_chorusSpeed] = 0.25f;
    for (int ch = 0; ch < 2; ++ch)
    {
        p[bodyParam(ch, 0)] = static_cast<float>(model);
        p[bodyParam(ch, 1)] = model == body::off ? 0.0f : 0.85f;
        p[bodyParam(ch, 2)] = ch == 0 ? 0.55f : 0.70f;
        p[bodyParam(ch, 3)] = ch == 0 ? 1.20f : 0.80f;
        p[chParam(ch, CP_feet)] = 1.0f; // 8 foot; notes are sounding register.
        p[chParam(ch, CP_hpfCutoff)] = ch == 0 ? 40.0f : 220.0f;
        p[chParam(ch, CP_lpfCutoff)] = ch == 0 ? 6000.0f : 8000.0f;
        p[chParam(ch, CP_hpfRes)] = 0.04f;
        p[chParam(ch, CP_lpfRes)] = 0.12f;
        p[chParam(ch, CP_hpfOn)] = p[chParam(ch, CP_lpfOn)] = 1.0f;
        p[chParam(ch, CP_fIl)] = 0.0f;
        p[chParam(ch, CP_fAl)] = ch == 0 ? 0.15f : 0.25f;
        p[chParam(ch, CP_fA)] = 0.004f;
        p[chParam(ch, CP_fD)] = 0.9f;
        p[chParam(ch, CP_vA)] = ch == 0 ? 0.004f : 0.005f;
        p[chParam(ch, CP_vD)] = ch == 0 ? 6.0f : 3.0f;
        p[chParam(ch, CP_vS)] = ch == 0 ? 0.75f : 0.55f;
        p[chParam(ch, CP_vR)] = 0.8f;
        p[chParam(ch, CP_vLevel)] = ch == 0 ? 0.36f : 0.32f;
        p[chParam(ch, CP_initBrill)] = 0.12f;
        p[chParam(ch, CP_afterBrill)] = 0.20f;
    }
    return p;
}

std::uint64_t hashBytes(std::uint64_t hash, const void* ptr, std::size_t size)
{
    const auto* data = static_cast<const unsigned char*>(ptr);
    for (std::size_t i = 0; i < size; ++i) hash = (hash ^ data[i]) * UINT64_C(1099511628211);
    return hash;
}

double threadSeconds()
{
    timespec value{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) != 0) throw std::runtime_error("thread CPU clock failed");
    return static_cast<double>(value.tv_sec) + 1e-9 * value.tv_nsec;
}

void chordOn(Engine& engine, int firstNote)
{
    for (int delta : chord) engine.noteOn(firstNote + delta, 0.8f);
    engine.channelPressure(1, 0.35f);
}

void runScenario(const Scenario& s)
{
    const auto p = patch(s.model);
    ControlFrame frame; frame.effectiveValues = p;
    auto a = std::make_unique<Engine>();
    auto b = s.engines == 2 ? std::make_unique<Engine>() : nullptr;
    std::array<Engine*, 2> engines { a.get(), b.get() };
    std::vector<float> audio(static_cast<std::size_t>(s.engines * measuredFrames * 2));
    std::array<float, blockFrames * 2> warmup{};
    for (int e = 0; e < s.engines; ++e)
    {
        engines[e]->prepare(hostRate);
        if (!engines[e]->setControlFrame(frame)) throw std::runtime_error("fixture frame rejected");
        if (engines[e]->getRenderPolicy() != RenderPolicy::sourcePerformance
            || engines[e]->getOversamplingFactor() != 2) throw std::runtime_error("wrong render policy or factor");
        chordOn(*engines[e], s.firstNote + e);
        for (int offset = 0; offset < warmupFrames; offset += blockFrames)
            engines[e]->render(warmup.data(), blockFrames);
    }
    const double start = threadSeconds();
    for (int offset = 0; offset < measuredFrames; offset += blockFrames)
    {
        const int frames = std::min(blockFrames, measuredFrames - offset);
        for (int e = 0; e < s.engines; ++e)
        {
            auto& engine = *engines[e];
            if (!engine.setControlFrame(frame)) throw std::runtime_error("held frame rejected");
            // Reattack during the measured interval so struck bodies are not
            // benchmarked only after their useful upper partials have faded.
            if (offset == 24576)
            {
                for (int delta : chord) engine.noteOff(s.firstNote + e + delta);
                chordOn(engine, s.firstNote + e);
            }
            engine.render(audio.data() + static_cast<std::size_t>(2 * (e * measuredFrames + offset)), frames);
        }
    }
    const double seconds = threadSeconds() - start;
    double peak = 0.0, energy = 0.0;
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (float sample : audio)
    {
        if (!std::isfinite(sample)) throw std::runtime_error("nonfinite benchmark sample");
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
        energy += static_cast<double>(sample) * sample;
        hash = hashBytes(hash, &sample, sizeof(sample));
    }
    const auto controlsHash = hashBytes(UINT64_C(14695981039346656037), p.v.data(), 198 * sizeof(float));
    std::printf("{\"scenario\":\"%s\",\"cpu_seconds\":%.9f,\"audio_seconds\":1,\"rate\":%d,\"voices\":%d,\"engines\":%d,\"oversampling\":2,\"core_parameters\":%d,\"engine_bytes\":%zu,\"body_bytes\":%zu,\"prefix198_hash\":\"%016llx\",\"audio_hash\":\"%016llx\",\"peak\":%.9g,\"rms\":%.9g}\n",
        s.name, seconds, hostRate, 8 * s.engines, s.engines, P_numParams, sizeof(Engine), sizeof(body::InstrumentBody),
        static_cast<unsigned long long>(controlsHash), static_cast<unsigned long long>(hash), peak, std::sqrt(energy / audio.size()));
}

int allocations()
{
    auto* (*volatile allocate)(size_t) = &std::malloc;
    void (*volatile release)(void*) = &std::free;
    probe::calls = probe::frees = 0;
    probe::armed = true;
    auto* p = allocate(17); release(p);
    auto* q = ::operator new(23); ::operator delete(q);
    probe::armed = false;
    const bool positive = probe::calls == 2 && probe::frees == 2;
    if (!positive) throw std::runtime_error("allocator positive control failed");
    unsigned totalCalls = 0, totalFrees = 0;
    int cases = 0;
    for (int model = body::off; model <= body::maxModel; ++model)
    {
        auto engine = std::make_unique<Engine>();
        engine->prepare(hostRate);
        auto params = patch(model);
        ControlFrame frame; frame.effectiveValues = params;
        if (!engine->setControlFrame(frame)) throw std::runtime_error("allocation fixture rejected");
        std::array<float, blockFrames * 2> audio{};
        probe::calls = probe::frees = 0;
        probe::armed = true;
        chordOn(*engine, 36);
        for (int i = 0; i < 4; ++i) engine->render(audio.data(), blockFrames);
        engine->channelPressure(1, 0.9f);
        engine->setControlFrame(frame);
        engine->render(audio.data(), blockFrames);
        engine->noteOff(36);
        engine->render(audio.data(), 37);
        engine->noteOn(84, 0.9f); // released-voice handoff
        engine->render(audio.data(), blockFrames);
        engine->noteOn(85, 0.9f); // occupied voice reassignment
        engine->render(audio.data(), blockFrames);
        engine->allNotesOff(false);
        engine->render(audio.data(), blockFrames);
        probe::armed = false;
        totalCalls += probe::calls; totalFrees += probe::frees; ++cases;
    }
    std::printf("{\"positive_control\":true,\"cases\":%d,\"rate\":%d,\"allocations\":%u,\"deallocations\":%u,\"scope\":\"static-core C/C++ allocation entry points during note/controller/render/handoff/reassignment/panic; host wrapper excluded\"}\n", cases, hostRate, totalCalls, totalFrees);
    return totalCalls || totalFrees ? 1 : 0;
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) throw std::runtime_error("provide a scenario name or --allocations");
        if (std::strcmp(argv[1], "--allocations") == 0) return allocations();
        for (const auto& scenario : scenarios)
            if (std::strcmp(argv[1], scenario.name) == 0) { runScenario(scenario); return 0; }
        throw std::runtime_error("unknown scenario");
    }
    catch (const std::exception& error)
    {
        probe::armed = false;
        std::fprintf(stderr, "runtime probe failed: %s\n", error.what());
        return 2;
    }
}
