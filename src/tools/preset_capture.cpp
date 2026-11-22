// Reproducible audition of the actual Circuit factory programs. Measurements
// describe software output; they are not an acoustic-instrument similarity score.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using namespace cs80;

namespace
{
#if defined(_MSC_VER)
// MSVC/clang-cl preserve the native Unicode command line through wmain.
using ArgumentChar = wchar_t;
std::string argumentText(const ArgumentChar* text)
{
    const auto utf8 = fs::path(text).u8string();
    return std::string(utf8.begin(), utf8.end());
}
#else
using ArgumentChar = char;
std::string argumentText(const ArgumentChar* text) { return text; }
#endif

struct Options
{
    fs::path out;
    fs::path inventory;
    std::vector<std::string> names;
    bool all = false, wav = false, list = false;
    int rate = 48000, note = -1, rank = -1;
    float velocity = 0.8f, pressure = 0.0f;
    double hold = 0.0, tail = 1.0;
};

double db(double v) { return 20.0 * std::log10(std::max(v, 1e-12)); }

std::string jsonString(const std::string& text)
{
    std::string out = "\"";
    for (const unsigned char c : text)
    {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 32) throw std::runtime_error("unsupported control character in program metadata");
        else out += static_cast<char>(c);
    }
    return out + '"';
}

std::string csv(const std::string& text)
{
    std::string out = "\"";
    for (char c : text) { if (c == '"') out += '"'; out += c; }
    return out + '"';
}

std::uint64_t digest(const ParamValues& values)
{
    std::uint64_t h = 14695981039346656037ull;
    for (float value : values.v)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        for (int i = 0; i < 4; ++i) { h ^= bits & 255u; h *= 1099511628211ull; bits >>= 8; }
    }
    return h;
}

double number(const std::string& text)
{
    size_t used = 0;
    const double value = std::stod(text, &used);
    if (used != text.size() || !std::isfinite(value)) throw std::runtime_error("invalid number: " + text);
    return value;
}

void usage()
{
    std::cout << "cs80_preset_capture --list | --inventory FILE | --out DIR (--all | --preset NAME ...)\n"
                 "  [--wav] [--rate 44100|48000|96000] [--note 0..127]\n"
                 "  [--velocity 0..1] [--pressure 0..1] [--hold 0.05..30] [--tail 0..30]\n"
                 "  [--rank 1|2]  (isolated-rank causal diagnostic; default is the complete patch)\n"
                 "Default hold is at least 3 seconds and extends beyond slow authored attacks.\n";
}

Options parse(int argc, ArgumentChar** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string key = argumentText(argv[i]);
        if (key == "--all") { o.all = true; continue; }
        if (key == "--wav") { o.wav = true; continue; }
        if (key == "--list") { o.list = true; continue; }
        if (i + 1 >= argc) throw std::runtime_error("missing argument for " + key);
        const std::string value = argumentText(argv[++i]);
        if (key == "--out" || key == "--inventory")
        {
            if (value.empty()) throw std::runtime_error("empty path for " + key);
            if (key == "--out") o.out = fs::path(argv[i]);
            else o.inventory = fs::path(argv[i]);
        }
        else if (key == "--preset") o.names.push_back(value);
        else if (key == "--rate" || key == "--note" || key == "--rank")
        {
            const double n = number(value);
            if (n != std::floor(n) || n < 0 || n > 96000) throw std::runtime_error("invalid integer: " + value);
            if (key == "--rate") o.rate = static_cast<int>(n);
            if (key == "--note") o.note = static_cast<int>(n);
            if (key == "--rank") o.rank = static_cast<int>(n) - 1;
            if (key == "--rank" && n != 1 && n != 2) throw std::runtime_error("rank must be 1 or 2");
        }
        else if (key == "--velocity" || key == "--pressure")
        {
            const double n = number(value);
            // Check before float rounding: e.g. 1.00000001 rounds to 1.0f.
            if (n < 0 || n > 1) throw std::runtime_error(key + " must be in [0,1]");
            if (key == "--velocity") o.velocity = static_cast<float>(n);
            else o.pressure = static_cast<float>(n);
        }
        else if (key == "--hold") { o.hold = number(value); if (o.hold < 0.05) throw std::runtime_error("hold is too short"); }
        else if (key == "--tail") o.tail = number(value);
        else throw std::runtime_error("unknown option: " + key);
    }
    if (static_cast<int>(o.list) + static_cast<int>(!o.inventory.empty()) + static_cast<int>(!o.out.empty()) > 1)
        throw std::runtime_error("choose exactly one of --list, --inventory, or --out");
    if (o.rate != 44100 && o.rate != 48000 && o.rate != 96000) throw std::runtime_error("unsupported capture rate");
    if (o.note > 127 || o.velocity < 0 || o.velocity > 1 || o.pressure < 0 || o.pressure > 1
        || o.hold > 30 || o.tail < 0 || o.tail > 30) throw std::runtime_error("capture argument out of range");
    if (o.all && !o.names.empty()) throw std::runtime_error("choose --all or named presets");
    if (!o.list && o.inventory.empty() && (o.out.empty() || (!o.all && o.names.empty())))
        throw std::runtime_error("select an output directory and programs");
    return o;
}

