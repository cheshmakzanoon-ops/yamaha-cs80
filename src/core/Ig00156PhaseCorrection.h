#pragma once
// Numerical phase correction for the existing declared IG00156 network, not a new
// circuit law. Fourth-order Padé phase correction of the linear nodal part;
// the same OTA currents are retained in the implicit trapezoidal residual.
// Derivation/evaluation: docs/IG00156_PHASE_CORRECTION.md. The template merely
// keeps the coefficient kernel independent of the public network declaration.
#include "Dsp.h"
#include "FastMath.h"
#include <array>
#include <cfenv>
#include <cmath>
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#define CS80_PHASE_SIMD_X86 1
#else
#define CS80_PHASE_SIMD_X86 0
#endif

namespace cs80::detail
{
template <typename Network>
class Ig00156PhaseCorrection
{
public:
    using Vector = std::array<double, 4>;
    using Matrix = std::array<Vector, 4>;
    explicit Ig00156PhaseCorrection(const Network& n) noexcept : net(n)
    {
        gs = 1 / net.sourceOhms; gg = 1 / net.groundOhms; gf = 1 / net.feedbackOhms;
        gt = gs + gg + gf;
        scale = gt / (2 * gs);
        ff = 1 / (net.feedforwardOhms * net.integratorFarads);
        b = {gs / net.inputFarads, gs / net.feedbackFarads, 0, 0};
        inputRow = -gs / net.inputFarads;
        feedbackInput = -gs / net.feedbackFarads;
        feedbackDiagonal = -gt / net.feedbackFarads;
    }
    void reset() noexcept { v = {}; inputs = {}; slope1 = slope2 = 1.0; previousHpTanh = previousBpTanh = 0.0; }
    const Vector& nodes() const noexcept { return v; }
    struct Output { float lp = 0.0f, hp = 0.0f, bp = 0.0f; };
    const Vector& inputHistory() const noexcept { return inputs; }
    double inputRate() const noexcept { return lastFs; }
    double firstSlope() const noexcept { return slope1; }
    double secondSlope() const noexcept { return slope2; }
    double solverResidual() const noexcept { return residualAtExit; }
    unsigned newtonSteps() const noexcept { return stepsAtExit; }
    double getHpTanh() const noexcept { return previousHpTanh; }
    double getBpTanh() const noexcept { return previousBpTanh; }
    void restore(const Vector& nodes, const Vector& history, double s1, double s2, double inputRate) noexcept
    {
        v = nodes; inputs = history; slope1 = s1; slope2 = s2;
        refreshOtaCurrents();
        lastFc = lastQ = -1.0f; lastFs = static_cast<float>(inputRate);
    }
    void observeIdle(const Vector& nodes, double input, double sampleRate) noexcept
    {
        v = nodes; inputs.fill(input); lastFc = lastQ = -1.0f; lastFs = static_cast<float>(sampleRate);
        refreshOtaCurrents();
    }
    void set(float cutoff, float q, float fs) noexcept
    {
        if (cutoff == lastFc && q == lastQ && fs == lastFs) return;
        // Histories of input samples are time coordinates, not capacitors.
        // A clock transition starts a constant-history reconstruction; the
        // physical nodes remain untouched. The idle replay rebuilds its past.
        if (lastFs > 0.0f && lastFs != fs) inputs.fill(inputs[0]);
        lastFc = cutoff; lastQ = q; lastFs = fs;
        const double gq=std::max(0.0, gt/(2*std::clamp(double(q),.35,9.0))-gf);
        const double fc=std::clamp(double(cutoff),0.0,.45*fs-ff/(2*3.14159265358979323846))+ff/(2*3.14159265358979323846);
        const double w=ig00156IntegratorRate(net,fc,gq);
        gm=std::max(0.0,w-ff);
        h=1.0/fs; beta=h*h/12;
        Matrix a {};
        a[0]={inputRow,inputRow,inputRow,0};
        a[1]={feedbackInput,feedbackDiagonal,(gq-gs-gg)/net.feedbackFarads,0};
        a[2]={0,-2*w,-2*w,w};
        a[3]={0,0,-w,0};
        // Fixed sparse nodal graph. Each nonzero product retains the original
        // k=0,1,2,3 summation order; no numerical or OTA law is changed.
        const double ai=inputRow, bi=feedbackInput, ci=feedbackDiagonal;
        const double di=a[1][2], ei=-2*w;
        ab={ai*b[0]+ai*b[1], bi*b[0]+ci*b[1], ei*b[1], 0};
        const double ai2=ai*ai, aibi=ai*bi, bici=bi*ci, ei2=ei*ei, minusW2=(-w)*w;
        Matrix a2 {{
            {ai2+aibi, ai2+ai*ci+ai*ei, ai2+ai*di+ai*ei, ai*w},
            {bi*ai+bici, bi*ai+ci*ci+di*ei, bi*ai+ci*di+di*ei, di*w},
            {ei*bi, ei*ci+ei2, ei*di+ei2+minusW2, ei*w},
            {0, (-w)*ei, (-w)*ei, minusW2}
        }};
        for(int i=0;i<4;++i) for(int j=0;j<4;++j) {
            const double identity=i==j?1:0;
            left[i][j]=identity-h*.5*a[i][j]+beta*a2[i][j];
            right[i][j]=identity+h*.5*a[i][j]+beta*a2[i][j];
        }
        // Remove the linear OTA part on the left; restore it below through
        // the exact (declared) nonlinear current, just as in Ig00156Stage.
        const double g=h*.5*gm;
        left[2][1]-=2*g; left[2][2]-=2*g; left[2][3]+=g;
        left[3][2]-=g;
        nonlinear=g*kIg00156OtaLevel;
        constexpr int e[3]={0,1,3};
        const double a00=left[0][0], a01=left[0][1], a02=left[0][3];
        const double a10=left[1][0], a11=left[1][1], a12=left[1][3];
        const double a21=left[3][1], a22=left[3][3];
        const double determinant=a00*(a11*a22-a12*a21)-a01*a10*a22+a02*a10*a21;
        const double reciprocal=1.0/determinant;
        inverse[0][0]=(a11*a22-a12*a21)*reciprocal;
        inverse[0][1]=(a02*a21-a01*a22)*reciprocal;
        inverse[0][2]=(a01*a12-a02*a11)*reciprocal;
        inverse[1][0]=-a10*a22*reciprocal;
        inverse[1][1]=a00*a22*reciprocal;
        inverse[1][2]=(a02*a10-a00*a12)*reciprocal;
        inverse[2][0]=a10*a21*reciprocal;
        inverse[2][1]=-a00*a21*reciprocal;
        inverse[2][2]=(a00*a11-a01*a10)*reciprocal;
        fromBp={}; fromTanh={};
        for(int i=0;i<3;++i) for(int j=0;j<3;++j) {
            fromBp[i]-=inverse[i][j]*left[e[j]][2];
            if(j==2) fromTanh[i]-=inverse[i][j]*nonlinear;
        }
        hpBp=2+2*fromBp[1]-fromBp[2];
        hpTanh=2*fromTanh[1]-fromTanh[2];
        residualBp=left[2][2]; residualTanh=0;
        for(int i=0;i<3;++i) {
            residualBp+=left[2][e[i]]*fromBp[i];
            residualTanh+=left[2][e[i]]*fromTanh[i];
        }
    }
    Output process(float input) noexcept
    {
        const double u=input*scale;
        const double invLevel=1.0/kIg00156OtaLevel;
        // Endpoint derivative difference, from the causal degree-three
        // backward interpolating polynomial; no lookahead or added delay.
        const double derivativeDelta=(11*u-29*inputs[0]+27*inputs[1]-11*inputs[2]+2*inputs[3])/(6*h);
        const double drive=.5*h*(u+inputs[0])-beta*derivativeDelta;
        const double du=u-inputs[0];
        Vector rhs {};
        for(int i=0;i<4;++i) {
            rhs[i]=b[i]*drive-beta*ab[i]*du;
            for(int j=0;j<4;++j) rhs[i]+=right[i][j]*v[j];
        }
        const double hpPrev=2*(v[1]+v[2])-v[3];
        rhs[2]+=.5*h*gm*(hpPrev-kIg00156OtaLevel*previousHpTanh);
        rhs[3]+=.5*h*gm*(v[2]-kIg00156OtaLevel*previousBpTanh);
        constexpr int e[3]={0,1,3};
        std::array<double,3> zero {};
        for(int i=0;i<3;++i) for(int j=0;j<3;++j) zero[i]+=inverse[i][j]*rhs[e[j]];
        const double hp0=2*zero[1]-zero[2];
        double residual0=-rhs[2];
        for(int i=0;i<3;++i) residual0+=left[2][e[i]]*zero[i];
        const double s1=slope1*invLevel, s2=slope2*invLevel;
        double bp=-(residual0+nonlinear*s1*hp0)/(residualBp+residualTanh*s2+nonlinear*s1*(hpBp+hpTanh*s2));
        double hp=0,t1=0,t2=0;
        for(int it=0;it<8;++it) {
            t2=fastTanh(bp*invLevel);
            hp=hp0+hpBp*bp+hpTanh*t2;
            t1=fastTanh(hp*invLevel);
            const double residual=residual0+residualBp*bp+residualTanh*t2+nonlinear*t1;
            if(std::abs(residual)<1e-10 || it==7) {
                residualAtExit = residual; stepsAtExit = static_cast<unsigned>(it); break;
            }
            // Converged states need no Jacobian or Newton correction. Retain
            // the same current derivatives whenever a correction is required.
            const double d2=(1-t2*t2)*invLevel, d1=(1-t1*t1)*invLevel;
            const double jacobian=residualBp+residualTanh*d2+nonlinear*d1*(hpBp+hpTanh*d2);
            bp-=residual/jacobian;
        }
        for(int i=0;i<3;++i) v[e[i]]=zero[i]+fromBp[i]*bp+fromTanh[i]*t2;
        v[2]=bp;
        slope1=std::abs(hp)>1e-9?t1/(hp*invLevel):1;
        slope2=std::abs(bp)>1e-9?t2/(bp*invLevel):1;
        previousHpTanh=t1; previousBpTanh=t2;
        for(int i=3;i>0;--i) inputs[i]=inputs[i-1];
        inputs[0]=u;
        return {float(v[3]),float(hp),float(-bp)};
    }
    // Experimental exact 4-lane full-nonlinear continuation. Only call this
    // after a real runtime ISA/FP check and with four distinct active lanes.
    // No scalar process is called for any accepted lane: the complete RHS,
    // predictor, two OTA tanh currents, Newton loop and state commit are packed.
    // Ineligible cases use the caller's original per-lane process verbatim.
    static bool tryProcessFour(Ig00156PhaseCorrection* const* lane,
                               const float* input, Output* output) noexcept
    {
#if CS80_PHASE_SIMD_X86
        if (!canProcessFour())
            return false;
        // Qualify exactly the original production rendering denormal state;
        // foreign FTZ/DAZ modes and rounding use original scalar fallback.
        if ((_mm_getcsr() & 0x8040u)!=0x8040u)
            return false;
        for(int i=0;i<4;++i) {
            if (!lane[i] || !std::isfinite(input[i])) return false;
            for(int j=0;j<i;++j) if (lane[i]==lane[j]) return false;
            const auto& p=*lane[i];
            if (!std::isfinite(p.h) || !std::isfinite(p.gm) ||
                !std::isfinite(p.scale) || !std::isfinite(p.nonlinear) ||
                !std::isfinite(p.slope1) || !std::isfinite(p.slope2) ||
                !std::isfinite(p.previousHpTanh) || !std::isfinite(p.previousBpTanh))
                return false;
            for(double x:p.v) if (!std::isfinite(x)) return false;
            for(double x:p.inputs) if (!std::isfinite(x)) return false;
        }
        processFourAvx2(lane,input,output);
        return true;
#else
        (void)lane; (void)input; (void)output;
        return false;
#endif
    }
    static bool canProcessFour() noexcept
    {
#if CS80_PHASE_SIMD_X86
#if defined(CS80_PHASE_BATCH_MUTATE_SPLIT_FP)
        // Negative control: incorrectly trusts x87 rounding alone.
        return __builtin_cpu_supports("avx2") && std::fegetround()==FE_TONEAREST
            && (_mm_getcsr() & 0x8040u)==0x8040u;
#else
        // x87 rounding and SSE rounding are independently selectable; the
        // latter governs packed double arithmetic. Qualify BOTH directions.
        return __builtin_cpu_supports("avx2") && std::fegetround()==FE_TONEAREST
            && (_mm_getcsr() & 0xe040u)==0x8040u;
#endif
#else
        return false;
#endif
    }
private:
#if CS80_PHASE_SIMD_X86
    using Packed = __m256d;
    __attribute__((target("avx2,no-fma"),always_inline))
    static Packed packed(double a, double b, double c, double d) noexcept
    { return _mm256_set_pd(d,c,b,a); }

