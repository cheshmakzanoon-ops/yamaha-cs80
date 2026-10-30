// Offline sound-quality audit of the core engine.
//
// Every number this tool prints is a property of THIS software on THIS machine's
// arithmetic. None of it is a measurement of, or a claim about, a Yamaha CS-80.
// It exists to find engineering defects (NaN, DC, clicks, aliasing, level and
// sample-rate dependence), not to score fidelity.
//
//   cs80_engine_audit --out NEW_DIR [--suite phrase|fx|alias|latency|all]
//                     [--rates 44100,48000,96000] [--quick]
//
// Metric definitions are in docs/ENGINE_AUDIT.md. Output is one CSV per suite.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace cs80;
namespace fs = std::filesystem;
constexpr int kBlock = 64; // small blocks so event times are within ~1.5 ms

void require(bool ok, const char* msg) { if (!ok) throw std::runtime_error(msg); }

// ---------------------------------------------------------------------------
// Scenario: either a full control frame (hardware tabs) or PANEL parameters.
// ---------------------------------------------------------------------------
struct Setup
{
    std::string name;
    std::string group;
    ParamValues values;
    std::array<int, 2> sources { 13, 13 };
    const char* policy = "legacy";
};

struct Event { double t; bool on; int note; float vel; };
struct Rendered
{
    std::vector<float> l, r;
    int rate = 48000;
    std::uint64_t nonfinite = 0;
    int voicesAtEnd = 0;
};

Rendered render(const Setup& s, int rate, const std::vector<Event>& events, double seconds,
                bool (*midBlock)(Engine&, double, void*) = nullptr, void* user = nullptr)
{
    Engine e;
    e.prepare(rate);
    ControlFrame frame;
    frame.effectiveValues = s.values;
    frame.hardwareSources = s.sources;
    require(e.setControlFrame(frame), "frame rejected");
    e.reset();
    const long frames = (long) std::llround(seconds * rate);
    Rendered out;
    out.rate = rate;
    out.l.resize((std::size_t) frames);
    out.r.resize((std::size_t) frames);
    std::vector<float> buf((std::size_t) kBlock * 2);
    std::size_t next = 0;
    for (long f = 0; f < frames; f += kBlock)
    {
        const int n = (int) std::min<long>(kBlock, frames - f);
        while (next < events.size() && (long) std::ceil(events[next].t * rate / kBlock) * kBlock <= f)
        {
            const auto& ev = events[next++];
            if (ev.on) e.noteOn(ev.note, ev.vel, 1); else e.noteOff(ev.note, 1);
        }
        if (midBlock) midBlock(e, (double) f / rate, user);
        require(e.setControlFrame(frame), "frame rejected");
        e.render(buf.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const float a = buf[(std::size_t) i * 2], b = buf[(std::size_t) i * 2 + 1];
            if (!std::isfinite(a) || !std::isfinite(b)) { ++out.nonfinite; }
            out.l[(std::size_t) (f + i)] = std::isfinite(a) ? a : 0.0f;
            out.r[(std::size_t) (f + i)] = std::isfinite(b) ? b : 0.0f;
        }
    }
    out.voicesAtEnd = e.activeVoices();
    return out;
}

// ---------------------------------------------------------------------------
// Standard test phrase. Times are in seconds and independent of the host rate.
// ---------------------------------------------------------------------------
namespace T {
constexpr double singleOn = 0.20, singleOff = 1.40;
constexpr double chordOn = 3.00, chordOff = 4.40;
constexpr double retrigOn1 = 6.00, retrigOff1 = 6.20, retrigOn2 = 6.25, retrigOff2 = 6.55;
// Eight-voice chord, then a four-note chord 150 ms after its release: with eight circuits busy
// in release, the second chord must reuse four voices that are still sounding (a voice steal).
constexpr double bigOn = 7.30, bigOff = 8.50, stealOn = 8.65, stealOff = 9.65;
constexpr double length = 12.0;
}