void inventory(const fs::path& path)
{
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cannot create inventory");
    f << std::setprecision(std::numeric_limits<float>::max_digits10)
      << "{\"schema\":1,\"library_revision\":" << jsonString(curatedLibraryRevision) << ",\"parameter_ids\":[";
    for (int i = 0; i < P_numParams; ++i) f << (i ? "," : "") << jsonString(paramId(i));
    f << "],\"programs\":[";
    const auto& bank = curatedLibrary();
    for (size_t i = 0; i < bank.size(); ++i)
    {
        const auto& p = bank[i];
        f << (i ? "," : "") << "{\"index\":" << i << ",\"host_index\":" << programIndexForCurated(static_cast<int>(i))
          << ",\"name\":" << jsonString(p.name) << ",\"category\":" << jsonString(p.category)
          << ",\"description\":" << jsonString(p.description) << ",\"tags\":[";
        for (size_t t = 0; t < p.tags.size(); ++t) f << (t ? "," : "") << jsonString(p.tags[t]);
        f << "],\"values\":[";
        for (size_t j = 0; j < p.values.v.size(); ++j) f << (j ? "," : "") << p.values.v[j];
        f << "]}";
    }
    f << "]}\n";
    f.flush(); f.close();
    if (!f) throw std::runtime_error("inventory write failed");
}

void u32(std::ostream& out, std::uint32_t n)
{
    for (int i = 0; i < 4; ++i) { out.put(static_cast<char>(n & 255u)); n >>= 8; }
}
void u16(std::ostream& out, std::uint16_t n)
{
    out.put(static_cast<char>(n & 255u)); out.put(static_cast<char>(n >> 8));
}
void wav(const fs::path& path, const std::vector<float>& audio, int rate)
{
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    if (audio.empty() || audio.size() % 2 != 0
        || audio.size() > (std::numeric_limits<std::uint32_t>::max() - 50u) / sizeof(float))
        throw std::runtime_error("WAV sample count outside RIFF bounds");
    for (float sample : audio)
        if (!std::isfinite(sample)) throw std::runtime_error("cannot export nonfinite WAV samples");
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot create WAV: " + path.string());
    const auto bytes = static_cast<std::uint32_t>(audio.size() * sizeof(float));
    f.write("RIFF", 4); u32(f, 50u + bytes); f.write("WAVEfmt ", 8); u32(f, 18);
    u16(f, 3); u16(f, 2); u32(f, static_cast<std::uint32_t>(rate));
    u32(f, static_cast<std::uint32_t>(rate) * 8u); u16(f, 8); u16(f, 32); u16(f, 0);
    // Non-PCM RIFF stores the decoded frame count in a fact chunk.
    f.write("fact", 4); u32(f, 4); u32(f, static_cast<std::uint32_t>(audio.size() / 2));
    f.write("data", 4); u32(f, bytes);
    // IEEE float WAV retains measured peaks; no normalization or clipping.
    for (float sample : audio)
    {
        std::uint32_t bits = 0; std::memcpy(&bits, &sample, sizeof(bits)); u32(f, bits);
    }
    f.flush(); f.close();
    if (!f)
    {
        std::error_code ignored;
        fs::remove(path, ignored);
        throw std::runtime_error("WAV write failed");
    }
}

struct Metrics
{
    double peak = 0, rms = 0, early = 0, body = 0, tail = 0;
    double peakTime = 0, centroid = 0, low = 0, mid = 0, high = 0, tilt = 0, correlation = 0;
    std::uint64_t nonfinite = 0, overFullScale = 0;
};

double rms(const std::vector<float>& a, int begin, int end)
{
    begin = std::clamp(begin, 0, static_cast<int>(a.size() / 2));
    end = std::clamp(end, begin, static_cast<int>(a.size() / 2));
    double sum = 0;
    for (int i = begin * 2; i < end * 2; ++i) sum += static_cast<double>(a[static_cast<size_t>(i)]) * a[static_cast<size_t>(i)];
    return end > begin ? std::sqrt(sum / (2.0 * (end - begin))) : 0.0;
}

