// Numerical known-deviation guards, not a new filter model or parity claim.
// The graph and control mapping here are pinned independently of production
// constants. The matrix oracle is never used by the running stage.
#include "core/Ig00156Filter.h"
#include "reference/IG00156Network.h"
#include "Ws1Tolerances.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <exception>
#include <string>

namespace {
namespace ref = cs80::reference::ig00156;
using Complex = std::complex<double>;
constexpr double pi = 3.14159265358979323846;
constexpr double ffHz = 1.0 / (2.0 * pi * 4.7e6 * 1.5e-9);
constexpr std::array<double, 4> rates { 88200.0, 96000.0, 192000.0, 384000.0 };
constexpr std::array<double, 5> qs { 0.5, 1.0, 2.0, 5.0, 9.0 };
// Literal declared graph, not values copied from the DUT at runtime.
const ref::StageNetwork hpNet { 75000.0, 33000.0, 1e-6, 1.2e6, 180e-12,
                                1.5e-9, 1.5e-9, 4.7e6, 4.7e6 };
const ref::StageNetwork lpNet { 150000.0, 33000.0, 1e-6, 1.2e6, 180e-12,
                                1.5e-9, 1.5e-9, 4.7e6, 4.7e6 };
int checks = 0, failures = 0;
void check(bool ok, const char* what)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", what); }
}

ref::StageControl control(const ref::StageNetwork& n, double totalHz, double q)
{
    const double conductance = 1.0 / n.sourceOhms + 1.0 / n.groundOhms + 1.0 / n.feedbackOhms;
    // Declared natural-frequency compensation, independently evaluated here.
    const double omega = 2.0 * pi * std::min(totalHz, 20000.0);
    const double beta = omega * omega * n.feedbackFarads / conductance;
    const double rate = (beta + std::sqrt(beta * beta + omega * omega)) / omega * 2.0 * pi * totalHz;
    return { std::max(0.0, rate * n.c1Farads - 1.0 / n.feedforward1Ohms),
             std::max(0.0, rate * n.c2Farads - 1.0 / n.feedforward2Ohms),
             std::max(0.0, conductance / (2.0 * q) - 1.0 / n.feedbackOhms), 0.0 };
}

struct Fixture
{
    double fs, fc, q, hpHz, lpHz;
    int config;
    ref::LinearSystem<8> pair, warpedPair;
    ref::LinearSystem<4> single, warpedSingle;
    ref::Vector<8> out8 {};
    ref::Vector<4> out4 {};
    double peak = 0.0;

    Fixture(double sampleRate, double cutoff, double resonance, int configuration)
        : fs(sampleRate), fc(cutoff), q(resonance),
          hpHz(configuration == 1 ? 0.0 : configuration == 2 ? std::max(60.0, cutoff / 4.0) : cutoff),
          lpHz(configuration == 0 ? 0.0 : cutoff), config(configuration)
    {
        const auto warp = [this](double hz) { const double t = pi * hz / fs; return std::tan(t) / t; };
        if (config == 2)
        {
            pair = ref::pairSystem(hpNet, control(hpNet, hpHz, q), lpNet, control(lpNet, lpHz, q),
                                   1.0 / ref::passbandGain(hpNet), 1.0 / ref::passbandGain(lpNet));
            warpedPair = pair;
            out8 = ref::lpRow<8>(4);
            for (int i = 0; i < 8; ++i)
            {
                const double p = warp(i < 4 ? hpHz : lpHz);
                for (double& value : warpedPair.a[static_cast<std::size_t>(i)]) value *= p;
                warpedPair.b[static_cast<std::size_t>(i)] *= p;
            }
        }
        else
        {
            const auto& n = config == 0 ? hpNet : lpNet;
            single = ref::stageSystem(n, control(n, fc, q), 1.0 / ref::passbandGain(n));
            warpedSingle = ref::scaled(single, warp(fc));
            out4 = config == 0 ? ref::hpRow<4>(0) : ref::lpRow<4>(0);
        }
        // Same physical peak-normalization search as the original gate.
        for (double f = 5.0; f < 24000.0; f *= 1.01)
            peak = std::max(peak, std::abs(continuous(f)));
    }
    Complex continuous(double hz) const
    {
        const Complex s { 0.0, 2.0 * pi * hz };
        return config == 2 ? ref::response(pair, out8, s) : ref::response(single, out4, s);
    }
    Complex discrete(double hz) const
    {
        const Complex s { 0.0, 2.0 * fs * std::tan(pi * hz / fs) };
        return config == 2 ? ref::response(warpedPair, out8, s) : ref::response(warpedSingle, out4, s);
    }
    double error(double hz) const { return std::abs(discrete(hz) - continuous(hz)) / peak; }
};

