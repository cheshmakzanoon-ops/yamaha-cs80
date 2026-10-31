// Modern FX layer (src/core/FxSlot.h, FxModels.h, FxOversampler.h, FxLoudness.h):
// K-weighting against ITU-R BS.1770-4, half-band oversampling, drive aliasing against a
// 64x double-precision truth model, static and matched loudness, reverb RT60s, tape
// echo timing, sample-rate stability, classic-FX byte-identity and PURE CS-80 inertness.
// Modern effects only; nothing here is CS-80 evidence.
//
//   cs80_modern_fx_tests              run the gates
//   cs80_modern_fx_tests --calibrate  print the static loudness tables for FxModels.h
#include "core/Engine.h"
#include "core/FxSlot.h"
#include "ModernLayerTolerances.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace cs80;
using namespace cs80::fx;
namespace tol = cs80::tests::ws5;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* what)
{
    ++checks;
    if (! ok)
    {
        if (failures < 40)
            std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

constexpr double kPi = 3.14159265358979323846;
double db(double powerRatio) { return 10.0 * std::log10(std::max(powerRatio, 1.0e-300)); }

// ---- signal helpers ------------------------------------------------------------------

// Deterministic pink noise (P. Kellet's three-pole filter on white noise), scaled to a
// given RMS. A reference test signal, not a model of anything.
std::vector<float> pinkNoise(int length, double rms, std::uint32_t seed = 0x1234567u)
{
    Rng rng(seed);
    std::vector<float> out((std::size_t) length);
    double b0 = 0, b1 = 0, b2 = 0, sum = 0;
    for (int i = 0; i < length; ++i)
    {
        const double w = rng.nextBipolar();
        b0 = 0.99765 * b0 + w * 0.0990460;
        b1 = 0.96300 * b1 + w * 0.2965164;
        b2 = 0.57000 * b2 + w * 1.0526913;
        const double p = b0 + b1 + b2 + w * 0.1848;
        out[(std::size_t) i] = (float) p;
        sum += p * p;
    }
    const double scale = rms / std::sqrt(sum / length);
    for (auto& x : out) x = (float) (x * scale);
    return out;
}

// Offline K-weighted mean square of a stereo signal over [from, end): the loudness
// measure of ITU-R BS.1770-4 without gating (steady test signals need none).
double kEnergy(const std::vector<float>& l, const std::vector<float>& r, double fs, int from)
{
    KWeighting kl, kr;
    kl.prepare(fs);
    kr.prepare(fs);
    double sum = 0.0;
    for (std::size_t i = 0; i < l.size(); ++i)
    {
        const double a = kl.process(l[i]), b = kr.process(r[i]);
        if ((int) i >= from) sum += 0.5 * (a * a + b * b);
    }
    return sum / (double) ((int) l.size() - from);
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
        const std::complex<double> w = std::polar(1.0, -2.0 * kPi / (double) len);
        for (std::size_t i = 0; i < n; i += len)
        {
            std::complex<double> u(1.0);
            for (std::size_t k = 0; k < len / 2; ++k)
            {
                const auto x = a[i + k], y = a[i + k + len / 2] * u;
                a[i + k] = x + y;
                a[i + k + len / 2] = x - y;
                u *= w;
            }
        }
    }
}

// Amplitude of the component at `hz` (quadrature correlation over whole cycles).
double toneAmplitude(const std::vector<float>& x, double fs, double hz, int from)
{
    std::complex<double> acc;
    const int cycles = (int) std::floor(((double) x.size() - from) * hz / fs);
    const int n = std::max(1, (int) std::lround(cycles * fs / hz));
    for (int i = 0; i < n && from + i < (int) x.size(); ++i)
        acc += (double) x[(std::size_t) (from + i)] * std::polar(1.0, -2.0 * kPi * hz * i / fs);
    return 2.0 * std::abs(acc) / n;
}

// ---- K-weighting --------------------------------------------------------------------

void testKWeighting()
{
    // ITU-R BS.1770-4 Annex 1, Table 1 (pre-filter) and Table 2 (RLB), 48 kHz.
    const auto shelf = kWeightingShelf(48000.0), hp = kWeightingHighPass(48000.0);
    const double t = tol::kKWeightingCoefficientTolerance;
    check(std::abs(shelf.b0 - 1.53512485958697) < t && std::abs(shelf.b1 + 2.69169618940638) < t
              && std::abs(shelf.b2 - 1.19839281085285) < t && std::abs(shelf.a1 + 1.69065929318241) < t
              && std::abs(shelf.a2 - 0.73248077421585) < t,
          "K-weighting pre-filter reproduces BS.1770-4 Table 1 at 48 kHz");
    check(hp.b0 == 1.0 && hp.b1 == -2.0 && hp.b2 == 1.0 && std::abs(hp.a1 + 1.99004745483398) < t
              && std::abs(hp.a2 - 0.99007225036621) < t,
          "K-weighting RLB filter reproduces BS.1770-4 Table 2 at 48 kHz");
    // The re-derived filter keeps its response at every rate: +4 dB shelf at 10 kHz, RLB
    // high-pass at 20 Hz, flat-ish at 1 kHz.
    for (double fs : { 44100.0, 96000.0, 192000.0, 384000.0 })
    {
        auto gainAt = [fs](double hz)
        {
            KWeighting k;
            k.prepare(fs);
            std::vector<float> x((std::size_t) fs);
            for (std::size_t i = 0; i < x.size(); ++i) x[i] = (float) k.process(std::sin(2.0 * kPi * hz * (double) i / fs));
            return 20.0 * std::log10(toneAmplitude(x, fs, hz, (int) (0.5 * fs)));
        };
        check(std::abs(gainAt(10000.0) - 3.99) < 0.1 && std::abs(gainAt(1000.0) - 0.69) < 0.1 && gainAt(20.0) < -10.0,
              "K-weighting response is rate-independent");
    }
}

// ---- half-band oversampling ---------------------------------------------------------

void testHalfband()
{
    const auto& d = sharedHalfband();
    check(d.count >= 2 && d.count <= kMaxHalfbandCoefficients, "half-band design has a sane coefficient count");
    bool ordered = true;
    for (int i = 0; i < d.count; ++i)
        ordered = ordered && d.coefficients[(std::size_t) i] > 0.0 && d.coefficients[(std::size_t) i] < 1.0
                  && (i == 0 || d.coefficients[(std::size_t) i] > d.coefficients[(std::size_t) i - 1]);
    check(ordered, "half-band all-pass coefficients are in (0, 1) and increasing");

    // Up/down cascades are transparent in the audio band at a 96 kHz base rate.
    const double fs = 96000.0;
    for (int stages = 1; stages <= 3; ++stages)
    {
        double worst = 0.0;
        for (double hz : { 20.0, 1000.0, 5000.0, 10000.0, 15000.0, 20000.0 })
        {
            Oversampler os;
            std::vector<float> y((std::size_t) fs);
            for (std::size_t i = 0; i < y.size(); ++i)
                y[i] = os.process((float) std::sin(2.0 * kPi * hz * (double) i / fs), stages, [](float v) { return v; });
            worst = std::max(worst, std::abs(20.0 * std::log10(toneAmplitude(y, fs, hz, (int) (0.25 * fs)))));
        }
        check(worst <= tol::kHalfbandPassbandRippleDb, "half-band up/down cascade is flat 20 Hz..20 kHz");
    }
    // Image rejection of one up-sampling stage: a tone at f leaves an image at fs - f.
    for (double hz : { 1000.0, 10000.0, 20000.0 })
    {
        HalfbandUpsampler up;
        up.setup(d);
        std::vector<float> y((std::size_t) (2.0 * fs));
        for (std::size_t i = 0; i < y.size() / 2; ++i)
            up.process((float) std::sin(2.0 * kPi * hz * (double) i / fs), y[2 * i], y[2 * i + 1]);
        const double wanted = toneAmplitude(y, 2.0 * fs, hz, (int) fs);
        const double image = toneAmplitude(y, 2.0 * fs, fs - hz, (int) fs);
        check(20.0 * std::log10(image / wanted) <= tol::kHalfbandStopbandDb, "half-band up-sampler rejects the image");
    }
}

// ---- drive aliasing against the 64x truth model ----------------------------------------

// Coherent test tone: 2799 cycles in 65536 samples at 96 kHz = 4100.1 Hz.
constexpr int kAliasN = 65536;
constexpr int kAliasBin = 2799;
constexpr double kAliasFs = 96000.0;

std::vector<double> driveSpectrumPower(int stages)
{
    DriveFx drive;
    drive.prepare(kAliasFs);
    const float g = DriveFx::gainFor(1.0f);
    std::vector<std::complex<double>> x((std::size_t) kAliasN);
    // Settle the IIR filters over 4 periods of the record, then capture one exact period.
    for (int pass = 0; pass < 5; ++pass)
        for (int n = 0; n < kAliasN; ++n)
        {
            const float in = 0.5f * (float) std::sin(2.0 * kPi * kAliasBin * (double) n / kAliasN);
            const float y = drive.processCore(0, in, g, stages);
            if (pass == 4) x[(std::size_t) n] = y;
        }
    fft(x);
    std::vector<double> power((std::size_t) kAliasN / 2);
    for (int k = 0; k < kAliasN / 2; ++k) power[(std::size_t) k] = std::norm(x[(std::size_t) k]);
    return power;
}

double aliasRatioDb(const std::vector<double>& power)
{
    const int lo = (int) std::ceil(20.0 * kAliasN / kAliasFs), hi = (int) std::floor(20000.0 * kAliasN / kAliasFs);
    double harmonic = 0.0, other = 0.0;
    for (int k = lo; k <= hi; ++k)
        (k % kAliasBin == 0 ? harmonic : other) += power[(std::size_t) k];
    return db(other / (harmonic + other));
}

// Truth: the same shaper in double precision at 64x the internal rate, harmonics read
// directly at that rate (images and aliases from harmonics above 3 MHz are negligible).
std::vector<double> truthHarmonicsDb()
{
    const int over = 64, n = kAliasN * over;
    const double g = DriveFx::gainFor(1.0f), beta = DriveFx::kAsymmetry, t = std::tanh(beta);
    std::vector<std::complex<double>> acc(5);
    for (int i = 0; i < n; ++i)
    {
        const double phase = 2.0 * kPi * kAliasBin * (double) i / n;
        const double y = (std::tanh(g * 0.5 * std::sin(phase) + beta) - t) / (g * (1.0 - t * t));
        for (int h = 1; h <= 4; ++h) acc[(std::size_t) h] += y * std::polar(1.0, -(double) h * phase);
    }
    std::vector<double> levels(5);
    for (int h = 1; h <= 4; ++h) levels[(std::size_t) h] = 20.0 * std::log10(2.0 * std::abs(acc[(std::size_t) h]) / n);
    return levels;
}

void testDriveOversampling()
{
    std::array<double, 4> alias {};
    std::vector<double> realtime4x;
    for (int stages = 0; stages <= 3; ++stages)
    {
        const auto power = driveSpectrumPower(stages);
        alias[(std::size_t) stages] = aliasRatioDb(power);
        if (stages == 2) realtime4x = power;
    }
    std::printf("drive alias ratio (dB): 1x %.1f  2x %.1f  4x %.1f  8x %.1f\n", alias[0], alias[1], alias[2], alias[3]);
    check(alias[1] <= tol::kDriveAliasDb2x, "2x drive alias ratio");
    check(alias[2] <= tol::kDriveAliasDb4x, "4x drive alias ratio");
    check(alias[3] <= tol::kDriveAliasDb8x, "8x drive alias ratio");
    for (int s = 1; s <= 3; ++s)
        check(alias[(std::size_t) s] <= std::max(alias[(std::size_t) s - 1] - tol::kDriveAliasStepDb, -80.0),
              "each oversampling doubling improves aliasing by 10 dB until -80 dB");

    const auto truth = truthHarmonicsDb();
    for (int h = 1; h <= 4; ++h)
    {
        if (truth[(std::size_t) h] - truth[1] < -60.0) continue; // below the alias floor
        const double level = 20.0 * std::log10(2.0 * std::sqrt(realtime4x[(std::size_t) (h * kAliasBin)]) / kAliasN);
        std::printf("harmonic %d: truth %.2f dB, realtime 4x %.2f dB\n", h, truth[(std::size_t) h], level);
        check(std::abs(level - truth[(std::size_t) h]) <= tol::kDriveHarmonicToleranceDb,
              "4x drive harmonics match the 64x double-precision truth");
    }
}

// ---- static and matched loudness ---------------------------------------------------------

constexpr double kFs = 96000.0;

// Loudness change (dB) of a slot's output over its input, steady pink noise.
double slotLoudnessDb(int model, int type, float mix, float a, float b, double rmsDb, bool gainComp, double fs = kFs,
                      bool wetOnly = false, double settle = 4.0, double measure = 4.0)
{
    const int settleN = (int) (settle * fs), total = settleN + (int) (measure * fs);
    const auto noiseL = pinkNoise(total, std::pow(10.0, rmsDb / 20.0), 0x1234567u);
    const auto noiseR = pinkNoise(total, std::pow(10.0, rmsDb / 20.0), 0x7654321u);
    FxSlot slot;
    slot.prepare((float) fs);
    SlotControls c;
    c.type = type;
    c.model = model;
    c.mix = mix;
    c.a = a;
    c.b = b;
    c.gainComp = gainComp;
    std::vector<float> outL((std::size_t) total), outR((std::size_t) total);
    float drift = 0.0f;
    for (int i = 0; i < total; ++i)
    {
        drift += 0.031f / (float) fs;
        drift -= std::floor(drift);
        const auto o = wetOnly ? slot.modern.wet(model, noiseL[(std::size_t) i], noiseR[(std::size_t) i], a, b, 2)
                               : slot.process(c, noiseL[(std::size_t) i], noiseR[(std::size_t) i], drift, (float) fs);
        outL[(std::size_t) i] = o.l;
        outR[(std::size_t) i] = o.r;
    }
    return db(kEnergy(outL, outR, fs, settleN) / kEnergy(noiseL, noiseR, fs, settleN));
}

void calibrate()
{
    const char* names[] = { "", "kEnsembleCalibrationDb", "kSpringCalibrationDb", "kPlateCalibrationDb", "kTapeCalibrationDb" };
    for (int model = 1; model <= 4; ++model)
    {
        std::printf("inline constexpr CalibrationTable %s {{\n", names[model]);
        for (int i = 0; i < 5; ++i)
        {
            std::printf("    {{ ");
            for (int j = 0; j < 5; ++j)
            {
                // Measured with the CURRENT table applied: add the residual to the old trim.
                const float a = (float) i / 4.0f, b = (float) j / 4.0f;
                const double residual = slotLoudnessDb(model, 0, 1.0f, a, b, -18.0, false, kFs, true);
                const CalibrationTable* table = model == 1 ? &kEnsembleCalibrationDb : model == 2 ? &kSpringCalibrationDb
                                              : model == 3 ? &kPlateCalibrationDb : &kTapeCalibrationDb;
                std::printf("%.2ff%s", (*table)[(std::size_t) i][(std::size_t) j] - residual, j < 4 ? ", " : "");
            }
            std::printf(" }}%s\n", i < 4 ? "," : "");
        }
        std::printf("}};\n");
    }
}

void testStaticLoudness()
{
    for (int model = 1; model < kModelCount; ++model)
        for (float a : { 0.1f, 0.5f, 0.9f })
            for (float b : { 0.1f, 0.5f, 0.9f })
            {
                const double change = slotLoudnessDb(model, 0, 1.0f, a, b, -18.0, false, kFs, true,
                                                     model == (int) Model::drive ? 6.0 : 4.0);
                std::printf("static loudness %s a=%.1f b=%.1f: %+.2f dB\n", modelName(model), a, b, change);
                check(std::abs(change) <= tol::kFxStaticLoudnessToleranceDb,
                      "modern FX model is loudness-calibrated at full mix");
            }
}

void testMatchedLoudness()
{
    // Gain compensation on: every classic type and modern model, mixes 0.5 and 1, three levels.
    for (int kind = 1; kind <= 10; ++kind)
        for (float mix : { 0.5f, 1.0f })
            for (double level : { -30.0, -18.0, -6.0 })
            {
                const int type = kind <= 5 ? kind : 0, model = kind <= 5 ? 0 : kind - 5;
                const double change = slotLoudnessDb(model, type, mix, 0.5f, 0.5f, level, true, kFs, false,
                                                     tol::kFxMatchSettleSeconds, 3.0);
                const char* classicNames[] = { "", "classic chorus", "classic phaser", "classic tape", "classic plate", "classic drive" };
                std::printf("matched loudness %s mix %.1f level %.0f dBFS: %+.2f dB\n",
                            kind <= 5 ? classicNames[kind] : modelName(model), mix, level, change);
                check(std::abs(change) <= tol::kFxMatchedLoudnessToleranceDb,
                      "FX loudness match holds output loudness to input loudness");
            }
}

// ---- reverb decay and echo timing --------------------------------------------------------

// Schroeder backward integration of the 1 kHz octave band (707..1414 Hz), -5..-35 dB fit.
double rt60Of(const std::vector<float>& ir, double fs)
{
    auto hp = Eq::highPass(fs, 707.0, 0.7071), hp2 = Eq::highPass(fs, 707.0, 0.7071);
    auto lp = Eq::lowPass(fs, 1414.0, 0.7071), lp2 = Eq::lowPass(fs, 1414.0, 0.7071);
    std::vector<double> e(ir.size());
    for (std::size_t i = 0; i < ir.size(); ++i)
    {
        const double y = lp2.process(lp.process(hp2.process(hp.process(ir[i]))));
        e[i] = y * y;
    }
    for (std::size_t i = e.size() - 1; i-- > 0;) e[i] += e[i + 1];
    const double total = e[0];
    double t5 = -1, t35 = -1;
    for (std::size_t i = 0; i < e.size(); ++i)
    {
        const double level = db(e[i] / total);
        if (t5 < 0 && level <= -5.0) t5 = (double) i / fs;
        if (t35 < 0 && level <= -35.0) { t35 = (double) i / fs; break; }
    }
    return t35 > t5 && t5 >= 0 ? 2.0 * (t35 - t5) : -1.0;
}

void testReverbDecay()
{
    for (double fs : { 88200.0, 96000.0, 192000.0 })
        for (float a : { 0.1f, 0.5f, 0.9f })
        {
            PlateReverbFx plate;
            plate.prepare(fs);
            SpringReverbFx spring;
            spring.prepare(fs);
            const int length = (int) ((PlateReverbFx::targetRt60(a) * 1.6 + 0.5) * fs);
            std::vector<float> irPlate((std::size_t) length), irSpring((std::size_t) (SpringReverbFx::targetRt60(a) * 1.6 * fs));
            for (int i = 0; i < length; ++i)
                irPlate[(std::size_t) i] = plate.process(i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, a, 1.0f).l;
            for (std::size_t i = 0; i < irSpring.size(); ++i)
                irSpring[i] = spring.process(i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, a, 1.0f).l;
            const double plateRt = rt60Of(irPlate, fs), springRt = rt60Of(irSpring, fs);
            const double plateTarget = PlateReverbFx::targetRt60(a), springTarget = SpringReverbFx::targetRt60(a);
            std::printf("RT60 fs %.0f a %.1f: plate %.2f / %.2f s, spring %.2f / %.2f s\n", fs, a, plateRt, plateTarget,
                            springRt, springTarget);
            check(std::abs(plateRt / plateTarget - 1.0) <= tol::kReverbRt60RelativeTolerance, "plate RT60 at 1 kHz meets its target");
            check(std::abs(springRt / springTarget - 1.0) <= tol::kReverbRt60RelativeTolerance, "spring RT60 at 1 kHz meets its target");
        }
    // The spring's dispersion: a click comes back as a descending chirp, so the energy at
    // 400 Hz arrives later than the energy at 3 kHz within the first round trip.
    {
        SpringReverbFx spring;
        spring.prepare(96000.0);
        std::vector<float> ir(9600);
        for (std::size_t i = 0; i < ir.size(); ++i) ir[i] = spring.process(i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f, 0.5f, 1.0f).l;
        auto centroid = [&](double hz)
        {
            auto bp = Eq::peak(96000.0, hz, 4.0, 24.0);
            double sum = 0, weighted = 0;
            for (std::size_t i = 0; i < ir.size(); ++i)
            {
                const double y = bp.process(ir[i]) - ir[i]; // peak minus dry ~ band-pass
                sum += y * y;
                weighted += y * y * (double) i;
            }
            return weighted / sum;
        };
        check(centroid(400.0) > centroid(3000.0) + 200.0, "spring returns a descending chirp (low frequencies travel slower)");
    }
}

void testTapeTiming()
{
    for (double fs : { 88200.0, 96000.0, 192000.0 })
        for (float a : { 0.0f, 0.4f, 1.0f })
        {
            TapeEchoFx tape;
            tape.prepare(fs);
            const double expected = TapeEchoFx::delaySeconds(a);
            const int length = (int) ((expected + 0.2) * fs);
            int arrival = -1;
            float peak = 0.0f;
            for (int i = 0; i < length; ++i)
            {
                const float y = std::abs(tape.process(i < 8 ? 1.0f : 0.0f, i < 8 ? 1.0f : 0.0f, a, 0.0f).l);
                if (y > peak) { peak = y; arrival = i; }
            }
            check(std::abs((double) arrival / fs / expected - 1.0) <= tol::kTapeDelayRelativeTolerance,
                  "first tape echo arrives at the set time");
        }
    // Repeats: each echo is the previous one times the repeat gain, which sets the T60.
    {
        TapeEchoFx tape;
        tape.prepare(96000.0);
        const float a = 0.3f, b = 0.6f;
        const double T = TapeEchoFx::delaySeconds(a), expected = std::pow(10.0, -3.0 * T / TapeEchoFx::tailSeconds(b));
        std::vector<float> y((std::size_t) (6.5 * T * 96000.0));
        // A 500 Hz burst: inside the flat part of the loop's record/playback filters.
        for (std::size_t i = 0; i < y.size(); ++i)
            y[i] = tape.process(i < 960 ? 0.2f * std::sin((float) i * 0.0327249f) : 0.0f, 0.0f, a, b).l;
        auto echoEnergy = [&](int k)
        {
            double sum = 0;
            const int c = (int) (k * T * 96000.0), w = (int) (0.02 * 96000.0);
            for (int i = c - w; i < c + w; ++i) sum += (double) y[(std::size_t) i] * y[(std::size_t) i];
            return sum;
        };
        // Ping-pong: odd echoes of a left-only input land on the left, even on the right.
        const double ratio = std::sqrt(echoEnergy(5) / echoEnergy(3));
        check(std::abs(20.0 * std::log10(ratio / (expected * expected))) < 1.0, "tape repeat gain follows the set echo decay time");
    }
}

// ---- classic byte identity --------------------------------------------------------------

// Frozen copy of the classic slot effects as written in Engine.cpp at 6719ce4.
struct FrozenClassicSlot
{
    Chorus chorus;
    Phaser phaser;
    Reverb reverb;
    DelayLine delayL, delayR;
    OnePole dampL, dampR;
    void prepare(float rate)
    {
        chorus.prepare(rate); phaser.prepare(rate); reverb.prepare(rate);
        delayL.prepare(0.8f, rate); delayR.prepare(0.8f, rate);
        dampL.set(9000.0f, rate); dampR.set(9000.0f, rate); dampL.reset(); dampR.reset();
    }
    void process(int live, float& mixL, float& mixR, float amount, float driftPhase, float sr)
    {
        switch (live)
        {
            case 1: { const auto m = chorus.process(mixL, mixR, 0.62f, 0.62f, true, false);
                      mixL = lerp(mixL, m.l, amount * 0.62f); mixR = lerp(mixR, m.r, amount * 0.62f); break; }
            case 2: { const auto p = phaser.process(mixL, mixR, 0.32f, 0.72f);
                      mixL = lerp(mixL, p.l, amount * 0.72f); mixR = lerp(mixR, p.r, amount * 0.72f); break; }
            case 3: { const float wow = std::sin(kTwoPi * driftPhase) * 0.0007f;
                      const float d = (0.34f + wow) * sr;
                      const float wl = dampL.process(delayL.read(d));
                      const float wr = dampR.process(delayR.read(d));
                      delayL.write(mixL + wr * 0.36f); delayR.write(mixR + wl * 0.36f);
                      mixL = lerp(mixL, wl, amount * 0.58f); mixR = lerp(mixR, wr, amount * 0.58f); break; }
            case 4: { const float wl = reverb.process(mixL, 0); const float wr = reverb.process(mixR, 1);
                      mixL = lerp(mixL, wl, amount * 0.62f); mixR = lerp(mixR, wr, amount * 0.62f); break; }
            case 5: { mixL = lerp(mixL, softClip(mixL * 3.2f) * 0.68f, amount);
                      mixR = lerp(mixR, softClip(mixR * 3.2f) * 0.68f, amount); break; }
            default: break;
        }
    }
};

void testClassicByteIdentity()
{
    for (float sr : { 88200.0f, 96000.0f, 384000.0f })
        for (int type = 1; type <= 5; ++type)
            for (float mix : { 0.3f, 1.0f })
            {
                FxSlot slot;
                slot.prepare(sr);
                FrozenClassicSlot frozen;
                frozen.prepare(sr);
                SlotControls c;
                c.type = type;
                c.mix = mix;
                Rng rng(77u + (std::uint32_t) type);
                float drift = 0.0f;
                bool same = true;
                for (int i = 0; i < (int) sr && same; ++i)
                {
                    drift += 0.031f / sr;
                    drift -= std::floor(drift);
                    float l = 0.3f * rng.nextBipolar(), r = 0.3f * rng.nextBipolar();
                    const auto o = slot.process(c, l, r, drift, sr);
                    frozen.process(type, l, r, mix, drift, sr);
                    same = std::memcmp(&o.l, &l, sizeof l) == 0 && std::memcmp(&o.r, &r, sizeof r) == 0;
                }
                check(same, "classic slot effects are bit-identical to the original Engine.cpp code");
            }
}

// ---- engine level: stability, purity, default invariance -----------------------------------

std::vector<float> renderFx(double rate, int policy, int model, float mix, float a, float b, int os, bool comp,
                            double seconds, double silenceAfter, float* drivePower = nullptr, float* tailPower = nullptr)
{
    ParamValues p;
    p[P_parityMode] = policy == 2 ? 1.0f : 0.0f;
    p[P_sourcePerformance] = policy == 1 ? 1.0f : 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        p[chParam(ch, CP_noise)] = 1.0f; // broadband excitation
        p[chParam(ch, CP_vR)] = 0.05f;
    }
    p[P_analogNoise] = 0.0f;
    p[P_fx1Model] = (float) model;
    p[P_fx1Mix] = mix;
    p[P_fx1ParamA] = a;
    p[P_fx1ParamB] = b;
    p[P_fx1Oversampling] = (float) os;
    p[P_fxGainCompensation] = comp ? 1.0f : 0.0f;
    Engine e;
    e.prepare(rate);
    e.setParams(p);
    for (int note = 40; note < 80; note += 7) e.noteOn(note, 1.0f);
    std::vector<float> out;
    std::vector<float> block(2 * 512);
    const int driveBlocks = (int) (seconds * rate / 512), tailBlocks = (int) (silenceAfter * rate / 512);
    double drive = 0.0, tail = 0.0;
    for (int b2 = 0; b2 < driveBlocks + tailBlocks; ++b2)
    {
        if (b2 == driveBlocks)
            for (int note = 40; note < 80; note += 7) e.noteOff(note);
        e.render(block.data(), 512);
        for (float x : block)
        {
            if (b2 >= driveBlocks / 2 && b2 < driveBlocks) drive += (double) x * x;
            if (b2 >= driveBlocks + tailBlocks - (int) (rate / 512)) tail += (double) x * x;
        }
        out.insert(out.end(), block.begin(), block.end());
    }
    if (drivePower) *drivePower = (float) (drive / std::max(1, driveBlocks - driveBlocks / 2));
    if (tailPower) *tailPower = (float) (tail / std::max(1, (int) (rate / 512)));
    return out;
}