std::vector<Event> phrase()
{
    std::vector<Event> ev;
    ev.push_back({ T::singleOn, true, 60, 0.8f });
    ev.push_back({ T::singleOff, false, 60, 0.0f });
    for (int n : { 48, 52, 55, 60 }) ev.push_back({ T::chordOn, true, n, 0.8f });
    for (int n : { 48, 52, 55, 60 }) ev.push_back({ T::chordOff, false, n, 0.0f });
    ev.push_back({ T::retrigOn1, true, 60, 0.8f });
    ev.push_back({ T::retrigOff1, false, 60, 0.0f });
    ev.push_back({ T::retrigOn2, true, 60, 0.8f });
    ev.push_back({ T::retrigOff2, false, 60, 0.0f });
    for (int n : { 36, 43, 48, 52, 55, 60, 64, 67 }) ev.push_back({ T::bigOn, true, n, 0.8f });
    for (int n : { 36, 43, 48, 52, 55, 60, 64, 67 }) ev.push_back({ T::bigOff, false, n, 0.0f });
    for (int n : { 48, 52, 55, 60 }) ev.push_back({ T::stealOn, true, n, 0.8f });
    for (int n : { 48, 52, 55, 60 }) ev.push_back({ T::stealOff, false, n, 0.0f });
    std::stable_sort(ev.begin(), ev.end(), [](const Event& a, const Event& b) { return a.t < b.t; });
    return ev;
}

// ---------------------------------------------------------------------------
// Metrics
// ---------------------------------------------------------------------------
struct Win { double a, b; };
long idx(const Rendered& r, double t) { return std::clamp((long) std::llround(t * r.rate), 0L, (long) r.l.size()); }

double meanOf(const std::vector<float>& x, long a, long b)
{
    if (b <= a) return 0.0;
    double s = 0.0;
    for (long i = a; i < b; ++i) s += x[(std::size_t) i];
    return s / (double) (b - a);
}
double rmsOf(const Rendered& r, long a, long b)
{
    double s = 0.0;
    for (long i = a; i < b; ++i)
        s += (double) r.l[(std::size_t) i] * r.l[(std::size_t) i] + (double) r.r[(std::size_t) i] * r.r[(std::size_t) i];
    return b > a ? std::sqrt(s / (2.0 * (double) (b - a))) : 0.0;
}
double peakOf(const Rendered& r, long a, long b)
{
    double p = 0.0;
    for (long i = a; i < b; ++i)
        p = std::max({ p, (double) std::fabs(r.l[(std::size_t) i]), (double) std::fabs(r.r[(std::size_t) i]) });
    return p;
}
double maxJump(const Rendered& r, long a, long b)
{
    double j = 0.0;
    for (long i = std::max(1L, a); i < b; ++i)
        j = std::max({ j, (double) std::fabs(r.l[(std::size_t) i] - r.l[(std::size_t) i - 1]),
                       (double) std::fabs(r.r[(std::size_t) i] - r.r[(std::size_t) i - 1]) });
    return j;
}
double db(double x) { return 20.0 * std::log10(std::max(x, 1.0e-12)); }

// Rough loudness weighting: 2nd-order Butterworth high-pass at 100 Hz (not K-weighting, not LUFS).
double weightedRms(const Rendered& r, long a, long b)
{
    const double fc = 100.0, w = std::tan(M_PI * fc / r.rate), q = M_SQRT1_2;
    const double n = 1.0 / (1.0 + w / q + w * w);
    const double b0 = n, b1 = -2.0 * n, b2 = n, a1 = 2.0 * (w * w - 1.0) * n, a2 = (1.0 - w / q + w * w) * n;
    double s = 0.0;
    for (const auto* ch : { &r.l, &r.r })
    {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (long i = std::max(0L, a - 4L * r.rate / 100); i < b; ++i)
        {
            const double x = (*ch)[(std::size_t) i];
            const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            if (i >= a) s += y * y;
        }
    }
    return b > a ? std::sqrt(s / (2.0 * (double) (b - a))) : 0.0;
}

