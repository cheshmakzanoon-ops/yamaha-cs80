// ws4 Step 3: OE1/OE2 chorus and tremolo. The realtime BbdChorus stereo transfer, noise,
// clock feedthrough and OE1 tremolo are held to the offline truth models
// (reference/Mn3001Packet.h NonidealChannel, reference/Oe2Filters.h branchTransfer,
// reference/Oe1Modulation.h) by the pre-registered bounds in tests/Ws4Tolerances.h.
// Agreement with these equations is not agreement with a measured CS-80; see
// docs/workstreams/ws4-circuits.md.
#include "core/BbdChorus.h"
#include "reference/Mn3001Packet.h"
#include "reference/Oe1Modulation.h"
#include "reference/Oe2Filters.h"
#include "Ws4Tolerances.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using cs80::BbdChorus;
using Complex = std::complex<double>;
namespace mn = cs80::reference::mn3001;
namespace oe1 = cs80::reference::oe1;
namespace oe2 = cs80::reference::oe2_filters;
namespace tol = cs80::ws4_tolerances::oe2;

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
double db(double x) { return 20.0 * std::log10(x); }

// Declared truth values (literals, so a changed library constant cannot move the oracle).
constexpr double kClockHz = 106904.5;
constexpr double kInefficiency = 5.0e-4;
constexpr double kWetGain = 0.5;

// ---- Truth: MN3001 baseband transfer from the nonideal packet graph --------------------
// Uniform clock, CP2 at m T and CP1 at (m + 1/2) T. The normalized pin sum is sampled in
// each half slot (s1 after CP2, s2 after CP1); with input x_m = exp(j w m T) the steady
// ratios H1 = s1/x, H2 = s2/x give the continuous output of the two-level staircase:
//   H(f) = 0.5 sinc(f T / 2) exp(-j pi f T / 2) (H1 + H2 exp(-j pi f T)).
Complex packetTransfer(double hz, double inefficiency)
{
    const double t = 1.0 / kClockHz;
    const int periods = 900;
    double a[2] {}, b[2] {};
    for (int part = 0; part < 2; ++part)
    {
        mn::NonidealChannel chain(mn::Phase::cp2, { inefficiency, 0.0, 0.0 });
        for (int m = 0; m < periods; ++m)
        {
            const double angle = 2.0 * kPi * hz * m * t;
            const double x = part == 0 ? std::cos(angle) : std::sin(angle);
            a[part] = chain.step(m * t, mn::Phase::cp2, x).normalizedSum();
            b[part] = chain.step((m + 0.5) * t, mn::Phase::cp1, 0.0).normalizedSum();
        }
    }
    // cos response + j sin response = H exp(j w m T) at the last period.
    const Complex back = std::polar(1.0, -2.0 * kPi * hz * (periods - 1) * t);
    const Complex h1 = Complex(a[0], a[1]) * back, h2 = Complex(b[0], b[1]) * back;
    const double x = hz * t;
    const double halfSinc = x == 0.0 ? 1.0 : std::sin(kPi * x / 2.0) / (kPi * x / 2.0);
    return 0.5 * halfSinc * std::polar(1.0, -kPi * x / 2.0) * (h1 + h2 * std::polar(1.0, -kPi * x));
}

// Complete truth wet branch, normalized like the VCA trims (unit magnitude at 1 kHz, sign
// kept: the IC5/IC6 inverting amplifier is in the truth path), times the wet gain.
Complex truthWet(double hz, oe2::Branch branch)
{
    const Complex trim = oe2::branchTransfer(1000.0, branch, packetTransfer(1000.0, kInefficiency));
    return kWetGain * oe2::branchTransfer(hz, branch, packetTransfer(hz, kInefficiency)) / std::abs(trim);
}