void testStability()
{
    // Every model at maximum settings, at the internal rate of each host rate (2x 44.1,
    // 48, 96 and 192 kHz): 6 s of full-scale white-noise bursts, then 12 s of silence.
    for (double host : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int model = 1; model < kModelCount; ++model)
        {
            const double fs = 2.0 * host;
            FxSlot slot;
            slot.prepare((float) fs);
            SlotControls c;
            c.model = model;
            c.mix = 1.0f;
            c.a = c.b = 1.0f;
            c.oversampling = 3;
            c.gainComp = true;
            Rng rng(0xfeedu + (std::uint32_t) model);
            const int burst = (int) (6.0 * fs), total = burst + (int) (12.0 * fs);
            double drive = 0.0, tail = 0.0, peak = 0.0;
            bool finite = true;
            for (int i = 0; i < total; ++i)
            {
                const bool on = i < burst && (i / (int) (0.25 * fs)) % 3 != 2; // 0.5 s on, 0.25 s off
                const float x = on ? rng.nextBipolar() : 0.0f;
                const auto o = slot.process(c, x, on ? rng.nextBipolar() : 0.0f, 0.0f, (float) fs);
                const double e = 0.5 * ((double) o.l * o.l + (double) o.r * o.r);
                finite = finite && std::isfinite(o.l) && std::isfinite(o.r);
                peak = std::max(peak, (double) std::max(std::abs(o.l), std::abs(o.r)));
                if (i >= burst / 2 && i < burst) drive += e;
                if (i >= total - (int) fs) tail += e;
            }
            drive /= (double) (burst - burst / 2);
            tail /= fs;
            std::printf("stability %s at %.0f Hz host: peak %.2f, tail %.1f dB\n", modelName(model), host, peak, db(tail / drive));
            check(finite && peak <= tol::kFxMaxAbsOutput, "modern FX at maximum settings stays finite and bounded");
            check(db(tail / drive) <= tol::kFxTailDecayDb, "modern FX tail decays after the input stops");
        }
}