// Largest excursion of a 2nd-order Butterworth low-pass at 10 Hz (-57 dB at 250 Hz): a DC-step/thump
// detector that an ordinary audible note does not excite.
double subsonicPeak(const Rendered& r)
{
    const double w = std::tan(M_PI * 10.0 / r.rate), q = M_SQRT1_2, n = 1.0 / (1.0 + w / q + w * w);
    const double b0 = w * w * n, b1 = 2.0 * b0, b2 = b0, a1 = 2.0 * (w * w - 1.0) * n, a2 = (1.0 - w / q + w * w) * n;
    double p = 0;
    for (const auto* ch : { &r.l, &r.r })
    {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (float xf : *ch)
        {
            const double x = xf, y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            p = std::max(p, std::fabs(y));
        }
    }
    return p;
}

// Highest 300 ms weighted loudness inside [a, b): works for percussive programs whose chord window decays.
double shortTermLoudness(const Rendered& r, long a, long b)
{
    const long w = (long) (0.3 * r.rate), hop = (long) (0.05 * r.rate);
    double best = 0;
    for (long i = a; i + w <= b; i += hop) best = std::max(best, weightedRms(r, i, i + w));
    return best;
}

struct Row
{
    std::string name, group, policy;
    int rate = 0;
    std::uint64_t nonfinite = 0;
    double peak = 0, peakDb = 0, over1 = 0, softclipFrac = 0;
    double rmsSingleDb = 0, rmsChordDb = 0, weightedChordDb = 0, loudShortDb = 0;
    double dcSingleRel = 0, dcChordRel = 0, dcIdleAbs = 0, idleRms = 0;
    double subsonic = 0, subsonicRel = 0;
    double clickOff = 0, clickChordOff = 0, clickRetrig = 0, clickSteal = 0, clickFreshChord = 0;
    int voicesAtEnd = 0;
};

Row analyse(const Setup& s, const Rendered& r)
{
    Row row;
    row.name = s.name; row.group = s.group; row.policy = s.policy; row.rate = r.rate;
    row.nonfinite = r.nonfinite;
    const long N = (long) r.l.size();
    row.peak = peakOf(r, 0, N);
    row.peakDb = db(row.peak);
    for (long i = 0; i < N; ++i)
        if (std::fabs(r.l[(std::size_t) i]) > 1.0f || std::fabs(r.r[(std::size_t) i]) > 1.0f) row.over1 += 1;
    const long sa = idx(r, 0.85), sb = idx(r, T::singleOff);
    const long ca = idx(r, 3.80), cb = idx(r, T::chordOff);
    row.rmsSingleDb = db(rmsOf(r, sa, sb));
    const double chordRms = rmsOf(r, ca, cb);
    row.rmsChordDb = db(chordRms);
    row.weightedChordDb = db(weightedRms(r, ca, cb));
    row.loudShortDb = db(shortTermLoudness(r, idx(r, T::chordOn), idx(r, T::chordOff)));
    row.dcSingleRel = (std::fabs(meanOf(r.l, sa, sb)) + std::fabs(meanOf(r.r, sa, sb))) * 0.5 / std::max(rmsOf(r, sa, sb), 1.0e-9);
    row.dcChordRel = (std::fabs(meanOf(r.l, ca, cb)) + std::fabs(meanOf(r.r, ca, cb))) * 0.5 / std::max(chordRms, 1.0e-9);
    // 0.6 host-rate amplitude is where the legacy tanh(0.85x)/0.85 output stage has lost about 1 dB.
    {
        long hot = 0;
        for (long i = ca; i < cb; ++i) hot += (std::fabs(r.l[(std::size_t) i]) >= 0.6f) + (std::fabs(r.r[(std::size_t) i]) >= 0.6f);
        row.softclipFrac = (double) hot / (2.0 * (double) std::max(1L, cb - ca));
    }
    row.voicesAtEnd = r.voicesAtEnd;
    const long ia = idx(r, T::length - 0.4), ib = N;
    row.idleRms = rmsOf(r, ia, ib);
    row.dcIdleAbs = (std::fabs(meanOf(r.l, ia, ib)) + std::fabs(meanOf(r.r, ia, ib))) * 0.5;
    row.subsonic = subsonicPeak(r);
    row.subsonicRel = row.subsonic / std::max(row.peak, 1.0e-9);

    // Jump scores. A "jump" is the worst sample-to-sample step. For note-ons the reference is the worst
    // step that the program's own playing makes in the whole chord window (attack included, so
    // percussive programs are judged against their own attacks); a score above ~1 is a step bigger than
    // that program ever makes on its own, i.e. a click. For note-offs the reference is the 60 ms before.
    auto blockStart = [&](double t) { return (long) std::ceil(t * r.rate / kBlock) * kBlock; };
    const long win = (long) std::llround(0.008 * r.rate);
    const double steadyChord = std::max(maxJump(r, idx(r, T::chordOn), cb), 1.0e-7);
    auto onScore = [&](double t, double ref) { const long f = blockStart(t); return maxJump(r, f, f + win) / ref; };
    auto offScore = [&](double t) {
        const long f = blockStart(t);
        const double before = std::max(maxJump(r, f - (long) (0.06 * r.rate), f), 1.0e-7);
        return maxJump(r, f, f + win) / before;
    };
    row.clickOff = offScore(T::singleOff);
    row.clickChordOff = offScore(T::chordOff);
    row.clickRetrig = onScore(T::retrigOn2, steadyChord);
    // Fresh chord onset (idle voices) versus the same chord stealing four sounding tails: each is
    // normalised by the worst step of the chord that precedes it, so the ratio steal/fresh isolates
    // what the voice reassignment itself adds.
    row.clickFreshChord = onScore(T::chordOn, steadyChord);
    row.clickSteal = onScore(T::stealOn, std::max(maxJump(r, idx(r, T::bigOn), idx(r, T::bigOff)), 1.0e-7));
    return row;
}

