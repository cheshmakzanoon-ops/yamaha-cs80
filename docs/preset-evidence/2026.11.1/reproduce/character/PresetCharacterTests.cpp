// Finished-mix character checks for the 2026.11.1 factory refinements.
// These measure a few named sound-design roles, not acoustic-instrument or
// physical-CS-80 equivalence. Complete-bank level/identity checks live in
// LibraryAuditTests; the broader articulation checks live in PresetVoicingTests.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cs80;

namespace
{
constexpr double sampleRate = 44100.0;
constexpr double pi = 3.14159265358979323846;
constexpr double minimumBodyRms = 0.001; // The role cannot pass in silence.
int assertions = 0, failures = 0;
double generatedSeconds = 0.0;

void check(bool ok, const std::string& message)
{
    ++assertions;
    if (!ok)
    {
        ++failures;
        std::printf("FAIL: %s\n", message.c_str());
    }
}

double ratio(double a, double b) { return a / std::max(b, 1.0e-15); }
double db(double value) { return 20.0 * std::log10(std::max(value, 1.0e-15)); }

const FactoryPreset& preset(const char* name)
{
    const auto& bank = curatedLibrary();
    const auto found = std::find_if(bank.begin(), bank.end(),
        [name](const auto& program) { return program.name == name; });
    if (found == bank.end())
        throw std::runtime_error(std::string("Missing Circuit program: ") + name);
    return *found;
}

struct Audio
{
    std::vector<float> stereo;
    int frames() const { return static_cast<int>(stereo.size() / 2); }
    int frame(double seconds) const
    {
        return std::clamp(static_cast<int>(std::llround(seconds * sampleRate)), 0, frames());
    }
    double rms(double begin, double end) const
    {
        const int first = frame(begin), last = frame(end);
        if (last <= first) throw std::runtime_error("Empty character measurement window");
        double energy = 0.0;
        for (int i = first * 2; i < last * 2; ++i)
            energy += static_cast<double>(stereo[static_cast<size_t>(i)]) * stereo[static_cast<size_t>(i)];
        return std::sqrt(energy / (2.0 * (last - first)));
    }
};

Audio render(const FactoryPreset& program, int note, float velocity, double seconds, int rank = -1)
{
    auto values = program.values;
    // Isolate only through the actual rank-balance control. Both oscillator and
    // circuit histories continue running exactly as they do in the full voice.
    if (rank >= 0) values[P_mix] = static_cast<float>(rank);
    auto engine = std::make_unique<Engine>();
    engine->prepare(sampleRate);
    engine->setParams(values);
    check(engine->getRenderPolicy() == RenderPolicy::sourcePerformance,
          program.name + ": uses the Circuit render policy");
    engine->noteOn(note, velocity, 1);

    Audio audio;
    audio.stereo.resize(static_cast<size_t>(2 * std::llround(seconds * sampleRate)));
    for (int at = 0; at < audio.frames();)
    {
        const int count = std::min(128, audio.frames() - at);
        engine->render(audio.stereo.data() + 2 * at, count);
        at += count;
    }
    bool finite = true;
    double peak = 0.0;
    for (const float sample : audio.stereo)
    {
        finite = finite && std::isfinite(sample);
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    check(finite, program.name + ": every character-capture sample is finite");
    if (rank < 0)
        check(peak < 1.0, program.name + ": complete held voice remains below full scale");
    generatedSeconds += seconds;
    return audio;
}

struct Layers
{
    Audio full, body, detail;
    double bodyWeight, detailWeight;

    double bodyRms(double begin, double end) const { return bodyWeight * body.rms(begin, end); }
    double detailRms(double begin, double end) const { return detailWeight * detail.rms(begin, end); }
    double contribution(double begin, double end) const
    {
        return ratio(detailRms(begin, end), bodyRms(begin, end));
    }
    double reconstructionError(double begin, double end) const
    {
        const int first = full.frame(begin), last = full.frame(end);
        if (last <= first) throw std::runtime_error("Empty reconstruction window");
        double error = 0.0;
        for (int sample = first * 2; sample < last * 2; ++sample)
        {
            const auto i = static_cast<size_t>(sample);
            const double residual = full.stereo[i]
                - (bodyWeight * body.stereo[i] + detailWeight * detail.stereo[i]);
            error += residual * residual;
        }
        return ratio(std::sqrt(error / (2.0 * (last - first))), full.rms(begin, end));
    }
};

void checkReconstruction(const Layers& voice, const std::string& name,
                         double begin, double end, const char* window)
{
    const double error = voice.reconstructionError(begin, end);
    check(error < 0.01, name + ": weighted ranks reconstruct " + window + " within 1 percent RMS");
    std::printf("RECONSTRUCT %-20s %5.3f-%5.3f s error %7.1f dB relative\n",
                name.c_str(), begin, end, db(error));
}

Layers layers(const FactoryPreset& program, int note, float velocity, double seconds)
{
    // Match the engine's float subtraction before promoting the weight. A
    // diagnostic must reconstruct the actual patch, not a convenient proxy.
    Layers result { render(program, note, velocity, seconds),
                    render(program, note, velocity, seconds, 0),
                    render(program, note, velocity, seconds, 1),
                    static_cast<double>(1.0f - program.values[P_mix]),
                    static_cast<double>(program.values[P_mix]) };
    checkReconstruction(result, program.name, 0.0, seconds, "the complete voice");
    return result;
}

void sustainedBreath()
{
    const auto flute = layers(preset("Flute"), 67, 0.8f, 0.75);
    const auto breathy = layers(preset("Breathy Flute"), 67, 0.8f, 0.75);
    const double quietAir = flute.contribution(0.30, 0.50);
    const double strongAir = breathy.contribution(0.30, 0.50);
    // Deliberately broad role bounds: quiet air at least -32 dB below the body,
    // aspirated air at least -25 dB, and neither allowed to replace the pitch.
    // These are authoring contracts for these voices, not hearing thresholds.
    check(quietAir > 0.025 && quietAir < 0.16,
          "Flute: sustained air is present while its pitched body remains dominant");
    check(strongAir > 0.055 && strongAir < 0.25,
          "Breathy Flute: sustained aspiration is present while its body remains dominant");
    check(strongAir > 1.5 * quietAir,
          "Breathy Flute: more sustained air relative to its body than Flute");
    const auto checkSustain = [](const Layers& voice, const char* name)
    {
        // A whole-capture average could dilute an error in either actual role
        // window. Qualify each local estimate with the same 1% RMS bound.
        checkReconstruction(voice, name, 0.30, 0.50, "the sustained-breath window");
        checkReconstruction(voice, name, 0.50, 0.70, "the later breath window");
        check(voice.bodyRms(0.50, 0.70) > minimumBodyRms,
              std::string(name) + ": the held pitched body remains above the silence floor");
        check(voice.detailRms(0.50, 0.70) > 0.5 * voice.detailRms(0.30, 0.50),
              std::string(name) + ": breath continues after the onset instead of becoming only a chiff");
    };
    checkSustain(flute, "Flute");
    checkSustain(breathy, "Breathy Flute");
    std::printf("BREATH      Flute %6.1f dB, Breathy Flute %6.1f dB air/body\n", db(quietAir), db(strongAir));
}

void struckDetail()
{
    struct Case { const char* name; int note; double minimumContribution; };
    for (const auto& test : {
        Case { "Harp", 60, 0.08 }, Case { "Kalimba", 60, 0.08 },
        Case { "Sine Bass", 48, 0.05 }, Case { "Upright Pluck", 48, 0.05 } })
    {
        const auto voice = layers(preset(test.name), test.note, 0.9f, 1.2);
        checkReconstruction(voice, test.name, 0.004, 0.020, "the immediate strike window");
        checkReconstruction(voice, test.name, 0.80, 1.10, "the later held-note window");
        const double onset = voice.contribution(0.004, 0.020);
        const double late = voice.contribution(0.80, 1.10);
        // The upper component must contribute in the actual first 16 ms,
        // without becoming as strong as the resonator. The final window is
        // still a held key, so releasing the note cannot satisfy the decay.
        check(onset > test.minimumContribution && onset < 0.5,
              std::string(test.name) + ": supporting strike is present in the finished onset mix");
        check(voice.bodyRms(0.004, 0.020) > minimumBodyRms,
              std::string(test.name) + ": onset has an audible pitched body");
        check(voice.bodyRms(0.80, 1.10) > minimumBodyRms,
              std::string(test.name) + ": body remains after the strike has faded");
        check(late < 0.25 * onset,
              std::string(test.name) + ": upper contribution falls by at least 12 dB relative to its body");
        std::printf("STRIKE      %-20s onset/body %6.1f dB, late/body %7.1f dB\n",
                    test.name, db(onset), db(late));
    }
}

// Hann-windowed DFT power in a band around the known main register. An 8192
// sample window includes detuned/chorused neighbors without requiring the note
// to land on an FFT bin. Check mono at the pitch as well as across all energy.
double fundamentalMonoLoss(const Audio& audio, double nominalHz)
{
    constexpr int size = 8192;
    const int first = audio.frame(1.7);
    if (first + size > audio.frames()) throw std::runtime_error("Short stereo capture");
    const int low = std::max(1, static_cast<int>(std::ceil(0.9 * nominalHz * size / sampleRate)));
    const int high = std::min(size / 2 - 1, static_cast<int>(std::floor(1.1 * nominalHz * size / sampleRate)));
    double stereoPower = 0.0, monoPower = 0.0;
    for (int bin = low; bin <= high; ++bin)
    {
        const std::complex<double> step = std::polar(1.0, -2.0 * pi * bin / size);
        std::complex<double> phase(1.0, 0.0), left(0.0, 0.0), right(0.0, 0.0);
        for (int i = 0; i < size; ++i)
        {
            const double window = 0.5 - 0.5 * std::cos(2.0 * pi * i / (size - 1));
            const auto index = static_cast<size_t>(2 * (first + i));
            left += phase * (window * audio.stereo[index]);
            right += phase * (window * audio.stereo[index + 1]);
            phase *= step;
        }
        stereoPower += 0.5 * (std::norm(left) + std::norm(right));
        monoPower += std::norm(0.5 * (left + right));
    }
    if (stereoPower < 1.0e-8) throw std::runtime_error("Silent stereo fundamental diagnostic");
    return db(std::sqrt(ratio(stereoPower, monoPower)));
}

void stereoEnsembles()
{
    for (const char* name : { "Noir Swell Brass", "Brass Ensemble 16", "Neo-Noir Pad",
                             "Arctic Pad", "Synthwave Chords" })
    {
        const auto voice = render(preset(name), 60, 0.8f, 2.5);
        double ll = 0.0, rr = 0.0, lr = 0.0, mid = 0.0, side = 0.0;
        for (int i = voice.frame(1.0); i < voice.frame(2.4); ++i)
        {
            const auto index = static_cast<size_t>(2 * i);
            const double l = voice.stereo[index], r = voice.stereo[index + 1];
            ll += l * l; rr += r * r; lr += l * r;
            mid += 0.25 * (l + r) * (l + r);
            side += 0.25 * (l - r) * (l - r);
        }
        const double width = std::sqrt(ratio(side, mid));
        const double correlation = ratio(lr, std::sqrt(ll * rr));
        const double balance = db(std::sqrt(ratio(ll, rr)));
        const double monoLoss = db(std::sqrt(ratio(0.5 * (ll + rr), mid)));
        const double fundamental = 440.0 * std::exp2((60.0 - 69.0) / 12.0)
            * (std::string(name) == "Brass Ensemble 16" ? 0.5 : 1.0);
        const double pitchLoss = fundamentalMonoLoss(voice, fundamental);
        check(voice.rms(1.0, 2.4) > minimumBodyRms, std::string(name) + ": stereo window is audible");
        check(width > 0.04, std::string(name) + ": ensemble has side energy above a near-mono output");
        // Constant channel-gain imbalance alone also produces side energy;
        // correlation rejects that substitute for distinct stereo content.
        check(correlation < 0.997, std::string(name) + ": stereo difference is more than channel gain imbalance");
        check(std::abs(balance) < 1.5, std::string(name) + ": sustained stereo image stays centrally balanced");
        check(monoLoss < 0.5, std::string(name) + ": mono sum retains overall power within half a dB");
        check(pitchLoss < 0.75, std::string(name) + ": mono sum retains the main pitch band within three-quarter dB");
        std::printf("STEREO      %-20s side/mid %6.1f dB, correlation %.5f, balance %+5.2f dB, mono %.3f / pitch %.3f dB\n",
                    name, db(width), correlation, balance, monoLoss, pitchLoss);
    }
}
} // namespace

int main()
{
    try
    {
        sustainedBreath();
        struckDetail();
        stereoEnsembles();
    }
    catch (const std::exception& error)
    {
        check(false, error.what());
    }
    std::printf("Preset character regressions: %d/%d checks passed; %.1f seconds generated.\n",
                assertions - failures, assertions, generatedSeconds);
    return failures == 0 ? 0 : 1;
}