    __attribute__((target("avx2,no-fma"),always_inline))
    static Packed packedTanh(Packed x) noexcept
    {
        const Packed hi=_mm256_set1_pd(4.97), lo=_mm256_set1_pd(-4.97);
        const Packed high=_mm256_cmp_pd(x,hi,_CMP_GT_OQ);
        const Packed low=_mm256_cmp_pd(x,lo,_CMP_LT_OQ);
        const Packed clipped=_mm256_or_pd(high,low);
        // Original fastTanh clamps first: do not evaluate a potentially
        // overflowing polynomial on an input whose scalar path returns +/-1.
        const Packed y=_mm256_blendv_pd(x,_mm256_setzero_pd(),clipped);
        const Packed y2=_mm256_mul_pd(y,y);
        const Packed numerator=_mm256_mul_pd(y,_mm256_add_pd(_mm256_set1_pd(135135.0),
            _mm256_mul_pd(y2,_mm256_add_pd(_mm256_set1_pd(17325.0),
                _mm256_mul_pd(y2,_mm256_add_pd(_mm256_set1_pd(378.0),y2))))));
        const Packed denominator=_mm256_add_pd(_mm256_set1_pd(135135.0),
            _mm256_mul_pd(y2,_mm256_add_pd(_mm256_set1_pd(62370.0),
                _mm256_mul_pd(y2,_mm256_add_pd(_mm256_set1_pd(3150.0),
                    _mm256_mul_pd(y2,_mm256_set1_pd(28.0)))))));
        Packed value=_mm256_div_pd(numerator,denominator);
#if defined(CS80_PHASE_BATCH_MUTATE_SIMD_OTA)
        value=_mm256_add_pd(value,_mm256_set1_pd(0.0001));
#endif
        value=_mm256_blendv_pd(value,_mm256_set1_pd(1.0),high);
        value=_mm256_blendv_pd(value,_mm256_set1_pd(-1.0),low);
        return value;
    }