std::string header()
{
    return "name,group,policy,rate,nonfinite,peak,peak_db,samples_over_1,softclip_frac_chord,rms_single_db,rms_chord_db,"
           "weighted_chord_db,loud_short_db,dc_single_rel,dc_chord_rel,dc_idle_abs,idle_rms,subsonic_peak,subsonic_rel,"
           "click_off,click_chord_off,click_retrigger,click_fresh_chord,click_steal,voices_at_end";
}
std::string csv(const Row& r)
{
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << std::setprecision(6) << '"' << r.name << "\"," << r.group << ',' << r.policy << ',' << r.rate << ',' << r.nonfinite
      << ',' << r.peak << ',' << r.peakDb << ',' << r.over1 << ',' << r.softclipFrac << ',' << r.rmsSingleDb << ','
      << r.rmsChordDb << ',' << r.weightedChordDb << ',' << r.loudShortDb << ',' << r.dcSingleRel << ',' << r.dcChordRel << ',' << r.dcIdleAbs
      << ',' << r.idleRms << ',' << r.subsonic << ',' << r.subsonicRel << ',' << r.clickOff
      << ',' << r.clickChordOff << ',' << r.clickRetrig << ',' << r.clickFreshChord << ',' << r.clickSteal << ',' << r.voicesAtEnd;
    return o.str();
}

// ---------------------------------------------------------------------------
// Scenario factories
// ---------------------------------------------------------------------------
void setPolicy(ParamValues& p, bool source) { p[P_sourcePerformance] = source ? 1.0f : 0.0f; p[P_parityMode] = 0.0f; }

std::vector<Setup> programSetups(bool source)
{
    std::vector<Setup> v;
    for (const auto& preset : factoryPresets())
    {
        Setup s;
        s.name = preset.name; s.group = "program:" + preset.category; s.values = preset.values;
        setPolicy(s.values, source);
        s.policy = source ? "source" : "legacy";
        v.push_back(s);
    }
    return v;
}