void testEngineFx()
{
    // Default invariance: model params set but no mix -> byte-identical to defaults.
    for (int policy = 0; policy < 3; ++policy)
    {
        const auto reference = renderFx(48000.0, policy, 0, 0.0f, 0.5f, 0.5f, 2, false, 0.5, 0.1);
        bool same = true;
        for (int model = 1; model < kModelCount; ++model)
            same = same && renderFx(48000.0, policy, model, 0.0f, 0.9f, 0.1f, 3, true, 0.5, 0.1) == reference;
        check(same, "modern FX controls without mix leave the render byte-identical");
    }
    // PURE CS-80: a modern model at full mix and loudness matching change nothing.
    const auto pure = renderFx(48000.0, 2, 0, 0.0f, 0.5f, 0.5f, 2, false, 0.5, 0.1);
    for (int model = 1; model < kModelCount; ++model)
        check(renderFx(48000.0, 2, model, 1.0f, 0.8f, 0.3f, 3, true, 0.5, 0.1) == pure, "modern FX are inert under PURE CS-80");
    // ...and are audible elsewhere.
    for (int model = 1; model < kModelCount; ++model)
        check(renderFx(48000.0, 0, model, 1.0f, 0.8f, 0.3f, 3, false, 0.5, 0.1) != renderFx(48000.0, 0, 0, 0.0f, 0.5f, 0.5f, 2, false, 0.5, 0.1),
              "modern FX are audible in the playable modes");
}

} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--calibrate") == 0)
    {
        calibrate();
        return 0;
    }
    // --only <group> runs one group (development aid); the default runs every gate.
    const char* only = argc > 2 && std::strcmp(argv[1], "--only") == 0 ? argv[2] : nullptr;
    auto run = [only](const char* name, void (*fn)()) { if (only == nullptr || std::strcmp(only, name) == 0) fn(); };
    run("kweighting", testKWeighting);
    run("halfband", testHalfband);
    run("drive", testDriveOversampling);
    run("classic", testClassicByteIdentity);
    run("static", testStaticLoudness);
    run("matched", testMatchedLoudness);
    run("reverb", testReverbDecay);
    run("tape", testTapeTiming);
    run("stability", testStability);
    run("engine", testEngineFx);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
