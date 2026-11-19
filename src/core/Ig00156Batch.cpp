#include "Ig00156Batch.h"

#if CS80_IG00156_EXACT_BATCH_BUILD
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <immintrin.h>

// The translation unit is compiled with contraction disabled, but NOT with a
// global AVX target. Only these functions may contain AVX instructions.
#define CS80_AVX2 __attribute__((target("avx2,no-fma")))
#define CS80_AVX2_INLINE CS80_AVX2 __attribute__((always_inline)) inline

namespace cs80
{
namespace
{
struct V
{
    __m256d x;
    CS80_AVX2_INLINE V() : x(_mm256_setzero_pd()) {}
    CS80_AVX2_INLINE V(double a) : x(_mm256_set1_pd(a)) {}
    CS80_AVX2_INLINE V(__m256d a) : x(a) {}
    friend CS80_AVX2_INLINE V operator+(V a, V b) { return _mm256_add_pd(a.x,b.x); }
    friend CS80_AVX2_INLINE V operator-(V a, V b) { return _mm256_sub_pd(a.x,b.x); }
    friend CS80_AVX2_INLINE V operator*(V a, V b) { return _mm256_mul_pd(a.x,b.x); }
    friend CS80_AVX2_INLINE V operator/(V a, V b) { return _mm256_div_pd(a.x,b.x); }
};
struct Tile { V a, b, c, d; };
CS80_AVX2_INLINE Tile transpose(V a, V b, V c, V d) noexcept
{
    const auto ab0 = _mm256_unpacklo_pd(a.x, b.x), ab1 = _mm256_unpackhi_pd(a.x, b.x);
    const auto cd0 = _mm256_unpacklo_pd(c.x, d.x), cd1 = _mm256_unpackhi_pd(c.x, d.x);
    return { _mm256_permute2f128_pd(ab0, cd0, 0x20), _mm256_permute2f128_pd(ab1, cd1, 0x20),
             _mm256_permute2f128_pd(ab0, cd0, 0x31), _mm256_permute2f128_pd(ab1, cd1, 0x31) };
}
CS80_AVX2_INLINE V select(V mask,V yes,V no) { return _mm256_blendv_pd(no.x,yes.x,mask.x); }
CS80_AVX2_INLINE V absolute(V x) { return _mm256_andnot_pd(_mm256_set1_pd(-0.0),x.x); }
CS80_AVX2_INLINE V less(V a,V b) { return _mm256_cmp_pd(a.x,b.x,_CMP_LT_OQ); }
CS80_AVX2_INLINE V greater(V a,V b) { return _mm256_cmp_pd(a.x,b.x,_CMP_GT_OQ); }
CS80_AVX2_INLINE __m256i outsideMagnitudeBound(V value,double limit)
{
    const auto magnitude=_mm256_and_si256(_mm256_castpd_si256(value.x),
                                          _mm256_set1_epi64x(0x7fffffffffffffffLL));
    return _mm256_cmpgt_epi64(magnitude,
        _mm256_set1_epi64x(std::bit_cast<std::int64_t>(limit)));
}
CS80_AVX2_INLINE __m256i outsideStateBound(V value)
{
    return outsideMagnitudeBound(value,1024.0);
}

// For the fast certificate only: signed integer comparisons of the raw
// encoding admit strictly positive normals in [DBL_MIN,upper]. Negative
// values, both zeros, NaNs, infinities and subnormals take ordered admission.
CS80_AVX2_INLINE __m256i outsidePositiveNormal(V value,double upper)
{
    const auto bits=_mm256_castpd_si256(value.x);
    return _mm256_or_si256(
        _mm256_cmpgt_epi64(_mm256_set1_epi64x(0x0010000000000000LL),bits),
        _mm256_cmpgt_epi64(bits,_mm256_set1_epi64x(std::bit_cast<std::int64_t>(upper))));
}
CS80_AVX2_INLINE V tanh(V original)
{
    // The scalar branches do not evaluate the polynomial outside its clamp.
    const V x=select(greater(absolute(original),4.97),0.0,original);
    const V x2=x*x;
    V result=x*(135135.0+x2*(17325.0+x2*(378.0+x2)))
                /(135135.0+x2*(62370.0+x2*(3150.0+x2*28.0)));
    result=select(greater(original,4.97),1.0,result);
    return select(less(original,-4.97),-1.0,result);
}

// Bit inspection cannot raise invalid for a signaling NaN. These are
// optimization eligibility limits, NOT clamps: outside them scalar behavior
// is retained. The tested physical stage domain is much smaller.
bool bounded(double x,double limit) noexcept
{
    return (std::bit_cast<std::uint64_t>(x)&0x7fffffffffffffffULL)
        <= std::bit_cast<std::uint64_t>(limit);
}
bool bounded(float x,float limit) noexcept
{
    return (std::bit_cast<std::uint32_t>(x)&0x7fffffffU)
        <= std::bit_cast<std::uint32_t>(limit);
}
} // namespace

struct Ig00156BatchAccess
{
    // The stage vectors are lane-major; histories and nodes are contiguous
    // within each authoritative scalar stage. Transfer each four-member row
    // as bytes of its complete object, not through pointer arithmetic across
    // adjacent double members. Loads, shuffles and copies perform no FP math.
    using Stage = Ig00156Stage;
    static_assert(std::is_standard_layout_v<Stage> && std::is_trivially_copyable_v<Stage>);
    static_assert(offsetof(Stage, xFromIn) == offsetof(Stage, xFromFb) + sizeof(double) &&
                  offsetof(Stage, xFromU) == offsetof(Stage, xFromFb) + 2 * sizeof(double) &&
                  offsetof(Stage, xFromBp) == offsetof(Stage, xFromFb) + 3 * sizeof(double));
    static_assert(offsetof(Stage, zFb) == offsetof(Stage, zIn) + sizeof(double) &&
                  offsetof(Stage, zBp) == offsetof(Stage, zIn) + 2 * sizeof(double) &&
                  offsetof(Stage, zLp) == offsetof(Stage, zIn) + 3 * sizeof(double));
    static_assert(offsetof(Stage, vFb) == offsetof(Stage, vIn) + sizeof(double) &&
                  offsetof(Stage, vBp) == offsetof(Stage, vIn) + 2 * sizeof(double) &&
                  offsetof(Stage, vLp) == offsetof(Stage, vIn) + 3 * sizeof(double));

