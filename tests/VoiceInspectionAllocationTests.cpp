#include "plugin/PluginProcessor.h"
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

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    unsigned checks=0, failures=0;
    auto check=[&](bool ok,const char* label) { ++checks; if(!ok) { ++failures; std::cerr<<"FAIL "<<label<<" allocations="<<probe::calls<<" frees="<<probe::frees<<'\n'; } };
    auto* (*volatile mallocProbe)(size_t)=&std::malloc;
    auto* (*volatile callocProbe)(size_t,size_t)=&std::calloc;
    auto* (*volatile reallocProbe)(void*,size_t)=&std::realloc;
    auto* (*volatile alignedProbe)(size_t,size_t)=&std::aligned_alloc;
    int (*volatile posixProbe)(void**,size_t,size_t)=&posix_memalign;
    void (*volatile freeProbe)(void*)=&std::free;
    probe::calls=probe::frees=0; probe::armed=true;
    auto* allocation=::operator new(32); auto* aligned=::operator new(32,std::align_val_t(32));
    auto* c=callocProbe(1,32); c=reallocProbe(c,64);
    auto* a=alignedProbe(32,64); void* p=nullptr; posixProbe(&p,32,64);
    auto* m=mallocProbe(32);
    ::operator delete(allocation); ::operator delete(aligned,std::align_val_t(32));
    freeProbe(c); freeProbe(a); freeProbe(p); freeProbe(m);
    probe::armed=false;
    check(probe::calls==7 && probe::frees==6,"positive probe observes allocator and deallocator families");
    for (bool circuit : {false,true}) for (bool sixteen : {false,true}) {
        juce::TemporaryFile fixture("-voice-inspection-allocation");
        auto host=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        host->setParamValue(cs80::P_voiceMode,sixteen?1:0);host->prepareToPlay(48000,128);
        juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;midi.ensureSize(4096);
        for(int i=0;i<(sixteen?16:8);++i) midi.addEvent(juce::MidiMessage::noteOn(1,48+i,.7f),0);
        for(int slot=0;slot<16;++slot)for(int rank=0;rank<2;++rank){
            host->requestVoiceInspection(slot,rank,true);audio.clear();
            probe::calls=probe::frees=0;probe::armed=true;host->processBlock(audio,midi);probe::armed=false;
            check(probe::calls==0 && probe::frees==0,"inspection callback allocations/frees");midi.clear();
        }
        audio.setSize(2,0);probe::calls=probe::frees=0;probe::armed=true;host->processBlock(audio,midi);probe::armed=false;
        check(probe::calls==0 && probe::frees==0,"zero-frame inspection callback");
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