// Same adapter as the plugin's physical tab selector and the existing benchmark.
std::vector<Setup> tabSetups(bool source)
{
    std::vector<Setup> v;
    for (int sel = 0; sel < 11; ++sel)
    {
        Setup s;
        s.group = "tab"; s.policy = source ? "source" : "legacy";
        s.name = std::string(hardwareToneName(sel, 0)) + " + " + hardwareToneName(sel, 1);
        ParamValues& p = s.values;
        p = ParamValues();
        setPolicy(p, source);
        p[P_subOriginalMode] = 1.0f;
        for (int rank = 0; rank < 2; ++rank)
        {
            s.sources[(std::size_t) rank] = sel;
            ParamValues legacy;
            applyHardwareTone(legacy, rank, sel);
            for (int local = 0; local < CP_count; ++local)
                if (local != CP_feet && local != CP_pan && local != CP_hpfOn && local != CP_lpfOn)
                    p[chParam(rank, local)] = legacy[chParam(rank, local)];
            p[chParam(rank, CP_feet)] = 1.0f;
            p[chParam(rank, CP_pan)] = 0.0f;
        }
        v.push_back(s);
    }
    return v;
}

std::vector<int> parseRates(const std::string& list)
{
    std::vector<int> rates;
    std::stringstream ss(list);
    std::string tok;
    while (std::getline(ss, tok, ',')) rates.push_back(std::stoi(tok));
    require(!rates.empty(), "no rates");
    return rates;
}

// ---------------------------------------------------------------------------
// FFT (iterative radix 2) for the alias suite
// ---------------------------------------------------------------------------
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
        const double ang = -2.0 * M_PI / (double) len;
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

struct AliasResult { double nonHarmonicDb = 0, worstSpurDbc = 0, fundamentalDb = 0, f0 = 0; };

