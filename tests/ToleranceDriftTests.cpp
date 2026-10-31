// Plan step 2.5: circuit-engine component tolerances and thermal drift
// (src/core/ToleranceModel.h). Limits pre-registered in
// tests/parity/tolerances-v2/ws1-voice.json, stage "tolerance_drift". These
// check the declared model and its wiring, not a measured CS-80.
#include "core/Engine.h"
#include "core/ToleranceModel.h"
#include "Ws1Tolerances.h"

#include <cmath>
#include <array>
#include <complex>
#include <cstdio>
#include <string>
#include <vector>

using namespace cs80;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what.c_str()); }
}

void fft(std::vector<std::complex<double>>& a)
{
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * 3.14159265358979323846 / (double) len;
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0);
            for (std::size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl;
            }
        }
    }
}

void testDraws(const ws1::Tolerances& tol)
{
    constexpr int n = 4096;
    struct Field { const char* name; double sigma, limit; double tolerance::CardDraws::* member; };
    const Field fields[] = {
        { "hp C1", 0.05, 0.10, &tolerance::CardDraws::hpC1 }, { "hp C2", 0.05, 0.10, &tolerance::CardDraws::hpC2 },
        { "lp C1", 0.05, 0.10, &tolerance::CardDraws::lpC1 }, { "lp C2", 0.05, 0.10, &tolerance::CardDraws::lpC2 },
        { "level Rf", 0.025, 0.05, &tolerance::CardDraws::levelRf }, { "level Rin", 0.025, 0.05, &tolerance::CardDraws::levelRin },
        { "filter EG R", 0.025, 0.05, &tolerance::CardDraws::filterEnvR }, { "filter EG C", 0.05, 0.10, &tolerance::CardDraws::filterEnvC },
        { "amp EG R", 0.025, 0.05, &tolerance::CardDraws::ampEnvR }, { "amp EG C", 0.05, 0.10, &tolerance::CardDraws::ampEnvC },
    };
    std::vector<tolerance::CardDraws> cards;
    for (int i = 0; i < n; ++i) cards.push_back(tolerance::drawCard(i));
    // A truncated normal at +/- 2 sigma has a standard deviation of 0.8796 sigma.
    constexpr double truncatedFactor = 0.879626;
    for (const auto& f : fields)
    {
        double sum = 0.0, sq = 0.0, worst = 0.0;
        for (const auto& c : cards) { const double x = c.*(f.member); sum += x; sq += x * x; worst = std::max(worst, std::abs(x)); }
        const double mean = sum / n, sd = std::sqrt(sq / n - mean * mean);
        const double expected = f.sigma * truncatedFactor;
        std::printf("draw %-12s mean %+.5f  sd %.5f (declared sigma %.3f, truncated %.5f)  max %.4f\n",
                    f.name, mean, sd, f.sigma, expected, worst);
        check(std::abs(sd - expected) <= tol["draw_sigma_rel"] * expected, std::string("draw sigma ") + f.name);
        check(std::abs(mean) <= 3.0 * sd / std::sqrt((double) n), std::string("draw mean ") + f.name);
        check(worst <= f.limit * (1.0 + tol["truncation_rel"]), std::string("draw truncation ") + f.name);
    }
    double tuneWorst = 0.0;
    for (const auto& c : cards) tuneWorst = std::max(tuneWorst, std::abs(c.tune));
    check(tuneWorst <= 2.0, "tune residual truncated at 2 sigma");
    // Determinism: the same channel always draws the same card.
    const auto a = tolerance::drawCard(5), b = tolerance::drawCard(5);
    check(a.hpC1 == b.hpC1 && a.ampEnvC == b.ampEnvC && a.tune == b.tune, "draws are deterministic per channel");
}

void testMapping(const ws1::Tolerances& tol)
{
    const double limit = tol["mapping_law_rel"];
    double worst = 0.0;
    for (int i = 0; i < 64; ++i)
    {
        const auto d = tolerance::drawCard(i);
        for (double scale : { 0.5, 1.0, 2.0 })
        {
            const auto r = tolerance::ratios(d, scale);
            const auto rel = [&](double got, double want) { worst = std::max(worst, std::abs(got / want - 1.0)); };
            rel(r.hpCutoff, std::pow(1.0 / std::sqrt((1 + d.hpC1) * (1 + d.hpC2)), scale));
            rel(r.lpCutoff, std::pow(1.0 / std::sqrt((1 + d.lpC1) * (1 + d.lpC2)), scale));
            rel(r.hpQ, std::pow(std::sqrt((1 + d.hpC1) / (1 + d.hpC2)), scale));
            rel(r.lpQ, std::pow(std::sqrt((1 + d.lpC1) / (1 + d.lpC2)), scale));
            rel(r.level, std::pow((1 + d.levelRf) / (1 + d.levelRin), scale));
            rel(r.filterEnvTime, std::pow((1 + d.filterEnvR) * (1 + d.filterEnvC), scale));
            rel(r.ampEnvTime, std::pow((1 + d.ampEnvR) * (1 + d.ampEnvC), scale));
            rel(1.0 + r.tuneSemitones, 1.0 + scale * d.tune * tolerance::kTuneResidualSigmaCents / 100.0);
        }
        const auto zero = tolerance::ratios(d, 0.0);
        check(zero.hpCutoff == 1.0f && zero.lpQ == 1.0f && zero.level == 1.0f && zero.ampEnvTime == 1.0f
                  && zero.tuneSemitones == 0.0f, "dispersion 0 gives exact identity ratios");
    }
    std::printf("mapping laws: worst relative error %.3g\n", worst);
    check(worst <= limit, "mapping laws");
}