// ---- Realtime measurement ---------------------------------------------------------------
struct Response { Complex left, right; };
Response measure(double rate, double hz)
{
    BbdChorus chorus;
    chorus.prepare((float) rate);
    const int window = (int) std::lround(rate / 10.0) * 10; // 1 s: integer cycles of every test tone
    const int settle = (int) rate;
    Complex l, r;
    for (int n = 0; n < settle + window; ++n)
    {
        const double phase = 2.0 * kPi * hz * (double) n / rate;
        const float in = (float) std::sin(phase);
        const auto out = chorus.process(in, in, 1.0f, 0.0f, true, false);
        if (n >= settle)
        {
            l += (double) (out.l - in) * std::polar(1.0, -phase);
            r += (double) (out.r - in) * std::polar(1.0, -phase);
        }
    }
    const Complex scale = (2.0 / (double) window) * Complex(0.0, 1.0); // sine reference
    return { l * scale, r * scale };
}

void idealEquivalence()
{
    std::mt19937 rng(0x0E2u);
    std::uniform_real_distribution<double> un(-1.0, 1.0);
    mn::Channel ideal(mn::Phase::cp2);
    mn::NonidealChannel lossless(mn::Phase::cp2, {});
    bool same = true;
    double time = 0.0;
    for (int event = 0; event < 3000; ++event)
    {
        const auto phase = event % 2 == 0 ? mn::Phase::cp2 : mn::Phase::cp1;
        const double x = un(rng);
        time += 1e-6 * (1.0 + 0.5 * (un(rng) + 1.0));
        same = same && ideal.step(time, phase, x).normalizedSum() == lossless.step(time, phase, x).normalizedSum();
    }
    check(same, "zero-option nonideal MN3001 reproduces the ideal packet reference exactly");
    // Charge left behind is conserved: a constant input settles at unity.
    mn::NonidealChannel lossy(mn::Phase::cp2, { kInefficiency, 0.0, 0.0 });
    double last = 0.0;
    for (int m = 0; m < 1200; ++m)
    {
        lossy.step(m * 1e-5, mn::Phase::cp2, 1.0);
        last = lossy.step((m + 0.5) * 1e-5, mn::Phase::cp1, 1.0).normalizedSum();
    }
    // Front storage resamples the input, so (1 - e) of it enters the chain each period; the
    // sensed nodes then hold the packet (1) and the residue it leaves (e).
    check(std::abs(last - (1.0 + kInefficiency)) < 1e-9, "lossy chain DC: packet plus sensed residue");
}

