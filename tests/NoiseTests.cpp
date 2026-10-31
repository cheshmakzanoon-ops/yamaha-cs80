// Circuit-engine noise sources against their declared PSDs (src/reference/
// NoiseSpectrum.h). Software statistics, not measurements of an instrument.
#include "core/Engine.h"
#include "reference/NoiseSpectrum.h"
#include "Ws2EngineTolerances.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
constexpr double pi = 3.14159265358979323846;

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
        const double ang = -2.0 * pi / (double) len;
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

// One-sided Welch PSD (Hann, 50 % overlap), units^2 / Hz.
std::vector<double> welch(const std::vector<double>& x, std::size_t length, double rate)
{
    std::vector<double> window(length), psd(length / 2 + 1, 0.0);
    double windowPower = 0;
    for (std::size_t i = 0; i < length; ++i)
    {
        window[i] = 0.5 - 0.5 * std::cos(2 * pi * (double) i / (double) length);
        windowPower += window[i] * window[i];
    }
    int segments = 0;
    std::vector<std::complex<double>> a(length);
    for (std::size_t start = 0; start + length <= x.size(); start += length / 2, ++segments)
    {
        for (std::size_t i = 0; i < length; ++i) a[i] = window[i] * x[start + i];
        fft(a);
        for (std::size_t k = 0; k < psd.size(); ++k) psd[k] += std::norm(a[k]);
    }
    for (auto& p : psd) p *= 2.0 / (rate * windowPower * segments);
    return psd;
}

struct Fit { double worstDb = 0, worstHz = 0; };
Fit compare(const std::vector<double>& psd, std::size_t length, double rate, const std::function<double(double)>& truth)
{
    Fit fit;
    for (double centre = ws2::kNoisePsdLowHz; centre <= ws2::kNoisePsdHighHz * 1.001; centre *= std::pow(2.0, 1.0 / 3.0))
    {
        const double lo = centre * std::pow(2.0, -1.0 / 6.0), hi = centre * std::pow(2.0, 1.0 / 6.0);
        double measured = 0, expected = 0;
        int bins = 0;
        for (std::size_t k = 1; k < psd.size(); ++k)
        {
            const double f = (double) k * rate / (double) length;
            if (f < lo || f >= hi) continue;
            measured += psd[k];
            expected += truth(f);
            ++bins;
        }
        if (bins == 0) continue;
        const double db = 10 * std::log10(measured / expected);
        if (std::abs(db) > std::abs(fit.worstDb)) { fit.worstDb = db; fit.worstHz = centre; }
    }
    return fit;
}

double bandPower(const std::vector<double>& psd, std::size_t length, double rate, double lo, double hi)
{
    double sum = 0;
    for (std::size_t k = 1; k < psd.size(); ++k)
    {
        const double f = (double) k * rate / (double) length;
        if (f >= lo && f < hi) sum += psd[k] * rate / (double) length;
    }
    return sum;
}

struct Moments { double mean = 0, sigma = 0, kurtosis = 0; };
Moments moments(const std::vector<double>& x)
{
    double m = 0;
    for (double v : x) m += v;
    m /= (double) x.size();
    double m2 = 0, m4 = 0;
    for (double v : x) { const double d = (v - m) * (v - m); m2 += d; m4 += d * d; }
    m2 /= (double) x.size();
    m4 /= (double) x.size();
    return { m, std::sqrt(m2), m4 / (m2 * m2) };
}

