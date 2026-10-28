#include "reference/IG00156Linear.h"
#include <complex>
#include <cstdio>
#include <limits>
#include <numbers>
#include <string>

using namespace cs80::reference::ig00156;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", label); }
}
template<class F> void rejects(F f, const char* label)
{
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    check(rejected, label);
}
constexpr double pi = std::numbers::pi;
PhysicalParameters fixture()
{
    // Illustrative fixture, NOT an identified CS-80 Q/control calibration.
    return {33000.0, 33000.0, 4.5e-6, 5.0e-6, 20e-6};
}
using Complex = std::complex<double>;
std::array<Complex, 3> transfer(const PhysicalParameters& p, double omega)
{
    // Independently eliminate a,b,l from node KCL, not the matrix solver:
    // D=(Gtotal+s*Cfb)*(s*s+w1*w2)+2*w1*s*(Gf+gq+s*Cfb).
    const Complex s(0, omega);
    const double gs=1/p.sourceOhms, gg=1/p.groundOhms, gf=1/p.feedbackOhms;
    const double w1=(p.gm1Siemens+1/p.feedforward1Ohms)/p.c1Farads;
    const double w2=(p.gm2Siemens+1/p.feedforward2Ohms)/p.c2Farads;
    const auto d=(gs+gg+gf+s*p.feedbackFarads)*(s*s+w1*w2)
                +2*w1*s*(gf+p.qSiemens+s*p.feedbackFarads);
    return {2*gs*s*s/d, -2*gs*w1*s/d, 2*gs*w1*w2/d};
}
Vector<3> physicalDerivative(const PhysicalParameters& p, Vector<3> z, double u)
{
    const double ai=z[0]+z[1], hp=2*ai-z[2];
    return {((u-ai)/p.sourceOhms-ai/p.groundOhms-z[0]/p.feedbackOhms
             +p.qSiemens*z[1])/p.feedbackFarads,
            -(p.gm1Siemens+1/p.feedforward1Ohms)*hp/p.c1Farads,
            -(p.gm2Siemens+1/p.feedforward2Ohms)*z[1]/p.c2Farads};
}
Vector<3> plus(Vector<3> a, Vector<3> b, double scale)
{
    for (std::size_t j=0;j<3;++j) a[j]+=scale*b[j];
    return a;
}
Vector<3> rk4(const PhysicalParameters& p, int count, double duration)
{
    Vector<3> z {};
    const double dt=duration/count;
    for (int i=0;i<count;++i)
    {
        auto k1=physicalDerivative(p,z,1);
        auto k2=physicalDerivative(p,plus(z,k1,dt/2),1);
        auto k3=physicalDerivative(p,plus(z,k2,dt/2),1);
        auto k4=physicalDerivative(p,plus(z,k3,dt),1);
        for (std::size_t j=0;j<3;++j) z[j]+=dt*(k1[j]+2*k2[j]+2*k3[j]+k4[j])/6;
    }
    return z;
}
double distance(Vector<3> a, Vector<3> b)
{
    double e=0; for (std::size_t i=0;i<3;++i) e=std::max(e,std::abs(a[i]-b[i]));
    return e;
}
Vector<3> trapezoid(const DynamicModel& model, int count, double duration)
{
    Vector<3> z {};
    for (int i=0;i<count;++i) z=model.system.step(z,1,1,duration/count).value;
    return z;
}
}
int main()
{
    try
    {
        const auto p=fixture();
        const DynamicModel model(p);
        Vector<3> zero {};
        for (int i=0;i<1000;++i) zero=model.system.step(zero,0,0,1.0/48000).value;
        check(zero==Vector<3>{}, "zero state and input remain exactly zero");
        const Vector<3> z {0.3,-0.1,0.7};
        const auto o=model.outputs(z);
        check(std::abs(o.hpVolts+o.lpVolts-2*o.aiVolts)<1e-15, "OP1 KCL");
        const auto dz=model.system.derivative(z,0.8);
        check(distance(dz,physicalDerivative(p,z,0.8))<1e-9, "matrix equals independent node equations");
        const double dt=1.0/192000;
        const auto next=model.system.step(z,0.8,0.2,dt);
        const auto on=model.outputs(next.value);
        const double gm1=p.gm1Siemens+1/p.feedforward1Ohms;
        const double gm2=p.gm2Siemens+1/p.feedforward2Ohms;
        check(std::abs(p.c1Farads*(on.bpVolts-o.bpVolts)/dt
                       +gm1*(on.hpVolts+o.hpVolts)/2)<1e-16, "integrator 1 trapezoid KCL");
        check(std::abs(p.c2Farads*(on.lpVolts-o.lpVolts)/dt
                       +gm2*(on.bpVolts+o.bpVolts)/2)<1e-16, "integrator 2 trapezoid KCL");
        check(next.diagnostics.relativeResidual<1e-13, "linear solve backward residual");

        // Routh-Hurwitz for the cubic: all positive and a2*a1>a3*a0.
        const double gt=1/p.sourceOhms+1/p.groundOhms+1/p.feedbackOhms;
        const double w1=gm1/p.c1Farads, w2=gm2/p.c2Farads;
        const double a3=p.feedbackFarads, a2=gt+2*w1*p.feedbackFarads;
        const double a1=p.feedbackFarads*w1*w2+2*w1*(1/p.feedbackOhms+p.qSiemens);
        const double a0=gt*w1*w2;
        check(a3>0&&a2>0&&a1>0&&a0>0&&a2*a1>a3*a0, "fixture continuous poles in left half-plane");
        for (double step : {1.0/8000,1.0/48000,1.0/192000})
        {
            Vector<3> decay {0.1,-0.3,0.5};
            for (int i=0;i<static_cast<int>(0.1/step);++i)
                decay=model.system.step(decay,0,0,step).value;
            check(distance(decay,{})<1e-10, "discrete unforced fixture decays");
        }
        for (double f : {100.0,500.0,2000.0})
        {
            constexpr int sampleRate=192000, settle=19200, measure=19200;
            Vector<3> state {};
            std::array<Complex,3> sum {};
            double previous=1.0;
            for (int i=1;i<=settle+measure;++i)
            {
                const double phase=2*pi*f*i/sampleRate;
                const double input=std::cos(phase);
                state=model.system.step(state,previous,input,1.0/sampleRate).value;
                previous=input;
                if (i>settle)
                {
                    const auto v=model.outputs(state);
                    const auto carrier=std::exp(Complex(0,-phase))*2.0/static_cast<double>(measure);
                    sum[0]+=v.hpVolts*carrier; sum[1]+=v.bpVolts*carrier; sum[2]+=v.lpVolts*carrier;
                }
            }
            const auto analog=transfer(p,2*pi*f);
            const auto bilinear=transfer(p,2*sampleRate*std::tan(pi*f/sampleRate));
            for (std::size_t j=0;j<3;++j)
            {
                check(std::abs(sum[j]-bilinear[j])<1e-8, "tone complex response equals bilinear analytic transfer");
                check(std::abs(sum[j]-analog[j])<0.002, "tone complex response close to continuous transfer");
            }
        }
        // Independent physical-node RK4, 5e-8 and 2.5e-8 s reference steps.
        constexpr double duration=0.001;
        const auto reference=rk4(p,20000,duration);
        const auto finer=rk4(p,40000,duration);
        check(distance(reference,finer)<1e-10, "independent RK4 reference convergence");
        const double e1=distance(trapezoid(model,100,duration),finer);
        const double e2=distance(trapezoid(model,200,duration),finer);
        const double e3=distance(trapezoid(model,400,duration),finer);
        std::printf("Trapezoid errors: %.9g %.9g %.9g\n",e1,e2,e3);
        check(e1>e2*3.5&&e2>e3*3.5&&e3<1e-5, "second order timestep convergence");

        auto reduced=p; reduced.feedbackFarads=0;
        const AlgebraicModel algebraic(reduced);
        const double gain=2/p.sourceOhms/gt, damping=2*(1/p.feedbackOhms+p.qSiemens)/gt;
        check(std::abs(algebraic.inputGain-gain)<1e-15
              &&std::abs(algebraic.damping-damping)<1e-15, "algebraic reduction coefficients");
        const auto ao=algebraic.outputs({-0.2,0.3},0.7);
        check(std::abs((ao.aiVolts-0.7)/p.sourceOhms+ao.aiVolts/p.groundOhms
              +(ao.aiVolts-ao.bpVolts)/p.feedbackOhms-p.qSiemens*ao.bpVolts)<1e-19,
              "Cfb zero algebraic node KCL");
        Vector<2> az {};
        for (int i=0;i<4000;++i) az=algebraic.system.step(az,1,1,1.0/192000).value;
        check(std::abs(az[0])<1e-8&&std::abs(az[1]-gain)<1e-8, "algebraic DC step response");
        for (double f : {100.0,500.0,2000.0})
        {
            const Complex s(0,2*pi*f);
            const Complex denominator=s*s+damping*w1*s+w1*w2;
            const std::array<Complex,3> limit {
                gain*s*s/denominator, -gain*w1*s/denominator, gain*w1*w2/denominator};
            const auto cubicLimit=transfer(reduced,2*pi*f);
            for (std::size_t j=0;j<3;++j)
                check(std::abs(cubicLimit[j]-limit[j])<1e-14, "cubic reduces to independent second-order transfer");
            Vector<2> state {};
            std::array<Complex,3> measured {};
            double previous=1.0;
            constexpr int sampleRate=192000, settle=19200, count=19200;
            for (int i=1;i<=settle+count;++i)
            {
                const double phase=2*pi*f*i/sampleRate, input=std::cos(phase);
                state=algebraic.system.step(state,previous,input,1.0/sampleRate).value;
                previous=input;
                if (i>settle)
                {
                    const auto out=algebraic.outputs(state,input);
                    const auto carrier=std::exp(Complex(0,-phase))*2.0/static_cast<double>(count);
                    measured[0]+=out.hpVolts*carrier;
                    measured[1]+=out.bpVolts*carrier;
                    measured[2]+=out.lpVolts*carrier;
                }
            }
            const Complex sw(0,2*sampleRate*std::tan(pi*f/sampleRate));
            const Complex dw=sw*sw+damping*w1*sw+w1*w2;
            const std::array<Complex,3> warped {
                gain*sw*sw/dw,-gain*w1*sw/dw,gain*w1*w2/dw};
            for (std::size_t j=0;j<3;++j)
                check(std::abs(measured[j]-warped[j])<1e-8, "separate algebraic solver complex transfer");
            auto small=p; small.feedbackFarads=1e-14;
            const auto nearLimit=transfer(small,2*pi*f);
            for (std::size_t j=0;j<3;++j)
                check(std::abs(nearLimit[j]-limit[j])<2e-5, "Cfb tends to independent algebraic transfer");
        }
        auto small=p; small.feedbackFarads=1e-14;
        const auto smallState=trapezoid(DynamicModel(small),4000,4000.0/192000);
        check(std::abs(smallState[1]-az[0])<1e-5&&std::abs(smallState[2]-az[1])<1e-5,
              "dynamic small-Cfb step approaches separate algebraic model");

        for (double bad : {-1.0,0.0,std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::quiet_NaN()})
        {
            for (auto member : {&PhysicalParameters::sourceOhms,&PhysicalParameters::groundOhms,
                  &PhysicalParameters::feedbackOhms,&PhysicalParameters::feedbackFarads,
                  &PhysicalParameters::c1Farads,&PhysicalParameters::c2Farads,
                  &PhysicalParameters::feedforward1Ohms,&PhysicalParameters::feedforward2Ohms})
                rejects([&]{auto q=p;q.*member=bad;DynamicModel invalid(q);}, "reject invalid physical passive");
            rejects([&]{model.system.step({},0,0,bad);}, "reject invalid timestep");
        }
        for (auto member : {&PhysicalParameters::gm1Siemens,&PhysicalParameters::gm2Siemens,
                             &PhysicalParameters::qSiemens})
            for (double bad : {-1.0,std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::quiet_NaN()})
                rejects([&]{auto q=p;q.*member=bad;DynamicModel invalid(q);}, "reject invalid gm");
        rejects([&]{AlgebraicModel invalid(p);}, "algebraic model rejects nonzero Cfb");
        rejects([&]{model.system.step({},0,std::numeric_limits<double>::quiet_NaN(),dt);}, "reject NaN input");
        rejects([&]{model.system.step({0,INFINITY,0},0,0,dt);}, "reject nonfinite state");
        rejects([&]{solve<2>({{{1,1},{1,1}}},{1,1});}, "singular diagnostic");
        rejects([&]{solve<2>({{{1,0},{0,1e-16}}},{1,1});}, "ill-conditioned pivot diagnostic");
        const auto swapped=solve<2>({{{0,2},{1,3}}},{4,7});
        check(std::abs(swapped.value[0]-1)<1e-15&&std::abs(swapped.value[1]-2)<1e-15, "partial pivot swap");
    }
    catch (const std::exception& e)
    {
        ++failures; std::printf("Unexpected exception: %s\n",e.what());
    }
    std::printf("Conditional IG00156 reference: %d checks, %d failures\n",checks,failures);
    return failures==0?0:1;
}