struct Snapshot { Engine::RankInfo info[8][2]; };

Snapshot heldChord(double rate, float dispersion, float parity, float drift = 0.0f, float driftModel = 0.0f, int frames = 9600)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f;
    p[P_parityMode] = parity;
    p[P_voiceDispersion] = dispersion;
    p[P_drift] = drift;
    p[P_driftModel] = driftModel;
    p[P_analogNoise] = 0.0f;
    // IL = AL = 0: the filter EG adds nothing to the cutoff, so the readout is the
    // static control law times the card's ratio (the EG time ratio would otherwise
    // move the snapshot instant along the envelope).
    for (int r = 0; r < 2; ++r)
        p[chParam(r, CP_fIl)] = p[chParam(r, CP_fAl)] = 0.0f;
    Engine e;
    e.prepare(rate);
    e.setParams(p);
    for (int k = 0; k < 8; ++k) e.noteOn(48 + 3 * k, 0.8f);
    std::vector<float> out((size_t) frames * 2);
    e.render(out.data(), frames);
    Snapshot s;
    for (int v = 0; v < 8; ++v)
        for (int r = 0; r < 2; ++r) s.info[v][r] = e.getRankInfo(v, r);
    return s;
}

void testEngine(const ws1::Tolerances& tol)
{
    const double limit = tol["engine_trim_rel"];
    for (double rate : { 44100.0, 96000.0 })
    {
        const auto nominal = heldChord(rate, 0.0f, 0.0f);
        const auto spread = heldChord(rate, tolerance::kDefaultDispersion, 0.0f);
        double worst = 0.0;
        for (int v = 0; v < 8; ++v)
            for (int r = 0; r < 2; ++r)
            {
                const auto ratio = tolerance::ratios(tolerance::drawCard(v * 2 + r), 1.0);
                const auto& a = spread.info[v][r];
                const auto& b = nominal.info[v][r];
                const auto rel = [&](double got, double want) { worst = std::max(worst, std::abs(got / want - 1.0)); };
                rel(a.hpfHz / b.hpfHz, ratio.hpCutoff);
                rel(a.lpfHz / b.lpfHz, ratio.lpCutoff);
                rel(a.hpfQ / b.hpfQ, ratio.hpQ);
                rel(a.lpfQ / b.lpfQ, ratio.lpQ);
                rel(a.oscillatorHz / b.oscillatorHz, std::exp2(ratio.tuneSemitones / 12.0));
            }
        std::printf("engine trims at %.0f Hz: worst relative error %.3g\n", rate, worst);
        check(worst <= limit, "engine applies the card ratios at the default dispersion (" + std::to_string((int) rate) + " Hz)");
        // PURE CS-80 masks VOICE DISPERSION: identical to dispersion 0.
        const auto pureSpread = heldChord(rate, tolerance::kDefaultDispersion, 1.0f);
        const auto pureNominal = heldChord(rate, 0.0f, 1.0f);
        bool identical = true;
        for (int v = 0; v < 8; ++v)
            for (int r = 0; r < 2; ++r)
                identical = identical && pureSpread.info[v][r].lpfHz == pureNominal.info[v][r].lpfHz
                            && pureSpread.info[v][r].oscillatorHz == pureNominal.info[v][r].oscillatorHz
                            && pureSpread.info[v][r].hpfQ == pureNominal.info[v][r].hpfQ;
        check(identical, "PURE CS-80 applies no component spread");
    }
}

