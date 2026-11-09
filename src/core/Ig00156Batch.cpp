#include "Ig00156Batch.h"

#if CS80_IG00156_EXACT_BATCH_BUILD
#include <bit>
#include <cstdint>
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
CS80_AVX2_INLINE V select(V mask,V yes,V no) { return _mm256_blendv_pd(no.x,yes.x,mask.x); }
CS80_AVX2_INLINE V absolute(V x) { return _mm256_andnot_pd(_mm256_set1_pd(-0.0),x.x); }
CS80_AVX2_INLINE V less(V a,V b) { return _mm256_cmp_pd(a.x,b.x,_CMP_LT_OQ); }
CS80_AVX2_INLINE V greater(V a,V b) { return _mm256_cmp_pd(a.x,b.x,_CMP_GT_OQ); }
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
    static bool eligible(const Ig00156Batch::Stages& stages,
                         const Ig00156Batch::Inputs& input) noexcept
    {
        // Nearest rounding and masked traps only. Never change FTZ/DAZ or
        // clear existing status flags. Arithmetic may raise sticky flags;
        // exact flag equality with the scalar calculation is not promised.
        if (!Ig00156Batch::environmentSupported()) return false;
        for(std::size_t i=0;i<4;++i)
        {
            const auto& s=*stages[i];
            if(!bounded(input[i],64.0f) || !bounded(s.lastRate,1536000.0f)
               || s.lastRate<8000.0f || s.lastUnwarped || !s.cutoffCoefficientsValid)
                return false;
            for(double value : {s.inputScale,s.gmPart,s.ffPart,s.xFromFb,s.xFromIn,
                                 s.xFromU,s.xFromBp,s.kiGs,s.invA11,s.inFromX})
                if(!bounded(value,1000000.0)) return false;
            for(double value : {s.zIn,s.zFb,s.zBp,s.zLp,s.slope1,s.slope2})
                if(!bounded(value,1024.0)) return false;
            // Positive physical gains make both Newton denominators >= 1.
            // Arbitrary restored slopes/custom networks must not turn a
            // discarded vector lane into a division by zero. These are still
            // eligibility checks: unsupported states keep the scalar solver.
            if(s.gmPart<0.0 || s.ffPart<0.0 || s.xFromBp < -1.0
               || s.slope1<0.0 || s.slope1>1.0 || s.slope2<0.0 || s.slope2>1.0)
                return false;
        }
        return true;
    }

    static CS80_AVX2 bool process(const Ig00156Batch::Stages& stages,
                                  const Ig00156Batch::Inputs& input,
                                  Ig00156Batch::Outputs& output) noexcept
    {
        if(!eligible(stages,input)) return false;
        const auto gather=[&](double Ig00156Stage::*member) CS80_AVX2 {
            return V(_mm256_setr_pd(stages[0]->*member,stages[1]->*member,
                                   stages[2]->*member,stages[3]->*member));
        };
        const V scale=gather(&Ig00156Stage::inputScale);
        const V g1=gather(&Ig00156Stage::gmPart),f1=gather(&Ig00156Stage::ffPart),g2=g1,f2=f1;
        const V fromFb=gather(&Ig00156Stage::xFromFb),fromIn=gather(&Ig00156Stage::xFromIn);
        const V fromU=gather(&Ig00156Stage::xFromU),fromBp=gather(&Ig00156Stage::xFromBp);
        const V kiGs=gather(&Ig00156Stage::kiGs),invA11=gather(&Ig00156Stage::invA11),inFromX=gather(&Ig00156Stage::inFromX);
        const V zIn=gather(&Ig00156Stage::zIn),zFb=gather(&Ig00156Stage::zFb);
        const V zBp=gather(&Ig00156Stage::zBp),zLp=gather(&Ig00156Stage::zLp);
        const V slope1=gather(&Ig00156Stage::slope1),slope2=gather(&Ig00156Stage::slope2);
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
            if(iteration==4) break;
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
        const auto scatter=[&](double Ig00156Stage::*member,V value) CS80_AVX2 {
            alignas(32) double values[4];_mm256_store_pd(values,value.x);
            for(std::size_t i=0;i<4;++i)stages[i]->*member=values[i];
        };
        scatter(&Ig00156Stage::zIn,2.0*vin-zIn);scatter(&Ig00156Stage::zFb,2.0*x-zFb);
        scatter(&Ig00156Stage::zBp,2.0*bp-zBp);scatter(&Ig00156Stage::zLp,2.0*lp-zLp);
        scatter(&Ig00156Stage::vIn,vin);scatter(&Ig00156Stage::vFb,x);
        scatter(&Ig00156Stage::vBp,bp);scatter(&Ig00156Stage::vLp,lp);
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
