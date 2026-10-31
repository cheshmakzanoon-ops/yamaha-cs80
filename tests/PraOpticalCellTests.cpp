// ws4 Step 4: PRA wah optical cell with asymmetric attack and decay (declared LDR model),
// realtime PraWahCircuit and Engine held to the ODE truth model in reference/PraWah.h by
// the pre-registered bounds in tests/Ws4Tolerances.h. Agreement with these equations is
// not agreement with a measured CS-80 lamp or photoresistor; see
// docs/workstreams/ws4-circuits.md.
#include "core/Engine.h"
#include "core/PraWahCircuit.h"
#include "reference/PraWah.h"
#include "Ws4Tolerances.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace cs80;
namespace ref = cs80::reference::pra_wah;
namespace tol = cs80::ws4_tolerances::pra;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (! ok)
    {
        if (failures < 40)
            std::fprintf(stderr, "FAIL: %s\n", what.c_str());
        ++failures;
    }
}
constexpr double kPi = 3.14159265358979323846;
// Declared nominal profile values, as literals (a changed default cannot move the oracle).
constexpr double kHeel = 1000000.0, kToe = 2000.0, kAttack = 0.012, kDecay = 0.060;

void stepResponses()
{
    for (double rate : { 88200.0, 96000.0, 192000.0 })
    {
        PraWahCircuit wah;
        wah.prepare(rate, PraWahCircuit::Profile {}, 0.0); // dark (heel)
        const double lh = std::log(kHeel), lt = std::log(kToe);
        // Brighten: resistance falls with the attack time constant.
        wah.setPedal(1.0);
        const int na = (int) std::lround(kAttack * rate);
        for (int n = 0; n < na; ++n)
            wah.process(0.0);
        const double afterAttack = std::log(wah.opticalResistance());
        const double wantAttack = lt + (lh - lt) * std::exp(-(double) na / (kAttack * rate));
        check(std::abs(afterAttack - wantAttack) <= tol::kStepLogNepers, "attack follows its time constant");
        // Darken from there: resistance rises with the decay time constant.
        wah.setPedal(0.0);
        const int nd = (int) std::lround(kDecay * rate);
        for (int n = 0; n < nd; ++n)
            wah.process(0.0);
        const double wantDecay = lh + (afterAttack - lh) * std::exp(-(double) nd / (kDecay * rate));
        check(std::abs(std::log(wah.opticalResistance()) - wantDecay) <= tol::kStepLogNepers,
              "decay follows its slower time constant");
    }
    // 90 % traversal times scale with the two time constants.
    const auto traverse = [](double from, double to)
    {
        PraWahCircuit wah;
        wah.prepare(96000.0, PraWahCircuit::Profile {}, from);
        wah.setPedal(to);
        const double start = std::log(wah.opticalResistance());
        const double end = std::log(kHeel) + (std::log(kToe) - std::log(kHeel)) * to;
        int n = 0;
        while (std::abs(std::log(wah.opticalResistance()) - end) > 0.1 * std::abs(end - start))
        {
            wah.process(0.0);
            ++n;
        }
        return n / 96000.0;
    };
    const double up = traverse(0.0, 1.0), down = traverse(1.0, 0.0);
    check(std::abs(down / up - kDecay / kAttack) < 0.01, "darkening is slower than brightening by the declared ratio");
    std::printf("PRA optical 90%% traverse: brighten %.1f ms, darken %.1f ms\n", up * 1e3, down * 1e3);
}

// Continuous pedal sweep: realtime per-sample log R against the RK4 ODE truth.
struct SweepResult
{
    double worst = 0.0, meanRealtime = 0.0, meanTruth = 0.0, meanTarget = 0.0;
    double resonanceMin = 1e30, resonanceMax = 0.0;
};
SweepResult sweep(double rate, double hz, const PraWahCircuit::Profile& profile, const ref::OpticalCell& cell)
{
    PraWahCircuit wah;
    wah.prepare(rate, profile, 0.5);
    const double lh = std::log(profile.heelOhms), lt = std::log(profile.toeOhms);
    const auto pedal = [hz](double t) { return 0.5 + 0.5 * std::sin(2.0 * kPi * hz * t); };
    const auto target = [&](double t) { return lh + (lt - lh) * pedal(t); };
    double truth = std::log(wah.opticalResistance());
    SweepResult r;
    const int settle = (int) std::lround(rate / hz), cycle = settle; // one cycle to settle, one measured
    for (int n = 1; n <= settle + cycle; ++n)
    {
        const double t = n / rate;
        wah.setPedal(pedal(t));
        wah.process(0.0);
        truth = ref::integrateOptical(cell, truth, target, (n - 1) / rate, t, 64);
        if (n <= settle)
            continue;
        const double got = std::log(wah.opticalResistance());
        r.worst = std::max(r.worst, std::abs(got - truth));
        r.meanRealtime += got / cycle;
        r.meanTruth += truth / cycle;
        r.meanTarget += target(t) / cycle;
        // Nominal middle-network resonance 1 / (2 pi sqrt(t1 t2)), t1 = Rt 2.2 nF, t2 = 10 k 33 nF.
        const double ropt = wah.opticalResistance();
        const double rt = profile.vr1Ohms + ropt / (1.0 + ropt / profile.parallelOhms);
        const double f = 1.0 / (2.0 * kPi * std::sqrt(rt * 2.2e-9 * 1.0e4 * 33e-9));
        r.resonanceMin = std::min(r.resonanceMin, f);
        r.resonanceMax = std::max(r.resonanceMax, f);
    }
    return r;
}

