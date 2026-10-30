// Independent concurrency/representation audit. No DSP or hardware-level claim.
#include "plugin/OutputMeter.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86_FP)
#include <xmmintrin.h>
#define CS80_AUDIT_X86_MXCSR 1
#endif

namespace {
int checks=0, failures=0;
void check(bool ok,const char* what) {
    ++checks;
    if(!ok) {if(failures<30)std::fprintf(stderr,"FAIL: %s\n",what);++failures;}
}
std::uint32_t bits(float x) {return std::bit_cast<std::uint32_t>(x);}
float sample(std::uint32_t x) {return std::bit_cast<float>(x);}
struct FakePeakAtomic {
    std::uint32_t value=0, resets=0;
    int step=0,loads=0,cas=0,stores=0;
    std::vector<std::uint32_t> observed;
    void hook() {if((resets&(1u<<step))!=0){observed.push_back(value);value=0;}++step;}
    std::uint32_t load(std::memory_order) {++loads;hook();const auto v=value;hook();return v;}
    bool compare_exchange_strong(std::uint32_t& expected,std::uint32_t desired,std::memory_order) {
        ++cas;hook();const bool ok=value==expected;if(ok)value=desired;else expected=value;hook();return ok;
    }
    void store(std::uint32_t next,std::memory_order) {++stores;hook();value=next;hook();}
};
// Any concurrent publication may linearize before, between, or after the
// exchanges with which it overlaps, but every observed exchange must match.
bool legalHistory(std::uint32_t initial,std::uint32_t published,const FakePeakAtomic& actual) {
    for(std::size_t insertion=0;insertion<=actual.observed.size();++insertion) {
        auto value=initial;bool matches=true;
        for(std::size_t i=0;i<actual.observed.size();++i) {
            if(i==insertion)value=std::max(value,published);
            matches=matches&&value==actual.observed[i];value=0;
        }
        if(insertion==actual.observed.size())value=std::max(value,published);
        if(matches&&value==actual.value)return true;
    }
    return false;
}
void interleavings() {
    const std::array<std::uint32_t,7> peaks{0u,1u,0x007fffffu,0x00800000u,0x3f000000u,0x40000000u,0x7f7fffffu};
    for(auto initial:peaks)for(auto incoming:peaks)for(std::uint32_t resets=0;resets<64;++resets) {
        FakePeakAtomic mailbox;mailbox.value=initial;mailbox.resets=resets;
        cs80::detail::publishOutputPeakBits(mailbox,incoming);
        check(legalHistory(initial,incoming,mailbox),"peak history linearizes to exact max + UI exchanges");
        check(mailbox.loads==(incoming?1:0)&&mailbox.cas==(incoming?1:0)&&mailbox.stores<=1&&mailbox.step<=6,
              "positive publication validates once with bounded load/CAS/store; silence skips");
    }
}
// Inject an old relaxed-read value independently of the mailbox's current
// cleared value. This is a robustness/validation test, not a native weak-memory
// reproduction or a complete C++ memory-model proof.
void staleLoadValidation() {
    struct StaleAtomic : FakePeakAtomic {
        std::uint32_t historical=0;
        std::uint32_t load(std::memory_order) {++loads;return historical;}
    };
    for(auto previous:{0x3f000000u,0x40000000u,0x41100000u,0x7f7fffffu})
        for(auto incoming:{1u,0x3e000000u,0x3f800000u,0x40400000u}) {
            StaleAtomic mailbox;mailbox.value=0;mailbox.historical=previous;
            cs80::detail::publishOutputPeakBits(mailbox,incoming);
            check(mailbox.value==incoming,"stale larger load is validated so a post-clear smaller publication survives");
            check(mailbox.loads==1&&mailbox.cas==1&&mailbox.stores==1,"stale-load recovery remains one load, one CAS, one fallback store");
        }
}
void exactRepresentationAndReadOnly() {
    const std::array<std::uint32_t,17> sourceBits{0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,
        0x3f000000u,0xbf800000u,0x3f800001u,0xc0000000u,0x7f7fffffu,0xff7fffffu,
        0x7f800000u,0xff800000u,0x7fc12345u,0xffc54321u,0x7f800001u};
    std::array<float,sourceBits.size()> audio{};
    for(std::size_t i=0;i<audio.size();++i)audio[i]=sample(sourceBits[i]);
    cs80::OutputMeter meter;cs80::OutputMeterBlock block;
    block.addSamples(audio.data(),static_cast<int>(audio.size()));meter.publish(block);
    const auto reading=meter.consume();
    check(bits(reading.samplePeak)==0x7f7fffffu&&reading.overRange&&reading.nonFinite,
          "finite exact peak is separate from nonfinite and over-range events");
    check(std::memcmp(audio.data(),sourceBits.data(),sizeof(audio))==0,"meter never changes any audio sample bit, including NaN payloads");
    auto empty=meter.consume();check(bits(empty.samplePeak)==0&&!empty.overRange&&!empty.nonFinite,"consume clears observed values");
    for(auto raw:sourceBits) {
        cs80::OutputMeterBlock one;one.addSample(sample(raw));meter.publish(one);auto result=meter.consume();
        const auto magnitude=raw&0x7fffffffu;
        const bool finite=magnitude<0x7f800000u;
        check(bits(result.samplePeak)==(finite?magnitude:0u),"individual finite magnitude is represented without rounding or normalization");
        check(result.nonFinite==!finite&&result.overRange==(finite&&magnitude>0x3f800000u),"strictly greater than one and nonfinite flags have distinct definitions");
    }
    cs80::OutputMeterBlock none;none.addSamples(nullptr,100);none.addSamples(audio.data(),-1);meter.publish(none);
    empty=meter.consume();check(bits(empty.samplePeak)==0&&!empty.nonFinite&&!empty.overRange,"null/negative-count accumulation is empty");
    // Editor opening/clearing consumes, rather than replacing producer state.
    cs80::OutputMeterBlock old;old.addSample(3);meter.publish(old);(void)meter.consume();
    cs80::OutputMeterBlock fresh;fresh.addSample(.25f);meter.publish(fresh);auto post=meter.consume();
    check(bits(post.samplePeak)==bits(.25f)&&!post.overRange&&!post.nonFinite,"publication after completed UI clear is retained without stale flags");
    meter.publish(old);meter.reset();empty=meter.consume();
    check(bits(empty.samplePeak)==0&&!empty.overRange&&!empty.nonFinite,"stopped lifecycle reset clears telemetry only");
}
void noDenormalsAccuracy() {
#ifdef CS80_AUDIT_X86_MXCSR
    const auto original=_mm_getcsr();
    _mm_setcsr(original|0x8040u); // Match the DAZ/FTZ environment in processBlock.
    bool exact=true;
    for(std::uint32_t raw:{1u,2u,0x007fffffu,0x00800000u,0x3f800001u}) {
        volatile std::uint32_t runtime=raw;
        cs80::OutputMeter meter;cs80::OutputMeterBlock block;
        block.addSample(sample(runtime));meter.publish(block);
        exact=exact&&bits(meter.consume().samplePeak)==raw;
    }
    _mm_setcsr(original);
    check(exact,"finite sample peaks including subnormals remain bit exact with DAZ/FTZ enabled");
#else
    std::puts("DAZ/FTZ-specific check not applicable on this architecture");
#endif
}
void concurrentConservation() {
    cs80::OutputMeter meter;std::atomic<bool> done{false};
    std::uint32_t expected=0,observed=0;bool sawRange=false,sawInvalid=false,valid=true;
    std::thread producer([&] {
        for(std::uint32_t i=0;i<200000;++i) {
            const std::uint32_t magnitude=0x3f000000u+((i*2654435761u)&0x00ffffffu);
            expected=std::max(expected,magnitude);
            cs80::OutputMeterBlock block;block.addSample(sample(magnitude));
            if(i%127==0)block.addSample(sample(0x7f800001u));
            meter.publish(block);
            if(i%97==0)std::this_thread::yield();
        }
        done.store(true,std::memory_order_release);
    });
    auto consume=[&] {
        const auto r=meter.consume();const auto b=bits(r.samplePeak);
        observed=std::max(observed,b);sawRange=sawRange||r.overRange;sawInvalid=sawInvalid||r.nonFinite;
        valid=valid&&b<0x7f800000u;
    };
    while(!done.load(std::memory_order_acquire)){consume();std::this_thread::yield();}
    producer.join();consume();
    check(valid&&observed==expected,"concurrent consuming preserves global exact peak without invalid values");
    check(sawRange&&sawInvalid,"independent event mailboxes retain concurrent over-range and nonfinite events");
    const auto remaining=meter.consume();check(bits(remaining.samplePeak)==0&&!remaining.overRange&&!remaining.nonFinite,"quiescent final consume drains independent mailboxes");
}
}
int main() {
    interleavings();staleLoadValidation();exactRepresentationAndReadOnly();noDenormalsAccuracy();concurrentConservation();
    std::printf("Independent output-meter audit: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