void testSources()
{
    const reference::noise::Spectrum truth;
    double referencePanel = 0, referenceHiss = 0;
    for (double rate : { 88200.0, 96000.0, 176400.0, 192000.0, 352800.0, 384000.0 })
    {
        std::size_t length = 1;
        while ((double) length < rate / 1.4) length <<= 1; // < 1.4 Hz bins at every rate
        const std::size_t count = 17 * length;
        CircuitNoiseSources sources;
        sources.prepare(rate);
        std::vector<double> panel(count), left(count), right(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            panel[i] = sources.panel();
            float l = 0, r = 0;
            sources.hiss(l, r);
            left[i] = l;
            right[i] = r;
        }
        const auto panelPsd = welch(panel, length, rate), hissPsd = welch(left, length, rate);
        const auto panelFit = compare(panelPsd, length, rate, [&](double f) { return truth.panelPsd(f); });
        const auto hissFit = compare(hissPsd, length, rate, [&](double f) { return truth.hissPsd(f); });
        const double panelBand = bandPower(panelPsd, length, rate, 100.0, 10000.0);
        const double hissBand = bandPower(hissPsd, length, rate, 100.0, 10000.0);
        if (referencePanel == 0) { referencePanel = panelBand; referenceHiss = hissBand; }
        const auto mp = moments(panel), mh = moments(left), mr = moments(right);
        double lr = 0;
        for (std::size_t i = 0; i < count; ++i) lr += (left[i] - mh.mean) * (right[i] - mr.mean);
        const double rho = lr / ((double) count * mh.sigma * mr.sigma);
        std::printf("noise %6.0f Hz: panel PSD worst %+.2f dB @%5.0f Hz, rms %.4f, kurtosis %.3f | "
                    "hiss PSD worst %+.2f dB @%5.0f Hz, rms %.4f, kurtosis %.3f, L/R rho %+.4f\n",
                    rate, panelFit.worstDb, panelFit.worstHz, mp.sigma, mp.kurtosis,
                    hissFit.worstDb, hissFit.worstHz, mh.sigma, mh.kurtosis, rho);
        check(std::abs(panelFit.worstDb) <= ws2::kNoisePsdBandDb, "panel noise follows the declared SUB29 PSD");
        check(std::abs(hissFit.worstDb) <= ws2::kNoisePsdBandDb, "hiss follows the declared mix-bus PSD");
        check(std::abs(10 * std::log10(panelBand / referencePanel)) <= ws2::kNoiseRateIndependenceDb
                  && std::abs(10 * std::log10(hissBand / referenceHiss)) <= ws2::kNoiseRateIndependenceDb,
              "band power does not depend on internal rate");
        check(std::abs(mp.kurtosis - 3.0) <= ws2::kNoiseKurtosisError
                  && std::abs(mh.kurtosis - 3.0) <= ws2::kNoiseKurtosisError, "Gaussian amplitude statistics");
        check(std::abs(mp.mean) <= ws2::kNoiseMeanSigmaError * mp.sigma, "panel noise has no DC");
        check(std::abs(rho) <= ws2::kNoiseChannelCorrelation, "left and right hiss are independent");
    }
    GaussianNoise white(0x1234567u);
    std::vector<double> w(1 << 20);
    for (auto& v : w) v = white.next();
    const auto mw = moments(w);
    std::printf("white generator: mean %+.5f sigma %.5f kurtosis %.4f\n", mw.mean, mw.sigma, mw.kurtosis);
    check(std::abs(mw.mean) <= ws2::kNoiseMeanSigmaError * mw.sigma && std::abs(mw.sigma - 1) < 0.01
              && std::abs(mw.kurtosis - 3) <= ws2::kNoiseKurtosisError, "unit Gaussian generator");
}

// Panel noise is ONE generator shared by every voice: two equal voices add
// coherently (x2) in the circuit engine, independently (about x1.41) in legacy.
double twoVoiceRatio(bool circuit)
{
    auto rms = [&](int voices)
    {
        ParamValues p;
        p[P_sourcePerformance] = circuit ? 1.0f : 0.0f;
        p[P_analogNoise] = p[P_drift] = p[P_voiceDispersion] = p[P_chorusOn] = 0.0f;
        p[P_mix] = 0.0f;
        for (int rank = 0; rank < 2; ++rank)
        {
            p[chParam(rank, CP_saw)] = p[chParam(rank, CP_pulse)] = p[chParam(rank, CP_sine)] = 0.0f;
            p[chParam(rank, CP_noise)] = 1.0f;
            p[chParam(rank, CP_hpfOn)] = p[chParam(rank, CP_lpfOn)] = 0.0f;
        }
        Engine e;
        e.prepare(48000.0);
        e.setParams(p);
        for (int v = 0; v < voices; ++v) e.noteOn(60 + 7 * v, 0.8f);
        std::vector<float> audio(2 * 48000);
        e.render(audio.data(), 48000);
        double sum = 0;
        for (std::size_t i = 2 * 12000; i < audio.size(); i += 2) sum += (double) audio[i] * audio[i];
        return std::sqrt(sum);
    };
    return rms(2) / rms(1);
}

void testFanOut()
{
    const double circuit = twoVoiceRatio(true), legacy = twoVoiceRatio(false);
    std::printf("two-voice/one-voice noise rms: circuit %.5f, legacy %.4f\n", circuit, legacy);
    check(std::abs(circuit - 2.0) < 1.0e-3, "circuit engine: every voice reads the same SUB29 noise");
    check(std::abs(legacy - std::sqrt(2.0)) < 0.05, "legacy keeps independent per-voice draws");
}
} // namespace

int main()
{
    testSources();
    testFanOut();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