void sweepDynamics()
{
    const ref::OpticalCell cell { kAttack, kDecay };
    for (double rate : { 88200.0, 96000.0, 192000.0 })
        for (double hz : { 0.5, 3.0, 8.0 })
        {
            const auto r = sweep(rate, hz, PraWahCircuit::Profile {}, cell);
            const std::string where = std::to_string(hz) + " Hz sweep at " + std::to_string((int) rate);
            check(r.worst <= tol::kSweepLogNepers, "optical sweep follows the ODE truth: " + where);
            check(std::abs(r.meanRealtime - r.meanTruth) <= tol::kHysteresisNepers, "cycle-mean log R follows truth: " + where);
            // Asymmetry signature: the faster brightening biases the cycle toward low R.
            if (hz >= 3.0)
                check(r.meanRealtime < r.meanTarget - 0.05, "asymmetric cell lags toward the bright end: " + where);
            if (rate == 96000.0)
                std::printf("PRA %.1f Hz sweep: ODE error %.2e Np, cycle-mean bias %+.3f Np, resonance %.0f-%.0f Hz\n",
                            hz, r.worst, r.meanRealtime - r.meanTarget, r.resonanceMin, r.resonanceMax);
        }
    // Before -> after reference point: the previous symmetric 12 ms law on the same sweep.
    PraWahCircuit::Profile symmetric;
    symmetric.opticalDecaySeconds = kAttack;
    const auto before = sweep(96000.0, 3.0, symmetric, { kAttack, kAttack });
    const auto after = sweep(96000.0, 3.0, PraWahCircuit::Profile {}, cell);
    std::printf("PRA 3 Hz sweep before (symmetric 12 ms): bias %+.3f Np, resonance %.0f-%.0f Hz; "
                "after: bias %+.3f Np, resonance %.0f-%.0f Hz\n",
                before.meanRealtime - before.meanTarget, before.resonanceMin, before.resonanceMax,
                after.meanRealtime - after.meanTarget, after.resonanceMin, after.resonanceMax);
    check(std::abs(before.meanRealtime - before.meanTarget) < 0.02, "a symmetric cell has no cycle bias (control)");
}

// Engine: EXP-WAH in isolation; the optical state follows the asymmetric closed form.
void engineOpticalTrajectory()
{
    ParamValues p;
    p[P_parityMode] = 1.0f;
    p[P_footMode] = 1.0f;
    Engine e;
    e.setParams(p);
    e.prepare(48000.0);
    const double rate = e.getInternalSampleRate();
    float frame[2];
    // Reset convention: toe (bright). Darken to heel, then brighten half way.
    double start = std::log(kToe);
    const double heel = std::log(kHeel), half = std::log(kHeel) + (std::log(kToe) - std::log(kHeel)) * 0.5;
    e.setExpression(0.0f);
    bool follows = true;
    for (int frame0 = 1; frame0 <= 4800; ++frame0)
    {
        e.render(frame, 1);
        const double want = heel + (start - heel) * std::exp(-2.0 * frame0 / (kDecay * rate));
        follows = follows && std::abs(std::log(e.getPraWahState(0).opticalOhms) - want) <= tol::kEngineLogNepers;
    }
    check(follows, "engine optical cell darkens with the decay time constant");
    start = std::log(e.getPraWahState(0).opticalOhms);
    e.setExpression(0.5f);
    for (int frame0 = 1; frame0 <= 4800; ++frame0)
    {
        e.render(frame, 1);
        const double want = half + (start - half) * std::exp(-2.0 * frame0 / (kAttack * rate));
        follows = follows && std::abs(std::log(e.getPraWahState(0).opticalOhms) - want) <= tol::kEngineLogNepers;
    }
    check(follows, "engine optical cell brightens with the attack time constant");
    check(e.getPraWahState(0).opticalOhms == e.getPraWahState(1).opticalOhms, "both PRA channels share the optical trajectory");
}
} // namespace

int main()
{
    stepResponses();
    sweepDynamics();
    engineOpticalTrajectory();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