// Alias estimate for one sustained note of the given waveform. Only rank I sounds (MIX=0), with
// no detune, drift, dispersion or analogue noise, so every legitimate partial sits at k * f0.
// Everything else in 20 Hz..20 kHz (and outside +/-6 bins of a partial) counts as non-harmonic.
AliasResult aliasTest(int rate, int note, bool pulse, bool ring, bool source, int feetIndex = 1)
{
    ParamValues p;
    setPolicy(p, source);
    p[P_mix] = 0.0f; p[P_detune] = 0.0f; p[P_drift] = 0.0f; p[P_voiceDispersion] = 0.0f;
    p[P_analogNoise] = 0.0f; p[P_chorusOn] = 0.0f; p[P_volume] = 0.30f; p[P_resonance] = 0.0f;
    p[P_subOriginalMode] = 1.0f;
    if (ring) { p[P_ringAmount] = 1.0f; p[P_ringSpeed] = 150.0f; p[P_ringDepth] = 0.0f; }
    const int ch = 0;
    p[chParam(ch, CP_saw)] = pulse ? 0.0f : 1.0f;
    p[chParam(ch, CP_pulse)] = pulse ? 1.0f : 0.0f;
    p[chParam(ch, CP_pw)] = 0.5f;
    p[chParam(ch, CP_feet)] = (float) feetIndex;
    p[chParam(ch, CP_hpfOn)] = 0.0f;            // no high-pass: keep the fundamental and all the content
    p[chParam(ch, CP_lpfCutoff)] = 18000.0f;     // tracks the key; fully open at these pitches
    p[chParam(ch, CP_fIl)] = 0.0f; p[chParam(ch, CP_fAl)] = 0.0f;
    p[chParam(ch, CP_vS)] = 1.0f; p[chParam(ch, CP_vA)] = 0.001f; p[chParam(ch, CP_vD)] = 0.001f;
    p[chParam(ch, CP_vLevel)] = 0.5f; p[chParam(ch, CP_vcfLevel)] = 1.0f;
    Setup s; s.name = "alias"; s.values = p; s.policy = source ? "source" : "legacy";
    const std::size_t N = 65536;
    const double warm = 0.6;
    const double seconds = warm + (double) N / rate + 0.05;
    const auto r = render(s, rate, { { 0.05, true, note, 1.0f } }, seconds);
    AliasResult res;
    const double ratio = kFeetRatios[std::clamp(feetIndex, 0, 5)];
    res.f0 = midiToHz((float) note) * ratio;
    if (r.nonfinite) { res.nonHarmonicDb = 999; return res; }
    std::vector<std::complex<double>> a(N);
    const long start = (long) std::llround(warm * rate);
    for (std::size_t i = 0; i < N; ++i)
    {
        const double x = (double) (i) / (double) (N - 1);
        const double w = 0.35875 - 0.48829 * std::cos(2 * M_PI * x) + 0.14128 * std::cos(4 * M_PI * x) - 0.01168 * std::cos(6 * M_PI * x);
        a[i] = w * 0.5 * ((double) r.l[(std::size_t) start + i] + (double) r.r[(std::size_t) start + i]);
    }
    fft(a);
    const double binHz = (double) rate / (double) N;
    const std::size_t lo = (std::size_t) std::ceil(20.0 / binHz);
    const std::size_t hi = std::min((std::size_t) std::floor(20000.0 / binHz), N / 2 - 1);
    std::vector<char> harmonic(N / 2, 0);
    double harmP = 0, fundP = 0; // fundP: strongest legitimate line, the dBc reference
    // With the ring modulator on (depth 0, so a fixed carrier) the legitimate lines are k*f0 +/- fr.
    const double fr = 150.0;
    // k = 0 is included for the ring modulator: a DC offset in the saw multiplies the carrier and leaves
    // a line at fr, which is the modulator's own feed-through, not an alias.
    for (int k = ring ? 0 : 1; k * res.f0 < 0.5 * rate + fr; ++k)
        for (int m : ring ? std::vector<int> { -1, 1 } : std::vector<int> { 0 })
        {
            const double c = std::fabs(k * res.f0 + m * fr) / binHz;
            for (long b = (long) std::floor(c) - 6; b <= (long) std::ceil(c) + 6; ++b)
                if (b >= 0 && b < (long) N / 2) harmonic[(std::size_t) b] = 1;
        }
    double nonP = 0, worst = 0;
    for (std::size_t b = lo; b <= hi; ++b)
    {
        const double pw = std::norm(a[b]);
        if (harmonic[b]) { harmP += pw; fundP = std::max(fundP, pw); }
        else { nonP += pw; worst = std::max(worst, pw); }
    }
    res.nonHarmonicDb = 10.0 * std::log10(std::max(nonP, 1.0e-30) / std::max(harmP, 1.0e-30));
    res.worstSpurDbc = 10.0 * std::log10(std::max(worst, 1.0e-30) / std::max(fundP, 1.0e-30));
    res.fundamentalDb = 10.0 * std::log10(std::max(fundP, 1.0e-30));
    return res;
}

// ---------------------------------------------------------------------------
// Latency: group delay of the 2x -> 1x output path, measured the way Engine::render uses it.
// ---------------------------------------------------------------------------
struct LatencyResult { double delay = 0, gainDb = 0; };
LatencyResult measureLatency(int rate, double freq)
{
    Decimator dec;
    dec.prepare((float) (2 * rate));
    const int total = 60000, skip = 8000;
    double cs = 0, sn = 0, ci = 0, si = 0;
    for (int i = 0; i < total; ++i)
    {
        float y = 0;
        for (int os = 0; os < 2; ++os)
        {
            const double n = 2.0 * i + os;
            dec.push((float) std::sin(2 * M_PI * freq * n / (2.0 * rate)));
            if (os == Decimator::kRetainedPhase) y = dec.output();
        }
        if (i >= skip)
        {
            const double th = 2 * M_PI * freq * i / rate;
            cs += y * std::cos(th); sn += y * std::sin(th);
            ci += std::cos(th) * std::cos(th); si += std::sin(th) * std::sin(th);
        }
    }
    // y(i) = A sin(th - 2 pi f D / fs) => in-phase sn, quadrature cs = -A sin(2 pi f D / fs)
    const double a = std::hypot(sn / si, cs / ci);
    const double phase = std::atan2(-cs / ci, sn / si);
    LatencyResult r;
    // The phase only fixes the delay modulo one period; resolve it against the reported 63 samples.
    const double period = rate / freq;
    double d = phase / (2 * M_PI * freq / rate) - Decimator::kHostLatency;
    d -= period * std::round(d / period);
    r.delay = d + Decimator::kHostLatency;
    r.gainDb = 20 * std::log10(a);
    return r;
}

