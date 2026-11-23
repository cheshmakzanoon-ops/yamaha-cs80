#pragma once

// Instrument-specific spectral bodies for the optional factory-voicing layer.
// These are authored signal models, NOT measurements of a piano, a CS-80, or
// another physical instrument.  They complement the existing circuit ranks;
// the rank's envelope, touch VCA, pan, effects and key-release logic remain the
// owners of articulation and overall amplitude.
//
// A struck string/bar needs independently decaying modes, not a static sine
// plus a low-pass-filtered saw.  The piano model includes a stiffness law,
// velocity-dependent hammer spectrum, strike-position notches and unison
// doublets.  The distinctions between tine, reed, string and bar spectra are
// deliberate.  The general separation of string loss, dispersion, hammer and
// body is discussed by Bank & Sujbert, DAFx-02, pp.175-180:
// https://www.dafx.de/papers/DAFX02_Bank_Sujbert_piano_synthesis.pdf
// This compact modal implementation does not implement their measured model.
//
// All storage is fixed.  Expensive coefficient work happens at note onset or
// a 16-sample control boundary.  Each audible mode uses a double-precision
// complex rotation; no trigonometry, allocation or locks occur in its sample
// loop.  Host-rate Nyquist, not the oversampled internal rate, bounds the modes.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace cs80::body
{

enum Model : int
{
    off = 0, piano, tine, reed, clav, harpsichord, nylon, steel, bass,
    bowed, flute, clarinet, oboe, bassoon, brass, organ, bell, vibes,
    marimba, celesta
};

inline constexpr int modelCount = celesta + 1;
inline constexpr int maxModel = celesta;

inline constexpr const char* name(int model) noexcept
{
    constexpr const char* names[] = {
        "Off", "Piano strings", "Tine electric piano", "Reed electric piano",
        "Clavichord", "Harpsichord", "Nylon strings", "Steel strings",
        "Plucked bass", "Bowed strings", "Flute", "Clarinet", "Oboe",
        "Bassoon", "Brass", "Organ", "Bell", "Vibraphone", "Marimba", "Celesta"
    };
    return names[std::clamp(model, 0, maxModel)];
}

inline constexpr const char* modelName(int model) noexcept { return name(model); }

class InstrumentBody
{
public:
    void prepare(double internalRate, double hostRate) noexcept
    {
        sampleRate = std::isfinite(internalRate) ? std::clamp(internalRate, 8000.0, 1536000.0) : 96000.0;
        outputRate = std::isfinite(hostRate) ? std::clamp(hostRate, 8000.0, sampleRate) : std::min(48000.0, sampleRate);
        airLowCoefficient = 1.0 - std::exp(-twoPi * 5200.0 / sampleRate);
        airHighCoefficient = 1.0 - std::exp(-twoPi * 650.0 / sampleRate);
        flutterCoefficient = 1.0 - std::exp(-twoPi * 19.0 / sampleRate);
        reset();
    }

    void reset() noexcept
    {
        modes = {};
        count = 0;
        model = off;
        controlCountdown = 0;
        normalizationCountdown = 2048;
        coefficientRefresh = 0;
        lastFrequency = -1.0;
        lastColour = -1.0;
        airLow = airHigh = flutter = 0.0;
        onsetMissing = 1.0;
        hitEnvelope = 0.0;
        airAmount = hitAmount = 0.0;
        state = 0x7a4f39b1u;
    }

    // brightness is normalized [0,1]; decay scales relative partial losses.
    // Overall amplitude/decay/damping still comes from the external rank ADSR.
    void noteOn(int selectedModel, float midiNote, float velocity, float brightness,
                float decay, std::uint32_t seed) noexcept
    {
        reset();
        model = std::clamp(selectedModel, 0, maxModel);
        if (model == off) return;

        const double note = finiteClamp(midiNote, 0.0, 127.0, 60.0);
        keyHz = 440.0 * std::exp2((note - 69.0) / 12.0);
        strike = finiteClamp(velocity, 0.0, 1.0, 0.7);
        initialColour = finiteClamp(brightness, 0.0, 1.0, 0.5);
        decayScale = finiteClamp(decay, 0.25, 4.0, 1.0);
        // A quill and an organ key do not have a piano hammer's velocity law.
        // Fixed phases also prevent caller seed conventions from leaking
        // velocity into the deliberately touch-independent registrations.
        if (model == organ || model == harpsichord)
        {
            strike = 0.7;
            seed = 0x43733830u;
        }
        state = seed != 0 ? seed : 0x7a4f39b1u;

        sustained = model >= bowed && model <= organ;
        const double attack = model == bowed ? 0.0045
                            : model == brass ? 0.0023
                            : model == flute ? 0.0028 : 0.00055;
        onsetCoefficient = std::exp(-1.0 / (sampleRate * attack));
        hitCoefficient = std::exp(-1.0 / (sampleRate * hitSeconds()));
        hitEnvelope = 1.0;
        configureExcitation();

        if (model == bell || model == vibes || model == marimba || model == celesta)
            makeBarModes();
        else if (model == tine)
            makeTineModes();
        else
            makeHarmonicModes(note);

        // Bound the sum of absolute mode amplitudes at every colour setting.
        // This is an authored excitation level, not a hidden output limiter or
        // post-render normalization.  It does not grow as upper modes decay.
        double maximumSum = 0.0;
        for (int i = 0; i < count; ++i)
            maximumSum += std::max(modes[static_cast<std::size_t>(i)].dark,
                                   modes[static_cast<std::size_t>(i)].bright);
        const double scale = 0.72 / std::max(maximumSum, 1.0e-12);
        for (int i = 0; i < count; ++i)
        {
            auto& m = modes[static_cast<std::size_t>(i)];
            m.dark *= scale;
            m.bright *= scale;
        }
    }

    bool enabled() const noexcept { return model != off; }

    float tick(float frequency, float brightness, float pressure = 0.0f) noexcept
    {
        if (!enabled() || !std::isfinite(frequency) || frequency <= 0.0f) return 0.0f;
        if (controlCountdown == 0)
        {
            // Phase is never reset by bends, ribbon, vibrato or tuning.  A
            // control boundary is at most 15 internal samples away (<0.2 ms
            // at 96 kHz), well above the musical modulation bandwidth.
            updateControls(static_cast<double>(frequency), brightness, pressure);
            controlCountdown = controlPeriod;
        }
        --controlCountdown;

        double output = 0.0;
        for (int i = 0; i < count; ++i)
        {
            auto& m = modes[static_cast<std::size_t>(i)];
            const double nextReal = m.real * m.cosine - m.imaginary * m.sine;
            m.imaginary = m.imaginary * m.cosine + m.real * m.sine;
            m.real = nextReal;
            m.gain += m.gainStep;
            m.envelope *= m.loss;
            if (m.envelope < 1.0e-20) m.envelope = 0.0;
            output += m.imaginary * m.gain * m.envelope;
        }
        // Roundoff in unit rotations is tiny in double.  A sparse normalized
        // reset of magnitude bounds it for arbitrarily long held organ notes.
        if (--normalizationCountdown == 0)
        {
            for (int i = 0; i < count; ++i)
            {
                auto& m = modes[static_cast<std::size_t>(i)];
                const double norm = m.real * m.real + m.imaginary * m.imaginary;
                if (norm > 0.0 && std::isfinite(norm))
                {
                    const double reciprocal = 1.0 / std::sqrt(norm);
                    m.real *= reciprocal;
                    m.imaginary *= reciprocal;
                }
                else { m.real = 1.0; m.imaginary = 0.0; }
            }
            normalizationCountdown = 2048;
        }

        // Quiet, independently seeded contact/air texture.  A two-pole-band
        // shaped noise path avoids full-band white hiss or a DC hammer step.
        if (airAmount > 0.0 || hitAmount > 0.0)
        {
            const double noise = randomBipolar();
            airHigh += airHighCoefficient * (noise - airHigh);
            airLow += airLowCoefficient * (noise - airHigh - airLow);
            flutter += flutterCoefficient * (noise - flutter);
            output *= 1.0 + (sustained ? 0.012 * flutter : 0.0);
            output += airLow * (airAmount + hitAmount * hitEnvelope);
        }
        hitEnvelope *= hitCoefficient;
        if (hitEnvelope < 1.0e-20) hitEnvelope = 0.0;
        onsetMissing *= onsetCoefficient;
        return static_cast<float>(output * (1.0 - onsetMissing));
    }

private:
    static constexpr double pi = 3.1415926535897932384626433832795;
    static constexpr double twoPi = 2.0 * pi;
    static constexpr int maximumModes = 32;
    static constexpr int controlPeriod = 16;

    struct Mode
    {
        double ratio = 1.0;
        double dark = 0.0, bright = 0.0;
        double real = 1.0, imaginary = 0.0;
        double cosine = 1.0, sine = 0.0;
        double envelope = 1.0, loss = 1.0;
        double gain = 0.0, gainStep = 0.0;
    };

    std::array<Mode, maximumModes> modes {};
    double sampleRate = 96000.0, outputRate = 48000.0;
    double keyHz = 261.625565, strike = 0.7, initialColour = 0.5, decayScale = 1.0;
    double lastFrequency = -1.0, lastColour = -1.0;
    double onsetMissing = 1.0, onsetCoefficient = 0.98;
    double hitEnvelope = 0.0, hitCoefficient = 0.99;
    double airLow = 0.0, airHigh = 0.0, flutter = 0.0;
    double airLowCoefficient = 0.3, airHighCoefficient = 0.04, flutterCoefficient = 0.001;
    double airAmount = 0.0, hitAmount = 0.0;
    std::uint32_t state = 0x7a4f39b1u;
    int model = off, count = 0, controlCountdown = 0, normalizationCountdown = 2048;
    unsigned int coefficientRefresh = 0;
    bool sustained = false;

    static double finiteClamp(double value, double low, double high, double fallback) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    }

    double randomBipolar() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<double>(state >> 8) * (2.0 / 16777216.0) - 1.0;
    }

    static double resonance(double frequency, double center, double width) noexcept
    {
        const double distance = (frequency - center) / width;
        return 1.0 / (1.0 + distance * distance);
    }

    double hitSeconds() const noexcept
    {
        switch (model)
        {
            case piano: return 0.004 - 0.0018 * strike;
            case tine: case reed: return 0.0025;
            case clav: return 0.0032;
            case harpsichord: return 0.002;
            case nylon: case steel: case bass: return 0.006;
            case bowed: return 0.030;
            case flute: case clarinet: case oboe: case bassoon: return 0.025;
            case brass: return 0.018;
            default: return 0.0018;
        }
    }

    void configureExcitation() noexcept
    {
        switch (model)
        {
            case piano: hitAmount = 0.009 + 0.018 * strike * strike; break;
            case tine: hitAmount = 0.003 + 0.006 * strike; break;
            case reed: hitAmount = 0.004 + 0.007 * strike; break;
            case clav: hitAmount = 0.014; break;
            case harpsichord: hitAmount = 0.012; break;
            case nylon: hitAmount = 0.008 + 0.007 * strike; break;
            case steel: hitAmount = 0.013 + 0.012 * strike; break;
            case bass: hitAmount = 0.012 + 0.008 * strike; break;
            case bowed: airAmount = 0.0023; hitAmount = 0.008; break;
            case flute: airAmount = 0.0042; hitAmount = 0.007; break;
            case clarinet: airAmount = 0.0018; hitAmount = 0.004; break;
            case oboe: case bassoon: airAmount = 0.0022; hitAmount = 0.005; break;
            case brass: airAmount = 0.0012; hitAmount = 0.005; break;
            case organ: break;
            case marimba: hitAmount = 0.010 + 0.008 * strike; break;
            default: hitAmount = 0.004 + 0.006 * strike; break;
        }
    }

    void addMode(double ratio, double darkWeight, double brightWeight, double lossPerSecond,
                 double phase = 0.0) noexcept
    {
        if (count >= maximumModes) return;
        auto& m = modes[static_cast<std::size_t>(count++)];
        m.ratio = ratio;
        m.dark = std::max(darkWeight, 0.0);
        m.bright = std::max(brightWeight, 0.0);
        m.real = std::cos(phase);
        m.imaginary = std::sin(phase);
        m.loss = std::exp(-std::max(lossPerSecond, 0.0) / (sampleRate * decayScale));
    }

    double stringWeight(int harmonic, double colour) const noexcept
    {
        const double h = static_cast<double>(harmonic);
        const double frequency = keyHz * h;
        switch (model)
        {
            case piano:
            {
                const double hardness = 2.1 + 15.0 * strike * strike + 8.0 * colour;
                const double position = 0.116 + 0.007 * initialColour;
                const double comb = std::abs(std::sin(pi * h * position)) / std::sin(pi * position);
                // Hammer contact suppresses high modes at gentle strikes.
                const double contact = 1.0 / (1.0 + std::pow(h / hardness, 2.8));
                const double board = 0.65 + 0.28 * resonance(frequency, 280.0, 220.0)
                                          + 0.30 * resonance(frequency, 2300.0, 1500.0);
                return std::pow(h, -1.15) * comb * contact * board;
            }
            case reed:
            {
                // A reed pickup has much stronger even harmonics and a broad
                // midrange bark than the predominantly odd tine registration.
                const double even = harmonic % 2 == 0 ? 0.84 : 1.0;
                const double bark = 1.0 + 0.55 * strike * resonance(frequency, 1400.0, 1000.0);
                return even * bark * std::pow(h, -1.25) * std::exp(-(h - 1.0) / (2.0 + 5.0 * strike + 8.0 * colour));
            }
            case clav:
            {
                const double comb = 0.18 + 0.82 * std::abs(std::sin(pi * h * 0.235));
                return comb * std::pow(h, -0.95) * std::exp(-(h - 1.0) / (4.0 + 10.0 * colour + 4.0 * strike));
            }
            case harpsichord:
            {
                const double comb = 0.10 + 0.90 * std::abs(std::sin(pi * h * 0.168));
                return comb * std::pow(h, -0.72) * std::exp(-(h - 1.0) / (5.0 + 16.0 * colour));
            }
            case nylon: case steel: case bass:
            {
                const double position = model == nylon ? 0.22 : model == bass ? 0.19 : 0.135;
                const double comb = std::abs(std::sin(pi * h * position)) / std::sin(pi * position);
                const double falloff = model == nylon ? 1.75 : model == bass ? 1.6 : 1.32;
                const double body = 0.65 + 0.45 * resonance(frequency, model == bass ? 130.0 : 210.0, 130.0)
                                        + 0.25 * resonance(frequency, 620.0, 370.0);
                return body * comb * std::pow(h, -falloff)
                     * std::exp(-(h - 1.0) / (3.0 + 12.0 * colour + 5.0 * strike));
            }
            case bowed:
            {
                const double formant = 0.32 + 0.62 * resonance(frequency, 460.0, 340.0)
                                           + 0.48 * resonance(frequency, 1600.0, 700.0)
                                           + 0.72 * resonance(frequency, 2900.0, 1000.0);
                return formant * std::pow(h, -0.92) * std::exp(-(h - 1.0) / (4.0 + 20.0 * colour));
            }
            case flute:
            {
                const double weights[] = { 1.0, 0.26, 0.085, 0.035, 0.012, 0.007 };
                const double weight = harmonic <= 6 ? weights[harmonic - 1] : 0.003 / (h - 5.0);
                return weight * std::exp(-(h - 1.0) * (0.7 - 0.62 * colour));
            }
            case clarinet:
            {
                const double even = harmonic % 2 == 0 ? 0.045 + 0.15 * colour : 1.0;
                return even * std::pow(h, -0.85) * std::exp(-(h - 1.0) / (3.5 + 13.0 * colour));
            }
            case oboe:
            {
                const double formant = 0.28 + 1.65 * resonance(frequency, 1300.0, 650.0)
                                           + 0.75 * resonance(frequency, 2800.0, 1000.0);
                return formant * std::pow(h, -0.77) * std::exp(-(h - 1.0) / (4.0 + 17.0 * colour));
            }
            case bassoon:
            {
                const double formant = 0.34 + 1.25 * resonance(frequency, 500.0, 280.0)
                                           + 0.80 * resonance(frequency, 1500.0, 700.0);
                return formant * std::pow(h, -0.88) * std::exp(-(h - 1.0) / (3.5 + 15.0 * colour));
            }
            case brass:
            {
                const double formant = 0.55 + 0.65 * resonance(frequency, 1250.0, 1100.0);
                return formant * std::pow(h, -0.68) * std::exp(-(h - 1.0) / (2.0 + 18.0 * colour));
            }
            case organ:
            {
                switch (harmonic)
                {
                    case 1: return 1.0;
                    case 2: return 0.35 + 0.28 * colour;
                    case 3: return 0.12 + 0.32 * colour;
                    case 4: return 0.12 + 0.27 * colour;
                    case 6: return 0.05 + 0.17 * colour;
                    case 8: return 0.025 + 0.13 * colour;
                    case 10: return 0.015 + 0.075 * colour;
                    case 12: return 0.008 + 0.060 * colour;
                    case 16: return 0.005 + 0.045 * colour;
                    default: return 0.0;
                }
            }
            default: return 0.0;
        }
    }

    double stringLoss(int harmonic) const noexcept
    {
        if (sustained) return 0.0;
        const double h = static_cast<double>(harmonic - 1);
        const double registerScale = std::clamp(std::sqrt(keyHz / 261.625565), 0.45, 2.8);
        switch (model)
        {
            case piano: return registerScale * (0.022 + 0.25 * std::sqrt(h) + 0.055 * h * h);
            case reed: return 0.025 + 0.65 * h + 0.08 * h * h;
            case clav: return 0.045 + 0.48 * h + 0.045 * h * h;
            case harpsichord: return 0.035 + 0.22 * h + 0.025 * h * h;
            case nylon: return 0.045 + 0.42 * h + 0.048 * h * h;
            case steel: return 0.025 + 0.21 * h + 0.027 * h * h;
            case bass: return 0.035 + 0.50 * h + 0.060 * h * h;
            default: return 0.0;
        }
    }

    void makeHarmonicModes(double note) noexcept
    {
        const double treble = std::clamp((note - 48.0) / 40.0, 0.0, 1.6);
        const double stiffness = model == piano ? 0.000045 + 0.00020 * treble * treble
                               : model == steel ? 0.000012
                               : model == harpsichord ? 0.000025 : 0.0;
        for (int harmonic = 1; harmonic <= 24; ++harmonic)
        {
            const double h = static_cast<double>(harmonic);
            const double ratio = h * std::sqrt((1.0 + stiffness * h * h) / (1.0 + stiffness));
            const double dark = stringWeight(harmonic, 0.0), bright = stringWeight(harmonic, 1.0);
            if (dark == 0.0 && bright == 0.0) continue;
            const double phase = model == organ ? 0.11 * harmonic
                               : model == harpsichord ? 0.0 : 0.075 * randomBipolar();
            addMode(ratio, dark, bright, stringLoss(harmonic), phase);
        }
        if (model == piano && note > 31.0)
        {
            // Low bass single strings become coupled courses above the bass.
            // The doublet keeps a slowly beating aftersound as the prompt
            // high modes decay.  Each strike has a bounded private variation.
            for (int harmonic = 1; harmonic <= 6; ++harmonic)
            {
                const double h = static_cast<double>(harmonic);
                const double cents = (1.15 + 0.35 * randomBipolar()) * (harmonic % 2 ? 1.0 : -1.0);
                const double ratio = h * std::sqrt((1.0 + stiffness * h * h) / (1.0 + stiffness))
                                   * std::exp2(cents / 1200.0);
                const double course = note < 48.0 ? 0.24 : 0.34;
                addMode(ratio, course * stringWeight(harmonic, 0.0), course * stringWeight(harmonic, 1.0),
                        0.55 * stringLoss(harmonic), 0.12 * randomBipolar());
            }
        }
    }

    void makeTineModes() noexcept
    {
        // Fundamental + pickup odd harmonics, with a brief inharmonic tine
        // bending component.  A reed model uses a separate dense even/odd law.
        const double edge = 0.20 + 0.80 * strike * strike;
        addMode(1.0, 1.0, 1.0, 0.018);
        addMode(2.0, 0.012, 0.025 * edge, 1.8);
        addMode(3.0, 0.105 * edge, 0.62 * edge, 2.0);
        addMode(5.0, 0.020 * edge, 0.17 * edge, 4.5);
        addMode(7.0, 0.004 * edge, 0.055 * edge, 8.0);
        addMode(9.0, 0.0, 0.009 * edge, 11.0);
        addMode(6.267, 0.008 * edge, 0.062 * edge, 10.0);
        addMode(17.55, 0.0, 0.012 * edge, 22.0);
        addMode(1.00048, 0.07, 0.07, 0.035, 0.08);
    }

    void makeBarModes() noexcept
    {
        // Tuned bars do not share a harmonic series.  Vibraphone and marimba
        // intentionally emphasize the tuned fourth, while bell/celesta use
        // their own inharmonic profiles.  Higher modes decay independently.
        constexpr double bellRatios[] = { 1.0, 2.756, 5.404, 8.933, 13.34, 18.64, 24.82, 31.87 };
        constexpr double vibesRatios[] = { 1.0, 4.0, 9.86, 17.22, 26.52, 37.8 };
        constexpr double marimbaRatios[] = { 1.0, 4.0, 10.0, 18.0, 28.4, 40.5 };
        constexpr double celestaRatios[] = { 1.0, 2.76, 5.40, 8.93, 13.36, 18.62, 25.0, 32.0 };
        const double* ratios = model == bell ? bellRatios : model == vibes ? vibesRatios
                             : model == marimba ? marimbaRatios : celestaRatios;
        const int number = model == bell || model == celesta ? 8 : 6;
        const double edge = 0.30 + 0.70 * strike;
        for (int i = 0; i < number; ++i)
        {
            const double index = static_cast<double>(i);
            double dark = i == 0 ? 1.0 : std::exp(-1.35 * index) * edge;
            double bright = i == 0 ? 1.0 : std::exp(-0.75 * index) * edge;
            double loss = i == 0 ? 0.018 : 0.65 * index + 0.35 * index * index;
            if (model == vibes)
            {
                dark = i == 0 ? 1.0 : (i == 1 ? 0.045 : 0.010 / index) * edge;
                bright = i == 0 ? 1.0 : (i == 1 ? 0.17 : 0.040 / index) * edge;
                loss = i == 0 ? 0.012 : 0.90 * index + 0.60 * index * index;
            }
            else if (model == marimba)
            {
                dark = i == 0 ? 1.0 : std::exp(-1.95 * index) * edge;
                bright = i == 0 ? 1.0 : std::exp(-1.05 * index) * edge;
                loss = i == 0 ? 0.10 : 3.0 * index + 2.0 * index * index;
            }
            else if (model == celesta)
            {
                dark *= i == 0 ? 1.0 : 0.65;
                loss = i == 0 ? 0.035 : 1.2 * index + 0.4 * index * index;
            }
            addMode(ratios[i], dark, bright, loss, 0.045 * randomBipolar());
        }
        if (model == bell || model == celesta)
            addMode(1.0012, 0.075, 0.075, 0.035, 0.09);
    }

    void updateControls(double frequency, float brightness, float pressure) noexcept
    {
        frequency = std::clamp(frequency, 0.01, 0.49 * sampleRate);
        const double pressureColour = model == brass || model == oboe || model == bassoon ? 0.30
                                    : model == bowed || model == clarinet || model == flute ? 0.18 : 0.0;
        const double colour = std::clamp(finiteClamp(brightness, 0.0, 1.0, initialColour)
                            + pressureColour * finiteClamp(pressure, 0.0, 1.0, 0.0), 0.0, 1.0);
        const bool pitchChanged = frequency != lastFrequency;
        const bool colourChanged = colour != lastColour;
        const bool exactCoefficients = lastFrequency < 0.0 || (++coefficientRefresh & 127u) == 0u;
        for (int i = 0; i < count; ++i)
        {
            auto& m = modes[static_cast<std::size_t>(i)];
            if (pitchChanged)
            {
                const double modeFrequency = frequency * m.ratio;
                const double angle = twoPi * std::min(modeFrequency, 0.49 * sampleRate) / sampleRate;
                const double previousAngle = twoPi * std::min(lastFrequency * m.ratio, 0.49 * sampleRate) / sampleRate;
                const double difference = angle - previousAngle;
                if (exactCoefficients || std::abs(difference) >= 0.125)
                {
                    m.cosine = std::cos(angle);
                    m.sine = std::sin(angle);
                }
                else
                {
                    // Typical vibrato/drift changes the rotation by much less
                    // than 0.125 rad.  A small-angle rotation avoids a sin/cos
                    // pair for every mode on every control tick.  Periodic
                    // exact refreshes prevent cumulative coefficient error.
                    const double d2 = difference * difference;
                    const double ds = difference * (1.0 + d2 * (-1.0 / 6.0 + d2 * (1.0 / 120.0 - d2 / 5040.0)));
                    const double dc = 1.0 + d2 * (-0.5 + d2 * (1.0 / 24.0 - d2 / 720.0));
                    const double real = m.cosine * dc - m.sine * ds;
                    m.sine = m.sine * dc + m.cosine * ds;
                    m.cosine = real;
                }
            }
            if (pitchChanged || colourChanged)
            {
                const double band = frequency * m.ratio / outputRate;
                const double t = std::clamp((0.465 - band) / 0.085, 0.0, 1.0);
                const double antialias = t * t * (3.0 - 2.0 * t);
                const double target = (m.dark + colour * (m.bright - m.dark)) * antialias;
                m.gainStep = (target - m.gain) / controlPeriod;
            }
            else m.gainStep = 0.0;
        }
        lastFrequency = frequency;
        lastColour = colour;
    }
};

} // namespace cs80::body
