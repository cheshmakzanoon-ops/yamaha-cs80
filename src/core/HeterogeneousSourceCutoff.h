#pragma once
// Current sample only. Computes heterogeneous drives in voice pairs, then
// the HP/LP pair per voice. Branches back to the scalar transaction before
// any floating-point operation unless every input and map is provably safe.
#include "SourceFactoryTones.h"
#include <bit>
#include <cstddef>
#include <cstring>
#if defined(__linux__) && defined(__x86_64__) && defined(__GNUC__) && __GNUC__==14 && __GNUC_MINOR__==2 && defined(__SSE2_MATH__) && !defined(__clang__) \
    && !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER) && !defined(__NVCOMPILER) && !defined(__PGI) && !defined(__ibmxl__) && !defined(_MSC_VER) \
    && !defined(__FMA__) && !defined(__FMA4__) && !defined(__AVX512F__) && !defined(__FP_FAST_FMA) && !defined(__FP_FAST_FMAF) && !defined(__FAST_MATH__) \
    && !defined(__ASSOCIATIVE_MATH__) && !defined(__RECIPROCAL_MATH__) && !defined(__NO_SIGNED_ZEROS__) && !defined(__NO_TRAPPING_MATH__) \
    && (!defined(__FINITE_MATH_ONLY__) || !__FINITE_MATH_ONLY__) && defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__==0 \
    && !defined(CS80_DISABLE_UNIFORM_CUTOFF) && !defined(CS80_DISABLE_HETEROGENEOUS_CUTOFF)
#include <emmintrin.h>
#define CS80_DETAIL_HETEROGENEOUS_CUTOFF 1
#endif