void stereoTransferMatchesTruth()
{
    for (double rate : { 88200.0, 96000.0, 192000.0 })
        for (double hz : { 50.0, 159.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const auto got = measure(rate, hz);
            const Complex wantL = truthWet(hz, oe2::Branch::upper);
            const Complex wantR = truthWet(hz, oe2::Branch::lower);
            const bool low = hz <= tol::kLowBandMaxHz;
            const double dbTol = low ? tol::kLowBandDb : rate >= 192000.0 ? tol::kHighBandDbAt192k : tol::kHighBandDb;
            const double radTol = low ? tol::kLowBandRadians : rate >= 192000.0 ? tol::kHighBandRadiansAt192k : tol::kHighBandRadians;
            const std::string where = std::to_string((int) hz) + " Hz at " + std::to_string((int) rate);
            for (int side = 0; side < 2; ++side)
            {
                const Complex g = side == 0 ? got.left : got.right, w = side == 0 ? wantL : wantR;
                const double magnitude = db(std::abs(g) / std::abs(w)), phase = std::arg(g / w);
                check(std::abs(magnitude) <= dbTol, (side ? "right" : "left") + std::string(" wet magnitude follows OE2 truth: ") + where);
                check(std::abs(phase) <= radTol, (side ? "right" : "left") + std::string(" wet phase follows OE2 truth: ") + where);
                if (rate == 96000.0 && (hz == 1000.0 || hz == 10000.0) && side == 0)
                    std::printf("OE2 wet %s: %+.3f dB, %+.4f rad versus truth\n", where.c_str(), magnitude, phase);
            }
        }
}

// Noise only: zero input, CHORUS on, DEPTH 0 (nominal clock).
void noiseLevelAndIndependence()
{
    const double rate = 96000.0;
    BbdChorus a, b;
    a.prepare((float) rate);
    b.prepare((float) rate);
    const int settle = (int) (0.5 * rate), n = 1 << 17;
    double ll = 0, rr = 0, lr = 0;
    bool deterministic = true;
    for (int i = 0; i < settle + n; ++i)
    {
        const auto x = a.process(0.0f, 0.0f, 1.0f, 0.0f, true, false);
        const auto y = b.process(0.0f, 0.0f, 1.0f, 0.0f, true, false);
        deterministic = deterministic && x.l == y.l && x.r == y.r;
        if (i >= settle) { ll += (double) x.l * x.l; rr += (double) x.r * x.r; lr += (double) x.l * x.r; }
    }
    // Truth: packet noise of variance s^2 at the clock rate, held one period, through the
    // normalized reconstruction branch (no input low-pass: noise arises in the chain).
    const double sigma = BbdChorus::kPacketNoiseRms;
    const double t = 1.0 / kClockHz;
    const Complex trim = oe2::branchTransfer(1000.0, oe2::Branch::upper, packetTransfer(1000.0, kInefficiency));
    double variance = 0.0;
    const double df = 5.0;
    for (double f = df / 2; f < rate / 2; f += df)
    {
        const auto in = oe2::solveInput({ 0.0, { 0.0, 0.0 } }, f, 1.0).y;
        const Complex branch = oe2::branchTransfer(f, oe2::Branch::upper, 1.0) / in;
        const double x = f * t, sinc = std::sin(kPi * x) / (kPi * x);
        variance += 2.0 * sigma * sigma * t * sinc * sinc * std::norm(kWetGain * branch / std::abs(trim)) * df;
    }
    const double want = std::sqrt(variance);
    const double gotL = std::sqrt(ll / n), gotR = std::sqrt(rr / n);
    check(deterministic, "BBD noise is a deterministic reproducible stream");
    check(std::abs(db(gotL / want)) <= tol::kNoiseDb, "left wet noise level follows truth");
    check(std::abs(db(gotR / want)) <= tol::kNoiseDb, "right wet noise level follows truth");
    check(std::abs(lr / std::sqrt(ll * rr)) <= tol::kNoiseCorrelation, "left and right chains have independent noise");
    std::printf("OE2 wet noise %.3g / %.3g rms (truth %.3g), L/R correlation %.4f\n", gotL, gotR, want, lr / std::sqrt(ll * rr));
    // Reset reproduces a freshly prepared instance exactly (noise reseeds with the chain).
    a.reset();
    BbdChorus fresh;
    fresh.prepare((float) rate);
    bool reproduces = true;
    for (int i = 0; i < 4000; ++i)
    {
        const auto x = a.process(0.1f, 0.1f, 1.0f, 0.5f, true, false);
        const auto y = fresh.process(0.1f, 0.1f, 1.0f, 0.5f, true, false);
        reproduces = reproduces && x.l == y.l && x.r == y.r;
    }
    check(reproduces, "reset reproduces a freshly prepared chorus exactly");
}

double windowedLine(const std::vector<double>& x, double rate, double hz)
{
    double coherent = 0.0;
    Complex acc;
    const std::size_t n = x.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const double t = 2.0 * kPi * (double) i / (double) n;
        const double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t);
        coherent += w;
        acc += x[i] * w * std::polar(1.0, -2.0 * kPi * hz * (double) i / rate);
    }
    return 2.0 * std::abs(acc) / coherent;
}
std::vector<double> silentWetLeft(double rate, int n)
{
    BbdChorus chorus;
    chorus.prepare((float) rate);
    for (int i = 0; i < (int) (0.3 * rate); ++i)
        chorus.process(0.0f, 0.0f, 1.0f, 0.0f, true, false);
    std::vector<double> x((std::size_t) n);
    for (int i = 0; i < n; ++i)
        x[(std::size_t) i] = chorus.process(0.0f, 0.0f, 1.0f, 0.0f, true, false).l;
    return x;
}