struct Sweep { double core = 0.0, band = 0.0, worstHz = 20.0; };
Sweep sweep(const Fixture& f, double hi)
{
    Sweep result;
    for (double hz = 20.0; hz <= hi; hz *= 1.02)
    {
        const double error = f.error(hz);
        if (error > result.band) { result.band = error; result.worstHz = hz; }
        if (hz >= 0.5 * f.fc && hz <= 2.0 * f.fc && hz <= f.fs / 8.0)
            result.core = std::max(result.core, error);
    }
    return result;
}

// Exercise actual process() output, not the response formula. Coherent bins
// make the complex lock-in independent of the starting phase and avoid FFT
// leakage or a magnitude-only comparison that would hide phase error.
Complex measured(const Fixture& f, double requestedHz, double& actualHz, int substeps = 1)
{
    constexpr int measure = 8192;
    actualHz = std::round(requestedHz * measure / f.fs) * f.fs / measure;
    const int settle = static_cast<int>(std::ceil(0.35 * f.fs));
    constexpr double amplitude = 1e-5;
    cs80::Ig00156Stage hp { cs80::kIg00156HighPassNetwork }, lp { cs80::kIg00156LowPassNetwork };
    hp.set(static_cast<float>(f.hpHz - ffHz), static_cast<float>(f.q), static_cast<float>(f.fs * substeps));
    lp.set(static_cast<float>(f.lpHz - ffHz), static_cast<float>(f.q), static_cast<float>(f.fs * substeps));
    Complex acc {};
    double previousInput = 0.0;
    for (int i = 0; i < settle + measure; ++i)
    {
        const double phase = 2.0 * pi * actualHz * i / f.fs;
        const double input = amplitude * std::sin(phase);
        float y = 0.0f;
        for (int k = 1; k <= substeps; ++k)
        {
            y = static_cast<float>(previousInput + (input - previousInput) * k / substeps);
            if (f.config != 1) y = hp.process(y).hp;
            if (f.config != 0) y = lp.process(y).lp;
        }
        previousInput = input;
        if (i >= settle) acc += static_cast<double>(y) * Complex(std::sin(phase), std::cos(phase));
    }
    return acc * (2.0 / measure / amplitude);
}