void fft(std::array<std::complex<double>, 4096>& a)
{
    constexpr size_t n = 4096;
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const auto step = std::polar(1.0, -2.0 * std::numbers::pi / static_cast<double>(len));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w = 1;
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= step;
            }
        }
    }
}

Metrics measure(const std::vector<float>& a, int rate, int held)
{
    Metrics m;
    const int frames = static_cast<int>(a.size() / 2);
    double energy = 0, difference = 0, ll = 0, rr = 0, lr = 0;
    for (int i = 0; i < frames; ++i)
    {
        const double l = a[static_cast<size_t>(i) * 2], r = a[static_cast<size_t>(i) * 2 + 1];
        for (double x : {l, r})
        {
            if (!std::isfinite(x)) ++m.nonfinite;
            else { m.peak = std::max(m.peak, std::abs(x)); if (std::abs(x) > 1.0) ++m.overFullScale; }
        }
        energy += l * l + r * r;
        if (i < held)
        {
            ll += l * l; rr += r * r; lr += l * r;
            if (i > 0)
            {
                const double dl = l - a[static_cast<size_t>(i - 1) * 2];
                const double dr = r - a[static_cast<size_t>(i - 1) * 2 + 1];
                difference += dl * dl + dr * dr;
            }
        }
    }
    m.rms = std::sqrt(energy / std::max(1, frames * 2));
    m.early = rms(a, 0, std::min(held, rate / 5));
    m.body = rms(a, std::max(0, held - rate / 2), held);
    m.tail = rms(a, std::max(held, frames - rate / 2), frames);
    m.tilt = difference / std::max(1e-24, ll + rr);
    m.correlation = lr / std::max(1e-24, std::sqrt(ll * rr));
    double loudest = -1;
    int peakStart = 0;
    const int window = std::max(1, rate / 20);
    for (int at = 0; at < held; at += window)
    {
        const double level = rms(a, at, std::min(held, at + window));
        if (level > loudest) { loudest = level; peakStart = at; }
    }
    m.peakTime = static_cast<double>(peakStart) / rate;
    // Average stereo power, avoiding the cancellation of a mono sum. Report
    // the spectrum around the loudest held window, not an inaudible late tail.
    std::array<double, 2049> spectrum {};
    const int first = std::clamp(peakStart - 2048 + window / 2, 0, std::max(0, held - 4096));
    for (int ch = 0; ch < 2; ++ch)
    {
        std::array<std::complex<double>, 4096> bins {};
        for (int i = 0; i < 4096; ++i)
        {
            const int frame = first + i;
            const double x = frame < held ? a[static_cast<size_t>(frame) * 2 + static_cast<size_t>(ch)] : 0;
            bins[static_cast<size_t>(i)] = x * (0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / 4095.0));
        }
        fft(bins);
        // Fold the negative-frequency power into interior bins. Nyquist has
        // no separate negative-frequency partner and must not be doubled.
        for (size_t i = 1; i < spectrum.size(); ++i)
            spectrum[i] += std::norm(bins[i]) * (i + 1 == spectrum.size() ? 1.0 : 2.0);
    }
    double total = 0;
    for (size_t i = 1; i < spectrum.size(); ++i)
    {
        const double hz = static_cast<double>(i) * rate / 4096;
        const double power = spectrum[i]; total += power; m.centroid += power * hz;
        if (hz < 500) m.low += power;
        else if (hz < 2000) m.mid += power;
        else m.high += power;
    }
    if (total > 1e-24) { m.centroid /= total; m.low /= total; m.mid /= total; m.high /= total; }
    return m;
}

int defaultNote(const std::string& category)
{
    if (category == "Bass") return 48; // 16' body sounds C2.
    if (category == "Winds" || category == "Leads") return 67;
    if (category == "Bells") return 72;
    return 60;
}

double heldSeconds(const FactoryPreset& preset, const Options& o)
{
    if (o.hold > 0) return o.hold;
    double attack = 0;
    for (int ch = 0; ch < 2; ++ch)
        if ((ch == 0 && preset.values[P_mix] < 0.99f) || (ch == 1 && preset.values[P_mix] > 0.01f))
            attack = std::max({attack, static_cast<double>(preset.values[chParam(ch, CP_vA)]),
                                      static_cast<double>(preset.values[chParam(ch, CP_fA)])});
    return std::clamp(attack + 1.5, 3.0, 10.0);
}

