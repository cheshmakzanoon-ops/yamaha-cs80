#pragma once
// Circuit-derived ideal-op-amp PRA model with an explicitly INFERRED profile.
// Not measured CS-80 pedal calibration or IC/FET/headroom equivalence.
// See docs/REALTIME_PRA_WAH_MODEL.md for graph, equations and limitations.
#include <algorithm>
#include <array>
#include <cmath>

namespace cs80 {
class PraWahCircuit {
public:
    struct Profile {
        double parallelOhms = 470000.0; // PRA54 candidate: assignment still unresolved.
        double vr1Ohms = 1000.0;
        double vr2Ohms = 20000.0; // Explicit trim gain, NOT extra output normalization.
        double sourceOhms = 1000.0;
        double dryLoadSiemens = 0.0;
        double heelOhms = 1000000.0;
        double toeOhms = 2000.0;
        // Declared LDR model (ws4-circuits Step 4): a log-resistance one-pole that is faster
        // while the cell brightens (resistance falling) than while it darkens. Both INFERRED.
        double opticalTauSeconds = 0.012;   // brightening (attack) time constant
        double opticalDecaySeconds = 0.060; // darkening (decay) time constant
    };
    struct Capacitor { double voltage = 0.0, current = 0.0; };
    using Capacitors = std::array<Capacitor, 4>; // Input, P, Q, output (D to C).
    struct Nodes {
        double x=0, n=0, a=0, p=0, b=0, q=0, c=0, d=0, y=0;
    };
    static constexpr double minimumRate = 8000.0, maximumRate = 1536000.0;
    static constexpr double minimumOpticalOhms = 100.0, maximumOpticalOhms = 10000000.0;
    static constexpr double maximumInputMagnitude = 1000000.0;

    PraWahCircuit() noexcept { prepare(96000.0, Profile{}, 1.0); }
    static bool validProfile(const Profile& p) noexcept {
        const auto within=[](double x,double lo,double hi) {
            return std::isfinite(x) && x>=lo && x<=hi;
        };
        return within(p.parallelOhms,10000.0,10000000.0)
            && within(p.vr1Ohms,0.0,25000.0) && within(p.vr2Ohms,0.0,100000.0)
            && within(p.sourceOhms,0.0,100000.0) && within(p.dryLoadSiemens,0.0,0.001)
            && within(p.heelOhms,minimumOpticalOhms,maximumOpticalOhms)
            && within(p.toeOhms,minimumOpticalOhms,maximumOpticalOhms)
            && within(p.opticalTauSeconds,0.0,1.0)
            && within(p.opticalDecaySeconds,0.0,5.0);
    }
    // Setup only. Invalid configuration leaves the entire prior model untouched.
    bool prepare(double sampleRate,const Profile& p,double pedal) noexcept {
        if (!validProfile(p) || !std::isfinite(sampleRate)
            || sampleRate<minimumRate || sampleRate>maximumRate || !std::isfinite(pedal))
            return false;
        profile=p;
        rate=sampleRate;
        for (std::size_t k=0;k<g.size();++k) g[k]=2.0*capacitance[k]*rate;
        opticalStep=p.opticalTauSeconds==0.0 ? 1.0 : -std::expm1(-1.0/(p.opticalTauSeconds*rate));
        decayStep=p.opticalDecaySeconds==0.0 ? 1.0 : -std::expm1(-1.0/(p.opticalDecaySeconds*rate));
        heelLog=std::log(p.heelOhms);
        toeLog=std::log(p.toeOhms);
        setPedal(pedal);
        reset();
        return true;
    }
    void reset() noexcept { caps={}; opticalLog=targetLog; }
    void reset(double pedal) noexcept { setPedal(pedal); reset(); }
    // Invalid controls are ignored; finite travel is clamped to physical endpoints.
    // Neither setter changes capacitor charge/current or jumps the optical state.
    void setPedal(double pedal) noexcept {
        if (std::isfinite(pedal))
            targetLog=heelLog+(toeLog-heelLog)*std::clamp(pedal,0.0,1.0);
    }
    // Explicit resistance control for conditional fixtures/model refinement.
    // Same smoothing as the pedal; reset() selects it immediately at startup.
    void setOpticalResistance(double ohms) noexcept {
        if (std::isfinite(ohms) && ohms>=minimumOpticalOhms && ohms<=maximumOpticalOhms)
            targetLog=std::log(ohms);
    }
    Nodes process(double input) noexcept {
        // This is a numerical interface guard, not a model of amplifier clipping.
        const double u=std::isfinite(input)
            ? std::clamp(input,-maximumInputMagnitude,maximumInputMagnitude) : 0.0;
        // Falling resistance = brightening = attack; rising = darkening = decay.
        opticalLog += (targetLog<opticalLog ? opticalStep : decayStep)*(targetLog-opticalLog);
        const double optical=std::exp(opticalLog);
        const double rt=profile.vr1Ohms+optical/(1.0+optical/profile.parallelOhms);
        std::array<double,4> j;
        for (std::size_t k=0;k<j.size();++k)
            j[k]=-g[k]*caps[k].voltage-caps[k].current;

        constexpr double R=10000.0, r=470.0;
        const double aP=1.0/(1.0+rt*g[1]), bP=-rt*j[1]*aP;
        const double aQ=1.0/(1.0+R*g[2]), bQ=-R*j[2]*aQ;
        const double bN=1.0-2.0*aP, b0=2.0*bP;
        const double cN=(2.0*aQ-1.0)*bN, c0=(2.0*aQ-1.0)*b0+2.0*bQ;
        const double dN=2.0/R+(1.0-cN)/r;
        const double nX=(1.0/R)/dN, n0=(c0/r)/dN;
        const double sourceDenominator=1.0+profile.sourceOhms*g[0];
        const double yI=g[0]/sourceDenominator, jSource=j[0]/sourceDenominator;
        Nodes out;
        out.x=(yI*u+jSource+n0/R)/(yI+(2.0-nX)/R+profile.dryLoadSiemens);
        out.n=nX*out.x+n0;
        out.a=-out.n;
        out.p=aP*out.a+bP;
        out.b=2.0*out.p-out.a;
        out.q=aQ*out.b+bQ;
        out.c=2.0*out.q-out.b;
        out.d=(g[3]*out.c-j[3])/(g[3]+2.0/R);
        out.y=-(profile.vr2Ohms/R)*out.d;
        const double inputCurrent=yI*(u-out.x)+jSource;
        const std::array<double,4> voltage {
            u-profile.sourceOhms*inputCurrent-out.x, out.p, out.q, out.d-out.c
        };
        for (std::size_t k=0;k<caps.size();++k) {
            caps[k].voltage=voltage[k];
            caps[k].current=g[k]*voltage[k]+j[k];
        }
        return out;
    }
    const Capacitors& capacitorState() const noexcept { return caps; }
    double opticalResistance() const noexcept { return std::exp(opticalLog); }
    double targetOpticalResistance() const noexcept { return std::exp(targetLog); }
    double sampleRate() const noexcept { return rate; }
    const Profile& getProfile() const noexcept { return profile; }
private:
    inline static constexpr std::array<double,4> capacitance {10e-6,2.2e-9,33e-9,1e-6};
    Profile profile;
    Capacitors caps{};
    std::array<double,4> g{};
    double rate=96000.0, opticalStep=1.0, decayStep=1.0, heelLog=0.0, toeLog=0.0;
    double opticalLog=0.0, targetLog=0.0;
};
} // namespace cs80
