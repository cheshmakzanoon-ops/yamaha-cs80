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
// Sustained bodies use authored effort-dependent spectra and partial onsets.
// The distinction between the clarinet's registers and between a string's
// pitch and its body's resonances follows the qualitative acoustics at:
// https://www.phys.unsw.edu.au/jw/clarinetacoustics.html
// https://newt.phys.unsw.edu.au/jw/articulations.html
// https://euphonics.org/5-3-signature-modes-and-formants/
// The numerical formants, onset times and motion amounts below are voicing
// choices, not a fit to a measured performer or instrument.
//
// All storage is fixed.  Expensive coefficient work happens at note onset or
// a 16-sample control boundary.  Each audible mode uses a double-precision
// complex rotation; no trigonometry, allocation or locks occur in its sample
// loop.  Host-rate Nyquist, not the oversampled internal rate, bounds the modes.

#include "InstrumentBodyVariants.h"
#include "InstrumentBodyMaterials.h"
#include "ElectricPickup.h"
#include "PianoHammer.h"
#include "InstrumentDamper.h"

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
static_assert(organ == 15 && vibes == 17 && marimba == 18,
              "saved material variants retain their published model identities");
static_assert(tine == 2 && nylon == 6 && steel == 7 && bell == 16 && celesta == 19,
              "material specializations retain their published model identities");

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
        // White draws have fixed variance per sample. Preserve the authored
        // 96 kHz texture energy when host rate or oversampling changes the
        // filter bandwidth in samples. Flutter has its own 19 Hz low-pass;
        // scaling their shared input would apply the wrong gain to it.
        constexpr double referenceRate = 96000.0;
        const double referenceLow = 1.0 - std::exp(-twoPi * 5200.0 / referenceRate);
        const double referenceHigh = 1.0 - std::exp(-twoPi * 650.0 / referenceRate);
        const double referenceFlutter = 1.0 - std::exp(-twoPi * 19.0 / referenceRate);
        airNoiseGain = sampleRate == referenceRate ? 1.0
            : std::sqrt(airNoiseVariance(referenceLow, referenceHigh)
                        / airNoiseVariance(airLowCoefficient, airHighCoefficient));
        flutterDepth = sampleRate == referenceRate ? 0.012
            : 0.012 * std::sqrt((referenceFlutter / (2.0 - referenceFlutter))
                                / (flutterCoefficient / (2.0 - flutterCoefficient)));
        effortCoefficient = 1.0 - std::exp(-controlPeriod / (sampleRate * 0.012));
        motionCoefficient = 1.0 - std::exp(-twoPi * 0.9 * controlPeriod / sampleRate);
        reset();
    }

    void reset() noexcept
    {
        modes = {};
        pianoHammer.reset();
        count = 0;
        model = off;
        variant = standard;
        material = defaultMaterial;
        controlCountdown = 0;
        normalizationCountdown = 2048;
        coefficientRefresh = 0;
        lastFrequency = -1.0;
        lastColour = -1.0;
        airLow = airHigh = flutter = 0.0;
        onsetMissing = 1.0;
        hitEnvelope = 0.0;
        airAmount = hitAmount = 0.0;
        pickupMotion = 0.0;
        damperStrength = damperSmoothing = 0.0;
        damperSeconds = -1.0;
        pickupReleaseLoss = pickupReleaseTarget = 1.0;
        released = damperMoving = false;
        liveEffort = motionTarget = motionLow = motionSmooth = 0.0;
        motionCountdown = 0;
        expressive = false;
        transientControls = false;
        gainRamping = false;
        state = 0x7a4f39b1u;
    }

    // brightness is normalized [0,1]; decay scales relative partial losses.
    // Overall amplitude/decay still comes from the external rank ADSR. The
    // supported key dampers add only release-colour changes after noteOff.
    void noteOn(int selectedModel, float midiNote, float velocity, float brightness,
                float decay, std::uint32_t seed, float size = 1.0f,
                int selectedVariant = standard, int selectedMaterial = defaultMaterial) noexcept
    {
        reset();
        model = std::clamp(selectedModel, 0, maxModel);
        variant = compatibleVariant(model, selectedVariant);
        material = compatibleMaterial(model, selectedMaterial);
        if (model == off) return;

        const double note = finiteClamp(midiNote, 0.0, 127.0, 60.0);
        keyHz = 440.0 * std::exp2((note - 69.0) / 12.0);
        strike = finiteClamp(velocity, 0.0, 1.0, 0.7);
        initialColour = finiteClamp(brightness, 0.0, 1.0, 0.5);
        decayScale = finiteClamp(decay, 0.25, 4.0, 1.0);
        bodySize = finiteClamp(size, 0.5, 2.5, 1.0);
        const auto damper = captureDamper(model == piano ? DamperFamily::piano
            : model == tine && material != kalimba ? DamperFamily::tine
            : model == reed ? DamperFamily::reed : DamperFamily::none, note);
        damperStrength = damper.strength;
        if (damperStrength > 0.0)
            damperSmoothing = 1.0 - std::exp(-controlPeriod / (sampleRate * damper.contactSeconds));
        // A quill and an organ key do not have a piano hammer's velocity law.
        // Fixed phases also prevent caller seed conventions from leaking
        // velocity into the deliberately touch-independent registrations.
        if ((model == organ && !vocal()) || model == harpsichord)
        {
            strike = 0.7;
            seed = 0x43733830u;
        }
        state = seed != 0 ? seed : 0x7a4f39b1u;
        motionState = state ^ 0x9e3779b9u;
        if (motionState == 0) motionState = 0x613c47a9u;
        // Small excitation differences belong to a strike, never to the
        // instrument's tuning. The seed remains independent of host blocks.
        contactVariation = 1.0 + 0.018 * randomBipolar();
        pluckOffset = 0.006 * randomBipolar();

        // An EP pickup's harmonic output follows the decreasing excursion of
        // its tine/reed. This bounded additive approximation avoids generating
        // unbandlimited products with a sample-domain distortion function.
        pickupMotion = (model == tine && material != kalimba) || model == reed ? 1.0 : 0.0;
        if (pickupMotion > 0.0) electricPickup.configure(model == reed, bodySize, strike);
        const double pickupRate = (model == tine ? 0.72 : 1.05) * registerLossScale(0.35);
        pickupLoss = std::exp(-controlPeriod * pickupRate / (sampleRate * decayScale));

        sustained = model >= bowed && model <= organ;
        expressive = (model >= bowed && model <= brass) || vocal();
        // Equivalent fingering register follows the bore length; a bass
        // clarinet reaches the corresponding timbre transition an octave
        // below the standard-size body. Smoothstep avoids a split-key seam.
        const double equivalentNote = note + 12.0 * std::log2(bodySize);
        const double registerPosition = std::clamp((equivalentNote - 62.0) / 7.0, 0.0, 1.0);
        clarinetRegister = registerPosition * registerPosition * (3.0 - 2.0 * registerPosition);
        bowedMiddleScale = std::pow(bodySize, 0.65);
        bowedBridgeScale = std::pow(bodySize, 0.40);
        const double attack = vocal() ? 0.008 + 0.012 * (1.0 - strike)
                            : model == bowed ? 0.0045
                            : model == brass ? 0.0023
                            : model == flute ? 0.0028 : 0.00055;
        onsetCoefficient = std::exp(-1.0 / (sampleRate * attack));
        hitCoefficient = std::exp(-1.0 / (sampleRate * hitSeconds()));
        hitEnvelope = 1.0;
        configureExcitation();
        if (model == piano)
            pianoHammer.noteOn(sampleRate, outputRate, keyHz, strike, bodySize);

        double maximumSum = 0.0;
        if (material >= tubular && material <= kalimba)
            maximumSum = makeMaterialModes();
        else if (model == bell || model == vibes || model == marimba || model == celesta)
            maximumSum = makeBarModes();
        else if (model == tine || model == reed)
            maximumSum = makeElectricModes();
        else
            maximumSum = makeHarmonicModes(note);

        transientControls = model != organ || vocal();

        // Bound the sum of absolute mode amplitudes at every colour setting.
        // This is an authored excitation level, not a hidden output limiter or
        // post-render normalization.  It does not grow as upper modes decay.
        // Every touch-sensitive struck body supplies its full-strike bound.
        // Normalizing a quieter strike by its smaller spectral sum would
        // boost its fundamental and compress the intended touch response.
        // Reference weights share the actual note, geometry and mode coverage;
        // they never consume random draws or rebuild the oscillator bank.
        // Sustained bodies and fixed-touch quills retain their authored sum.
        // The dense gong bank has a lower crest factor than the sparse bars.
        // Its authored 0.80 modal ceiling leaves room for the largest 0.017
        // contact impulse, while retaining the same full-strike denominator
        // and therefore the complete soft-to-hard excitation response.
        const double excitationCeiling = material == gong ? 0.80 : 0.72;
        const double scale = excitationCeiling / std::max(maximumSum, 1.0e-12);
        for (int i = 0; i < count; ++i)
        {
            auto& m = modes[static_cast<std::size_t>(i)];
            m.dark *= scale;
            m.bright *= scale;
        }
        if (tracksFormants())
        {
            bowedResponseBound = 0.0;
            bowedBoundGain = 1.0;
            for (int i = 0; i < count; ++i)
            {
                const auto index = static_cast<std::size_t>(i);
                const auto& m = modes[index];
                bowedResponseInverse[index] = 1.0 / fixedFormant(keyHz * m.ratio);
                bowedResponseGain[index] = 1.0;
                bowedResponseBound += std::max(m.dark, m.bright);
            }
        }
    }

    bool enabled() const noexcept { return model != off; }

    // Only the engine's accepted gate release calls this. CC64-held keys keep
    // their original modal/pickup losses until the existing pedal policy lets
    // the gate go. Repeated releases retime without restarting contact.
    void noteOff(float releaseSeconds) noexcept
    {
        if (damperStrength <= 0.0) return;
        released = true;
        setRelease(releaseSeconds);
    }

    // Follow live rank/master/key-release timing without rebuilding a body,
    // changing phase, restoring lost energy or overriding its amplitude VCA.
    void setRelease(float releaseSeconds) noexcept
    {
        if (!released) return;
        const double seconds = damperReleaseSeconds(releaseSeconds);
        if (seconds == damperSeconds) return;
        damperSeconds = seconds;
        if (model == piano)
        {
            for (int i = 0; i < count; ++i)
            {
                auto& mode = modes[static_cast<std::size_t>(i)];
                mode.dampedLoss = mode.naturalLoss
                    * damperLoss(damperStrength * pianoDamperWeight(mode.ratio), seconds, 1.0 / sampleRate);
            }
        }
        else
            pickupReleaseTarget = damperLoss(damperStrength, seconds, controlPeriod / sampleRate);
        damperMoving = true;
    }

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
            output *= 1.0 + (sustained ? flutterDepth * flutter : 0.0);
            output += (airLow * airNoiseGain) * (airAmount + hitAmount * hitEnvelope);
        }
        hitEnvelope *= hitCoefficient;
        if (hitEnvelope < 1.0e-20) hitEnvelope = 0.0;
        if (model == piano)
        {
            const auto contact = pianoHammer.tick();
            return static_cast<float>(output * contact.onset + contact.board);
        }
        onsetMissing *= onsetCoefficient;
        return static_cast<float>(output * (1.0 - onsetMissing));
    }

