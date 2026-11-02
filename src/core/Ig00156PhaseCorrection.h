#pragma once
// Numerical phase correction for the existing declared IG00156 network, not a new
// circuit law. Fourth-order Padé phase correction of the linear nodal part;
// the same OTA currents are retained in the implicit trapezoidal residual.
// Derivation/evaluation: docs/IG00156_PHASE_CORRECTION.md. The template merely
// keeps the coefficient kernel independent of the public network declaration.
#include "Dsp.h"
#include "FastMath.h"
#include <array>

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
private:
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
