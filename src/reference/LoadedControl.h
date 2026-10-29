#pragma once

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

// Offline, settled-DC boundary experiment only. Not an IC model or audio mapping.
// Primary-source values and exclusions: docs/LOADED_CONTROL_EVIDENCE.md.
namespace cs80::reference::loaded_control {
inline constexpr double unknown = std::numeric_limits<double>::quiet_NaN();
enum class Grade { unknown, A, B, C };
struct Source {
    double volts = unknown;          // open-circuit Thevenin voltage
    double ohms = unknown;           // caller-supplied impedance; 0 explicitly ideal
};
struct Receiver {
    double siemens = unknown;
    double amps = unknown;           // current INTO chip = siemens * Vpin + amps
};
struct Inputs {
    // Order: KC, TF, AB, IB, SUB, BR, TP1 (local envelope). Each feeds BOTH nodes.
    // "shared" means HP/LP fork, not rank-wide: KC/TF/AB/IB and TP1 are per voice.
    std::array<Source, 7> shared;
    Source fh, fl;                   // R39 master cutoff source endpoints
    Source hpTrim, lpTrim;           // VR7/VR9 wiper Thevenin equivalents
    Receiver hpReceiver, lpReceiver; // must be explicitly supplied, even for open fixture
    Grade hpGrade = Grade::unknown, lpGrade = Grade::unknown;
};
struct Result {
    double hpVolts, lpVolts;
    std::array<double, 7> sourceTerminalVolts;
};
inline constexpr std::array<double, 7> hpBranchOhms{120e3,120e3,120e3,120e3,120e3,680e3,100e3};
inline constexpr std::array<double, 7> lpBranchOhms{39e3,39e3,39e3,39e3,39e3,220e3,47e3};
inline constexpr double cutoffBranchOhms = 39e3;
inline constexpr double trimBranchOhms = 150e3;
inline constexpr double trimPotOhms = 100e3;
inline constexpr double nodeFarads = 0.01e-6; // open at DC; NO transient pole asserted
inline void require(bool ok) { if (!ok) throw std::invalid_argument("Unspecified/invalid loaded-control boundary"); }
inline void validate(Source s) { require(std::isfinite(s.volts) && std::isfinite(s.ohms) && s.ohms >= 0); }
inline void validate(Receiver r) { require(std::isfinite(r.siemens) && r.siemens >= 0 && std::isfinite(r.amps)); }
inline double shuntOhms(Grade grade, bool hp) {
    switch (grade) {
        case Grade::A: return hp ? 2700 : 3300;
        case Grade::B: return hp ? 2200 : 2700;
        case Grade::C: return hp ? 1800 : 2200;
        default: throw std::invalid_argument("Unknown IG00156 grade");
    }
}
// Position is electrical resistance fraction from low rail, NOT a panel taper.
// Ideal fixed rails are an explicit fixture assumption, not a measured supply model.
inline Source trimWiper(double lowVolts, double highVolts, double fraction) {
    require(std::isfinite(lowVolts) && std::isfinite(highVolts) &&
            std::isfinite(fraction) && fraction >= 0 && fraction <= 1);
    return {lowVolts + fraction * (highVolts-lowVolts), trimPotOhms*fraction*(1-fraction)};
}
inline Result solve(const Inputs& p) {
    validate(p.hpReceiver); validate(p.lpReceiver);
    double a = 1/shuntOhms(p.hpGrade,true) + p.hpReceiver.siemens;
    double d = 1/shuntOhms(p.lpGrade,false) + p.lpReceiver.siemens;
    double b = 0, h = -p.hpReceiver.amps, l = -p.lpReceiver.amps;
    for (std::size_t i=0; i<p.shared.size(); ++i) {
        const auto s=p.shared[i]; validate(s);
        const double gh=1/hpBranchOhms[i], gl=1/lpBranchOhms[i];
        // Eliminate source terminal from its KCL. This retains HP↔LP coupling
        // through finite source impedance, unlike two independent dividers.
        const double z=1+s.ohms*(gh+gl);
        a += gh*(1+s.ohms*gl)/z;
        d += gl*(1+s.ohms*gh)/z;
        b -= s.ohms*gh*gl/z;
        h += gh*s.volts/z; l += gl*s.volts/z;
    }
    const auto branch=[](Source s, double r, double& diagonal, double& rhs) {
        validate(s); const double g=1/(r+s.ohms); diagonal+=g; rhs+=g*s.volts;
    };
    branch(p.fh,cutoffBranchOhms,a,h); branch(p.fl,cutoffBranchOhms,d,l);
    branch(p.hpTrim,trimBranchOhms,a,h); branch(p.lpTrim,trimBranchOhms,d,l);
    const double det=a*d-b*b;
    require(std::isfinite(det) && det>0);
    Result out{(h*d-b*l)/det, (a*l-b*h)/det, {}};
    require(std::isfinite(out.hpVolts) && std::isfinite(out.lpVolts));
    for (std::size_t i=0; i<p.shared.size(); ++i) {
        const auto s=p.shared[i]; const double gh=1/hpBranchOhms[i],gl=1/lpBranchOhms[i];
        out.sourceTerminalVolts[i]=(s.volts+s.ohms*(gh*out.hpVolts+gl*out.lpVolts))/(1+s.ohms*(gh+gl));
        require(std::isfinite(out.sourceTerminalVolts[i]));
    }
    return out;
}
} // namespace cs80::reference::loaded_control