void clockFeedthrough()
{
    // Where the internal rate can carry it, the clock line follows the truth: the
    // normalized pin-sum pedestal square (mismatch x pedestal = kClockFeedthrough) through
    // the reconstruction branch only.
    {
        const double rate = 1536000.0;
        const auto x = silentWetLeft(rate, 1 << 17);
        const double got = windowedLine(x, rate, kClockHz);
        const Complex trim = oe2::branchTransfer(1000.0, oe2::Branch::upper, packetTransfer(1000.0, kInefficiency));
        const auto in = oe2::solveInput({ 0.0, { 0.0, 0.0 } }, kClockHz, 1.0).y;
        const double want = 4.0 / kPi * BbdChorus::kClockFeedthrough * kWetGain
                          * std::abs(oe2::branchTransfer(kClockHz, oe2::Branch::upper, 1.0) / in) / std::abs(trim);
        check(std::abs(db(got / want)) <= tol::kFeedthroughDb, "representable clock feedthrough follows truth");
        std::printf("OE2 clock feedthrough at 1.536 MHz internal: %.3g (truth %.3g)\n", got, want);
    }
    // At 96 kHz internal the clock is above Nyquist: no line may alias to 96000 - clock.
    {
        const double rate = 96000.0;
        const auto x = silentWetLeft(rate, 1 << 17);
        const double alias = windowedLine(x, rate, std::abs(rate - kClockHz));
        const double floor = windowedLine(x, rate, 12345.0);
        check(db(alias / floor) <= tol::kFeedthroughAliasDb, "unrepresentable clock feedthrough does not alias");
    }
}

// TREMOLO: left and right sample trajectories versus the truth OE1 VCA law applied to the
// truth wet path, for a steady 500 Hz tone.
void tremoloTrajectory()
{
    const double rate = 96000.0, hz = 500.0, amplitude = 0.5;
    const float speed = 4.0f, depth = 1.0f;
    BbdChorus chorus;
    chorus.prepare((float) rate);
    const Complex wL = truthWet(hz, oe2::Branch::upper) / kWetGain;
    const Complex wR = truthWet(hz, oe2::Branch::lower) / kWetGain;
    double worstL = 0.0, worstR = 0.0, swing = 0.0;
    const int settle = (int) rate, total = settle + (int) (2.0 * rate);
    for (int n = 0; n < total; ++n)
    {
        const double phase = 2.0 * kPi * hz * (double) n / rate;
        const float in = (float) (amplitude * std::sin(phase));
        const auto out = chorus.process(in, in, speed, depth, true, true);
        if (n < settle)
            continue;
        // process() advances its oscillators before use: sample n sees time (n + 1) / rate.
        const auto o = oe1::oscillators(speed, (double) (n + 1) / rate);
        const auto g = oe1::vcaGains(o, depth, true, kWetGain);
        const double wetL = (wL * amplitude * std::polar(1.0, phase)).imag();
        const double wetR = (wR * amplitude * std::polar(1.0, phase)).imag();
        const double wantL = in + (g[0] + g[1]) * wetL, wantR = in + (g[2] + g[3]) * wetR;
        worstL = std::max(worstL, std::abs(out.l - wantL));
        worstR = std::max(worstR, std::abs(out.r - wantR));
        swing = std::max(swing, std::abs((g[0] + g[1]) - (g[2] + g[3])));
    }
    check(worstL <= tol::kTremoloTrajectory * amplitude, "left tremolo trajectory follows OE1 truth");
    check(worstR <= tol::kTremoloTrajectory * amplitude, "right tremolo trajectory follows OE1 truth");
    check(swing > 0.5, "fixture exercises antiphase left/right VCA gains");
    std::printf("OE1 tremolo worst error L %.4g R %.4g (input amplitude %.2f)\n", worstL, worstR, amplitude);
    check(std::abs(BbdChorus::kDo2PerDo1 - oe1::do2PerDo1()) < 1e-15, "DO2/DO1 ratio from ADJ printed 21");
}
} // namespace

int main()
{
    idealEquivalence();
    stereoTransferMatchesTruth();
    noiseLevelAndIndependence();
    clockFeedthrough();
    tremoloTrajectory();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