// ---------------------------------------------------------------------------
// Suites
// ---------------------------------------------------------------------------
void suitePhrase(const fs::path& dir, const std::vector<int>& rates, bool quick)
{
    std::ofstream out(dir / "phrase.csv");
    out << header() << '\n';
    const auto ev = phrase();
    struct Job { std::vector<Setup> setups; std::vector<int> rates; };
    std::vector<Job> jobs;
    for (bool source : { false, true })
    {
        auto tabs = tabSetups(source);
        jobs.push_back({ tabs, rates });
        auto programs = programSetups(source);
        if (quick) programs.resize(std::min<std::size_t>(programs.size(), 12));
        // Full bank at the first rate; every 16th program at the other rates.
        std::vector<Setup> subset;
        for (std::size_t i = 0; i < programs.size(); ++i) if (i % 16 == 0) subset.push_back(programs[i]);
        jobs.push_back({ programs, { rates.front() } });
        if (rates.size() > 1) jobs.push_back({ subset, std::vector<int>(rates.begin() + 1, rates.end()) });
    }
    for (const auto& job : jobs)
        for (int rate : job.rates)
            for (const auto& s : job.setups)
            {
                const auto r = render(s, rate, ev, T::length);
                out << csv(analyse(s, r)) << '\n';
                out.flush();
            }
}

Setup fxBase(int rank, const char* label)
{
    Setup s;
    s.name = label; s.group = "fxbase"; s.policy = "legacy";
    ParamValues& p = s.values;
    p = ParamValues();
    p[P_subOriginalMode] = 1.0f;
    p[P_chorusOn] = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        p[chParam(ch, CP_saw)] = 1.0f;
        p[chParam(ch, CP_lpfCutoff)] = rank == 0 ? 3500.0f : 900.0f;
        p[chParam(ch, CP_vA)] = 0.02f; p[chParam(ch, CP_vD)] = 0.3f; p[chParam(ch, CP_vS)] = 0.8f; p[chParam(ch, CP_vR)] = 0.4f;
    }
    return s;
}

void suiteFx(const fs::path& dir, const std::vector<int>& rates)
{
    std::ofstream out(dir / "fx.csv");
    out << header() << ",fx_slots,fx_mix,tail_ratio_db,gain_vs_dry_db\n";
    const auto ev = phrase();
    struct Cfg { std::array<int, 3> type; float mix; };
    std::vector<Cfg> cfgs { { { 0, 0, 0 }, 0.0f } };
    for (int t = 1; t <= 5; ++t) { cfgs.push_back({ { t, 0, 0 }, 0.5f }); cfgs.push_back({ { t, 0, 0 }, 1.0f }); }
    cfgs.push_back({ { 3, 3, 3 }, 1.0f }); cfgs.push_back({ { 4, 4, 4 }, 1.0f });
    cfgs.push_back({ { 2, 2, 2 }, 1.0f }); cfgs.push_back({ { 5, 5, 5 }, 1.0f });
    cfgs.push_back({ { 1, 2, 3 }, 1.0f }); cfgs.push_back({ { 4, 3, 5 }, 1.0f });
    for (int rate : rates)
        for (bool source : { false, true })
        {
            double dryChord = 0;
            for (const auto& c : cfgs)
            {
                Setup s = fxBase(0, "bright saw");
                setPolicy(s.values, source);
                s.policy = source ? "source" : "legacy";
                for (int i = 0; i < 3; ++i)
                {
                    s.values[P_fx1Type + 2 * i] = (float) c.type[(std::size_t) i];
                    s.values[P_fx1Mix + 2 * i] = c.type[(std::size_t) i] ? c.mix : 0.0f;
                }
                const auto r = render(s, rate, ev, T::length);
                auto row = analyse(s, r);
                if (c.type[0] == 0) dryChord = row.rmsChordDb;
                std::ostringstream tag; tag << c.type[0] << c.type[1] << c.type[2];
                // Tail decay: level of the last second divided by level 2 s after the last release.
                const double late = db(rmsOf(r, idx(r, T::length - 1.0), idx(r, T::length)));
                const double early = db(rmsOf(r, idx(r, 4.9), idx(r, 5.4)));
                out << csv(row) << ',' << tag.str() << ',' << c.mix << ',' << (late - early) << ',' << (row.rmsChordDb - dryChord) << '\n';
                out.flush();
            }
        }
}

