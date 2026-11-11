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
    for(double rate : {44100.,48000.,96000.})
    for(bool circuit : {false,true}) for(bool sixteen : {false,true}) {
        juce::TemporaryFile fixture("-callback-allocation");
        auto host=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
        host->setParamValue(cs80::P_parityMode,0); host->setParamValue(cs80::P_voiceMode,sixteen ? 1.f : 0.f);
        host->setParamValue(cs80::P_tuningMode,2);
        host->prepareToPlay(rate,256);
        juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer midi; midi.ensureSize(4096);
        auto tick=[&](const char* label) {
            probe::calls=probe::frees=0; probe::armed=true; host->processBlock(audio,midi); probe::armed=false;
            check(probe::calls==0 && probe::frees==0,label); midi.clear();
        };
        tick("first prepared callback");
        for(int note=0;note<(sixteen ? 16 : 8);++note) midi.addEvent(juce::MidiMessage::noteOn(1+note%2,48+note,.8f),note);
        tick("polyphonic onset callback"); tick("held voices callback");
        midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),32);
        midi.addEvent(juce::MidiMessage::pitchWheel(2,12000),64);
        midi.addEvent(juce::MidiMessage::channelPressureChange(2,100),96);
        tick("controller callback");
        const std::array<unsigned char,21> mts {0xf0,0x7f,0x7f,0x08,0x08,0,0,2,89,89,89,89,89,89,89,89,89,89,89,89,0xf7};
        midi.addEvent(mts.data(),int(mts.size()),64); tick("MTS SysEx callback");
        check(host->getEngine().getMtsTuning(2).pitch[60]==60.25f, "valid MTS was applied");
        auto malformed=mts; malformed[8]=0xff;
        midi.addEvent(malformed.data(),int(malformed.size()),64); tick("malformed MTS callback");
        check(host->getEngine().getMtsTuning(2).pitch[60]==60.25f, "malformed MTS preserves table");
        midi.addEvent(juce::MidiMessage::programChange(1,1),32); tick("program change callback");
        host->setParamValue(cs80::P_voiceMode,sixteen ? 0.f : 1.f); tick("voice mode transition");
        const auto priorPolicy=host->getEngine().getRenderPolicy();
        host->setEngineMode(host->getEffectiveEngineMode()==Cs80AudioProcessor::EngineMode::circuit
                               ? Cs80AudioProcessor::EngineMode::legacy : Cs80AudioProcessor::EngineMode::circuit);
        tick("engine policy transition");
        check(host->getEngine().getRenderPolicy()!=priorPolicy,"policy transition actually occurred");
        host->setNonRealtime(true); tick("offline transition");
        host->setNonRealtime(false); tick("realtime transition");
        midi.addEvent(juce::MidiMessage::allSoundOff(1),0); tick("panic callback");
        audio.setSize(2,0); tick("zero frame callback");
    }
    // Exercise the real processor notifier without posting from the audio scope.
    // Timer servicing and host listeners are deliberately outside that scope.
    {
        struct DisplayListener final : juce::AudioProcessorListener {
            int programs=0;bool wrongThread=false;
            cs80::UndoableParameterState* requestDuringDelivery=nullptr;
            void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {}
            void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& details) override {
                if(!details.programChanged)return;
                ++programs;
                wrongThread=wrongThread || !juce::MessageManager::getInstance()->isThisTheMessageThread();
                if(auto* state=requestDuringDelivery) {
                    requestDuringDelivery=nullptr;
                    state->requestProgramDisplayUpdate();
                }
            }
        } listener;
        juce::TemporaryFile fixture("-program-display");
        auto host=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        host->prepareToPlay(48000,256);
        auto serviceTimers=[] {
            juce::Thread::sleep(120);
            juce::Timer::callPendingTimersSynchronously();
        };
        serviceTimers();host->addListener(&listener);
        juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;midi.ensureSize(4096);
        for(int program : {1,2}) {
            midi.clear();midi.addEvent(juce::MidiMessage::programChange(1,program),0);
            probe::calls=probe::frees=0;probe::armed=true;host->processBlock(audio,midi);probe::armed=false;
            check(probe::calls==0 && probe::frees==0,"program display request has no callback allocation or free");
        }
        check(listener.programs==0,"program display delivery stays deferred");
        check(host->getCurrentProgram()==cs80::programIndexForMidi(0,0,2),"latest program selection is immediate before display notification");
        serviceTimers();
        check(listener.programs==1 && !listener.wrongThread,"timer coalesces program changes on message thread");
        serviceTimers();check(listener.programs==1,"serviced program display request is not repeated");
        listener.requestDuringDelivery=&host->getUndoableState();
        probe::calls=probe::frees=0;probe::armed=true;
        for(int i=0;i<256;++i)host->getUndoableState().requestProgramDisplayUpdate();
        probe::armed=false;
        check(probe::calls==0 && probe::frees==0,"repeated notifier publication is allocation-free");
        serviceTimers();check(listener.programs==2,"repeated requests coalesce into one further notification");
        serviceTimers();check(listener.programs==3,"request during delivery survives for the next timer");
        serviceTimers();check(listener.programs==3 && !listener.wrongThread,"last notification is consumed exactly once");
        // Listener outlives the processor and remains registered through teardown.
        host->getUndoableState().requestProgramDisplayUpdate();host.reset();
        serviceTimers();check(listener.programs==3,"pending destruction does not deliver to surviving listener");
    }
    std::cout<<"Audio callback allocation: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures ? 1 : 0;
}