    __attribute__((target("avx2,no-fma"),noinline))
    static void processFourAvx2(Ig00156PhaseCorrection* const* p,
                                const float* input, Output* output) noexcept
    {
        using V=Packed;
#define CS80_GATHER(field) packed(p[0]->field,p[1]->field,p[2]->field,p[3]->field)
        const V one=_mm256_set1_pd(1.0), two=_mm256_set1_pd(2.0);
        const V zeroV=_mm256_setzero_pd(), invLevel=_mm256_set1_pd(1.0/kIg00156OtaLevel);
        V gathered=packed(input[0],input[1],input[2],input[3]);
#if defined(CS80_PHASE_BATCH_MUTATE_SIMD_LANE)
        gathered=_mm256_permute4x64_pd(gathered,0xe1); // swap independent lanes 0/1
#endif
        const V u=_mm256_mul_pd(gathered,CS80_GATHER(scale));
        const V history0=CS80_GATHER(inputs[0]);
        const V history1=CS80_GATHER(inputs[1]);
        const V history2=CS80_GATHER(inputs[2]);
        const V history3=CS80_GATHER(inputs[3]);
        V derivative=_mm256_mul_pd(_mm256_set1_pd(11.0),u);
        derivative=_mm256_sub_pd(derivative,_mm256_mul_pd(_mm256_set1_pd(29.0),history0));
        derivative=_mm256_add_pd(derivative,_mm256_mul_pd(_mm256_set1_pd(27.0),history1));
        derivative=_mm256_sub_pd(derivative,_mm256_mul_pd(_mm256_set1_pd(11.0),history2));
        derivative=_mm256_add_pd(derivative,_mm256_mul_pd(two,history3));
        derivative=_mm256_div_pd(derivative,_mm256_mul_pd(_mm256_set1_pd(6.0),CS80_GATHER(h)));
        const V drive=_mm256_sub_pd(_mm256_mul_pd(_mm256_mul_pd(_mm256_set1_pd(.5),CS80_GATHER(h)),
            _mm256_add_pd(u,history0)),_mm256_mul_pd(CS80_GATHER(beta),derivative));
        const V du=_mm256_sub_pd(u,history0);
        V rhs[4] {};
        for(int i=0;i<4;++i) {
            // Preserve the scalar i,j and k=0,1,2,3 accumulation order
            // independently inside each SIMD double lane; no FMA contraction.
            rhs[i]=_mm256_sub_pd(_mm256_mul_pd(CS80_GATHER(b[i]),drive),
                                  _mm256_mul_pd(_mm256_mul_pd(CS80_GATHER(beta),
                                                               CS80_GATHER(ab[i])),du));
            for(int j=0;j<4;++j)
                rhs[i]=_mm256_add_pd(rhs[i],_mm256_mul_pd(CS80_GATHER(right[i][j]),
                                                                 CS80_GATHER(v[j])));
        }
        const V hpPrev=_mm256_sub_pd(_mm256_mul_pd(two,_mm256_add_pd(CS80_GATHER(v[1]),
                                                                 CS80_GATHER(v[2]))),CS80_GATHER(v[3]));
        const V g=_mm256_mul_pd(_mm256_mul_pd(_mm256_set1_pd(.5),CS80_GATHER(h)),CS80_GATHER(gm));
        rhs[2]=_mm256_add_pd(rhs[2],_mm256_mul_pd(g,_mm256_sub_pd(hpPrev,
            _mm256_mul_pd(_mm256_set1_pd(kIg00156OtaLevel),CS80_GATHER(previousHpTanh)))));
        rhs[3]=_mm256_add_pd(rhs[3],_mm256_mul_pd(g,_mm256_sub_pd(CS80_GATHER(v[2]),
            _mm256_mul_pd(_mm256_set1_pd(kIg00156OtaLevel),CS80_GATHER(previousBpTanh)))));
        constexpr int e[3]={0,1,3};
        V roots[3] {};
        for(int i=0;i<3;++i) {
            roots[i]=zeroV;
            for(int j=0;j<3;++j)
                roots[i]=_mm256_add_pd(roots[i],_mm256_mul_pd(CS80_GATHER(inverse[i][j]),rhs[e[j]]));
        }
        const V hp0=_mm256_sub_pd(_mm256_mul_pd(two,roots[1]),roots[2]);
        V residual0=_mm256_sub_pd(zeroV,rhs[2]);
        for(int i=0;i<3;++i)
            residual0=_mm256_add_pd(residual0,_mm256_mul_pd(CS80_GATHER(left[2][e[i]]),roots[i]));
        const V s1=_mm256_mul_pd(CS80_GATHER(slope1),invLevel);
        const V s2=_mm256_mul_pd(CS80_GATHER(slope2),invLevel);
        const V nonlinearS1=_mm256_mul_pd(CS80_GATHER(nonlinear),s1);
        const V numer=_mm256_add_pd(residual0,_mm256_mul_pd(nonlinearS1,hp0));
        const V denom=_mm256_add_pd(
            _mm256_add_pd(CS80_GATHER(residualBp),_mm256_mul_pd(CS80_GATHER(residualTanh),s2)),
            _mm256_mul_pd(nonlinearS1,_mm256_add_pd(CS80_GATHER(hpBp),
                                                      _mm256_mul_pd(CS80_GATHER(hpTanh),s2))));
        const V sign=_mm256_set1_pd(-0.0);
        V bp=_mm256_div_pd(_mm256_xor_pd(numer,sign),denom);
        V hp=zeroV,t1=zeroV,t2=zeroV;
        V active=_mm256_castsi256_pd(_mm256_set1_epi64x(-1));
        for(int it=0;it<8;++it) {
            const V nextT2=packedTanh(_mm256_mul_pd(bp,invLevel));
            const V nextHp=_mm256_add_pd(_mm256_add_pd(hp0,
                _mm256_mul_pd(CS80_GATHER(hpBp),bp)),_mm256_mul_pd(CS80_GATHER(hpTanh),nextT2));
            const V nextT1=packedTanh(_mm256_mul_pd(nextHp,invLevel));
            const V residual=_mm256_add_pd(_mm256_add_pd(
                _mm256_add_pd(residual0,_mm256_mul_pd(CS80_GATHER(residualBp),bp)),
                _mm256_mul_pd(CS80_GATHER(residualTanh),nextT2)),
                _mm256_mul_pd(CS80_GATHER(nonlinear),nextT1));
            hp=_mm256_blendv_pd(hp,nextHp,active);
            t1=_mm256_blendv_pd(t1,nextT1,active);
            t2=_mm256_blendv_pd(t2,nextT2,active);
            const V converged=(it==7)?active:_mm256_and_pd(active,
                _mm256_cmp_pd(_mm256_andnot_pd(sign,residual),_mm256_set1_pd(1e-10),_CMP_LT_OQ));
            const int exitMask=_mm256_movemask_pd(converged);
            if (exitMask!=0) {
                alignas(32) double residualValues[4];
                _mm256_store_pd(residualValues,residual);
                for(int l=0;l<4;++l) if (exitMask & (1<<l)) {
                    p[l]->residualAtExit=residualValues[l];
                    p[l]->stepsAtExit=static_cast<unsigned>(it
#if defined(CS80_PHASE_BATCH_MUTATE_SIMD_NEWTON)
                                                          +1
#endif
                                                          );
                }
            }
            active=_mm256_andnot_pd(converged,active);
            if (_mm256_movemask_pd(active)==0) break;
            const V d2=_mm256_mul_pd(_mm256_sub_pd(one,_mm256_mul_pd(t2,t2)),invLevel);
            const V d1=_mm256_mul_pd(_mm256_sub_pd(one,_mm256_mul_pd(t1,t1)),invLevel);
            const V jacobian=_mm256_add_pd(_mm256_add_pd(CS80_GATHER(residualBp),
                _mm256_mul_pd(CS80_GATHER(residualTanh),d2)),
                _mm256_mul_pd(_mm256_mul_pd(CS80_GATHER(nonlinear),d1),
                    _mm256_add_pd(CS80_GATHER(hpBp),_mm256_mul_pd(CS80_GATHER(hpTanh),d2))));
            const V safeResidual=_mm256_blendv_pd(zeroV,residual,active);
            const V safeJacobian=_mm256_blendv_pd(one,jacobian,active);
            const V corrected=_mm256_sub_pd(bp,_mm256_div_pd(safeResidual,safeJacobian));
            bp=_mm256_blendv_pd(bp,corrected,active);
        }
        V nodes[3];
        for(int i=0;i<3;++i)
            nodes[i]=_mm256_add_pd(_mm256_add_pd(roots[i],
                _mm256_mul_pd(CS80_GATHER(fromBp[i]),bp)),
                _mm256_mul_pd(CS80_GATHER(fromTanh[i]),t2));
        const V hpValid=_mm256_cmp_pd(_mm256_andnot_pd(sign,hp),
                                       _mm256_set1_pd(1e-9),_CMP_GT_OQ);
        const V bpValid=_mm256_cmp_pd(_mm256_andnot_pd(sign,bp),
                                       _mm256_set1_pd(1e-9),_CMP_GT_OQ);
        const V slopeHp=_mm256_blendv_pd(one,_mm256_div_pd(
            _mm256_blendv_pd(zeroV,t1,hpValid),
            _mm256_mul_pd(_mm256_blendv_pd(one,hp,hpValid),invLevel)),hpValid);
        const V slopeBp=_mm256_blendv_pd(one,_mm256_div_pd(
            _mm256_blendv_pd(zeroV,t2,bpValid),
            _mm256_mul_pd(_mm256_blendv_pd(one,bp,bpValid),invLevel)),bpValid);
        alignas(32) double node0[4],node1[4],node3[4], bpValue[4],hpValue[4],
                           tanh1Value[4],tanh2Value[4],slopeHpValue[4],slopeBpValue[4],uValue[4];
        _mm256_store_pd(node0,nodes[0]); _mm256_store_pd(node1,nodes[1]);
        _mm256_store_pd(node3,nodes[2]); _mm256_store_pd(bpValue,bp);
        _mm256_store_pd(hpValue,hp); _mm256_store_pd(tanh1Value,t1);
        _mm256_store_pd(tanh2Value,t2); _mm256_store_pd(slopeHpValue,slopeHp);
        _mm256_store_pd(slopeBpValue,slopeBp); _mm256_store_pd(uValue,u);
        for(int i=0;i<4;++i) {
            auto& item=*p[i];
            item.v[0]=node0[i]; item.v[1]=node1[i]; item.v[3]=node3[i]; item.v[2]=bpValue[i];
            item.slope1=slopeHpValue[i]; item.slope2=slopeBpValue[i];
            item.previousHpTanh=tanh1Value[i]; item.previousBpTanh=tanh2Value[i];
            for(int j=3;j>0;--j) item.inputs[j]=item.inputs[j-1];
            item.inputs[0]=uValue[i];
            output[i]={float(item.v[3]),float(hpValue[i]),float(-bpValue[i])};
        }
#undef CS80_GATHER
    }
#endif
    void refreshOtaCurrents() noexcept
    {
        previousHpTanh = fastTanh((2.0 * (v[1] + v[2]) - v[3]) / kIg00156OtaLevel);
        previousBpTanh = fastTanh(v[2] / kIg00156OtaLevel);
    }
    Network net;
    double gs=0,gg=0,gf=0,gt=0,ff=0;
    double inputRow=0,feedbackInput=0,feedbackDiagonal=0;
    Vector v {}, inputs {}, b {}, ab {};
    Matrix left {}, right {};
    double inverse[3][3] {};
    std::array<double,3> fromBp {},fromTanh {};
    double scale=1,h=0,beta=0,gm=0,nonlinear=0;
    double hpBp=0,hpTanh=0,residualBp=0,residualTanh=0,slope1=1,slope2=1;
    double residualAtExit = 0.0;
    double previousHpTanh = 0.0, previousBpTanh = 0.0;
    unsigned stepsAtExit = 0;
    float lastFc=-1,lastQ=-1,lastFs=-1;
};
}