void suiteAlias(const fs::path& dir, const std::vector<int>& rates)
{
    std::ofstream out(dir / "alias.csv");
    out << "rate,policy,wave,ring,note,feet_ratio,f0_hz,nonharmonic_db_vs_partials,worst_spur_dbc\n";
    for (int rate : rates)
        for (bool source : { false, true })
            for (bool pulse : { false, true })
                for (bool ring : { false, true })
                    for (int note : { 72, 84, 96, 108 })
                        for (int feet : { 1, 5 })
                        {
                            if (note * 1.0 + 12.0 * std::log2(kFeetRatios[feet]) > 120.0) continue;
                            const auto a = aliasTest(rate, note, pulse, ring, source, feet);
                            out << rate << ',' << (source ? "source" : "legacy") << ',' << (pulse ? "pulse" : "saw") << ','
                                << ring << ',' << note << ',' << kFeetRatios[feet] << ',' << std::fixed << std::setprecision(1) << a.f0
                                << std::setprecision(2) << ',' << a.nonHarmonicDb << ',' << a.worstSpurDbc << '\n';
                            out.flush();
                        }
}

void suiteLatency(const fs::path& dir, const std::vector<int>& rates)
{
    std::ofstream out(dir / "latency.csv");
    out << "rate,freq_hz,delay_host_samples,reported_latency,gain_db\n";
    Engine e;
    for (int rate : rates)
        for (double f : { 250.0, 1000.0, 5000.0, 10000.0, 15000.0, 18000.0, 20000.0 })
        {
            if (f > 0.45 * rate) continue;
            const auto r = measureLatency(rate, f);
            out << rate << ',' << f << ',' << std::fixed << std::setprecision(4) << r.delay << ',' << e.getLatencySamples()
                << ',' << r.gainDb << '\n';
        }
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        fs::path dir;
        std::string suite = "all";
        std::vector<int> rates { 48000, 44100, 96000 };
        bool quick = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--quick") { quick = true; continue; }
            require(i + 1 < argc, "missing value");
            if (arg == "--out") dir = argv[++i];
            else if (arg == "--suite") suite = argv[++i];
            else if (arg == "--rates") rates = parseRates(argv[++i]);
            else throw std::runtime_error("unknown option " + arg);
        }
        require(!dir.empty(), "usage: cs80_engine_audit --out NEW_DIR [--suite phrase|fx|alias|latency|all] [--rates a,b,c] [--quick]");
        require(!fs::exists(dir), "output exists; refusing to overwrite");
        fs::create_directories(dir);
        if (suite == "all" || suite == "latency") suiteLatency(dir, rates);
        if (suite == "all" || suite == "alias") suiteAlias(dir, rates);
        if (suite == "all" || suite == "fx") suiteFx(dir, rates);
        if (suite == "all" || suite == "phrase") suitePhrase(dir, rates, quick);
        std::cout << "wrote " << dir.string() << '\n';
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "engine_audit: " << e.what() << '\n';
        return 2;
    }
}