void accuracyGuards(const ws1::Tolerances& tol)
{
    double worstDirect = 0.0;
    for (double fs : rates)
        for (double fc : { 4000.0, 12000.0 })
        {
            const auto bounds = tol.range("limits_" + std::to_string(static_cast<int>(fs)) + "_" + std::to_string(static_cast<int>(fc)));
            Sweep aggregate;
            double directBand = 0.0;
            for (double q : qs)
                for (int config = 0; config < 3; ++config)
                {
                    const Fixture f(fs, fc, q, config);
                    const auto r = sweep(f, std::min(20000.0, fs / 5.0));
                    aggregate.core = std::max(aggregate.core, r.core);
                    aggregate.band = std::max(aggregate.band, r.band);
                    for (double requested : { fc, r.worstHz })
                    {
                        double hz = 0.0;
                        const auto h = measured(f, requested, hz);
                        const double discreteError = std::abs(h - f.discrete(hz)) / f.peak;
                        const double continuousError = std::abs(h - f.continuous(hz)) / f.peak;
                        worstDirect = std::max(worstDirect, discreteError);
                        directBand = std::max(directBand, continuousError);
                        if (discreteError > tol["direct_discrete_max_rel"] || continuousError > bounds.second)
                            std::fprintf(stderr, "direct fs=%g fc=%g q=%g config=%d f=%g discrete=%.9g continuous=%.9g\n",
                                         fs, fc, q, config, hz, discreteError, continuousError);
                        check(std::isfinite(discreteError) && discreteError <= tol["direct_discrete_max_rel"],
                              "actual high-cutoff output matches independent discrete nodal truth");
                        check(std::isfinite(continuousError) && continuousError <= bounds.second,
                              "actual high-cutoff output stays within known-deviation ceiling, not parity acceptance");
                    }
                }
            std::printf("known deviation fs=%g fc=%g core=%.9g band=%.9g direct-band=%.9g ceilings=%.6g/%.6g\n",
                        fs, fc, aggregate.core, aggregate.band, directBand, bounds.first, bounds.second);
            check(aggregate.core <= bounds.first, "high-cutoff core error does not exceed declared known-deviation ceiling");
            check(aggregate.band <= bounds.second, "high-cutoff band error does not exceed declared known-deviation ceiling");
        }
    std::printf("direct high-cutoff discrete-equivalence worst=%.9g, limit=%.9g\n", worstDirect, tol["direct_discrete_max_rel"]);
}

void rateRefinement(const ws1::Tolerances& tol)
{
    for (double base : { 88200.0, 96000.0 })
        for (double fc : { 4000.0, 12000.0 })
        {
            double previous = 0.0;
            for (int multiplier : { 1, 2, 4 })
            {
                double worst = 0.0;
                for (double q : qs)
                    for (int config = 0; config < 3; ++config)
                        worst = std::max(worst, sweep(Fixture(base * multiplier, fc, q, config),
                                                     std::min(20000.0, base / 5.0)).band);
                std::printf("common-band refinement base=%g fc=%g multiplier=%d error=%.9g ratio=%.9g\n",
                            base, fc, multiplier, worst, multiplier == 1 ? 1.0 : worst / previous);
                if (multiplier > 1)
                    check(worst <= previous * tol["refinement_ratio_max"], "common-band error decreases with timestep refinement");
                previous = worst;
            }
            double finalFullBand = 0.0;
            for (double q : qs)
                for (int config = 0; config < 3; ++config)
                    finalFullBand = std::max(finalFullBand, sweep(Fixture(base * 4.0, fc, q, config), 20000.0).band);
            std::printf("refined high-cutoff full 20 kHz band fs=%g fc=%g error=%.9g limit=%.9g\n",
                        base * 4.0, fc, finalFullBand, tol["refinement_final_full_band_max_rel"]);
            check(finalFullBand <= tol["refinement_final_full_band_max_rel"],
                  "refined high-cutoff cells meet the original 2 percent high-rate full-band limit");
        }
}

void substepExperiment()
{
    // Diagnostic only: extra solves with linearly interpolated input are NOT
    // an accepted high-quality runtime path. The external input reconstruction
    // remains part of the discretization error even with many internal steps.
    for (int config = 0; config < 3; ++config)
    {
        const Fixture f(96000.0, 12000.0, 5.0, config);
        for (int steps : { 1, 2, 4, 8 })
        {
            double hz = 0.0;
            const auto h = measured(f, 18000.0, hz, steps);
            std::printf("UNPROMOTED linear-input substeps config=%d steps=%d f=%g continuous-error=%.9g\n",
                        config, steps, hz, std::abs(h - f.continuous(hz)) / f.peak);
        }
    }
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances tol(argc > 1 ? argv[1] : "tests/parity/known-deviations/ig00156-top-octave.json",
                                  "ig00156_top_octave_regressions");
        accuracyGuards(tol);
        rateRefinement(tol);
        if (argc > 2 && std::string(argv[2]) == "--substep-experiment") substepExperiment();
        std::printf("%d checks, %d failures. Known-deviation guards are NOT a full-grid parity pass.\n", checks, failures);
        return failures == 0 ? 0 : 1;
    }
    catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 2; }
}