private:
    static constexpr double pi = 3.1415926535897932384626433832795;
    static constexpr double twoPi = 2.0 * pi;
    static constexpr int maximumModes = 48;
    static constexpr int controlPeriod = 16;
    static constexpr int harmonicModeCount = 24;

    struct Mode
    {
        double ratio = 1.0;
        double dark = 0.0, bright = 0.0;
        double real = 1.0, imaginary = 0.0;
        double cosine = 1.0, sine = 0.0;
        double envelope = 1.0, loss = 1.0;
        double naturalLoss = 1.0, dampedLoss = 1.0;
        double gain = 0.0, gainStep = 0.0;
        // Partial-specific onset is evaluated at the control boundary and
        // interpolated by the existing gain ramp, with no extra sample loop.
        double onsetMissing = 0.0, onsetLoss = 0.0;
        unsigned int pickupPower = 0; // EP harmonic index; zero for mechanical modes.
    };

    std::array<Mode, maximumModes> modes {};
    PianoHammer pianoHammer;
    // Fixed-formant history covers the full bank: low choir notes can use
    // 48 modes. Keep it outside Mode rather than enlarging every oscillator.
    std::array<double, maximumModes> bowedResponseInverse {}, bowedResponseGain {};
    double bowedResponseBound = 0.0, bowedBoundGain = 1.0;
    double sampleRate = 96000.0, outputRate = 48000.0;
    double keyHz = 261.625565, strike = 0.7, initialColour = 0.5, decayScale = 1.0;
    double bodySize = 1.0, contactVariation = 1.0, pluckOffset = 0.0;
    double pickupMotion = 0.0, pickupLoss = 1.0;
    ElectricPickup electricPickup;
    double damperStrength = 0.0, damperSmoothing = 0.0, damperSeconds = -1.0;
    double pickupReleaseLoss = 1.0, pickupReleaseTarget = 1.0;
    double clarinetRegister = 0.0, bowedMiddleScale = 1.0, bowedBridgeScale = 1.0;
    double liveEffort = 0.0, effortCoefficient = 0.014;
    double motionTarget = 0.0, motionLow = 0.0, motionSmooth = 0.0, motionCoefficient = 0.001;
    double lastFrequency = -1.0, lastColour = -1.0;
    double onsetMissing = 1.0, onsetCoefficient = 0.98;
    double hitEnvelope = 0.0, hitCoefficient = 0.99;
    double airLow = 0.0, airHigh = 0.0, flutter = 0.0;
    double airLowCoefficient = 0.3, airHighCoefficient = 0.04, flutterCoefficient = 0.001;
    double airNoiseGain = 1.0, flutterDepth = 0.012;
    double airAmount = 0.0, hitAmount = 0.0;
    std::uint32_t state = 0x7a4f39b1u;
    std::uint32_t motionState = 0x613c47a9u;
    int model = off, variant = standard, material = defaultMaterial;
    int count = 0, controlCountdown = 0, normalizationCountdown = 2048;
    int motionCountdown = 0;
    unsigned int coefficientRefresh = 0;
    bool sustained = false;
    bool expressive = false;
    bool transientControls = false, gainRamping = false;
    bool released = false, damperMoving = false;

    bool vocal() const noexcept { return variant == choirAh || variant == choirOo; }

    static double airNoiseVariance(double low, double high) noexcept
    {
        // The high-pass uses the *updated* high-pole state. With r = 1-a,
        // H(z) = aL*rH*(1-z^-1)/((1-rH*z^-1)*(1-rL*z^-1)). Summing its
        // squared impulse response gives 2*aL^2*rH^2 /
        // ((1+rH)*(1+rL)*(1-rH*rL)). This coefficient form avoids subtracting
        // two nearly unit pole products and has no coincident-pole division.
        // The generator's common input variance cancels in the gain ratio.
        const double highPole = 1.0 - high;
        return 2.0 * low * low * highPole * highPole
             / ((2.0 - high) * (2.0 - low) * (high + low - high * low));
    }

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

    double randomMotion() noexcept
    {
        // Independent from the audio-rate air stream: changing sample rate
        // or a texture amount cannot turn timbre motion into a different
        // periodic LFO, and host block boundaries never enter either stream.
        motionState ^= motionState << 13;
        motionState ^= motionState >> 17;
        motionState ^= motionState << 5;
        return static_cast<double>(motionState >> 8) * (2.0 / 16777216.0) - 1.0;
    }

    static double resonance(double frequency, double center, double width) noexcept
    {
        const double distance = (frequency - center) / width;
        return 1.0 / (1.0 + distance * distance);
    }

    double bodyResonance(double frequency, double center, double width) const noexcept
    {
        // A larger resonating structure lowers its broad spectral features,
        // while the played pitch and modal ratios remain unchanged.
        return resonance(frequency * bodySize, center, width);
    }

    double registerLossScale(double exponent) const noexcept
    {
        return std::clamp(std::pow(keyHz / 261.625565, exponent), 0.55, 2.4);
    }

    double hitSeconds() const noexcept
    {
        switch (material)
        {
            case tubular: return 0.0028 - 0.0010 * strike;
            case gong: return 0.022 - 0.008 * strike;
            case temple: return 0.0070 - 0.0020 * strike;
            case agogo: return 0.0012;
            case musicBox: return 0.0009;
            case kalimba: return 0.0035;
            default: break;
        }
        if (variant == xylophone) return 0.0016 - 0.0006 * strike;
        if (variant == steelpan) return 0.0034 - 0.0008 * strike;
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
        switch (material)
        {
            case tubular: hitAmount = 0.006 + 0.009 * strike; return;
            case gong: hitAmount = 0.004 + 0.013 * strike * strike; return;
            case temple: hitAmount = 0.003 + 0.005 * strike; return;
            case agogo: hitAmount = 0.010 + 0.013 * strike; return;
            case musicBox: hitAmount = 0.004 + 0.003 * strike; return;
            case kalimba: hitAmount = 0.009 + 0.008 * strike; return;
            default: break;
        }
        if (vocal())
        {
            // Quiet turbulent air under a voiced source, with no hammer
            // transient or pitch jitter. The existing band-shaped path keeps
            // this breath below the harmonic vowel rather than adding hiss.
            airAmount = 0.0012 + 0.0008 * (1.0 - strike);
            return;
        }
        if (variant == xylophone)
        {
            hitAmount = 0.010 + 0.015 * strike;
            return;
        }
        if (variant == steelpan)
        {
            hitAmount = 0.006 + 0.010 * strike;
            return;
        }
        switch (model)
        {
            // Piano contact now drives its finite hammer/board response.
            case piano: break;
            case tine: hitAmount = 0.003 + 0.006 * strike; break;
            case reed: hitAmount = 0.004 + 0.007 * strike; break;
            case clav: hitAmount = 0.014; break;
            case harpsichord: hitAmount = 0.012; break;
            case nylon: hitAmount = 0.008 + 0.007 * strike; break;
            case steel: hitAmount = 0.013 + 0.012 * strike; break;
            case bass: hitAmount = 0.012 + 0.008 * strike; break;
            case bowed: airAmount = 0.0018 + 0.0007 * strike; hitAmount = 0.004 + 0.005 * strike; break;
            case flute: airAmount = 0.0033 + 0.0012 * strike; hitAmount = 0.003 + 0.005 * strike; break;
            case clarinet: airAmount = 0.0014 + 0.0006 * strike; hitAmount = 0.002 + 0.003 * strike; break;
            case oboe: case bassoon: airAmount = 0.0017 + 0.0007 * strike; hitAmount = 0.0025 + 0.003 * strike; break;
            case brass: airAmount = 0.0009 + 0.0004 * strike; hitAmount = 0.002 + 0.004 * strike; break;
            case organ: break;
            case marimba: hitAmount = 0.010 + 0.008 * strike; break;
            default: hitAmount = 0.004 + 0.006 * strike; break;
        }
    }

    void addMode(double ratio, double darkWeight, double brightWeight, double lossPerSecond,
                 double phase = 0.0, double onsetSeconds = 0.0, unsigned int pickupPower = 0) noexcept
    {
        if (count >= maximumModes) return;
        auto& m = modes[static_cast<std::size_t>(count++)];
        m.ratio = ratio;
        m.dark = std::max(darkWeight, 0.0);
        m.bright = std::max(brightWeight, 0.0);
        m.real = std::cos(phase);
        m.imaginary = std::sin(phase);
        // The control primarily changes overtone persistence. A weaker
        // fundamental dependence keeps it from acting as a second volume/ADSR
        // knob, while the authored register law still damps the whole body.
        const double lossScale = ratio < 1.05 ? std::pow(decayScale, 0.35) : decayScale;
        m.loss = std::exp(-std::max(lossPerSecond, 0.0) / (sampleRate * lossScale));
        m.naturalLoss = m.dampedLoss = m.loss;
        m.onsetMissing = onsetSeconds > 0.0 ? 1.0 : 0.0;
        m.onsetLoss = onsetSeconds > 0.0
            ? std::exp(-controlPeriod / (sampleRate * onsetSeconds * contactVariation)) : 0.0;
        m.pickupPower = pickupPower;
    }

    double bowedFormant(double frequency) const noexcept
    {
        // The body stays the same size during vibrato or a slide. Moving
        // string harmonics cross its response instead of transposing it.
        // https://www.phys.unsw.edu.au/jw/violinarticulation.html
        // These broad formants remain authored profiles, not a measured violin.
        return 0.32 + 0.62 * bodyResonance(frequency, 460.0, 340.0)
             + 0.48 * resonance(frequency * bowedMiddleScale, 1600.0, 700.0)
             + 0.72 * resonance(frequency * bowedBridgeScale, 2900.0, 1000.0);
    }

    double vocalFormant(double frequency) const noexcept
    {
        // The vocal folds set harmonic pitch; tract resonances shape the
        // vowel in Hz. They must not transpose with a keyboard note or bend.
        // Source/filter distinction and vowel identity through R1/R2:
        // https://www.phys.unsw.edu.au/jw/voice.html
        // These broad ah/oo profiles, widths and higher clusters are authored
        // singing colours, not a fit to a measured singer or phonetic corpus.
        if (variant == choirOo)
            return 0.012 + 1.00 * bodyResonance(frequency, 330.0, 75.0)
                         + 0.55 * bodyResonance(frequency, 850.0, 115.0)
                         + 0.10 * bodyResonance(frequency, 2400.0, 280.0)
                         + 0.045 * bodyResonance(frequency, 3200.0, 420.0);
        return 0.012 + 1.00 * bodyResonance(frequency, 760.0, 130.0)
                     + 0.82 * bodyResonance(frequency, 1180.0, 160.0)
                     + 0.28 * bodyResonance(frequency, 2800.0, 340.0)
                     + 0.12 * bodyResonance(frequency, 3500.0, 460.0);
    }

    double fixedFormant(double frequency) const noexcept
    {
        if (material == banjo || material == koto || material == harp)
            return pluckedMaterialResponse(frequency);
        // Bore/bell radiation and broad body resonances stay in Hz while a
        // performer bends or adds vibrato. The accepted fingering/register
        // and excitation remain captured; only the radiation response moves
        // across the sounding harmonics. As with the existing bowed/voice
        // profiles, these coefficients are authored, not measured spectra.
        // Radiation/cutoff background: https://www.phys.unsw.edu.au/jw/cutoff.html
        // https://www.phys.unsw.edu.au/jw/brassacoustics.html
        switch (model)
        {
            // The captured string excitation crosses the same soundboard/body
            // response during a bend. Geometry stays fixed; pluck position,
            // stiffness and losses remain those of the original note.
            // Coupling background (not a fit to measured response data):
            // https://www.phys.unsw.edu.au/music/guitaracoustics/construction.html
            case clav: return 0.70 + 0.35 * bodyResonance(frequency, 270.0, 220.0)
                                  + 0.18 * bodyResonance(frequency, 1400.0, 900.0);
            case harpsichord: return 0.72 + 0.30 * bodyResonance(frequency, 300.0, 210.0)
                                         + 0.23 * bodyResonance(frequency, 1700.0, 1000.0);
            case nylon: case steel: case bass:
                return 0.65 + 0.45 * bodyResonance(frequency, model == bass ? 130.0 : 210.0, 130.0)
                            + 0.25 * bodyResonance(frequency, 620.0, 370.0);
            case flute: return 0.75 + 0.35 * bodyResonance(frequency, 1800.0, 1700.0);
            case clarinet: return 0.82 + 0.28 * bodyResonance(frequency, 1500.0, 900.0);
            case oboe: return 0.28 + 1.65 * bodyResonance(frequency, 1300.0, 650.0)
                                  + 0.75 * bodyResonance(frequency, 2800.0, 1000.0);
            case bassoon: return 0.34 + 1.25 * bodyResonance(frequency, 500.0, 280.0)
                                     + 0.80 * bodyResonance(frequency, 1500.0, 700.0);
            case brass: return 0.55 + 0.65 * bodyResonance(frequency, 1250.0, 1100.0);
            default: return vocal() ? vocalFormant(frequency) : bowedFormant(frequency);
        }
    }

    bool tracksFormants() const noexcept
    {
        return (model >= bowed && model <= brass) || vocal()
            || (model >= clav && model <= bass)
            || material == banjo || material == koto || material == harp;
    }

    double pluckedMaterialResponse(double frequency) const noexcept
    {
        // One captured body keeps its resonances in Hz during a string bend.
        // The same function shapes onset weights and the existing bounded
        // live-pitch correction; standard nylon/steel/bass never call it.
        if (material == banjo)
            return 0.23 + 1.85 * bodyResonance(frequency, 650.0, 235.0)
                        + 0.68 * bodyResonance(frequency, 2850.0, 1050.0);
        if (material == koto)
            return 0.45 + 0.35 * bodyResonance(frequency, 85.0, 38.0)
                        + 0.65 * bodyResonance(frequency, 110.0, 65.0)
                        + 0.62 * bodyResonance(frequency, 510.0, 230.0)
                        + 0.48 * bodyResonance(frequency, 1850.0, 850.0);
        return 0.58 + 0.74 * bodyResonance(frequency, 190.0, 160.0)
                    + 0.32 * bodyResonance(frequency, 540.0, 340.0)
                    + 0.12 * bodyResonance(frequency, 1450.0, 850.0);
    }

    double pluckedMaterialWeight(int harmonic, double colour, double excitation) const noexcept
    {
        const double h = static_cast<double>(harmonic);
        const double frequency = keyHz * h;
        // Finger/pick displacement changes the energy of the whole string,
        // including its fundamental. The existing full-strike denominator
        // preserves that reduction instead of normalizing soft notes upward.
        const double displacement = 0.40 + 0.60 * excitation;
        // Banjo identity is strongly affected by membrane/bridge formants and
        // frequency-dependent string losses, not just a nasal low-pass:
        // Woodhouse, Politzer & Mansour, Acta Acustica 5, 15 (2021),
        // https://doi.org/10.1051/aacus/2021009 . Their measured instrument's
        // broad low formant lay around 500--800 Hz. Our centers, widths and
        // gains below are authored approximations, not their fitted model.
        if (material == banjo)
        {
            const double position = 0.115 + pluckOffset;
            const double comb = std::abs(std::sin(pi * h * position)) / std::sin(pi * position);
            const double membrane = pluckedMaterialResponse(frequency);
            return displacement * membrane * comb * std::pow(h, -1.08)
                 * std::exp(-(h - 1.0) / (4.0 + 14.0 * colour + 6.0 * excitation));
        }
        if (material == koto)
        {
            // A near-bridge plectrum excites a broad notched string spectrum.
            // The koto is a long paulownia body with individually movable
            // bridges, not a steel-string guitar. The accepted model slot is
            // only a stable saved ID. Coaldrake's measured CT study identifies
            // low air/body modes near 85/100 Hz:
            // https://pub.dega-akustik.de/ICA2019/data/articles/001444.pdf
            // The higher clusters and all losses here are voicing choices.
            const double position = 0.075 + 0.60 * pluckOffset;
            const double comb = std::abs(std::sin(pi * h * position)) / std::sin(pi * position);
            const double board = pluckedMaterialResponse(frequency);
            return displacement * board * comb * std::pow(h, -1.13)
                 * std::exp(-(h - 1.0) / (3.0 + 13.0 * colour + 7.0 * excitation));
        }
        // Harp finger width and plucking point affect the string spectrum:
        // Chadefaux et al., JASA 131, 844--855 (2012),
        // https://doi.org/10.1121/1.3651246 . A finite contact bandwidth and
        // a rounded low soundboard response distinguish this authored profile
        // from the shorter guitar-like nylon body. It is not a nonlinear
        // harp model and does not claim phantom or sympathetic partials.
        const double position = 0.325 + 0.50 * pluckOffset;
        const double comb = std::abs(std::sin(pi * h * position)) / std::sin(pi * position);
        const double finger = 1.0 / (1.0 + std::pow(h / (5.0 + 7.0 * excitation + 9.0 * colour), 2.0));
        const double board = pluckedMaterialResponse(frequency);
        return displacement * board * comb * std::pow(h, -1.53) * finger
             * std::exp(-(h - 1.0) / (4.0 + 10.0 * colour + 4.0 * excitation));
    }

    double stringWeight(int harmonic, double colour) const noexcept
    {
        return stringWeight(harmonic, colour, strike);
    }

    struct PianoStringContact
    {
        double blend = 0.0, seconds = 0.0021;
    };

    PianoStringContact pianoStringContact(double excitation) const noexcept
    {
        if (model != piano) return {};
        const double position = std::clamp(std::log2(keyHz / 261.625565), 0.0, 1.0);
        const double blend = position * position * (3.0 - 2.0 * position);
        if (blend == 0.0) return {};
        // Same authored duration trend as PianoHammer, evaluated for both
        // the played excitation and the full-strike normalization reference.
        const double seconds = std::clamp(0.0021 * std::pow(261.625565 / keyHz, 0.20)
            * std::pow((0.20 + excitation) / 0.90, -0.38), 0.00065, 0.0055);
        return {blend, seconds};
    }

    double stringWeight(int harmonic, double colour, double excitation) const noexcept
    {
        return stringWeight(harmonic, colour, excitation, pianoStringContact(excitation));
    }

    double stringWeight(int harmonic, double colour, double excitation,
                        const PianoStringContact& pianoContact) const noexcept
    {
        const double h = static_cast<double>(harmonic);
        const double frequency = keyHz * h;
        switch (model)
        {
            case piano:
            {
                // Preserve the established bass harmonic envelope. The
                // finite-Hz correction below only enters above middle C.
                const double bassBreadth = std::clamp(std::sqrt(261.625565 / keyHz), 1.0, 1.75);
                const double hardness = (2.1 + 15.0 * excitation * excitation + 8.0 * colour) * bassBreadth;
                const double position = 0.116 + 0.007 * initialColour;
                const double comb = std::abs(std::sin(pi * h * position)) / std::sin(pi * position);
                // Hammer contact suppresses high modes at gentle strikes.
                const double contact = 1.0 / (1.0 + std::pow(h / hardness, 2.8));
                const double board = 0.65 + 0.28 * bodyResonance(frequency, 280.0, 220.0)
                                          + 0.30 * bodyResonance(frequency, 2300.0, 1500.0);
                // Contact duration falls more slowly than string period,
                // so treble notes excite fewer relative harmonics:
                // https://www.speech.kth.se/music/5_lectures/askenflt/stricont.html
                // This monotone low-pass is an authored spectral surrogate,
                // not a measured hammer transfer or a force convolution.
                // Fade it in over C4--C5; all lower keys retain exact weights.
                const double legacyWeight = std::pow(h, -1.15) * comb * contact * board;
                if (pianoContact.blend == 0.0) return legacyWeight;
                const double bandwidthHz = (0.90 + 0.40 * colour) / pianoContact.seconds;
                const double relativeFrequency = frequency / bandwidthHz;
                const double finiteContact = 1.0 - pianoContact.blend
                    + pianoContact.blend / (1.0 + relativeFrequency * relativeFrequency);
                // Both contact factors are nondecreasing with excitation
                // and colour, preserving the existing full-strike bound.
                return legacyWeight * finiteContact;
            }
            case reed:
            {
                // A reed pickup has much stronger even harmonics and a broad
                // midrange bark than the predominantly odd tine registration.
                const double even = harmonic % 2 == 0 ? 0.84 : 1.0;
                const double bark = 1.0 + 0.55 * excitation * bodyResonance(frequency, 1400.0, 1000.0);
                return even * bark * std::pow(h, -1.25) * std::exp(-(h - 1.0) / (2.0 + 5.0 * excitation + 8.0 * colour));
            }
            case clav:
            {
                const double comb = 0.18 + 0.82 * std::abs(std::sin(pi * h * 0.235));
                const double body = fixedFormant(frequency);
                return body * comb * std::pow(h, -0.95)
                     * std::exp(-(h - 1.0) / (4.0 + 10.0 * colour + 4.0 * excitation));
            }
            case harpsichord:
            {
                const double comb = 0.10 + 0.90 * std::abs(std::sin(pi * h * 0.168));
                const double body = fixedFormant(frequency);
                return body * comb * std::pow(h, -0.72)
                     * std::exp(-(h - 1.0) / (5.0 + 16.0 * colour));
            }
            case nylon: case steel: case bass:
            {
                if (material == banjo || material == koto || material == harp)
                    return pluckedMaterialWeight(harmonic, colour, excitation);
                const double position = (model == nylon ? 0.22 : model == bass ? 0.19 : 0.135) + pluckOffset;
                const double comb = std::abs(std::sin(pi * h * position)) / std::sin(pi * position);
                const double falloff = model == nylon ? 1.75 : model == bass ? 1.6 : 1.32;
                const double body = fixedFormant(frequency);
                return body * comb * std::pow(h, -falloff)
                     * std::exp(-(h - 1.0) / (3.0 + 12.0 * colour + 5.0 * excitation));
            }
            case bowed:
            {
                // Low signature modes scale more strongly with body size
                // than the broad upper bridge/body response. Multiplying the
                // probe frequency scales both each center and its width.
                const double formant = bowedFormant(frequency);
                return formant * std::pow(h, -0.92 - 0.25 * (1.0 - excitation))
                     * std::exp(-(h - 1.0) / (3.0 + 16.0 * colour + 8.0 * excitation));
            }
            case flute:
            {
                const double weights[] = { 1.0, 0.26, 0.085, 0.035, 0.012, 0.007 };
                const double weight = harmonic <= 6 ? weights[harmonic - 1] : 0.003 / (h - 5.0);
                const double radiation = 0.75 + 0.35 * bodyResonance(frequency, 1800.0, 1700.0);
                return weight * radiation
                     * std::exp(-(h - 1.0) * (0.04 + 0.62 * (1.0 - colour) + 0.48 * (1.0 - excitation)));
            }
            case clarinet:
            {
                const double lowEven = 0.045 + 0.15 * colour + 0.02 * excitation;
                const double highEven = 0.55 + 0.35 * colour + 0.10 * excitation;
                const double even = harmonic % 2 == 0
                    ? lowEven + clarinetRegister * (highEven - lowEven) : 1.0;
                const double radiation = 0.82 + 0.28 * bodyResonance(frequency, 1500.0, 900.0);
                return even * radiation * std::pow(h, -0.85 - 0.20 * (1.0 - excitation))
                     * std::exp(-(h - 1.0) / (3.5 + 10.0 * colour + 5.0 * excitation));
            }
            case oboe:
            {
                const double formant = 0.28 + 1.65 * bodyResonance(frequency, 1300.0, 650.0)
                                           + 0.75 * bodyResonance(frequency, 2800.0, 1000.0);
                return formant * std::pow(h, -0.77 - 0.22 * (1.0 - excitation))
                     * std::exp(-(h - 1.0) / (4.0 + 13.0 * colour + 5.0 * excitation));
            }
            case bassoon:
            {
                const double formant = 0.34 + 1.25 * bodyResonance(frequency, 500.0, 280.0)
                                           + 0.80 * bodyResonance(frequency, 1500.0, 700.0);
                return formant * std::pow(h, -0.88 - 0.25 * (1.0 - excitation))
                     * std::exp(-(h - 1.0) / (3.5 + 12.0 * colour + 4.0 * excitation));
            }
            case brass:
            {
                const double formant = 0.55 + 0.65 * bodyResonance(frequency, 1250.0, 1100.0);
                return formant * std::pow(h, -0.68 - 0.28 * (1.0 - excitation))
                     * std::exp(-(h - 1.0) / (1.7 + 18.0 * colour + 4.0 * excitation));
            }
            case organ:
            {
                if (vocal())
                {
                    // A gently declining glottal spectrum carries every
                    // harmonic through the tract, unlike a sparse organ
                    // registration. Effort changes its tilt, not its pitch.
                    const double slope = -1.45 + 0.45 * colour + 0.15 * excitation;
                    return vocalFormant(frequency) * std::pow(h, slope)
                         * std::exp(-(h - 1.0) / (10.0 + 18.0 * colour));
                }
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
        const double registerScale = registerLossScale(model == piano ? 0.45 : 0.35);
        // These are authored relative losses, not measured instrument T60s.
        // They give the excitation a register-dependent tail; the rank VCA
        // still supplies the overall musical decay and key/pedal damping.
        // Efficient radiation through the banjo head removes energy from
        // its membrane-band string harmonics early. Harp's lower aftersound
        // rings longer; the near-bridge koto strike loses its upper edge first.
        // These are positive authored decay rates, not measured T60 values.
        if (material == banjo)
            return registerScale * (0.48 + 0.35 * h + 0.035 * h * h)
                 + 1.35 * bodyResonance(keyHz * harmonic, 650.0, 260.0);
        if (material == koto)
            return registerScale * (0.17 + 0.27 * h + 0.040 * h * h)
                 + 0.13 * bodyResonance(keyHz * harmonic, 510.0, 230.0);
        if (material == harp)
            return registerScale * (0.105 + 0.16 * h + 0.024 * h * h);
        switch (model)
        {
            case piano: return registerScale * (0.12 + 0.20 * std::sqrt(h) + 0.013 * h * h)
                             + 0.00011 * keyHz * h;
            case reed: return registerScale * (0.36 + 0.38 * h + 0.055 * h * h);
            case clav: return registerScale * (0.30 + 0.46 * h + 0.045 * h * h);
            case harpsichord: return registerScale * (0.18 + 0.22 * h + 0.025 * h * h);
            case nylon: return registerScale * (0.38 + 0.38 * h + 0.048 * h * h);
            case steel: return registerScale * (0.16 + 0.21 * h + 0.027 * h * h);
            case bass: return registerScale * (0.24 + 0.46 * h + 0.060 * h * h);
            default: return 0.0;
        }
    }

    double makeHarmonicModes(double note) noexcept
    {
        const double treble = std::clamp((note - 48.0) / 40.0, 0.0, 1.6);
        const double stiffness = model == piano ? 0.000045 + 0.00020 * treble * treble
                               : model == steel ? 0.000012
                               : model == harpsichord ? 0.000025 : 0.0;
        // Bass piano attacks reserve six slots for the slower second course.
        // Choir needs more harmonics at low pitch to reach its fixed-Hz upper
        // vowel regions. Size scales that coverage with the authored tract;
        // the existing 48-mode ceiling still limits the deepest registers.
        // No modes are permanently culled by onset pitch: a subsequent large
        // downward ribbon bend can bring a formerly ultrasonic mode back.
        const double harmonicBudget = model == piano
            ? std::clamp(24.0 + 0.75 * std::max(0.0, 60.0 - note), 24.0, 42.0)
            : vocal() ? std::clamp(4400.0 / (bodySize * keyHz),
                                   static_cast<double>(harmonicModeCount), static_cast<double>(maximumModes))
                      : harmonicModeCount;
        const int harmonics = static_cast<int>(std::ceil(harmonicBudget));
        const std::uint32_t principalPhaseSeed = state;
        std::uint32_t choirAirSeed = state;
        double maximumSum = 0.0;
        const bool referenceStrike = !sustained && model != harpsichord && strike != 1.0;
        // Contact/register terms depend on the note and strike, not the
        // partial. Cache them on the note-on stack for the two excitation
        // levels rather than repeating logarithms and powers per mode.
        const auto playedContact = pianoStringContact(strike);
        const auto fullContact = referenceStrike ? pianoStringContact(1.0) : playedContact;
        const auto includeBound = [&](int harmonic, double coverage, double dark, double bright)
        {
            if (referenceStrike)
            {
                dark = coverage * stringWeight(harmonic, 0.0, 1.0, fullContact);
                bright = coverage * stringWeight(harmonic, 1.0, 1.0, fullContact);
            }
            maximumSum += std::max(dark, bright);
        };
        for (int harmonic = 1; harmonic <= harmonics; ++harmonic)
        {
            const double h = static_cast<double>(harmonic);
            const double ratio = h * std::sqrt((1.0 + stiffness * h * h) / (1.0 + stiffness));
            // Fade the final admitted partial across fractional-note tuning
            // changes instead of introducing a hard spectral budget threshold.
            const double coverage = std::clamp(harmonicBudget + 1.0 - h, 0.0, 1.0);
            const double dark = coverage * stringWeight(harmonic, 0.0, strike, playedContact);
            const double bright = coverage * stringWeight(harmonic, 1.0, strike, playedContact);
            if (dark == 0.0 && bright == 0.0) continue;
            const double phase = model == organ && !vocal() ? 0.11 * harmonic
                               : model == harpsichord ? 0.0 : 0.075 * randomBipolar();
            double loss = stringLoss(harmonic);
            double partialAttack = 0.0;
            if (!sustained)
            {
                partialAttack = (model == piano ? pianoHammer.onsetScale() * (0.00045 + 0.0017 / std::sqrt(h))
                                               : 0.00030 + 0.00075 / std::sqrt(h));
                // Every principal piano partial supplies prompt sound. Keep
                // the low-course loss unchanged without letting h7+ outlast
                // h6 merely because the slower second course ends there.
                if (model == piano)
                    loss += registerLossScale(0.25) * (0.78 + 0.04 * std::min(harmonic - 1, 5));
            }
            else if (expressive)
            {
                // Relative partial growth provides a bow/breath/tongue
                // transient without imposing a second long global attack.
                // Higher effort opens the upper spectrum sooner. All time
                // constants are authored and captured at note onset.
                const double upper = (h - 1.0) / (h + 3.0);
                const double effortTime = 1.20 - 0.45 * strike;
                switch (model)
                {
                    case bowed: partialAttack = (0.010 + 0.060 * upper) * effortTime; break;
                    case flute: partialAttack = (0.003 + 0.023 * upper) * effortTime; break;
                    case clarinet: partialAttack = (0.005 + 0.032 * upper) * effortTime; break;
                    case oboe: partialAttack = (0.004 + 0.028 * upper) * effortTime; break;
                    case bassoon: partialAttack = (0.007 + 0.036 * upper) * effortTime; break;
                    case brass: partialAttack = (0.004 + 0.037 * upper) * effortTime; break;
                    case organ: partialAttack = (0.012 + 0.030 * upper) * effortTime; break;
                    default: break;
                }
            }
            const unsigned int pickup = model == reed && harmonic > 1
                ? static_cast<unsigned int>(std::min(harmonic - 1, 4)) : 0u;
            addMode(ratio, dark, bright, loss, phase, partialAttack, pickup);
            includeBound(harmonic, coverage, dark, bright);
            if (vocal() && harmonic == harmonicModeCount) choirAirSeed = state;
        }
        // Additional choir phases extend the original sequence, then the air
        // stream resumes at its original 24-mode position. Fractional budget
        // crossings cannot reseed breath; 24-mode notes retain their samples.
        if (vocal()) state = choirAirSeed;
        if (model == piano)
        {
            // Reserve the complete principal-mode random sequence even when
            // the register needs fewer modes. Admitting a nearly silent new
            // partial must not reseed the audible coupled courses or the
            // contact texture. All streams still vary with the strike seed.
            state = principalPhaseSeed;
            for (int slot = 0; slot < 42; ++slot) (void) randomBipolar();
            // Low bass single strings become coupled courses above the bass.
            // The doublet keeps a slowly beating aftersound as the prompt
            // high modes decay. Its geometry belongs to the instrument, not
            // the hammer strike: one neighbouring string has one detuning
            // shared by its partials. Separating string tuning from excitation
            // follows the string/hammer distinction in Bank & Sujbert, DAFx-02
            // (reference above); 1.15 cents is an authored voicing choice.
            // These are authored register contours, so fine tuning across a
            // nominal course boundary must not cause a split-key timbre step.
            const double detunePosition = std::clamp((note - 30.0) / 2.0, 0.0, 1.0);
            const double detuneBlend = detunePosition * detunePosition * (3.0 - 2.0 * detunePosition);
            const double courseRatio = std::exp2(detuneBlend * 1.15 / 1200.0);
            const double coursePosition = std::clamp((note - 47.0) / 2.0, 0.0, 1.0);
            const double courseBlend = coursePosition * coursePosition * (3.0 - 2.0 * coursePosition);
            const double course = 0.28 + 0.08 * courseBlend;
            for (int harmonic = 1; harmonic <= 6; ++harmonic)
            {
                const double h = static_cast<double>(harmonic);
                // Reserve the former detune draw so changing the fixed course
                // geometry does not reseed strike phases or contact texture.
                (void) randomBipolar();
                const double ratio = h * std::sqrt((1.0 + stiffness * h * h) / (1.0 + stiffness))
                                   * courseRatio;
                const double dark = course * stringWeight(harmonic, 0.0, strike, playedContact);
                const double bright = course * stringWeight(harmonic, 1.0, strike, playedContact);
                addMode(ratio, dark, bright,
                        0.70 * stringLoss(harmonic) + 0.12 * (h - 1.0),
                        0.12 * randomBipolar(), pianoHammer.onsetScale() * 0.0035 / std::sqrt(h));
                includeBound(harmonic, course, dark, bright);
            }
        }
        return maximumSum;
    }

    double makeElectricModes() noexcept
    {
        // A single mechanical excursion drives phase-related electrical
        // harmonics. Geometry and live colour act in the pickup, not through
        // separately chosen modal spectra or independently damped harmonics.
        // The rank VCA and the existing fundamental loss still own common
        // amplitude; the dimensionless excursion controls harmonic ratios.
        const bool capacitive = model == reed;
        const double loss = registerLossScale(capacitive ? 0.35 : 0.38);
        double maximumSum = electricPickup.fullStrikeBound();
        // The magnetic derivative's omitted tail above h12 is uniformly
        // below 9.35e-5 of its normalized fundamental across admitted
        // geometry and excursion. Keep all 16 capacitive harmonics and the
        // common full-strike bound; only skip four magnetic oscillators.
        const int electricalHarmonics = capacitive ? ElectricPickup::harmonicCount : 12;
        for (int harmonic = 1; harmonic <= electricalHarmonics; ++harmonic)
        {
            addMode(static_cast<double>(harmonic), 1.0, 1.0,
                    (capacitive ? 0.36 : 0.18) * loss, 0.0,
                    capacitive ? 0.00105 : 0.0013,
                    static_cast<unsigned int>(harmonic));
            // Pickup harmonics inherit the fundamental's common envelope.
            // Their relative decay comes only from shrinking excursion, so
            // the decay control cannot impose a second unrelated loss law.
            modes[static_cast<std::size_t>(count - 1)].loss = modes[0].loss;
        }
        if (!capacitive)
        {
            // Preserve the tine's brief mechanical bending components and
            // slowly beating companion. These are not pickup harmonics.
            const double edge = 0.20 + 0.80 * strike * strike;
            const double bellRegister = std::clamp(std::pow(keyHz / 261.625565, 0.18), 0.72, 1.35);
            addMode(6.267, 0.008 * edge * bellRegister, 0.062 * edge * bellRegister,
                    9.0 * loss, 0.0, 0.0003);
            addMode(17.55, 0.0, 0.012 * edge * bellRegister, 19.0 * loss, 0.0, 0.0002);
            addMode(1.00048, 0.07, 0.07, 0.11 * loss, 0.08, 0.0022);
            maximumSum += 0.074 * bellRegister + 0.07;
        }
        return maximumSum;
    }

    double makeVariantBarModes() noexcept
    {
        // These authored material profiles correct an instrument-family
        // mismatch without changing the published model selector. Yamaha
        // describes its xylophone's tuned third versus the marimba's fourth:
        // https://www.yamaha.com/en/musical_instrument_guide/marimba/structure/structure003.html
        // Rossing/Hansen/Hampton describe steelpan octave and twelfth (or
        // two-octave) modes: https://pubmed.ncbi.nlm.nih.gov/10955647/
        // Higher ratios, weights and losses below are original voicing
        // choices, not measurements or a reconstruction of either instrument.
        constexpr std::array<double, 8> xyloRatios {1.0, 3.0, 5.95, 9.92, 14.8, 20.6, 27.4, 35.2};
        constexpr std::array<double, 8> xyloDark {1.0, 0.20, 0.055, 0.020, 0.008, 0.003, 0.001, 0.0};
        constexpr std::array<double, 8> xyloBright {1.0, 0.72, 0.27, 0.13, 0.060, 0.025, 0.009, 0.003};
        constexpr std::array<double, 8> xyloLoss {1.10, 5.3, 10.8, 19.0, 29.0, 42.0, 57.0, 74.0};
        constexpr std::array<double, 8> panRatios {1.0, 2.0, 3.0, 4.0, 5.43, 7.07, 9.21, 12.4};
        constexpr std::array<double, 8> panDark {1.0, 0.26, 0.11, 0.045, 0.015, 0.008, 0.003, 0.001};
        constexpr std::array<double, 8> panBright {1.0, 0.70, 0.45, 0.23, 0.11, 0.055, 0.025, 0.010};
        constexpr std::array<double, 8> panLoss {0.52, 1.45, 2.3, 3.5, 7.5, 12.0, 19.0, 29.0};
        const bool wooden = variant == xylophone;
        const auto& ratios = wooden ? xyloRatios : panRatios;
        const auto& dark = wooden ? xyloDark : panDark;
        const auto& bright = wooden ? xyloBright : panBright;
        const auto& losses = wooden ? xyloLoss : panLoss;
        const double edge = wooden ? 0.25 + 0.75 * std::pow(strike, 1.5)
                                   : 0.18 + 0.82 * strike * strike;
        const double bandwidth = wooden ? 6500.0 + 25500.0 * strike
                                        : 4000.0 + 13000.0 * strike;
        const double maximumBandwidth = wooden ? 32000.0 : 17000.0;
        const double lossScale = registerLossScale(wooden ? 0.40 : 0.28);
        const double highRegister = std::max(0.0, std::log2(keyHz / 261.625565));
        double maximumSum = 0.0;
        for (std::size_t i = 0; i < ratios.size(); ++i)
        {
            const double highLoss = 0.10 * highRegister * static_cast<double>(i);
            const double contact = i == 0 ? 1.0
                : edge * std::exp(-highLoss - keyHz * ratios[i] / bandwidth);
            const double referenceContact = i == 0 ? 1.0
                : std::exp(-highLoss - keyHz * ratios[i] / maximumBandwidth);
            maximumSum += std::max(dark[i], bright[i]) * referenceContact;
            addMode(ratios[i], dark[i] * contact, bright[i] * contact,
                    losses[i] * lossScale, 0.045 * randomBipolar(),
                    (wooden ? 0.00022 : 0.00055) + 0.00085 / (1.0 + static_cast<double>(i)));
        }
        return maximumSum;
    }

    double makeMaterialModes() noexcept
    {
        // Modal ratios distinguish the geometry, not merely brightness of a
        // common bar. All amplitude, loss and contact values are authored.
        // They are not claimed as measurements of a particular instrument.
        // Source/decaying-mode separation and split-mode beating:
        // https://www.dafx.de/papers/DAFX02_Karjalainen_Valimaki_Esquef_bell-like_sounds.pdf
        struct Shape { double ratio, dark, bright, loss, onset; };
        const double registerScale = registerLossScale(material == kalimba ? 0.32 : 0.22);
        const double highRegister = std::max(0.0, std::log2(keyHz / 261.625565));
        const bool delayedPlate = material == gong;
        const auto build = [&](const auto& profile)
        {
            double maximumSum = 0.0;
            for (const auto& shape : profile)
            {
                const double upper = std::max(0.0, shape.ratio - 1.0);
                // Soft contact suppresses high absolute frequencies. The
                // denominator stays the full-strike bound: a soft hit never
                // recovers the missing upper energy through normalization.
                const double contact = std::exp(-keyHz * upper / (5000.0 + 15000.0 * strike)
                                               - 0.025 * highRegister * upper);
                const double fullContact = std::exp(-keyHz * upper / 20000.0
                                                   - 0.025 * highRegister * upper);
                const double edge = shape.ratio <= 1.05 ? 1.0
                    : delayedPlate ? 0.08 + 0.92 * strike * strike : 0.22 + 0.78 * strike;
                maximumSum += std::max(shape.dark, shape.bright) * fullContact;
                addMode(shape.ratio, shape.dark * edge * contact, shape.bright * edge * contact,
                        shape.loss * registerScale, 0.055 * randomBipolar(), shape.onset);
            }
            return maximumSum;
        };

        switch (material)
        {
            case tubular:
            {
                // Hibbert's 233-tube measurements list average cents relative
                // to the fourth mode. That mode is twice the nominal strike
                // pitch; the first two modes are weak. These source ratios
                // replace the old free-bar series, rather than adding a
                // fictitious strong harmonic fundamental to force the pitch.
                // https://www.hibberts.co.uk/tubular-bells/
                constexpr double cents[] {-3668, -1941, -816, 0, 631, 1139,
                                           1562, 1926, 2242, 2530, 2796, 3068};
                constexpr double dark[] {.006, .025, .12, .90, .47, .30,
                                         .16, .09, .055, .030, .018, .010};
                constexpr double bright[] {.010, .040, .20, 1.0, .76, .57,
                                           .40, .29, .20, .14, .095, .060};
                std::array<Shape, 12> profile {};
                for (std::size_t i = 0; i < profile.size(); ++i)
                {
                    const double index = static_cast<double>(i);
                    profile[i] = {2.0 * std::exp2(cents[i] / 1200.0), dark[i], bright[i],
                                  0.14 + 0.045 * index + 0.018 * index * index,
                                  0.0010 + 0.0020 / (1.0 + index)};
                }
                return build(profile);
            }
            case gong:
            {
                // Fletcher describes the tam-tam's transfer from a low thud
                // into many higher modes. This bounded linear approximation
                // authors delayed upper entrances; it does not claim to solve
                // nonlinear energy exchange or reproduce one measured gong.
                // https://newt.phys.unsw.edu.au/music/people/publications/Fletcher1999a.pdf
                constexpr Shape profile[] {
                    {1.000, .85, .90, .86, .008}, {1.421, .25, .42, .62, .025},
                    {1.917, .19, .35, .49, .045}, {2.283, .13, .31, .36, .075},
                    {2.719, .11, .32, .28, .14}, {3.153, .09, .33, .25, .22},
                    {3.687, .08, .34, .24, .32}, {4.079, .065, .32, .26, .42},
                    {4.531, .055, .30, .29, .53}, {5.137, .045, .28, .32, .62},
                    {5.773, .035, .25, .37, .72}, {6.319, .030, .23, .42, .82},
                    {7.081, .025, .21, .48, .89}, {7.813, .020, .19, .55, .95},
                    {8.671, .016, .16, .63, 1.02}, {9.617, .012, .14, .72, 1.10},
                    {10.723, .010, .12, .83, 1.17}, {12.211, .008, .095, 1.00, 1.25},
                    {13.367, .006, .075, 1.18, 1.31}, {14.827, .004, .055, 1.40, 1.36}
                };
                return build(profile);
            }
            case temple:
            {
                // Asian temple bells are not the tuned minor-third church
                // bell series. This authored bowl/shell has widely separated
                // low modes and stable close pairs for slow metallic beating.
                // Fletcher (1999), section 13.1, distinguishes these shapes.
                constexpr Shape profile[] {
                    {1.0000, 1.00, 1.00, .075, .004}, {1.0017, .28, .28, .094, .006},
                    {2.3610, .27, .44, .21, .004}, {2.3651, .10, .17, .24, .005},
                    {3.9840, .13, .32, .44, .003}, {3.9916, .045, .12, .48, .004},
                    {5.2670, .065, .23, .74, .0028}, {6.6180, .036, .17, 1.05, .0024},
                    {8.1530, .022, .115, 1.49, .0020}, {9.8470, .012, .075, 1.95, .0018},
                    {11.7390, .008, .050, 2.70, .0016}, {13.8280, .004, .030, 3.60, .0014}
                };
                return build(profile);
            }
            case agogo:
            {
                // A compact struck shell: a quick hard contact and several
                // broad-spaced metal modes, with much faster loss than the
                // suspended temple shell. Ratios are authored, not measured.
                constexpr Shape profile[] {
                    {1.000, 1.0, 1.0, 1.80, .0006}, {1.873, .30, .67, 2.30, .0005},
                    {2.647, .18, .51, 3.50, .0004}, {3.871, .095, .34, 5.10, .00035},
                    {5.143, .045, .22, 7.20, .0003}, {6.627, .022, .14, 9.80, .0003},
                    {8.239, .010, .085, 13.5, .00025}, {10.417, .005, .045, 18.0, .0002}
                };
                return build(profile);
            }
            case musicBox:
            {
                // Uniform clamped-free beam eigenfrequency ratios provide a
                // comb-tooth starting point: the first overtone is near 6.27,
                // unlike a free bar's 2.76 or an electric pickup's odd series.
                // Chapman (2012), doi:10.1121/1.3651090, contrasts the beam
                // boundary conditions. Actual shaped combs can differ.
                constexpr Shape profile[] {
                    {1.0, 1.0, 1.0, .36, .0005}, {6.2669, .035, .23, 2.7, .00025},
                    {17.5475, .006, .080, 7.8, .00018}, {34.3861, .001, .026, 15.0, .00013},
                    {56.8426, .0002, .008, 25.0, .00010}
                };
                return build(profile);
            }
            case kalimba:
            {
                // Chapman's measured first overtone is 5.3–5.9 times the
                // fundamental, set by clamped-supported-free geometry. This
                // smooth register law selects an authored position in that
                // range. Higher ratios/losses are authored; no EP pickup
                // harmonics or excursion-power decay enter this branch.
                // https://dael.euracoustics.org/confs/acoustics2008/data/articles/000215.pdf
                const double second = std::clamp(5.72 - 0.12 * std::log2(keyHz / 261.625565)
                                                       + 0.10 * std::log2(bodySize), 5.3, 5.9);
                const double geometry = second / 5.72;
                const Shape profile[] {
                    {1.0, 1.0, 1.0, .56, .0009}, {second, .075, .34, 3.10, .0004},
                    {14.8 * geometry, .010, .095, 8.2, .00025},
                    {28.6 * geometry, .002, .027, 16.0, .00018}
                };
                return build(profile);
            }
            default: return 0.0; // Dispatch excludes default and string materials.
        }
    }

    double makeBarModes() noexcept
    {
        if (variant != standard) return makeVariantBarModes();
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
        const double lossScale = registerLossScale(model == marimba ? 0.42 : 0.28);
        const double highRegister = std::max(0.0, std::log2(keyHz / 261.625565));
        double maximumSum = 0.0;
        for (int i = 0; i < number; ++i)
        {
            const double index = static_cast<double>(i);
            double dark = i == 0 ? 1.0 : std::exp(-1.35 * index);
            double bright = i == 0 ? 1.0 : std::exp(-0.75 * index);
            double loss = i == 0 ? 0.075 : 0.65 * index + 0.35 * index * index;
            if (model == vibes)
            {
                dark = i == 0 ? 1.0 : (i == 1 ? 0.045 : 0.010 / index);
                bright = i == 0 ? 1.0 : (i == 1 ? 0.17 : 0.040 / index);
                loss = i == 0 ? 0.09 : 0.90 * index + 0.60 * index * index;
            }
            else if (model == marimba)
            {
                dark = i == 0 ? 1.0 : std::exp(-1.95 * index);
                bright = i == 0 ? 1.0 : std::exp(-1.05 * index);
                loss = i == 0 ? 0.65 : 3.0 * index + 2.0 * index * index;
            }
            else if (model == celesta)
            {
                dark *= i == 0 ? 1.0 : 0.65;
                loss = i == 0 ? 0.23 : 1.2 * index + 0.4 * index * index;
            }
            // High-register bars have fewer prominent upper modes. Mallet
            // contact further attenuates high absolute frequencies, especially
            // at soft strikes, instead of transposing one fixed middle note.
            const double contact = i == 0 ? 1.0
                : std::exp(-0.12 * highRegister * index
                           - keyHz * ratios[i] / (7000.0 + 16000.0 * strike));
            const double referenceContact = i == 0 ? 1.0
                : std::exp(-0.12 * highRegister * index - keyHz * ratios[i] / 23000.0);
            maximumSum += std::max(dark * referenceContact, bright * referenceContact);
            if (i != 0) { dark *= edge; bright *= edge; }
            addMode(ratios[i], dark * contact, bright * contact, loss * lossScale,
                    0.045 * randomBipolar(), 0.00035 + 0.0012 / (1.0 + index));
        }
        if (model == bell || model == celesta)
        {
            addMode(1.0012, 0.075, 0.075, 0.10 * lossScale, 0.09, 0.0022);
            maximumSum += 0.075;
        }
        return maximumSum;
    }

    void advanceDamper() noexcept
    {
        if (!damperMoving) return;
        damperMoving = false;
        const auto approach = [this](double& current, double target)
        {
            const double difference = target - current;
            if (std::abs(difference) < 1.0e-12) current = target;
            else
            {
                current += damperSmoothing * difference;
                damperMoving = true;
            }
        };
        if (model == piano)
            for (int i = 0; i < count; ++i)
            {
                auto& mode = modes[static_cast<std::size_t>(i)];
                approach(mode.loss, mode.dampedLoss);
            }
        else
            approach(pickupReleaseLoss, pickupReleaseTarget);
    }

    void updateControls(double frequency, float brightness, float pressure) noexcept
    {
        // This precedes the held-colour fast path: settled piano onsets can
        // have no remaining spectral controls but still need damper contact.
        advanceDamper();
        frequency = std::clamp(frequency, 0.01, 0.49 * sampleRate);
        double motion = 0.0;
        if (expressive)
        {
            // Engine supplies pressure already multiplied by the recipe's
            // afterBrill amount. Smooth exactly that amount: never read raw
            // MIDI pressure or create an independent aftertouch route here.
            liveEffort += effortCoefficient * (finiteClamp(pressure, 0.0, 1.0, 0.0) - liveEffort);
            if (--motionCountdown <= 0)
            {
                motionTarget = randomMotion();
                motionCountdown = std::max(1, static_cast<int>(sampleRate / controlPeriod
                                    * (0.35 + 0.14 * randomMotion())));
            }
            // Two 0.9 Hz poles make bounded, nonperiodic changes to spectral
            // colour; this never alters frequency, phase or tuning. The
            // target changes only at a private, sample-counted interval.
            motionLow += motionCoefficient * (motionTarget - motionLow);
            motionSmooth += motionCoefficient * (motionLow - motionSmooth);
            const double depth = vocal() ? 0.008 : model == bowed ? 0.018 : model == brass ? 0.020
                               : model == flute ? 0.015 : 0.012;
            motion = depth * motionSmooth;
        }
        const double pressureColour = vocal() ? 0.24 : model == brass || model == oboe || model == bassoon ? 0.30
                                    : model == bowed || model == clarinet || model == flute ? 0.18 : 0.0;
        const double colour = std::clamp(finiteClamp(brightness, 0.0, 1.0, initialColour)
                            + pressureColour * liveEffort + motion, 0.0, 1.0);
        const bool pitchChanged = frequency != lastFrequency;
        const bool colourChanged = colour != lastColour;
        const bool evolving = transientControls;
        if (!pitchChanged && !colourChanged && !evolving && !gainRamping)
            return;

        pickupMotion *= pickupLoss;
        if (released && model != piano)
            pickupMotion *= pickupReleaseLoss;
        if (pickupMotion < 1.0e-8) pickupMotion = 0.0;
        transientControls = pickupMotion > 0.0;
        const bool electric = (model == tine && material != kalimba) || model == reed;
        ElectricPickup::Spectrum pickupCoefficients;
        if (electric) pickupCoefficients = electricPickup.coefficients(colour, pickupMotion);
        if (tracksFormants() && pitchChanged)
        {
            // tick() accepts float pitch. Returning to that exact onset pitch
            // restores the original weights without a rounding-sized colour
            // change. All fixed-pitch captures keep their prior samples.
            if (frequency == static_cast<double>(static_cast<float>(keyHz)))
            {
                bowedResponseGain.fill(1.0);
                bowedBoundGain = 1.0;
            }
            else
            {
                double bound = 0.0;
                for (int i = 0; i < count; ++i)
                {
                    const auto index = static_cast<std::size_t>(i);
                    const auto& m = modes[index];
                    bowedResponseGain[index] = fixedFormant(frequency * m.ratio)
                                             * bowedResponseInverse[index];
                    bound += std::max(m.dark, m.bright) * bowedResponseGain[index];
                }
                // Keep the original absolute-mode-sum ceiling. A common gain
                // preserves the corrected harmonic ratios; the existing gain
                // ramp interpolates between bounded targets without a step.
                bowedBoundGain = bound <= bowedResponseBound ? 1.0
                    : bowedResponseBound / bound;
            }
        }
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
            if (pitchChanged || colourChanged || evolving)
            {
                const double band = frequency * m.ratio / outputRate;
                const double t = std::clamp((0.465 - band) / 0.085, 0.0, 1.0);
                const double antialias = t * t * (3.0 - 2.0 * t);
                m.onsetMissing *= m.onsetLoss;
                if (m.onsetMissing < 1.0e-8) m.onsetMissing = 0.0;
                transientControls = transientControls || m.onsetMissing > 0.0;
                double pickup = 1.0;
                if (electric && m.pickupPower > 0)
                    pickup = pickupCoefficients[static_cast<std::size_t>(m.pickupPower - 1)];
                double target = (m.dark + colour * (m.bright - m.dark))
                              * antialias * (1.0 - m.onsetMissing) * pickup;
                if (tracksFormants())
                    target *= bowedResponseGain[static_cast<std::size_t>(i)] * bowedBoundGain;
                m.gainStep = (target - m.gain) / controlPeriod;
            }
            else m.gainStep = 0.0;
        }
        lastFrequency = frequency;
        lastColour = colour;
        gainRamping = pitchChanged || colourChanged || evolving;
    }
};

} // namespace cs80::body