namespace cs80::source_factory::heterogeneous_cutoff {
namespace sf = cs80::source_factory;
#if defined(CS80_DETAIL_HETEROGENEOUS_CUTOFF)
static inline bool qualifiedEnvironment() noexcept {
    const unsigned mx=_mm_getcsr(); unsigned short cw;
    asm volatile("fnstcw %0" : "=m"(cw) :: "memory");
    return (mx & 0x7f80u)==0x1f80u && (cw & 0x0c3fu)==0x003fu;
}
static inline bool coefficient(double x) noexcept {
    // Zero or [2^-32, 2^16). Absolute value by representation, never compare
    // a signaling NaN or denormal in the admission check.
    constexpr auto lo=std::bit_cast<std::uint64_t>(0x1p-32), hi=std::bit_cast<std::uint64_t>(0x1p16);
    const auto a=std::bit_cast<std::uint64_t>(x)&0x7fffffffffffffffULL;
    return a==0 || a-lo<hi-lo;
}
static inline bool driveWord(double x, double bound, bool allowNegative) noexcept {
    constexpr auto lo=std::bit_cast<std::uint64_t>(0x1p-160);
    const auto b=std::bit_cast<std::uint64_t>(x),a=b&std::uint64_t{0x7fffffffffffffff};
    return a==0 || ((allowNegative || b==a) && a-lo<=std::bit_cast<std::uint64_t>(bound)-lo);
}
static inline bool bounded(const sf::ToneControls& t,const sf::RankDrives& in) noexcept {
    if(!t.compiled || t.provenance.version!=sf::modelVersion || t.provenance.sourceHash!=sf::sourceFingerprint ||
       (t.provenance.inferredCells && (t.provenance.hypothesisVersion!=factory_hypotheses::version ||
        t.provenance.hypothesisHash!=factory_hypotheses::fingerprint)))return false;
    if(!coefficient(t.initialBrillianceVolts) || !coefficient(t.afterBrillianceVolts)) return false;
    for(const auto& m:t.cutoff)
        if(!coefficient(m.pinPerLoadedBus)||!coefficient(m.pinPerTouch)||!coefficient(m.pinPerEnvelopeVolt)||
           !coefficient(m.busBaseVolts)||!coefficient(m.busPerTouchSum)||!coefficient(m.busPerEnvelopeVoltSum)||
           !coefficient(m.effectiveCvPerPinVolt)||!coefficient(m.referencePinVolts)||!coefficient(m.effectiveCvOffset))return false;
    for(const auto& d:in)
        if(!driveWord(d.filterEnvelopeZ,3.,true)||!driveWord(d.velocity,1.,false)||!driveWord(d.pressure,1.,false))return false;
    return true;
}
template<class P> static inline __m128d both(const sf::ToneControls& t, P f) noexcept {
    return _mm_set_pd(t.cutoff[1].*f,t.cutoff[0].*f);
}
// Keep all new arithmetic behind a real compiler call boundary. Both packet
// lanes evaluate the very same expression tree as their scalar counterpart.
[[gnu::noipa]] static inline void packet(const sf::ToneControls& t,const sf::RankDrives& in,sf::CutoffReadout& result) noexcept {
    static_assert(sizeof(sf::VoiceDrives)==3*sizeof(double) && offsetof(sf::VoiceDrives,velocity)==sizeof(double)
                  && offsetof(sf::VoiceDrives,pressure)==2*sizeof(double));
    sf::CutoffReadout next;
    std::array<double,sf::voiceCount> tv{},ev{};
    const auto initial=_mm_set1_pd(t.initialBrillianceVolts),after=_mm_set1_pd(t.afterBrillianceVolts);
    const auto kz=_mm_set1_pd(5./3.);auto sum=_mm_setzero_pd();
    for(unsigned i=0;i<sf::voiceCount;i+=2){
        __m128d a,b;
        std::memcpy(&a,reinterpret_cast<const unsigned char*>(&in[i])+offsetof(sf::VoiceDrives,velocity),sizeof(a));
        std::memcpy(&b,reinterpret_cast<const unsigned char*>(&in[i+1])+offsetof(sf::VoiceDrives,velocity),sizeof(b));
        const auto v=_mm_unpacklo_pd(a,b),p=_mm_unpackhi_pd(a,b);
        const auto t2=_mm_add_pd(_mm_mul_pd(initial,v),_mm_mul_pd(after,p));
        const auto e2=_mm_mul_pd(kz,_mm_set_pd(in[i+1].filterEnvelopeZ,in[i].filterEnvelopeZ));
        _mm_storeu_pd(tv.data()+i,t2);_mm_storeu_pd(ev.data()+i,e2);
        sum=_mm_add_pd(sum,_mm_unpacklo_pd(t2,e2)); // voice i first, one sum per lane
        sum=_mm_add_pd(sum,_mm_unpackhi_pd(t2,e2)); // then voice i+1
    }
    using M=sf::CutoffSideMap;
    const auto st=_mm_unpacklo_pd(sum,sum),se=_mm_unpackhi_pd(sum,sum);
    const auto bus=_mm_add_pd(_mm_add_pd(both(t,&M::busBaseVolts),_mm_mul_pd(both(t,&M::busPerTouchSum),st)),
                               _mm_mul_pd(both(t,&M::busPerEnvelopeVoltSum),se));
    const auto base=_mm_mul_pd(both(t,&M::pinPerLoadedBus),bus);
    const auto pt=both(t,&M::pinPerTouch),pe=both(t,&M::pinPerEnvelopeVolt);
    const auto ref=both(t,&M::referencePinVolts),off=both(t,&M::effectiveCvOffset),gain=both(t,&M::effectiveCvPerPinVolt);
    _mm_storeu_pd(next.loadedBusVolts.data(),bus);
    for(unsigned i=0;i<sf::voiceCount;++i) {
        const auto pin=_mm_add_pd(_mm_add_pd(base,_mm_mul_pd(pt,_mm_set1_pd(tv[i]))),_mm_mul_pd(pe,_mm_set1_pd(ev[i])));
        const auto cv=_mm_add_pd(off,_mm_mul_pd(gain,_mm_sub_pd(pin,ref)));
        _mm_storeu_pd(next.pinVolts[i].data(),pin); _mm_storeu_pd(next.effectiveCv[i].data(),cv);
    }
    result=next;
}
#endif
static inline bool evaluateOrScalar(const sf::ToneControls& t,const sf::RankDrives& in,sf::CutoffReadout& out) noexcept {
#if defined(CS80_DETAIL_HETEROGENEOUS_CUTOFF)
    if(qualifiedEnvironment() && bounded(t,in)) {packet(t,in,out);return true;}
#endif
    return sf::evaluateCutoff(t,in,out);
}
} // namespace cs80::source_factory::heterogeneous_cutoff
#undef CS80_DETAIL_HETEROGENEOUS_CUTOFF