    template<std::size_t Offset>
    static CS80_AVX2_INLINE Tile gatherTile(const Ig00156Batch::Stages& stages) noexcept
    {
        static_assert(Offset == offsetof(Stage, xFromFb) || Offset == offsetof(Stage, zIn));
        const auto load = [](const Stage* s) CS80_AVX2 {
            V row;
            std::memcpy(&row.x, reinterpret_cast<const unsigned char*>(s) + Offset, sizeof(row.x));
            return row;
        };
        return transpose(load(stages[0]), load(stages[1]), load(stages[2]), load(stages[3]));
    }
    template<std::size_t Offset>
    static CS80_AVX2_INLINE void scatterTile(const Ig00156Batch::Stages& stages, V a, V b, V c, V d) noexcept
    {
        static_assert(Offset == offsetof(Stage, zIn) || Offset == offsetof(Stage, vIn));
        const auto rows = transpose(a, b, c, d);
        const auto store = [](Stage* s, V row) CS80_AVX2 {
            std::memcpy(reinterpret_cast<unsigned char*>(s) + Offset, &row.x, sizeof(row.x));
        };
        store(stages[0], rows.a); store(stages[1], rows.b);
        store(stages[2], rows.c); store(stages[3], rows.d);
    }
#if defined(__NO_TRAPPING_MATH__)
    // This compiler profile may speculate even short-circuited scalar FP
    // comparisons. Retain its original admission and call site, including
    // their observable sticky flags. The four-lane solver remains enabled.
    static bool eligible(const Ig00156Batch::Stages& stages,
                         const Ig00156Batch::Inputs& input) noexcept
    {
        // Nearest rounding and masked traps only. Never change FTZ/DAZ or
        // clear existing status flags. Arithmetic may raise sticky flags;
        // exact flag equality with the scalar calculation is not promised.
        if (!Ig00156Batch::environmentSupported()) return false;
#else
    // Exact fallback for anything the common-case bit certificate does not
    // prove. In particular, preserve DAZ and the order of the FP comparisons
    // for custom networks, exceptional rates and any short-circuit rejection.
    static __attribute__((cold,noinline)) bool orderedEligible(const Ig00156Batch::Stages& stages,
                                                               const Ig00156Batch::Inputs& input) noexcept
    {
#endif
        for(std::size_t i=0;i<4;++i)
        {
            const auto& s=*stages[i];
            if(!bounded(input[i],64.0f) || !bounded(s.lastRate,1536000.0f)
               || s.lastRate<8000.0f || s.lastUnwarped || !s.cutoffCoefficientsValid)
                return false;
            for(double value : {s.inputScale,s.gmPart,s.ffPart,s.xFromFb,s.xFromIn,
                                 s.xFromU,s.xFromBp,s.kiGs,s.invA11,s.inFromX})
                if(!bounded(value,1000000.0)) return false;
            // Positive physical gains make both Newton denominators >= 1.
            // Arbitrary restored slopes/custom networks must not turn a
            // discarded vector lane into a division by zero. These are still
            // eligibility checks: unsupported states keep the scalar solver.
            if(s.gmPart<0.0 || s.ffPart<0.0 || s.xFromBp < -1.0)
                return false;
        }
        return true;
    }

    static CS80_AVX2 bool process(const Ig00156Batch::Stages& stages,
                                  const Ig00156Batch::Inputs& input,
                                  Ig00156Batch::Outputs& output) noexcept
    {
#if defined(__NO_TRAPPING_MATH__)
        if(!eligible(stages,input)) return false;
#else
        // Eligibility is reassessed for this audio thread; never change host
        // rounding, masks, FTZ/DAZ, or pre-existing sticky flags.
        if(!Ig00156Batch::environmentSupported()) return false;
#endif
        const auto gather=[&](double Ig00156Stage::*member) CS80_AVX2 {
            return V(_mm256_setr_pd(stages[0]->*member,stages[1]->*member,
                                   stages[2]->*member,stages[3]->*member));
        };
        const V scale=gather(&Ig00156Stage::inputScale);
        const V g1=gather(&Ig00156Stage::gmPart),f1=gather(&Ig00156Stage::ffPart),g2=g1,f2=f1;
        const auto [fromFb, fromIn, fromU, fromBp] = gatherTile<offsetof(Stage, xFromFb)>(stages);
        const V kiGs=gather(&Ig00156Stage::kiGs),invA11=gather(&Ig00156Stage::invA11),inFromX=gather(&Ig00156Stage::inFromX);
#if !defined(__NO_TRAPPING_MATH__)
        // Prove the usual admission case using integer operations on the
        // coefficient vectors the solver already needs. No FP comparisons
        // may run on a rejected sNaN or on a denormal an earlier lane would
        // have skipped. Unproven does NOT mean rejected: orderedEligible
        // supplies exactly the historical verdict and FP side effects.
        auto uncertain=outsideMagnitudeBound(scale,1000000.0);
        uncertain=_mm256_or_si256(uncertain,outsideMagnitudeBound(fromFb,1000000.0));
        uncertain=_mm256_or_si256(uncertain,outsideMagnitudeBound(fromIn,1000000.0));
        uncertain=_mm256_or_si256(uncertain,outsideMagnitudeBound(fromU,1000000.0));
        uncertain=_mm256_or_si256(uncertain,outsideMagnitudeBound(kiGs,1000000.0));
        uncertain=_mm256_or_si256(uncertain,outsideMagnitudeBound(invA11,1000000.0));
        uncertain=_mm256_or_si256(uncertain,outsideMagnitudeBound(inFromX,1000000.0));
        uncertain=_mm256_or_si256(uncertain,outsidePositiveNormal(g1,1000000.0));
        uncertain=_mm256_or_si256(uncertain,outsidePositiveNormal(f1,1000000.0));
        // |xFromBp| in [DBL_MIN,1] implies the original [-1,1e6] bound;
        // values outside this smaller common range are not clamped or lost.
        uncertain=_mm256_or_si256(uncertain,outsidePositiveNormal(absolute(fromBp),1.0));
        const auto rate=_mm_castps_si128(_mm_setr_ps(stages[0]->lastRate,stages[1]->lastRate,
                                                      stages[2]->lastRate,stages[3]->lastRate));
        const auto inputBits=_mm_and_si128(_mm_castps_si128(_mm_loadu_ps(input.data())),
                                           _mm_set1_epi32(0x7fffffff));
        auto badHeader=_mm_cmpgt_epi32(inputBits,_mm_set1_epi32(std::bit_cast<std::int32_t>(64.0f)));
        badHeader=_mm_or_si128(badHeader,_mm_cmpgt_epi32(_mm_set1_epi32(std::bit_cast<std::int32_t>(8000.0f)),rate));
        badHeader=_mm_or_si128(badHeader,_mm_cmpgt_epi32(rate,_mm_set1_epi32(std::bit_cast<std::int32_t>(1536000.0f))));
        const bool ordinary=(_mm256_movemask_pd(_mm256_castsi256_pd(uncertain))==0
            && _mm_movemask_ps(_mm_castsi128_ps(badHeader))==0
            && !stages[0]->lastUnwarped && stages[0]->cutoffCoefficientsValid
            && !stages[1]->lastUnwarped && stages[1]->cutoffCoefficientsValid
            && !stages[2]->lastUnwarped && stages[2]->cutoffCoefficientsValid
            && !stages[3]->lastUnwarped && stages[3]->cutoffCoefficientsValid);
        if(!ordinary && !orderedEligible(stages,input)) return false;
#endif
        const auto [zIn, zFb, zBp, zLp] = gatherTile<offsetof(Stage, zIn)>(stages);
        const V slope1=gather(&Ig00156Stage::slope1),slope2=gather(&Ig00156Stage::slope2);
        // Reuse gathered state for the dynamic admission gate. Integer bit
        // comparisons reject NaN (including signalling NaN) and infinities
        // without evaluating them or altering any authoritative state.
        auto bad=_mm256_or_si256(outsideStateBound(zIn),outsideStateBound(zFb));
        bad=_mm256_or_si256(bad,outsideStateBound(zBp));
        bad=_mm256_or_si256(bad,outsideStateBound(zLp));
        bad=_mm256_or_si256(bad,outsideStateBound(slope1));
        bad=_mm256_or_si256(bad,outsideStateBound(slope2));
        if(_mm256_movemask_pd(_mm256_castsi256_pd(bad))!=0) return false;
        // Bounds above prove both slope vectors finite before FP comparisons.
        auto badSlope=_mm256_or_pd(less(slope1,0.0).x,greater(slope1,1.0).x);
        badSlope=_mm256_or_pd(badSlope,less(slope2,0.0).x);
        badSlope=_mm256_or_pd(badSlope,greater(slope2,1.0).x);
        if(_mm256_movemask_pd(badSlope)!=0) return false;

        const V u=V(_mm256_setr_pd(input[0],input[1],input[2],input[3]))*scale;
        const V level=kIg00156OtaLevel,inv=1.0/kIg00156OtaLevel;
        const V x0=fromFb*zFb+fromIn*zIn+fromU*u;
        const V c0=2.0*x0-zLp,c1=2.0*(fromBp+1.0);
        const V e1=g1*slope1+f1,e2=g2*slope2+f2;
        V bp=(zBp-e1*c0)/(1.0+e1*(c1+e2)),hp,t1,t2;
        V active=_mm256_castsi256_pd(_mm256_set1_epi64x(-1));
        for(int iteration=0;;++iteration)
        {
            const V nextT2=tanh(bp*inv);
            const V nextHp=c0+c1*bp+g2*level*nextT2+f2*bp;
            const V nextT1=tanh(nextHp*inv);
            t2=select(active,nextT2,t2);hp=select(active,nextHp,hp);t1=select(active,nextT1,t1);
            const V f=bp-zBp+g1*level*t1+f1*hp;
#if CS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY
            if(iteration==4) {
                // No histories have been written yet. Let the caller redo all
                // lanes through the authoritative scalar recovery path.
                const V failed=_mm256_andnot_pd(less(absolute(f),1.0e-7).x,active.x);
                if(_mm256_movemask_pd(failed.x)!=0) return false;
                break;
            }
#else
            if(iteration==4) break;
#endif
            active=_mm256_andnot_pd(less(absolute(f),1.0e-7).x,active.x);
            if(_mm256_movemask_pd(active.x)==0) break;
            const V dh=c1+g2*(1.0-t2*t2)+f2;
            // Inactive lanes must not evaluate an unused hazardous division.
            const V numerator=select(active,f,0.0);
            const V denominator=select(active,1.0+(g1*(1.0-t1*t1)+f1)*dh,1.0);
            bp=select(active,bp-numerator/denominator,bp);
        }
        const V lp=zLp-g2*level*t2-f2*bp;
        const V x=x0+fromBp*bp;
        const V vin=(zIn+kiGs*u-kiGs*bp)*invA11-inFromX*x;
        const V use1=greater(absolute(hp*inv),1.0e-9),use2=greater(absolute(bp*inv),1.0e-9);
        const V newSlope1=select(use1,t1/select(use1,hp*inv,1.0),1.0);
        const V newSlope2=select(use2,t2/select(use2,bp*inv,1.0),1.0);
        // Snapshot the four destination pointers before writing object bytes;
        // the public wrapper guarantees that lanes refer to distinct stages.
        const auto destinations = stages;
        const auto scatter=[&](double Ig00156Stage::*member,V value) CS80_AVX2 {
            alignas(32) double values[4];_mm256_store_pd(values,value.x);
            for(std::size_t i=0;i<4;++i)destinations[i]->*member=values[i];
        };
        scatterTile<offsetof(Stage, zIn)>(destinations, 2.0*vin-zIn, 2.0*x-zFb, 2.0*bp-zBp, 2.0*lp-zLp);
        scatterTile<offsetof(Stage, vIn)>(destinations, vin, x, bp, lp);
        scatter(&Ig00156Stage::slope1,newSlope1);scatter(&Ig00156Stage::slope2,newSlope2);
        alignas(32) double l[4],h[4],b[4];
        _mm256_store_pd(l,lp.x);_mm256_store_pd(h,hp.x);_mm256_store_pd(b,bp.x);
        for(std::size_t i=0;i<4;++i)
            output[i]={static_cast<float>(l[i]),static_cast<float>(h[i]),static_cast<float>(-b[i])};
        return true;
    }
};

Ig00156Batch::Kernel Ig00156Batch::selectKernel(bool allowAcceleration) noexcept
{
    if(!allowAcceleration) return nullptr;
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") ? &Ig00156BatchAccess::process : nullptr;
}
bool Ig00156Batch::environmentSupported() noexcept
{
    const auto csr=_mm_getcsr();
    return (csr & 0x6000U) == 0 && (csr & 0x1f80U) == 0x1f80U;
}
} // namespace cs80
#else
namespace cs80
{
Ig00156Batch::Kernel Ig00156Batch::selectKernel(bool) noexcept { return nullptr; }
bool Ig00156Batch::environmentSupported() noexcept { return false; }
}
#endif