void testDrift(const ws1::Tolerances& tol)
{
    // 1/f slope of the warm process.
    constexpr int cards = 32, segments = 8, n = 1 << 16;
    const double fs = tolerance::kDriftControlHz;
    std::vector<double> psd(n / 2, 0.0);
    std::vector<std::complex<double>> buf(n);
    for (int c = 0; c < cards; ++c)
    {
        tolerance::ThermalDrift d;
        d.prepare(96000.0, c);
        for (int s = 0; s < segments; ++s)
        {
            for (int i = 0; i < n; ++i)
            {
                d.advance();
                const double w = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979323846 * i / (n - 1));
                buf[(size_t) i] = w * d.warmComponent();
            }
            fft(buf);
            for (int k = 1; k < n / 2; ++k) psd[(size_t) k] += std::norm(buf[(size_t) k]);
        }
    }
    double sx = 0, sy = 0, sxx = 0, sxy = 0; int m = 0;
    for (int k = 1; k < n / 2; ++k)
    {
        const double f = k * fs / n;
        if (f < 0.003 || f > 0.3) continue;
        const double x = std::log10(f), y = std::log10(psd[(size_t) k]);
        sx += x; sy += y; sxx += x * x; sxy += x * y; ++m;
    }
    const double slope = (m * sxy - sx * sy) / (m * sxx - sx * sx);
    const auto [lo, hi] = tol.range("drift_psd_slope");
    std::printf("thermal drift PSD slope %.3f over 0.003..0.3 Hz (%d bins)\n", slope, m);
    check(slope >= lo && slope <= hi, "thermal drift is 1/f");

    // RMS at the default amount, warm-up excluded.
    double sq = 0.0; long count = 0;
    for (int c = 0; c < cards; ++c)
    {
        tolerance::ThermalDrift d;
        d.prepare(96000.0, 100 + c);
        for (int i = 0; i < 60000; ++i) { d.advance(); const double x = d.warmComponent(); sq += x * x; ++count; }
    }
    const double rms = std::sqrt(sq / (double) count);
    const auto [rlo, rhi] = tol.range("drift_rms_cents_at_default");
    std::printf("thermal drift RMS %.3f cents at the default amount\n", rms);
    check(rms >= rlo && rms <= rhi, "thermal drift RMS at the default amount");

    // Warm-up time constant.
    tolerance::ThermalDrift d;
    d.prepare(48000.0, 3);
    const double start = d.warmupInitial();
    for (int i = 0; i < (int) std::lround(tolerance::kWarmupSeconds * fs); ++i) d.advance();
    const double ratio = d.warmupComponent() / start;
    std::printf("warm-up after 120 s: %.5f of its start (1/e = %.5f)\n", ratio, std::exp(-1.0));
    check(std::abs(ratio / std::exp(-1.0) - 1.0) <= tol["warmup_tau_rel"], "warm-up decays with the declared time constant");

    // Block-partition independence of the engine-side tick.
    tolerance::ThermalDrift a, b;
    a.prepare(96000.0, 7);
    b.prepare(96000.0, 7);
    bool same = true;
    for (int i = 0; i < 20000; ++i) same = same && a.tick() == b.tick();
    check(same, "drift ticks are deterministic");
}

void testWornUnit(const ws1::Tolerances& tol)
{
    // The worn-unit model is the historical sine (drift x 0.9 semitones at 0.031 Hz,
    // phase offsets 0.73 per voice). Registered check: the circuit engine's worn
    // unit renders the same oscillator frequency as the Legacy policy, which runs
    // that law, at the same settings (dispersion 0, no touch or bend terms).
    const double rate = 48000.0;
    const int frames = 24000;
    auto chord = [&](float source, float drift, float model)
    {
        ParamValues p;
        p[P_sourcePerformance] = source;
        p[P_voiceDispersion] = 0.0f;
        p[P_drift] = drift;
        p[P_driftModel] = model;
        p[P_analogNoise] = 0.0f;
        Engine e;
        e.prepare(rate);
        e.setParams(p);
        for (int k = 0; k < 8; ++k) e.noteOn(48 + 3 * k, 0.8f);
        std::vector<float> out((size_t) frames * 2);
        e.render(out.data(), frames);
        std::array<float, 8> hz {};
        for (int v = 0; v < 8; ++v) hz[(size_t) v] = e.getRankInfo(v, 0).oscillatorHz;
        return hz;
    };
    const auto legacy = chord(0.0f, 0.25f, 0.0f);
    const auto worn = chord(1.0f, 0.25f, 1.0f);
    const auto still = chord(1.0f, 0.0f, 1.0f);
    double worst = 0.0, swing = 0.0;
    for (int v = 0; v < 8; ++v)
    {
        worst = std::max(worst, std::abs(1200.0 * std::log2((double) worn[(size_t) v] / (double) legacy[(size_t) v])));
        swing = std::max(swing, std::abs(1200.0 * std::log2((double) worn[(size_t) v] / (double) still[(size_t) v])));
    }
    std::printf("worn-unit drift: worst deviation from the Legacy sine %.3g cents (largest offset %.2f cents)\n", worst, swing);
    check(worst <= tol["worn_unit_max_cents"] && swing > 1.0, "worn unit reproduces the historical sine drift");
    // Thermal model at the default amount moves the pitch by cents, not tens of cents.
    const auto thermal = chord(1.0f, 0.25f, 0.0f);
    double largest = 0.0;
    for (int v = 0; v < 8; ++v)
        largest = std::max(largest, std::abs(1200.0 * std::log2((double) thermal[(size_t) v] / (double) still[(size_t) v])));
    std::printf("thermal drift at the default amount: largest offset %.2f cents over 8 voices\n", largest);
    check(largest > 0.01 && largest < 15.0, "thermal drift moves the pitch by a few cents");
}
} // namespace

int main(int argc, char** argv)
{
    const std::string path = argc > 1 ? argv[1] : "tests/parity/tolerances-v2/ws1-voice.json";
    const ws1::Tolerances tol(path, "tolerance_drift");
    testDraws(tol);
    testMapping(tol);
    testEngine(tol);
    testDrift(tol);
    testWornUnit(tol);
    std::printf("Tolerance and drift: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