std::vector<float> render(const ParamValues& p, const Options& o, int note, int held, int frames)
{
    auto e = std::make_unique<Engine>();
    e->prepare(o.rate); e->setParams(p);
    e->noteOn(note, o.velocity, 1);
    e->channelPressure(1, o.pressure);
    std::vector<float> audio(static_cast<size_t>(frames) * 2);
    bool released = false;
    for (int at = 0; at < frames;)
    {
        if (at == held && !released) { e->noteOff(note, 1); released = true; }
        const int count = std::min({128, frames - at, released ? frames - at : held - at});
        if (count <= 0) throw std::runtime_error("capture timeline failed to advance");
        e->render(audio.data() + static_cast<size_t>(at) * 2, count);
        at += count;
    }
    return audio;
}
} // namespace

#if defined(_MSC_VER)
int wmain(int argc, ArgumentChar** argv)
#else
int main(int argc, ArgumentChar** argv)
#endif
{
    try
    {
        if (argc == 2 && argumentText(argv[1]) == "--help") { usage(); return 0; }
        const auto o = parse(argc, argv);
        const auto& bank = curatedLibrary();
        if (o.list)
        {
            for (size_t i = 0; i < bank.size(); ++i) std::cout << i << '\t' << bank[i].category << '\t' << bank[i].name << '\n';
            return 0;
        }
        if (!o.inventory.empty()) { inventory(o.inventory); return 0; }
        // Validate the entire selection before creating output files.
        std::vector<size_t> selected;
        if (o.all) { for (size_t i = 0; i < bank.size(); ++i) selected.push_back(i); }
        else for (const auto& name : o.names)
        {
            const auto it = std::find_if(bank.begin(), bank.end(), [&](const auto& p) { return p.name == name; });
            if (it == bank.end()) throw std::runtime_error("unknown Circuit preset: " + name);
            const auto index = static_cast<size_t>(it - bank.begin());
            if (std::find(selected.begin(), selected.end(), index) == selected.end()) selected.push_back(index);
        }
        fs::create_directories(o.out);
        std::ofstream report(o.out / "metrics.csv");
        if (!report) throw std::runtime_error("cannot create metrics report");
        report << "index,name,category,note,sample_rate,velocity,pressure,hold_seconds,tail_seconds,rank,parameter_digest,"
                  "peak_dbfs,rms_dbfs,early_rms_dbfs,body_rms_dbfs,tail_rms_dbfs,peak_window_seconds,"
                  "centroid_hz,low_power,mid_power,high_power,tilt,stereo_correlation,nonfinite,samples_over_full_scale\n"
               << std::setprecision(10);
        bool passed = true;
        for (size_t index : selected)
        {
            const auto& preset = bank[index];
            auto values = preset.values;
            if (o.rank >= 0) values[P_mix] = static_cast<float>(o.rank);
            const int note = o.note < 0 ? defaultNote(preset.category) : o.note;
            const double hold = heldSeconds(preset, o);
            const int held = static_cast<int>(std::lround(hold * o.rate));
            const int frames = held + static_cast<int>(std::lround(o.tail * o.rate));
            const auto audio = render(values, o, note, held, frames);
            const auto m = measure(audio, o.rate, held);
            passed = passed && m.nonfinite == 0 && m.overFullScale == 0;
            report << index << ',' << csv(preset.name) << ',' << csv(preset.category) << ',' << note << ',' << o.rate << ','
                   << o.velocity << ',' << o.pressure << ',' << hold << ',' << o.tail << ',' << (o.rank + 1) << ','
                   << std::hex << digest(values) << std::dec << ',' << db(m.peak) << ',' << db(m.rms) << ','
                   << db(m.early) << ',' << db(m.body) << ',' << db(m.tail) << ',' << m.peakTime << ','
                   << m.centroid << ',' << m.low << ',' << m.mid << ',' << m.high << ',' << m.tilt << ','
                   << m.correlation << ',' << m.nonfinite << ',' << m.overFullScale << '\n';
            if (o.wav)
            {
                std::string filename = std::to_string(index) + "-";
                for (const unsigned char c : preset.name)
                    filename += (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                        ? static_cast<char>(c) : '-';
                wav(o.out / (filename + ".wav"), audio, o.rate);
            }
            std::cout << index << ' ' << preset.name << ": peak " << db(m.peak) << " dBFS\n" << std::flush;
        }
        report.flush(); report.close();
        if (!report) throw std::runtime_error("metrics report write failed");
        return passed ? 0 : 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "preset capture: " << e.what() << '\n';
        return 2;
    }
}
