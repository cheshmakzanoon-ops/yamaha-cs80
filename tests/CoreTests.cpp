#include "core/Dsp.h"
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "core/Params.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <limits>
#include <string>
#include <vector>

using namespace cs80;

namespace
{
int failures = 0;
int checks = 0;

void check(bool condition, const std::string& what)
{
    ++checks;
    if (! condition)
    {
        std::printf("FAIL: %s\n", what.c_str());
        ++failures;
    }
}

void checkNear(float actual, float expected, float tolerance, const std::string& what)
{
    check(std::abs(actual - expected) <= tolerance,
          what + " (actual " + std::to_string(actual) + ", expected "
              + std::to_string(expected) + ")");
}

std::vector<float> render(Engine& engine, double sampleRate, int frames,
                          int blockSize = 256)
{
    (void) sampleRate;
    std::vector<float> buffer((size_t) frames * 2, 0.0f);
    for (int i = 0; i < frames; i += blockSize)
    {
        const int n = std::min(blockSize, frames - i);
        engine.render(buffer.data() + (size_t) i * 2, n);
    }
    return buffer;
}

struct Analysis
{
    float peak = 0.0f;
    double rms = 0.0;
    double mean = 0.0;
    bool finite = true;
};

Analysis analyse(const std::vector<float>& buffer)
{
    Analysis a;
    double sum = 0.0;
    double square = 0.0;
    for (float s : buffer)
    {
        if (! std::isfinite(s))
        {
            a.finite = false;
            continue;
        }
        a.peak = std::max(a.peak, std::abs(s));
        sum += s;
        square += (double) s * (double) s;
    }
    if (! buffer.empty())
    {
        a.mean = sum / (double) buffer.size();
        a.rms = std::sqrt(square / (double) buffer.size());
    }
    return a;
}

void testParameterTable()
{
    check(P_legacyNumParams == 122 && (int) P_legacyNumParams == (int) P_ch2Start + (int) CP_count,
          "legacy core layout remains122");
    check(P_subAtVcf == 122 && P_numParams == 127 && paramId(P_subAtVcf) == "sub_at_vcf",
          "new global control appended after both channel blocks");
    check(ParamValues{}[P_subAtVcf] == 0.0f, "new pressure VCF depth defaults off");
    check((int) P_ch2Start - (int) P_ch1Start == (int) CP_count, "channel blocks are contiguous");
    check(std::string(paramId(0)) == "master_tune", "global parameter id");
    check(std::string(paramId(P_ch2Start)) == "ch2_saw", "channel parameter id");

    for (int i = 0; i < P_numParams; ++i)
    {
        const auto& d = paramDef(i);
        check(d.min <= d.def && d.def <= d.max, std::string("default in range: ") + d.id);
        check(! paramId(i).empty(), std::string("parameter id: ") + d.id);
        check(normaliseParam(i, d.min) == 0.0f, std::string("normalise min: ") + d.id);
        check(normaliseParam(i, d.max) == 1.0f, std::string("normalise max: ") + d.id);
    }
}

void testSineFrequency()
{
    constexpr float sr = 48000.0f;
    constexpr float freq = 440.0f;
    Oscillator osc;
    Rng rng;
    int crossings = 0;
    float last = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const auto o = osc.tick(freq, sr, 0.5f, false, false, 0.0f, rng);
        if (i > 0 && ((last <= 0.0f && o.sine > 0.0f) || (last >= 0.0f && o.sine < 0.0f)))
            ++crossings;
        last = o.sine;
    }
    check(std::abs(crossings - 880) <= 3, "sine oscillator frequency");
}

void testFeetRatios()
{
    checkNear(midiToHz(69.0f, 440.0f),440,0.01f,"A4 reference");
    const float expected[6]={.5f,1.f,1.5f,2.f,3.f,4.f};
    for(int i=0;i<6;++i)
        checkNear(kFeetRatios[i],expected[i],1.e-7f,"nominal harmonic footage ratio");
    checkNear(kFeetRatios[2]/kFeetRatios[1],1.5f,1.e-7f,"5 1/3 foot uses nominal3:2 ratio relative to8foot");
    check(std::abs(kFeetRatios[2]-std::exp2(7.f/12))>.001f,
          "harmonic fifth is not equal-tempered seven semitones");
}

void testOscillatorDiscontinuities()
{
    // Subtract the explicitly modelled reset spike to isolate the falling saw.
    // At phase zero a band-limited discontinuity crosses its midpoint, rather
    // than doubling to +2 as the old inverted BLEP correction did.
    for (float sr : { 44100.0f, 48000.0f, 96000.0f, 192000.0f })
    {
        Oscillator osc;
        Rng rng;
        const float frequency = sr / 64.0f;
        osc.reset(1.0f - frequency / sr);
        const auto edge = osc.tick(frequency, sr, 0.5f, true, false, 0.0f, rng);
        checkNear(edge.saw - edge.narrow * 0.70f, 0.0f, 1.0e-6f,
                  "falling saw BLEP reaches edge midpoint");
        float peak = 0.0f;
        for (int i = 0; i < 640; ++i)
        {
            const auto o = osc.tick(frequency, sr, 0.5f, true, false, 0.0f, rng);
            peak = std::max(peak, std::abs(o.saw - o.narrow * 0.70f));
        }
        check(peak <= 1.00001f, "falling saw BLEP does not reinforce discontinuity");
    }
    // A 30us reset at 100Hz occupies 0.003 of a cycle. Test the generic
    // band-limited pulse rather than baking an unsupported 1% width limit in.
    checkNear(blepPulse(0.006f, 0.003f, 0.001f), -1.0f, 1.0e-6f,
              "fixed-time pulse falls before 1 percent phase");
    checkNear(blepPulse(0.0015f, 0.003f, 0.001f), 1.0f, 1.0e-6f,
              "sub-percent pulse retains positive interior");
}


void testOscillatorComparatorPulse()
{
    // Coherent, exactly representable phase grids isolate mean/plateau checks
    // from long-run float phase drift. No Yamaha voltage calibration is implied.
    for (float sr : { 44100.0f, 48000.0f, 96000.0f, 192000.0f })
    {
        for (int period : { 32, 128, 512, 2048 })
        {
            const float dt = 1.0f / (float) period;
            const float frequency = sr * dt;
            for (float width : { .125f, .25f, .5f, .75f, .875f })
            {
                Oscillator osc;
                Rng rng;
                double sum = 0.0;
                float maxPlateauError = 0.0f, maxBlepError = 0.0f;
                bool finite = true, bounded = true;
                int positivePlateaus = 0, negativePlateaus = 0;
                for (int n = 0; n < period; ++n)
                {
                    const auto out = osc.tick(frequency, sr, width, false, true, 0.0f, rng);
                    const float phase = osc.getPhase();
                    finite = finite && std::isfinite(out.pulse);
                    bounded = bounded && std::abs(out.pulse) <= 1.00001f;
                    sum += out.pulse;
                    // Preserve the existing antialiased edge kernel exactly,
                    // including wrap and duty transition; this is not a hardware fit.
                    maxBlepError = std::max(maxBlepError,
                        std::abs(out.pulse - blepPulse(phase, width, dt)));
                    const float wrapDistance = std::min(phase, 1.0f - phase);
                    if (wrapDistance > dt && std::abs(phase - width) > dt)
                    {
                        const float expected = phase < width ? 1.0f : -1.0f;
                        maxPlateauError = std::max(maxPlateauError,
                                                  std::abs(out.pulse - expected));
                        if (expected > 0.0f) ++positivePlateaus;
                        else ++negativePlateaus;
                    }
                }
                check(finite && bounded, "comparator pulse finite and bounded through both edges");
                check(positivePlateaus > 0 && negativePlateaus > 0,
                      "comparator fixture samples both plateaus away from BLEPs");
                checkNear(maxPlateauError, 0.0f, 1.e-6f,
                          "pulse has only unit plateaus, no independent reset level");
                checkNear(maxBlepError, 0.0f, 1.e-7f,
                          "pulse preserves original bandlimited edge kernel");
                checkNear((float) (sum / period), 2.0f * width - 1.0f, 2.e-6f,
                          "pulse mean follows duty without reset-dependent DC");
            }
        }

        // Zero frequency and overlapping edge kernels, including width clamps.
        // These are numerical boundary tests, not supported hardware operating points.
        for (float ratio : { 0.0f, .00001f, .01f, .125f, .45f, .6f })
        {
            for (float width : { 0.0f, .000001f, .05f, .5f, .95f, .999999f, 1.0f })
            {
                Oscillator on, off;
                Rng onRng, offRng;
                bool valid = true, otherPathsUnchanged = true;
                for (int n = 0; n < 1024; ++n)
                {
                    const auto a = on.tick(sr * ratio, sr, width, true, true, 0.0f, onRng);
                    const auto b = off.tick(sr * ratio, sr, width, true, false, 0.0f, offRng);
                    valid = valid && std::isfinite(a.pulse) && std::abs(a.pulse) <= 1.00001f;
                    otherPathsUnchanged = otherPathsUnchanged && b.pulse == 0.0f
                        && a.saw == b.saw && a.sine == b.sine && a.narrow == b.narrow
                        && on.getPhase() == off.getPhase();
                }
                check(valid, "zero/high-frequency and extreme-width pulse stays finite and bounded");
                check(otherPathsUnchanged, "pulse enable preserves saw, sine, reset and phase paths");
            }
        }
    }
}

void testHardwareRingAndRibbon()
{
    checkNear(ribbonFrequencyRatio(0.0f), 1.0f, 1.0e-7f, "ribbon first contact is unison");
    checkNear(ribbonFrequencyRatio(1.0f), 2.0f, 1.0e-7f, "ribbon full upward stroke is octave");
    checkNear(ribbonFrequencyRatio(-1.0f), 0.0f, 1.0e-7f, "ribbon downward endpoint reaches zero");
    check(ribbonFrequencyRatio(-0.9999f) < 0.001f, "ribbon reaches sub-audio ratio");
    checkNear(ringModulationFrequency(80.0f, 0.0f, 1.0f), 80.0f, 1.0e-7f,
              "zero ring depth keeps base speed");
    checkNear(ringModulationFrequency(80.0f, 1.0f, 0.0f), 80.0f, 1.0e-7f,
              "ring envelope returns to base speed");
    check(ringModulationFrequency(80.0f, 1.0f, 1.0f) > 80.0f,
          "ring envelope increases frequency, not amplitude");
    check(ringModulationFrequency(10.0f, 1.0f, 1.0f)
          < ringModulationFrequency(80.0f, 1.0f, 1.0f),
          "ring depth peak depends on base speed");

    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        ParamValues p;
        p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
        p[P_ringSpeed] = 80.0f;
        p[P_ringDepth] = 1.0f;
        p[P_ringAttack] = 0.1f;
        p[P_ringDecay] = 0.1f;
        p[P_ringAmount] = 0.0f; // envelope must advance even when wet path off
        Engine e;
        e.prepare(sr);
        e.setParams(p);
        e.noteOn(60, 0.8f);
        render(e, sr, (int) (sr * 0.02));
        check(e.getRingFrequency() > 80.0f, "ring speed rises with AD while bypassed");
        const float beforeLegato = e.getRingEnvelope();
        e.noteOn(64, 0.8f);
        checkNear(e.getRingEnvelope(), beforeLegato, 1.0e-7f,
                  "legato chord does not retrigger ring envelope");
        render(e, sr, (int) (sr * 0.5));
        checkNear(e.getRingFrequency(), 80.0f, 0.1f, "ring speed decays to panel speed");
        e.allNotesOff(false);
        e.noteOn(67, 0.8f);
        checkNear(e.getRingEnvelope(), 0.0f, 1.0e-7f, "new phrase retriggers ring AD");
        p[P_ringAmount] = 1.0f;
        p[P_ringSpeed] = paramDef(P_ringSpeed).max;
        e.setParams(p);
        e.setRibbon(-1.0f);
        auto audio = analyse(render(e, sr, (int) (sr * 0.1)));
        check(audio.finite && audio.peak < 2.0f, "zero-frequency ribbon with max ring finite");
        e.allNotesOff(true);
        render(e, sr, (int) (sr * 2.0));
        check(e.activeVoices() == 0, "zero-frequency ribbon does not hold envelope open");
    }

    for (int sustainMode : { 0, 1 })
    {
        Engine e;
        e.prepare(48000);
        ParamValues p;
        p[P_sustainMode] = (float) sustainMode;
        p[chParam(0, CP_vR)] = p[chParam(1, CP_vR)] = 3.0f;
        e.setParams(p);
        e.noteOn(60, 0.8f);
        e.setRibbon(0.5f);
        render(e, 48000, 32);
        checkNear(e.getVoiceInfo(0).ribbonRatio, 1.5f, 1.0e-6f, "held note tracks ribbon");
        e.noteOff(60);
        e.setRibbon(-0.5f);
        render(e, 48000, 32);
        checkNear(e.getVoiceInfo(0).ribbonRatio, sustainMode == 0 ? 1.5f : 0.5f,
                  1.0e-6f, "ribbon release-tail sustain mode routing");
    }
    Engine automated;
    automated.prepare(48000);
    ParamValues p;
    p[P_ribbon] = 0.5f;
    automated.setParams(p);
    automated.noteOn(60, 0.8f);
    render(automated, 48000, 32);
    checkNear(automated.getVoiceInfo(0).ribbonRatio, 1.5f, 1.0e-6f,
              "host ribbon parameter reaches engine");
}

void testFilterStability()
{
    Svf f;
    f.set(1200.0f, 8.5f, 48000.0f);
    Rng rng;
    float peak = 0.0f;
    for (int i = 0; i < 48000 * 4; ++i)
    {
        const float x = (i == 0 ? 1.0f : 0.0f) + rng.nextBipolar() * 0.00001f;
        const auto o = f.process(x);
        if (! std::isfinite(o.lp) || ! std::isfinite(o.hp))
        {
            check(false, "filter finite");
            return;
        }
        peak = std::max(peak, std::abs(o.lp));
    }
    check(peak < 20.0f, "filter does not run away");
}

void testEnvelope()
{
    Adsr e;
    e.reset();
    e.noteOn(0.01f, 0.1f, 0.5f, 0.2f);
    int attackSamples = 0;
    for (int i = 0; i < 48000; ++i)
    {
        const float v = e.tick(48000.0f, true);
        if (v > 0.99f)
        {
            attackSamples = i;
            break;
        }
    }
    check(attackSamples > 100 && attackSamples < 1200, "envelope attack time");
    for (int i = 0; i < 48000; ++i)
        e.tick(48000.0f, true);
    checkNear(e.getValue(), 0.5f, 0.02f, "envelope sustain");
    e.noteOff();
    for (int i = 0; i < 48000; ++i)
        e.tick(48000.0f, true);
    check(e.isIdle(), "envelope release finishes");

    Adsr gate;
    gate.reset();
    gate.noteOn(0.0f, 0.2f, 1.0f, 0.2f);
    checkNear(gate.tick(48000.0f, true), 1.0f, 0.0001f, "zero attack reaches full level");
}

void testHardwareFilterContour()
{
    checkNear(filterInitialOffset(0.0f), 0.0f, 1.0e-7f, "IL zero neutral");
    check(filterInitialOffset(1.0f) < 0.0f, "IL lowers initial cutoff");
    checkNear(filterAttackOffset(0.0f), 0.0f, 1.0e-7f, "AL zero neutral");
    check(filterAttackOffset(1.0f) > 0.0f, "AL raises attack peak");
    for (float sr : {44100.0f, 48000.0f, 96000.0f, 192000.0f})
    {
        FilterEnv contour;
        contour.noteOn(-2.0f, 1.0f, 0.1f, 0.1f, 0.2f, 0.0f);
        checkNear(contour.getValue(), -2.0f, 1.0e-6f, "filter starts at IL");
        float peak = -2.0f;
        for (int i = 0; i < (int)(sr * 0.5f); ++i)
            peak = std::max(peak, contour.tick(sr));
        checkNear(peak, 1.0f, 0.001f, "filter reaches AL");
        checkNear(contour.getValue(), 0.0f, 0.001f, "filter held state is panel cutoff");
        check(!contour.isIdle(), "settled held filter remains releasable");
        contour.noteOff();
        for (int i = 0; i < (int)(sr * 0.5f); ++i)
            contour.tick(sr);
        checkNear(contour.getValue(), -2.0f, 0.001f, "filter releases to IL after settled decay");
        check(contour.isIdle(), "released filter becomes idle");
        for (float keyDuration : {0.005f, 0.13f, 0.4f})
        {
            contour.noteOn(-2.0f, 1.0f, 0.1f, 0.1f, 0.2f, 0.0f);
            for (int i = 0; i < (int)(sr * keyDuration); ++i)
                contour.tick(sr);
            const float before = contour.getValue();
            contour.noteOff();
            checkNear(contour.getValue(), before, 1.0e-6f, "key release does not jump filter CV");
            for (int i = 0; i < (int)(sr * 0.5f); ++i)
                contour.tick(sr);
            checkNear(contour.getValue(), -2.0f, 0.001f, "early filter release returns to IL");
        }
    }

    ParamValues p;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_mix] = 0.0f;
    p[P_keyRelease] = 0.0f;
    p[chParam(0, CP_fIl)] = 0.6f;
    p[chParam(0, CP_fAl)] = 0.7f;
    p[chParam(0, CP_fA)] = 0.2f;
    p[chParam(0, CP_fD)] = 0.3f;
    p[chParam(0, CP_hpfCutoff)] = 200.0f;
    p[chParam(0, CP_lpfCutoff)] = 2000.0f;
    Engine e;
    e.prepare(48000);
    e.setParams(p);
    e.noteOn(60, 0.8f);
    render(e, 48000, 1);
    const auto first = e.getRankInfo(0,0);
    check(first.hpfHz < 200.0f && first.lpfHz < 2000.0f, "IL lowers both filter cutoffs");
    checkNear(first.hpfHz / 200.0f,
              std::pow(first.lpfHz / 2000.0f, kProvisionalHpfEnvelopeGain), 1.0e-6f,
              "separate provisional HP envelope gain is applied");
    render(e, 48000, 11000);
    const auto peak = e.getRankInfo(0,0);
    check(peak.hpfHz > 200.0f && peak.lpfHz > 2000.0f, "AL raises both filter cutoffs");
    render(e, 48000, 48000);
    const auto held = e.getRankInfo(0,0);
    p[P_masterSustain] = 1.0f;
    e.setParams(p);
    render(e, 48000, 64);
    checkNear(e.getRankInfo(0,0).hpfHz, held.hpfHz, 0.01f, "sustain time does not shift held HPF");
    checkNear(e.getRankInfo(0,0).lpfHz, held.lpfHz, 0.1f, "sustain time does not shift held LPF");

    // Sine must bypass the two filters, but still pass through the amplifier.
    auto sineRender = [](bool closed) {
        ParamValues v;
        v[P_mix] = 0.0f;
        v[P_voiceDispersion] = v[P_drift] = v[P_analogNoise] = 0.0f;
        v[chParam(0, CP_saw)] = v[chParam(0, CP_pulse)] = 0.0f;
        v[chParam(0, CP_noise)] = 0.0f;
        v[chParam(0, CP_sine)] = 1.0f;
        v[chParam(0, CP_hpfCutoff)] = closed ? 17000.0f : 20.0f;
        v[chParam(0, CP_lpfCutoff)] = closed ? 20.0f : 17000.0f;
        v[chParam(0, CP_fIl)] = closed ? 1.0f : 0.0f;
        v[chParam(0, CP_fAl)] = closed ? 0.0f : 1.0f;
        Engine engine;
        engine.prepare(48000);
        engine.setParams(v);
        engine.noteOn(69, 0.8f);
        return render(engine, 48000, 12000);
    };
    const auto open = sineRender(false), closed = sineRender(true);
    double error = 0.0;
    for (size_t i = 0; i < open.size(); ++i)
        error = std::max(error, std::abs((double)open[i] - closed[i]));
    check(error < 1.0e-7, "sine signal unchanged by filter and filter contour settings");
    check(analyse(open).rms > 0.001, "sine bypass test has audible signal");
}

void testDecimator()
{
    Decimator full, retained;
    full.prepare(96000); retained.prepare(96000);
    const auto& coeff = full.getCoefficients();
    std::vector<float> input(4096);
    Rng rng(0xdec180u);
    for(auto& x : input) x=rng.nextBipolar();
    double maxError=0, retainedError=0;
    for(int n=0;n<(int)input.size();++n)
    {
        double reference=0;
        for(int tap=0;tap<Decimator::kTaps && tap<=n;++tap)
            reference+=(double)coeff[(size_t)tap]*input[(size_t)(n-tap)];
        const float actual=full.process(input[(size_t)n]);
        maxError=std::max(maxError,std::abs((double)actual-reference));
        retained.push(input[(size_t)n]);
        if((n&1)==Decimator::kRetainedPhase)
            retainedError=std::max(retainedError,std::abs((double)retained.output()-reference));
    }
    check(maxError<1.e-6,"symmetric FIR matches independent full convolution");
    check(retainedError<1.e-6,"retained-phase FIR matches independent full convolution");
    full.reset(); retained.reset();
    double dc=0, centroid=0, retainedDc=0, symmetryError=0;
    int peakIndex=-1; float peak=0;
    std::vector<float> odd;
    for(int n=0;n<Decimator::kTaps+4;++n)
    {
        const float x=n==0 ? 1.f : 0.f;
        const float y=full.process(x);
        dc+=y;
        if(n<Decimator::kTaps)
            symmetryError=std::max(symmetryError,std::abs((double)y-coeff[(size_t)(Decimator::kTaps-1-n)]));
        retained.push(x);
        if((n&1)==Decimator::kRetainedPhase)
        {
            const float output=retained.output();
            const int host=(n-1)/2;
            odd.push_back(output); retainedDc+=output; centroid+=host*(double)output;
            if(std::abs(output)>peak) { peak=std::abs(output); peakIndex=host; }
        }
    }
    checkNear((float)dc,1,2.e-6f,"decimator unity DC gain");
    check(symmetryError<2.e-6,"impulse response is symmetric");
    check(peakIndex==63,"odd retained impulse peaks at host sample63");
    checkNear((float)(centroid/retainedDc),63,1.e-5f,"retained impulse centroid delay63");
    constexpr double pi=3.14159265358979323846;
    double re=0,im=0;
    for(size_t i=0;i<odd.size();++i) { re+=odd[i]*std::cos(2*pi*.001*i); im-=odd[i]*std::sin(2*pi*.001*i); }
    checkNear((float)(-std::atan2(im,re)/(2*pi*.001)),63,1.e-4f,"measured low-frequency phase delay63host");
    double minPass=2,maxPass=0,maxStop=0;
    // Independent dense DFT of the actual rounded float coefficients.
    for(int bin=0;bin<=8192;++bin)
    {
        const double f=.5*bin/8192;
        double real=0,imag=0;
        for(int tap=0;tap<Decimator::kTaps;++tap)
        {
            real+=coeff[(size_t)tap]*std::cos(2*pi*f*tap);
            imag-=coeff[(size_t)tap]*std::sin(2*pi*f*tap);
        }
        const double magnitude=std::sqrt(real*real+imag*imag);
        if(f<=20000.0/88200.0) { minPass=std::min(minPass,magnitude); maxPass=std::max(maxPass,magnitude); }
        if(f>=.25) maxStop=std::max(maxStop,magnitude);
    }
    check(minPass>=std::pow(10.0,-.1/20) && maxPass<=std::pow(10.0,.1/20),
          "decimator within0.1dB through20kHz at44.1kHz host");
    check(maxStop<=1.e-4,"decimator at least80dB rejection above host Nyquist");
    for(double sr : {44100.,48000.,96000.,192000.})
    {
        Engine e; e.prepare(sr);
        check(e.getLatencySamples()==63,"engine latency agrees with retained FIR phase");
    }
    retained.reset();
    checkNear(retained.output(),0,1.e-9f,"decimator reset removes history");
}

void testFilterControlDomain()
{
    checkNear(nominalFilterCutoff(.25f,5),1000,0.001f,"documented filter calibration anchor");
    checkNear(nominalFilterCutoff(.5f,5),2000,0.001f,"doubling Kv doubles cutoff");
    checkNear(nominalFilterCutoff(.25f,6),2000,0.001f,"one effective CV volt doubles cutoff");
    checkNear(provisionalFilterQ(0),.5f,1.e-6f,"documented minimum Q");
    checkNear(provisionalFilterQ(1),5,1.e-5f,"documented maximum Q");
    for(int i=-10;i<=110;++i)
        check(provisionalFilterQ(i*.01f)>=.5f && provisionalFilterQ(i*.01f)<=5.00001f,
              "provisional Q interpolation bounded by documented endpoints");
    ParamValues p;
    p[P_voiceDispersion]=p[P_drift]=p[P_analogNoise]=0;
    p[P_brilliance]=p[P_subVco]=p[P_subVcf]=0;
    for(int ch=0;ch<2;++ch)
    {
        p[chParam(ch,CP_fIl)]=p[chParam(ch,CP_fAl)]=0;
        p[chParam(ch,CP_initBrill)]=p[chParam(ch,CP_afterBrill)]=0;
        p[chParam(ch,CP_hpfCutoff)]=200;
        p[chParam(ch,CP_lpfCutoff)]=2000;
    }
    auto probe=[&](int note, const ParamValues& values) {
        Engine e; e.prepare(48000); e.setParams(values); e.noteOn(note,.8f);
        render(e,48000,64); return e.getRankInfo(0,0);
    };
    // Cache refresh must preserve the uncached effective-CV formula on changes.
    Engine cached; cached.prepare(48000); cached.setParams(p); cached.noteOn(60,.8f);
    for(float hz : {20.f,200.f,2000.f})
    {
        auto changed=p;
        changed[chParam(0,CP_hpfCutoff)]=changed[chParam(0,CP_lpfCutoff)]=hz;
        cached.setParams(changed); render(cached,48000,16);
        const float expected=nominalFilterCutoff(.5f,filterEffectiveCv(hz));
        checkNear(cached.getRankInfo(0,0).hpfHz,expected,.001f,"cached HP control equals uncached law");
        checkNear(cached.getRankInfo(0,0).lpfHz,expected,.001f,"cached LP control equals uncached law");
    }
    const auto base=probe(60,p), octave=probe(72,p);
    checkNear(base.keyVolts,.5f,1.e-6f,"MIDI C4 nominal key voltage");
    checkNear(octave.hpfHz/base.hpfHz,2,1.e-5f,"HPF neutral keytracking one octave per octave");
    checkNear(octave.lpfHz/base.lpfHz,2,1.e-5f,"LPF neutral keytracking one octave per octave");
    p[P_masterTune]=1;
    const auto tuned=probe(60,p);
    checkNear(tuned.lpfHz/base.lpfHz,std::exp2(1.f/12),1.e-5f,"master pitch changes shared filter Kv");
    p[P_masterTune]=0;
    for(int feet=0;feet<6;++feet)
    {
        p[chParam(0,CP_feet)]=(float)feet;
        const auto foot=probe(60,p);
        checkNear(foot.lpfHz,base.lpfHz,.001f,"footage independent of LPF keytracking");
        checkNear(foot.hpfHz,base.hpfHz,.001f,"footage independent of HPF keytracking");
        checkNear(foot.oscillatorHz/base.oscillatorHz,kFeetRatios[feet],1.e-6f,
                  "engine applies nominal harmonic footage ratio independently");
    }
    p[P_subVco]=1;
    const auto vibrato=probe(60,p);
    checkNear(vibrato.lpfHz,base.lpfHz,.001f,"VCO vibrato independent of filter Kv");
    p[P_subVco]=0;
    // Starting a second key from the preceding key verifies the actual engine
    // glide path, rather than just the standalone voltage conversion.
    Engine e; e.prepare(48000); e.setParams(p); e.noteOn(60,.8f); render(e,48000,64);
    p[P_portamento]=.2f; e.setParams(p); e.noteOn(72,.8f); render(e,48000,1);
    const auto start=e.getRankInfo(1,0);
    render(e,48000,4800); const auto moving=e.getRankInfo(1,0);
    render(e,48000,96000); const auto settled=e.getRankInfo(1,0);
    check(start.keyVolts<.501f,"gliding filter starts near previous key voltage");
    check(moving.keyVolts>start.keyVolts && moving.keyVolts<1,"filter follows intermediate glide voltage");
    checkNear(settled.keyVolts,1,.001f,"filter glide reaches target key voltage");
    checkNear(moving.lpfHz/start.lpfHz,moving.keyVolts/start.keyVolts,1.e-5f,
              "filter cutoff proportional to gliding key voltage");
}

void testSustainTimeAndWheelRouting()
{
    for (int mode : {0,1})
    {
        Engine e;
        e.prepare(48000);
        ParamValues p;
        p[P_sustainMode] = (float)mode;
        p[P_masterSustain] = 0.5f;
        p[P_keyRelease] = 0.0f;
        p[chParam(0, CP_vR)] = p[chParam(1, CP_vR)] = 0.01f;
        p[chParam(0, CP_fR)] = p[chParam(1, CP_fR)] = 0.01f;
        e.setParams(p);
        e.noteOn(60,0.8f);
        render(e,48000,12000);
        e.noteOff(60);
        render(e,48000,48000);
        check(e.activeVoices() == 1, "global sustain time lengthens released note");
        check(e.getRankInfo(0,0).filterEnvelope > filterInitialOffset(p[chParam(0, CP_fIl)]) + 0.01f,
              "global sustain also lengthens VCF release");
        e.noteOn(67,0.8f);
        render(e,48000,32);
        check(e.activeVoices() == (mode == 0 ? 2 : 1), "sustain II new note cuts only previous released tail");
        e.noteOff(67);
        render(e,48000,48000 * 8);
        check(e.activeVoices() == 0, "both sustain modes decay without another note");
    }
    ParamValues p;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = p[P_subVco] = 0.0f;
    p[P_subOriginalMode] = 0.0f; // this phase-sensitive fixture covers shipped legacy routing
    p[P_subSpeed] = 1.0f;
    p[P_subWave] = 0.0f;
    p[P_modWheelSub] = 1.0f;
    Engine e;
    e.prepare(48000);
    e.setParams(p);
    e.noteOn(69,0.8f);
    e.controlChange(1,1.0f);
    render(e,48000,12000);
    check(e.getRankInfo(0,0).oscillatorHz > 440.0f, "wheel increases positive sub pitch modulation");
    render(e,48000,24000);
    check(e.getRankInfo(0,0).oscillatorHz < 440.0f, "wheel also follows negative sub phase, not DC pitch offset");
    e.controlChange(1,0.0f);
    render(e,48000,64);
    checkNear(e.getRankInfo(0,0).oscillatorHz,440.0f,0.01f,"wheel zero restores unmodulated pitch");
}

void testSustainPedal()
{
    ParamValues p;
    p[P_volume] = 0.6f;
    p[P_keyRelease] = 0.05f;

    Engine engine;
    engine.prepare(48000.0);
    engine.setParams(p);
    engine.noteOn(60, 0.8f, 1);
    render(engine, 48000.0, 2048);

    engine.controlChange(64, 1.0f, 1);
    engine.noteOff(60, 1);
    const auto held = render(engine, 48000.0, 24000);
    check(engine.activeVoices() > 0, "sustain pedal holds a released note");
    check(analyse(held).finite, "sustain pedal render finite");

    engine.controlChange(64, 0.0f, 1);
    const auto tail = render(engine, 48000.0, 96000);
    check(engine.activeVoices() == 0, "sustain pedal releases when lifted");
    check(analyse(tail).finite, "sustain pedal release finite");
}

void testAllSoundOff()
{
    Engine engine;
    engine.prepare(48000.0);
    engine.noteOn(60, 0.8f, 1);
    render(engine, 48000.0, 2048);
    check(engine.activeVoices() > 0, "panic test voice active");
    engine.allNotesOff(false);
    check(engine.activeVoices() == 0, "all sound off stops voices immediately");
}

void testChannelPanic()
{
    auto countChannel=[](const Engine& e,int channel) {
        int count=0;
        for(int i=0;i<8;++i) { const auto v=e.getVoiceInfo(i); if(v.active && v.channel==channel) ++count; }
        return count;
    };
    Engine e; e.prepare(48000);
    e.noteOn(60,.8f,2); e.noteOn(64,.8f,3); render(e,48000,2400);
    e.controlChange(120,0,2);
    check(countChannel(e,2)==0 && countChannel(e,3)==1,"channel sound-off preserves other channel");
    check(e.getVoiceInfo(1).held,"unrelated key remains held after channel panic");
    const float envelope=e.getRingEnvelope();
    e.noteOn(67,.8f,4);
    checkNear(e.getRingEnvelope(),envelope,1.e-7f,"channel panic does not reset shared ring gate with other held notes");
    e.controlChange(123,0,3);
    check(!e.getVoiceInfo(1).held && e.getVoiceInfo(1).active,"channel notes-off starts release rather than hard kill");
    check(countChannel(e,4)==1 && e.getVoiceInfo(2).held,"channel release leaves another channel held");
    e.allNotesOff(false,-1); e.allNotesOff(false,17);
    check(countChannel(e,4)==1,"invalid panic channel does not become global panic");
    e.allNotesOff(false);
    check(e.activeVoices()==0,"default panic remains global");
    e.noteOn(72,.8f,2);
    checkNear(e.getRingEnvelope(),0,1.e-7f,"last-channel/global panic permits next ring phrase retrigger");
    e.allNotesOff(false);

    for(int mode : {0,1})
    {
        ParamValues p; p[P_sustainMode]=(float)mode;
        e.reset(); e.setParams(p);
        e.noteOn(60,.8f,2); e.noteOn(64,.8f,3); render(e,48000,1024);
        e.controlChange(64,1,2); e.allNotesOff(true,2);
        check(e.getVoiceInfo(0).pedalHeld && !e.getVoiceInfo(0).held,
              "channel notes-off respects pedal hold");
        e.allNotesOff(true,3);
        check(!e.getVoiceInfo(1).pedalHeld,"other channel does not inherit pedal");
        e.controlChange(64,0,3);
        check(e.getVoiceInfo(0).pedalHeld,"unrelated channel pedal-up does not release target");
        e.controlChange(64,0,2);
        render(e,48000,96000);
        check(e.activeVoices()==0,"channel pedal-up releases notes-off hold");
    }
    ParamValues p; e.reset(); e.setParams(p);
    e.controlChange(64,1,2); e.noteOn(60,.8f,2); e.noteOff(60,2);
    e.allNotesOff(false,2);
    check(countChannel(e,2)==0,"channel sound-off overrides pedal-held voice");
    e.noteOn(61,.8f,2); e.noteOff(61,2);
    check(e.getVoiceInfo(1).pedalHeld,"channel sound-off does not reset controller state");
    e.allNotesOff(false); e.noteOn(62,.8f,2); e.noteOff(62,2);
    check(!e.getVoiceInfo(2).pedalHeld,"legacy global panic clears pedal states");

    e.reset(); p[P_mpeOn]=1; e.setParams(p);
    e.controlChange(64,1,1); e.noteOn(60,.8f,2); e.noteOn(64,.8f,3);
    e.allNotesOff(true,2); e.noteOff(64,3);
    check(e.getVoiceInfo(0).pedalHeld && e.getVoiceInfo(1).pedalHeld,"MPE master pedal holds member channels");
    e.allNotesOff(false,2);
    check(e.getVoiceInfo(1).pedalHeld,"member panic preserves other MPE pedal-held notes");
    e.controlChange(64,0,1); render(e,48000,96000);
    check(e.activeVoices()==0,"MPE master pedal-up clears remaining holds");

    e.reset(); p=ParamValues{}; p[P_arpOn]=1; p[P_arpRate]=4; p[P_arpOctaves]=1;
    e.setParams(p); e.noteOn(60,.2f,2); e.noteOn(60,.8f,3); render(e,48000,1);
    check(e.getVoiceInfo(0).channel==2,"arpeggiator preserves source MIDI channel");
    checkNear(e.getVoiceInfo(0).velocity,.2f,1.e-6f,"arpeggiator preserves matching source velocity");
    e.allNotesOff(false,2); render(e,48000,24000);
    check(countChannel(e,2)==0 && countChannel(e,3)>0,"channel panic removes only matching arpeggiator source keys");
    e.noteOff(60,2); render(e,48000,24000);
    check(countChannel(e,3)>0,"wrong-channel arp note-off cannot remove identical other-channel pitch");
    e.allNotesOff(true,3); render(e,48000,96000);
    check(e.activeVoices()==0,"last-channel notes-off stops arp without stuck generated notes");
    e.allNotesOff(false);
}

void testCoreInputGuards()
{
    Engine e;
    const double invalidRates[]={0,-1,7999,768001,1.e300,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()};
    for(double rate : invalidRates)
    {
        e.prepare(rate);
        checkNear((float)e.getHostSampleRate(),48000,0,"invalid sample rate falls back before allocation");
        e.noteOn(60,.8f);
        check(analyse(render(e,48000,256)).finite,"invalid-rate fallback renders finite audio");
    }
    for(double rate : {8000.,768000.})
    {
        e.prepare(rate);
        check(e.getHostSampleRate()==rate,"supported sample-rate boundary accepted");
        e.noteOn(60,.8f);
        check(analyse(render(e,rate,256)).finite,"boundary sample rate renders finite audio");
    }
    e.prepare(48000);
    for(float invalid : {std::numeric_limits<float>::quiet_NaN(),
                         std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity()})
    {
        ParamValues p; for(auto& value:p.v) value=invalid;
        e.reset(); e.setParams(p);
        bool defaults=true;
        for(int i=0;i<P_numParams;++i) defaults=defaults && e.getParams()[i]==paramDef(i).def;
        check(defaults,"all nonfinite core parameters fall back to defaults");
        e.noteOn(60,.8f);
        check(analyse(render(e,48000,256)).finite,"nonfinite parameters cannot poison render");
    }
    for(bool high : {false,true})
    {
        ParamValues p;
        for(int i=0;i<P_numParams;++i) p[i]=high ? paramDef(i).max+1000000.f : paramDef(i).min-1000000.f;
        e.reset(); e.setParams(p);
        bool bounded=true;
        for(int i=0;i<P_numParams;++i)
            bounded=bounded && e.getParams()[i]==(high ? paramDef(i).max : paramDef(i).min);
        check(bounded,"finite out-of-range core parameters clamp to declared bounds");
        e.noteOn(60,.8f);
        check(analyse(render(e,48000,256)).finite,"clamped extreme parameters render finite");
    }
    ParamValues valid; valid[P_ringSpeed]=500;
    e.setParams(valid);
    check(e.getParams()[P_ringSpeed]==500,"valid legacy500Hz ring extension preserved");
}

void testMpeExpression()
{
    ParamValues p;
    p[P_mpeOn] = 1.0f;
    p[P_pitchBendRange] = 2.0f;
    p[P_mpeBendRange] = 48.0f;

    Engine engine;
    engine.prepare(48000.0);
    engine.setParams(p);
    engine.noteOn(60, 0.8f, 2);
    engine.noteOn(60, 0.8f, 3);
    engine.polyPressure(60, 0.9f, 2);
    engine.pitchBend(2, 1.0f);

    bool sawChannel2 = false;
    bool sawChannel3 = false;
    for (int i = 0; i < 8; ++i)
    {
        const auto info = engine.getVoiceInfo(i);
        if (! info.active || info.note != 60)
            continue;
        if (info.channel == 2)
        {
            checkNear(info.pressure, 0.9f, 0.001f, "MPE pressure is channel-scoped");
            checkNear(info.bend, 48.0f, 0.001f, "MPE bend uses MPE range");
            sawChannel2 = true;
        }
        else if (info.channel == 3)
        {
            checkNear(info.pressure, 0.0f, 0.001f, "MPE pressure leaves other channel alone");
            checkNear(info.bend, 0.0f, 0.001f, "MPE bend leaves other channel alone");
            sawChannel3 = true;
        }
    }
    check(sawChannel2 && sawChannel3, "MPE voices retain their member channels");
}

void testSubOscillatorWaves()
{
    for (int wave = 0; wave < 6; ++wave)
    {
        Lfo lfo;
        Rng rng(0x51c80u + (uint32_t) wave);
        lfo.reset(0.0f);
        float lo = 1.0f;
        float hi = -1.0f;
        bool finite = true;
        for (int i = 0; i < 48000; ++i)
        {
            const float value = lfo.tick(7.0f, 48000.0f, wave, rng);
            finite = finite && std::isfinite(value);
            lo = std::min(lo, value);
            hi = std::max(hi, value);
        }
        check(finite, "sub oscillator wave finite: " + std::to_string(wave));
        check(hi - lo > 0.05f, "sub oscillator wave moves: " + std::to_string(wave));
    }
}


void testOriginalSubOscillator()
{
    check(P_subOriginalMode == 123 && P_subOriginalFunction == 124
          && P_subOriginalSpeed == 125 && P_subExtLevel == 126,
          "original SUB controls append after existing AT VCF");
    check(ParamValues{}[P_subOriginalMode] == 1, "fresh core defaults to original SUB");
    checkNear(OriginalSubOscillator::targetRate(.7f,0,1),.7f,1.e-6f,"original base minimum");
    checkNear(OriginalSubOscillator::targetRate(60,0,1),60,1.e-6f,"original base maximum");
    checkNear(OriginalSubOscillator::targetRate(4,1,.5f),8,1.e-6f,
              "documented provisional AT frequency multiplier");
    checkNear(OriginalSubOscillator::targetRate(60,1,1),180,1.e-6f,
              "provisional combined rate bounded at180Hz, not measured maximum");

    for (float rate : {44100.f,48000.f,96000.f,192000.f})
        for (float hz : {.7f,60.f})
        {
            OriginalSubOscillator osc;
            const int n=(int)rate*2;
            for(int i=0;i<n;++i) osc.tick(hz,rate,0);
            double expected=std::fmod((double)n*(double)hz/rate,1.0);
            const double delta=std::abs(osc.getPhase()-expected);
            check(std::min(delta,1.0-delta)<1.e-8,
                  "original phase tracks endpoint rate across sample rates");
        }

    OriginalSubOscillator sine,down,up,square;
    for(int n=1;n<=16;++n)
    {
        const float a=sine.tick(1000,8000,0);
        const float d=down.tick(1000,8000,1);
        const float u=up.tick(1000,8000,2);
        const float q=square.tick(1000,8000,3);
        const float phase=(float)(n%8)/8;
        checkNear(a,std::sin(kTwoPi*phase),1.e-6f,"original position0 sine");
        checkNear(d,(1-2*phase)*(3.5f/3.2f),1.e-6f,"original position1 falling saw nominal ratio");
        checkNear(u,-d,1.e-6f,"original position2 unity inverted saw");
        check(q==(phase<.5f ? 1.f : -1.f),"original position3 normalized comparator square");
    }
    OriginalSubOscillator slowNoise,fastNoise,external;
    bool noiseChanges=false,noiseFinite=true;
    float previous=0;
    for(int n=0;n<4096;++n)
    {
        const float a=slowNoise.tick(.7f,48000,4);
        const float b=fastNoise.tick(120,48000,4);
        noiseFinite=noiseFinite&&std::isfinite(a)&&std::abs(a)<=1;
        check(a==b,"white noise sample sequence independent of frequency");
        if(n>0 && a!=previous)noiseChanges=true;
        previous=a;
        check(external.tick(60,48000,5)==0,"EXT function is silent while disconnected");
    }
    check(noiseChanges&&noiseFinite,"noise is finite white samples, not cycle sample-and-hold");

    // Dormant new controls cannot change the legacy stream or output.
    ParamValues legacy; legacy[P_subOriginalMode]=0;
    legacy[P_subVco]=.2f; legacy[P_subVcf]=.2f; legacy[P_subVca]=.2f;
    for(int wave=0;wave<6;++wave)
    {
        legacy[P_subWave]=(float)wave; legacy[P_subSpeed]=7;
        auto changed=legacy;
        changed[P_subOriginalFunction]=5; changed[P_subOriginalSpeed]=60; changed[P_subExtLevel]=1;
        Engine a,b; a.prepare(48000); b.prepare(48000);
        a.setParams(legacy); b.setParams(changed);
        a.noteOn(60,.7f); b.noteOn(60,.7f);
        check(render(a,48000,2048)==render(b,48000,2048),
              "inactive original controls preserve each legacy waveform render");
    }

    // Independent shipped LFO oracle, including old RNG consumption and
    // non-sample-rate-compensated speed smoothing. No voices are needed.
    for(int wave=0;wave<6;++wave)
    {
        Engine engine; engine.prepare(48000);
        legacy[P_subWave]=(float)wave; legacy[P_subSpeed]=7;
        engine.setParams(legacy);
        Rng random(0x9e3779b9u);
        float phase=0, speed=4.2f, held=0; bool started=false;
        bool same=true; float pair[2]{};
        for(int frame=0;frame<512;++frame)
        {
            float expected=0;
            for(int os=0;os<2;++os)
            {
                speed+=(7-speed)*.0015f;
                const float before=phase;
                phase+=speed/96000; phase-=std::floor(phase);
                switch(wave)
                {
                    case 0: expected=std::sin(kTwoPi*phase); break;
                    case 1: expected=1-4*std::abs(phase-.5f); break;
                    case 2: expected=2*phase-1; break;
                    case 3: expected=phase<.5f ? 1.f:-1.f; break;
                    case 4:
                        if(!started || phase<before) {held=random.nextBipolar();started=true;}
                        expected=held; break;
                    case 5: expected=random.nextBipolar(); break;
                }
            }
            engine.render(pair,1);
            same=same&&engine.getSubOscillatorValue()==expected
                     &&engine.getSubOscillatorRate()==speed;
        }
        check(same,"legacy LFO sample stream and speed match independent shipped oracle");
    }

    for(int function : {4,5})
    {
        ParamValues p; p[P_subOriginalFunction]=(float)function;
        p[P_subOriginalSpeed]=.7f; p[P_subAtSpeed]=0;
        p[P_subVco]=p[P_subVcf]=p[P_subVca]=.3f;
        auto fast=p; fast[P_subOriginalSpeed]=60; fast[P_subAtSpeed]=1;
        Engine a,b; a.prepare(48000); b.prepare(48000);
        a.setParams(p); b.setParams(fast);
        a.noteOn(60,.8f); b.noteOn(60,.8f);
        a.polyPressure(60,1); b.polyPressure(60,1);
        check(render(a,48000,4096)==render(b,48000,4096),
              "original noise and disconnected EXT ignore both base and AT SPEED in audio");
    }

    ParamValues p; p[P_subOriginalFunction]=1; p[P_subOriginalSpeed]=.7f;
    Engine whole,partitioned; whole.prepare(48000); partitioned.prepare(48000);
    whole.setParams(p); partitioned.setParams(p);
    whole.noteOn(60,.8f); partitioned.noteOn(60,.8f);
    const auto expected=render(whole,48000,4096);
    std::vector<float> actual(8192);
    int done=0;
    for(int count : {1,63,257,1024,2751})
    { partitioned.render(actual.data()+2*done,count); done+=count; }
    check(actual==expected,"original SUB continuous across arbitrary render partitions");
    const float before=whole.getSubOscillatorRate();
    whole.noteOn(64,.8f);
    check(whole.getSubOscillatorRate()==before,"note-on does not reset shared original rate state");
}

void testRepeatedNoteRelease()
{
    Engine engine;
    engine.prepare(48000.0);
    engine.noteOn(60, 0.2f, 1);
    engine.noteOn(60, 0.9f, 1);
    engine.noteOff(60, 1);
    // The released voice stays active while its release tail fades; let it
    // finish before checking which voice is still held.
    render(engine, 48000.0, 96000);

    int activeSixty = 0;
    for (int i = 0; i < 8; ++i)
    {
        const auto info = engine.getVoiceInfo(i);
        if (info.active && info.note == 60)
        {
            ++activeSixty;
            checkNear(info.velocity, 0.2f, 0.001f,
                      "repeated note-off releases the newest voice");
        }
    }
    check(activeSixty == 1, "repeated note leaves the older voice active");
    engine.noteOff(60, 1);
    engine.allNotesOff(false);
    check(engine.activeVoices() == 0, "repeated note cleanup");
}

void testHardwareVoiceAssignment()
{
    Engine engine; engine.prepare(48000);
    for(int press=0;press<24;++press)
    {
        engine.noteOn(60,.8f);
        check(engine.getVoiceInfo(press%8).held,"repeated key rotates through all eight circuits");
        engine.noteOff(60);
        int held=0; for(int i=0;i<8;++i) held+=engine.getVoiceInfo(i).held ? 1 : 0;
        check(held==0,"repeated key release targets held circuit, not older release tail");
    }
    engine.reset();
    for(int i=0;i<8;++i) engine.noteOn(48+i,.8f);
    engine.noteOn(80,.9f);
    engine.noteOff(80);
    for(int i=0;i<8;++i)
    {
        const auto v=engine.getVoiceInfo(i);
        check(v.held && v.note==48+i,"ninth held key ignored without disturbing occupied circuits");
    }
    engine.noteOff(51); engine.noteOn(81,.8f);
    check(engine.getVoiceInfo(3).held && engine.getVoiceInfo(3).note==81,
          "circular scan reuses only released circuit when seven remain held");
    engine.noteOff(81);
    for(int i=0;i<8;++i) engine.noteOff(48+i);
    render(engine,48000,48000*3);
    check(engine.activeVoices()==0,"ignored note does not leave phantom held count or stuck voice");

    engine.reset(); engine.controlChange(64,1);
    for(int i=0;i<8;++i) { engine.noteOn(48+i,.8f); engine.noteOff(48+i); }
    engine.noteOn(48,.9f);
    for(int i=0;i<8;++i)
        check(engine.getVoiceInfo(i).pedalHeld && !engine.getVoiceInfo(i).held,
              "pedal-held circuits are not stolen by retrigger");
    engine.controlChange(64,0); engine.noteOn(72,.8f);
    check(engine.getVoiceInfo(0).held && engine.getVoiceInfo(0).note==72,"pedal release makes next circular circuit available");
    engine.allNotesOff(false);

    ParamValues p; p[P_mpeOn]=1;
    engine.reset(); engine.setParams(p);
    for(int i=0;i<8;++i) engine.noteOn(60,.8f,i+2);
    engine.noteOn(60,.9f,10); engine.noteOff(60,10);
    for(int i=0;i<8;++i)
        check(engine.getVoiceInfo(i).held && engine.getVoiceInfo(i).channel==i+2,
              "ignored MPE ninth member note-off cannot release another member");
    engine.noteOff(60,1);
    for(int i=0;i<8;++i)
        check(engine.getVoiceInfo(i).held,"unmatched master-channel note-off is not a global panic");
    engine.noteOff(60,5); engine.noteOn(70,.8f,10);
    check(engine.getVoiceInfo(3).held && engine.getVoiceInfo(3).channel==10,
          "released MPE circuit is available without changing other members");
    engine.allNotesOff(false);
    engine.reset(); p[P_mpeOn]=0; engine.setParams(p);
    engine.noteOn(60,.8f,2); engine.noteOff(60,1);
    check(engine.getVoiceInfo(0).held,"non-MPE note-off still respects MIDI channel identity");
    engine.noteOff(60,2);
    check(!engine.getVoiceInfo(0).held,"matching non-MPE channel releases held note");
    // Modern arp is not original hardware. Stress bounded held-key storage and
    // repeated circuit wrap; all chooseVoice callers must handle no allocation.
    p[P_arpOn]=1; p[P_arpRate]=30; p[P_arpOctaves]=4;
    engine.setParams(p);
    for(int note=0;note<128;++note) engine.noteOn(note,.8f,2);
    const auto audio=render(engine,48000,48000);
    check(analyse(audio).finite && engine.activeVoices()<=8,"arpeggiator remains bounded across cyclic assignment");
    engine.allNotesOff(false);
    check(engine.activeVoices()==0,"arpeggiator allocation cleanup");
}

void testVoiceAllocation()
{
    Engine engine;
    engine.prepare(48000.0);
    for (int i = 0; i < 12; ++i)
        engine.noteOn(48 + i, 0.8f, 1);
    check(engine.activeVoices() <= 8, "voice limit is eight");
    const auto buffer = render(engine, 48000.0, 4096);
    const auto a = analyse(buffer);
    check(a.finite, "voice allocation render finite");
    check(a.peak > 0.001f, "voice allocation produces audio");
}

void testRenderAndRelease()
{
    Engine engine;
    engine.prepare(48000.0);
    engine.noteOn(60, 0.9f, 1);
    engine.noteOn(64, 0.9f, 1);
    engine.noteOn(67, 0.9f, 1);

    const auto held = render(engine, 48000.0, 512 * 40);
    const auto a = analyse(held);
    check(a.finite, "audio finite");
    check(a.peak > 0.001f && a.peak < 2.0f, "audio has sensible level");
    check(std::abs(a.mean) < 0.01, "audio has no large DC offset");

    engine.noteOff(60, 1);
    engine.noteOff(64, 1);
    engine.noteOff(67, 1);
    const auto tail = render(engine, 48000.0, 512 * 300);
    check(analyse(tail).finite, "release tail finite");
    check(engine.activeVoices() == 0, "voices release");
}

void testVelocityResponse()
{
    auto measure = [](float velocity) {
        ParamValues p;
        for (int ch = 0; ch < 2; ++ch)
        {
            // Isolate the explicit Initial Level destination; do not rely on
            // an unconditional velocity multiplier or a brightness change.
            p[chParam(ch, CP_vLevel)] = 0.25f;
            p[chParam(ch, CP_initLevel)] = 0.65f;
            p[chParam(ch, CP_initBrill)] = 0.0f;
        }
        Engine engine;
        engine.prepare(48000.0);
        engine.setParams(p);
        engine.noteOn(60, velocity, 1);
        const auto buffer = render(engine, 48000.0, 4800);
        return analyse(buffer).rms;
    };

    const double soft = measure(0.25f);
    const double hard = measure(1.0f);
    check(hard > soft * 1.5, "enabled Initial Level changes level with velocity");
    for (double rate : {44100.0,48000.0,88200.0,96000.0})
    {
        ParamValues neutral;
        neutral[P_initBend]=0;
        neutral[P_voiceDispersion]=0; neutral[P_drift]=0; neutral[P_analogNoise]=0;
        for(int ch=0;ch<2;++ch)
        {
            neutral[chParam(ch,CP_initLevel)]=0;
            neutral[chParam(ch,CP_initBrill)]=0;
        }
        Engine softEngine, hardEngine;
        softEngine.prepare(rate); hardEngine.prepare(rate);
        softEngine.setParams(neutral); hardEngine.setParams(neutral);
        softEngine.noteOn(60,.1f); hardEngine.noteOn(60,1.f);
        const auto softAudio=render(softEngine,rate,(int)(rate*.25));
        const auto hardAudio=render(hardEngine,rate,(int)(rate*.25));
        check(softAudio==hardAudio, "zero initial-touch depths make attack and hold velocity invariant");
        softEngine.noteOff(60); hardEngine.noteOff(60);
        check(render(softEngine,rate,(int)(rate*.5))==render(hardEngine,rate,(int)(rate*.5)),
              "zero initial-touch depths make release velocity invariant");
    }
}

void testSampleRates()
{
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        ParamValues p;
        p[P_volume] = 0.55f;
        p[P_chorusOn] = 1.0f;
        Engine engine;
        engine.prepare(sr);
        engine.setParams(p);
        checkNear((float) engine.getInternalSampleRate(), (float) (sr * 2.0), 0.1f,
                  "2x internal rate at " + std::to_string((int) sr));
        engine.noteOn(57, 0.8f, 1);
        const auto buffer = render(engine, sr, (int) (sr * 0.35));
        const auto a = analyse(buffer);
        check(a.finite, "sample-rate render finite");
        check(a.peak > 0.001f && a.peak < 2.0f, "sample-rate render level");
    }
}

void testArpeggiator()
{
    ParamValues p;
    p[P_arpOn] = 1.0f;
    p[P_arpRate] = 8.0f;
    p[P_arpMode] = 0.0f;
    p[P_arpOctaves] = 2.0f;
    p[P_volume] = 0.6f;

    Engine engine;
    engine.prepare(48000.0);
    engine.setParams(p);
    engine.noteOn(60, 0.8f, 1);
    engine.noteOn(64, 0.8f, 1);
    engine.noteOn(67, 0.8f, 1);

    std::set<int> notes;
    bool finite = true;
    for (int block = 0; block < 160; ++block)
    {
        const auto buffer = render(engine, 48000.0, 256);
        finite = finite && analyse(buffer).finite;
        for (int v = 0; v < 8; ++v)
        {
            const auto info = engine.getVoiceInfo(v);
            if (info.active)
                notes.insert(info.note);
        }
    }

    check(finite, "arpeggiator output finite");
    check(engine.activeVoices() >= 1, "arpeggiator keeps a voice running");
    check(notes.size() >= 4, "arpeggiator visits multiple notes");
    check(notes.count(72) > 0 && notes.count(76) > 0, "arpeggiator transposes octaves");

    engine.noteOff(60, 1);
    engine.noteOff(64, 1);
    engine.noteOff(67, 1);
    const auto tail = render(engine, 48000.0, 48000);
    check(analyse(tail).finite, "arpeggiator release finite");
    check(engine.activeVoices() == 0, "arpeggiator stops when chord is released");
}

void testArpeggiatorModes()
{
    for (int mode = 0; mode < 5; ++mode)
    {
        ParamValues p;
        p[P_arpOn] = 1.0f;
        p[P_arpRate] = 12.0f;
        p[P_arpMode] = (float) mode;
        p[P_arpOctaves] = 1.0f;
        Engine engine;
        engine.prepare(48000.0);
        engine.setParams(p);
        engine.noteOn(60, 0.8f, 1);
        engine.noteOn(63, 0.8f, 1);
        engine.noteOn(67, 0.8f, 1);
        const auto buffer = render(engine, 48000.0, 24000);
        check(analyse(buffer).finite,
              "arpeggiator mode finite: " + std::to_string(mode));
        check(engine.activeVoices() > 0,
              "arpeggiator mode active: " + std::to_string(mode));
    }
}

void testEffects()
{
    for (int type = 1; type <= 5; ++type)
    {
        ParamValues p;
        p[P_fx1Type] = (float) type;
        p[P_fx1Mix] = 0.9f;
        p[P_chorusOn] = 0.0f;
        p[P_volume] = 0.55f;
        Engine engine;
        engine.prepare(48000.0);
        engine.setParams(p);
        engine.noteOn(60, 0.85f, 1);
        const auto buffer = render(engine, 48000.0, 48000);
        const auto a = analyse(buffer);
        check(a.finite, "FX finite: type " + std::to_string(type));
        check(a.peak > 0.0005f && a.peak < 2.0f,
              "FX level: type " + std::to_string(type));

        const auto right = [&] {
            double sum = 0.0;
            for (size_t i = 1; i < buffer.size(); i += 2)
                sum += (double) buffer[i] * (double) buffer[i];
            return std::sqrt(sum / (double) (buffer.size() / 2));
        }();
        check(right > 0.0001, "stereo FX has right channel: type " + std::to_string(type));
    }
}

void testMacros()
{
    ParamValues base;
    base[P_macro1] = 0.5f;
    base[P_macro2] = 0.5f;
    base[P_macro3] = 0.5f;
    base[P_macro4] = 0.5f;

    ParamValues wide = base;
    wide[P_macro3] = 1.0f;
    wide[P_macro4] = 1.0f;

    Engine a;
    Engine b;
    a.prepare(48000.0);
    b.prepare(48000.0);
    a.setParams(base);
    b.setParams(wide);
    a.noteOn(60, 0.85f, 1);
    b.noteOn(60, 0.85f, 1);

    const auto aa = analyse(render(a, 48000.0, 24000));
    const auto bb = analyse(render(b, 48000.0, 24000));
    check(aa.finite && bb.finite, "macro renders finite");
    check(std::abs(aa.rms - bb.rms) > 1.0e-5, "macros change the sound");
    check(bb.peak < 2.0f, "macro output remains bounded");
}

void testPresetDefaults()
{
    ParamValues p;
    check(p[P_volume] > 0.0f, "volume default");
    check(p[chParam(0, CP_lpfCutoff)] > 100.0f, "channel cutoff default");
    check(p[chParam(1, CP_saw)] >= 0.5f, "channel two saw default");
}

void testFactoryBank()
{
    const auto& bank = factoryPresets();
    check(bank.size() >= 200, "factory bank has at least 200 presets");
    check(std::string(hardwareToneName(0, 0)) == "String 1", "rank I hardware tone identity");
    check(std::string(hardwareToneName(0, 1)) == "String 2", "rank II hardware tone identity");
    check(std::string(hardwareToneName(3, 0)) == "Flute", "rank I fourth tab identity");
    check(std::string(hardwareToneName(3, 1)) == "Brass 3", "rank II fourth tab identity");

    const char* expected[2][11] = {
        { "String 1", "String 3", "Brass 1", "Flute", "E. Piano", "Clavichord 1",
          "Harpsichord 1", "Organ 1", "Guitar 1", "Funky 1", "Funky 3" },
        { "String 2", "String 4", "Brass 2", "Brass 3", "Bass", "Clavichord 2",
          "Harpsichord 2", "Organ 2", "Guitar 2", "Funky 2", "Funky 4" }
    };
    for (int channel = 0; channel < 2; ++channel)
        for (int tone = 0; tone < 11; ++tone)
        {
            check(std::string(hardwareToneName(tone, channel)) == expected[channel][tone],
                  "original hardware tab order, bank " + std::to_string(channel));
            ParamValues selected;
            selected[P_volume] = 0.37f;
            selected[P_ringSpeed] = 17.0f;
            selected[chParam(1 - channel, CP_noise)] = 0.67f;
            const auto before = selected;
            applyHardwareTone(selected, channel, tone);
            bool globalsPreserved = true;
            for (int i = 0; i < P_numGlobals; ++i)
                globalsPreserved = globalsPreserved && selected[i] == before[i];
            check(globalsPreserved, "hardware selection preserves every global");
            bool otherPreserved = true;
            for (int i = 0; i < CP_count; ++i)
                otherPreserved = otherPreserved
                    && selected[chParam(1 - channel, i)] == before[chParam(1 - channel, i)];
            check(otherPreserved, "hardware selection preserves entire other rank");
        }

    ParamValues p;
    const float globalBefore = p[P_volume];
    applyHardwareTone(p, 0, 2);
    check(p[chParam(0, CP_lpfCutoff)] < 2000.0f, "hardware brass tone changes the channel");
    checkNear(p[P_volume], globalBefore, 1.0e-7f, "hardware tone preserves globals");
    check(p[chParam(1, CP_lpfCutoff)] > 2000.0f, "hardware tone preserves other channel");

    std::set<std::string> names;
    for (const auto& preset : bank)
    {
        check(preset.values[P_subOriginalMode] == 0, "shipped factory programs retain legacy SUB");
        check(! preset.name.empty(), "preset has a name");
        check(! preset.category.empty(), "preset has a category");
        check(names.insert(preset.name).second, "preset name unique: " + preset.name);
        for (int i = 0; i < P_numParams; ++i)
        {
            const auto& d = paramDef(i);
            if (preset.values[i] < d.min - 0.001f || preset.values[i] > d.max + 0.001f)
            {
                check(false, "preset value in range: " + preset.name + " / " + d.id);
                break;
            }
        }
    }
}


void testSubAftertouchVcf()
{
    for (double rate : {44100.0, 48000.0, 88200.0, 96000.0})
    {
        ParamValues p;
        p[P_voiceDispersion]=0; p[P_drift]=0; p[P_analogNoise]=0;
        p[P_subOriginalMode]=0; // explicitly exercise unchanged legacy aftertouch routing
        p[P_subSpeed]=4; p[P_subWave]=0; p[P_subVcf]=0;
        p[P_subAtSpeed]=0; p[P_subAtVco]=0;
        for (int rank=0; rank<2; ++rank)
        {
            p[chParam(rank,CP_fIl)]=0; p[chParam(rank,CP_fAl)]=0;
            p[chParam(rank,CP_initBrill)]=0; p[chParam(rank,CP_afterBrill)]=0;
            p[chParam(rank,CP_initLevel)]=0; p[chParam(rank,CP_afterLevel)]=0;
            p[chParam(rank,CP_hpfCutoff)]=200; p[chParam(rank,CP_lpfCutoff)]=1000;
        }
        Engine reference, pressure;
        reference.prepare(rate); pressure.prepare(rate);
        reference.setParams(p); pressure.setParams(p);
        for (auto* e : {&reference,&pressure})
        {
            e->noteOn(60,.8f,2); e->noteOn(67,.8f,3);
        }
        pressure.polyPressure(60,1,2);
        check(render(reference,rate,256)==render(pressure,rate,256),
              "zero AT VCF depth leaves audio unchanged by pressure");
        p[P_subAtVcf]=.75f;
        pressure.setParams(p);
        float low=100, high=-100;
        bool independent=true, bothFilters=true, bothRanks=true;
        for (int block=0; block<(int)rate/64; ++block)
        {
            render(reference,rate,64); render(pressure,rate,64);
            for (int rank=0;rank<2;++rank)
            {
                const auto a=reference.getRankInfo(0,rank), b=pressure.getRankInfo(0,rank);
                const float hp=std::log2(b.hpfHz/a.hpfHz);
                const float lp=std::log2(b.lpfHz/a.lpfHz);
                bothFilters=bothFilters && std::abs(hp-lp)<.00001f;
                if(rank==0) {low=std::min(low,lp); high=std::max(high,lp);}
                else
                {
                    const auto a0=reference.getRankInfo(0,0), b0=pressure.getRankInfo(0,0);
                    bothRanks=bothRanks && std::abs(lp-std::log2(b0.lpfHz/a0.lpfHz))<.00001f;
                }
                const auto otherA=reference.getRankInfo(1,rank), otherB=pressure.getRankInfo(1,rank);
                independent=independent && otherA.hpfHz==otherB.hpfHz && otherA.lpfHz==otherB.lpfHz;
            }
        }
        check(low<-.5f && high>.5f, "AT VCF follows bipolar sub waveform, not static brightness");
        check(bothFilters && bothRanks, "AT VCF reaches HPF and LPF of both ranks");
        check(independent, "poly pressure VCF does not modulate another held key");
        pressure.polyPressure(60,0,2);
        render(reference,rate,64); render(pressure,rate,64);
        for(int rank=0;rank<2;++rank)
        {
            const auto a=reference.getRankInfo(0,rank), b=pressure.getRankInfo(0,rank);
            check(a.hpfHz==b.hpfHz && a.lpfHz==b.lpfHz,
                  "releasing pressure removes AT VCF contribution");
        }
    }
}


void testNominalInitialPitchTransient()
{
    // Independent expected values use the conditional TWS C ideal pole,
    // not a constant read back from the implementation under test.
    auto settings = [] {
        ParamValues p;
        p[P_voiceDispersion]=p[P_drift]=p[P_analogNoise]=0;
        p[P_detune]=p[P_subVco]=p[P_subAtVco]=p[P_subAtSpeed]=0;
        p[P_initBend]=.8f;
        for(int rank=0;rank<2;++rank)
        {
            p[chParam(rank,CP_fIl)]=p[chParam(rank,CP_fAl)]=0;
            p[chParam(rank,CP_initBrill)]=p[chParam(rank,CP_initLevel)]=0;
            p[chParam(rank,CP_afterBrill)]=p[chParam(rank,CP_afterLevel)]=0;
            p[chParam(rank,CP_saw)]=p[chParam(rank,CP_pulse)]=0;
            p[chParam(rank,CP_sine)]=.2f;
            p[chParam(rank,CP_vLevel)]=.25f;
            p[chParam(rank,CP_vA)]=p[chParam(rank,CP_vD)]=.001f;
            p[chParam(rank,CP_vS)]=1;
        }
        return p;
    };
    auto bend = [](const Engine& e,int voice,int rank,int note) {
        return 12.0f*std::log2(e.getRankInfo(voice,rank).oscillatorHz/midiToHz((float)note));
    };
    for(double rate : {44100.0,48000.0,96000.0,192000.0})
    {
        const int period=(int)std::lround(rate*.050);
        auto p=settings();
        Engine e; e.prepare(rate); e.setParams(p); e.noteOn(69,.75f);
        float previous=-.8f*.75f;
        for(int multiple=1;multiple<=3;++multiple)
        {
            render(e,rate,period);
            const float expected=(float)(-.8*.75*std::exp(-multiple));
            for(int rank=0;rank<2;++rank)
                checkNear(bend(e,0,rank,69),expected,2.e-5f,
                          "initial pitch follows nominal 50/100/150ms positive-step decay");
            const float current=bend(e,0,0,69);
            check(current>previous && current<0,
                  "nominal initial pitch rises monotonically from below without overshoot");
            previous=current;
        }
        render(e,rate,(int)(rate*2)-3*period);
        check(e.getRankInfo(0,0).oscillatorHz==midiToHz(69),
              "initial pitch decay does not freeze a permanent subthreshold detuning");

        p[P_initBend]=0;
        Engine neutral, muted;
        neutral.prepare(rate); muted.prepare(rate);
        neutral.setParams(p); neutral.noteOn(69,.75f);
        auto initiallyEnabled=p; initiallyEnabled[P_initBend]=1;
        muted.setParams(initiallyEnabled); muted.noteOn(69,.75f); muted.setParams(p);
        check(render(neutral,rate,256)==render(muted,rate,256),
              "turning PB depth off before rendering is sample-identical to zero-depth onset");
        Engine live; live.prepare(rate); live.setParams(p); live.noteOn(69,.75f);
        render(live,rate,period);
        check(live.getRankInfo(0,0).oscillatorHz==midiToHz(69),
              "zero PB depth leaves pitch neutral while upstream onset state evolves");
        p[P_initBend]=.8f; live.setParams(p); render(live,rate,1);
        const double elapsed=(period+1)/rate;
        checkNear(bend(live,0,0,69),(float)(-.8*.75*std::exp(-elapsed/.050)),2.e-5f,
                  "enabling PB after50ms reveals decayed state, not zero or a restarted onset");
        p[P_initBend]=.4f; live.setParams(p); render(live,rate,1);
        checkNear(bend(live,0,0,69),(float)(-.4*.75*std::exp(-((period+2)/rate)/.050)),2.e-5f,
                  "live PB depth scales the existing transient without restarting it");

        p=settings();
        Engine poly; poly.prepare(rate); poly.setParams(p); poly.noteOn(69,.75f);
        render(poly,rate,period); poly.noteOn(76,.4f);
        const int quarter=(int)std::lround(rate*.025);
        render(poly,rate,quarter);
        checkNear(bend(poly,0,0,69),(float)(-.8*.75*std::exp(-((period+quarter)/rate)/.050)),2.e-5f,
                  "new polyphonic note does not restart the older initial pitch state");
        checkNear(bend(poly,1,0,76),(float)(-.8*.4*std::exp(-(quarter/rate)/.050)),2.e-5f,
                  "staggered voice has its independent velocity and onset age");

        // With PB disabled, this patch must not introduce a shared decay into
        // the existing held Initial Brilliance/Level behavior. This is a
        // software-preservation regression, not calibration of the b circuit.
        p[P_initBend]=0;
        for(int rank=0;rank<2;++rank)
            p[chParam(rank,CP_initBrill)]=.3f;
        Engine held; held.prepare(rate); held.setParams(p); held.noteOn(69,.75f);
        render(held,rate,period);
        const auto before=held.getRankInfo(0,0);
        render(held,rate,9*period);
        const auto after=held.getRankInfo(0,0);
        check(before.hpfHz==after.hpfHz && before.lpfHz==after.lpfHz,
              "pitch-only correction preserves current held initial brilliance");
        for(int rank=0;rank<2;++rank)
        {
            p[chParam(rank,CP_initBrill)]=0;
            p[chParam(rank,CP_initLevel)]=.65f;
        }
        Engine soft,hard; soft.prepare(rate); hard.prepare(rate);
        soft.setParams(p); hard.setParams(p);
        soft.noteOn(69,.25f); hard.noteOn(69,1);
        render(soft,rate,10*period); render(hard,rate,10*period);
        check(analyse(render(hard,rate,period)).rms >
              1.5*analyse(render(soft,rate,period)).rms,
              "pitch-only correction preserves current held initial level response");
    }
}


void testSharedPressureSpeedBus()
{
    // All other pressure destinations are zero: only shared speed -> SUB VCO
    // can distinguish these renders. Matching event histories preserve phase.
    auto settings = [] {
        ParamValues p;
        p[P_voiceDispersion]=p[P_drift]=p[P_analogNoise]=p[P_detune]=0;
        p[P_initBend]=p[P_subAtVco]=p[P_subAtVcf]=p[P_subVcf]=p[P_subVca]=0;
        p[P_subOriginalMode]=0; // shared pressure regression originally used legacy SUB
        p[P_subAtSpeed]=1; p[P_subVco]=.7f; p[P_subSpeed]=5; p[P_subWave]=0;
        p[P_mpeOn]=1;
        for(int rank=0;rank<2;++rank)
        {
            p[chParam(rank,CP_initBrill)]=p[chParam(rank,CP_initLevel)]=0;
            p[chParam(rank,CP_afterBrill)]=p[chParam(rank,CP_afterLevel)]=0;
            p[chParam(rank,CP_saw)]=p[chParam(rank,CP_pulse)]=0;
            p[chParam(rank,CP_sine)]=.2f;
            p[chParam(rank,CP_vLevel)]=.25f;
            p[chParam(rank,CP_vA)]=p[chParam(rank,CP_vD)]=.001f;
            p[chParam(rank,CP_vS)]=1; p[chParam(rank,CP_vR)]=1;
        }
        return p;
    };
    for(double rate : {44100.0,48000.0,96000.0,192000.0})
    {
        const auto p=settings();
        auto start = [&](Engine& e) {
            e.prepare(rate); e.setParams(p);
            e.noteOn(60,.7f,2); e.noteOn(67,.7f,3);
        };
        auto same = [&](Engine& a,Engine& b,const char* message) {
            check(render(a,rate,1024)==render(b,rate,1024),message);
        };
        Engine a,b; start(a); start(b);
        a.polyPressure(60,.8f,2); a.polyPressure(67,.2f,3);
        b.polyPressure(67,.2f,3); b.polyPressure(60,.8f,2);
        same(a,b,"shared pressure speed is independent of unequal pressure event order");
        a.polyPressure(67,.4f,3);
        same(a,b,"a nondominant pressure change does not alter shared speed");
        a.polyPressure(60,0,2);
        b.polyPressure(60,.4f,2); b.polyPressure(67,0,3);
        same(a,b,"zeroing dominant pressure exposes the remaining held maximum");
        a.polyPressure(99,1,2); a.polyPressure(67,1,9);
        a.channelPressure(9,1);
        same(a,b,"unmatched note or channel pressure cannot override shared speed");

        Engine release,reference; start(release); start(reference);
        for(auto* e : {&release,&reference})
        {
            e->polyPressure(60,.8f,2); e->polyPressure(67,.2f,3);
        }
        same(release,reference,"shared speed release comparison starts identically");
        release.noteOff(60,2); reference.noteOff(60,2);
        reference.polyPressure(60,0,2);
        same(release,reference,"released dominant voice exposes the remaining held pressure");
        release.noteOff(67,3); reference.noteOff(67,3);
        reference.polyPressure(67,0,3);
        same(release,reference,"all released voices leave shared pressure neutral during tails");

        Engine pedal,pedalReference; start(pedal); start(pedalReference);
        for(auto* e : {&pedal,&pedalReference})
        {
            e->polyPressure(60,.8f,2); e->polyPressure(67,.2f,3);
            e->controlChange(64,1,2); e->controlChange(64,1,3);
        }
        same(pedal,pedalReference,"pedal comparison starts with identical physical pressure");
        pedal.noteOff(60,2); pedalReference.noteOff(60,2);
        pedalReference.polyPressure(60,0,2);
        same(pedal,pedalReference,"pedal-held sound does not retain released physical pressure");
        pedal.noteOff(67,3); pedalReference.noteOff(67,3);
        pedalReference.polyPressure(67,0,3);
        same(pedal,pedalReference,"all pedal-held voices leave shared pressure neutral");

        // Fill all circuits so the next assignment really reuses voice zero.
        // No samples elapse before reuse: the histories differ only in stale
        // pressure, not oscillator or smoothing state.
        Engine reuse,clean; reuse.prepare(rate); clean.prepare(rate);
        reuse.setParams(p); clean.setParams(p);
        for(int i=0;i<8;++i)
        {
            reuse.noteOn(60+i,.7f,2); clean.noteOn(60+i,.7f,2);
            reuse.polyPressure(60+i,.9f,2);
        }
        for(int i=0;i<8;++i)
        {
            reuse.noteOff(60+i,2); clean.noteOff(60+i,2);
        }
        reuse.noteOn(72,.7f,2); clean.noteOn(72,.7f,2);
        same(reuse,clean,"reused circuit clears stale pressure before entering shared bus");

        Engine sensitive,neutral; start(sensitive); start(neutral);
        sensitive.polyPressure(60,.8f,2);
        const auto activeAudio=render(sensitive,rate,4096);
        const auto neutralAudio=render(neutral,rate,4096);
        check(analyse(activeAudio).rms>1.e-5 && activeAudio!=neutralAudio,
              "shared speed test fixture is audible and sensitive to held pressure");
    }
}

void testEveryFactoryPresetRenders()
{
    Engine engine;
    engine.prepare(48000.0);
    int rendered = 0;
    for (const auto& preset : factoryPresets())
    {
        engine.reset();
        engine.allNotesOff(false);
        engine.setParams(preset.values);
        engine.noteOn(60, 0.75f, 1);
        const auto buffer = render(engine, 48000.0, 2048);
        const auto a = analyse(buffer);
        if (! a.finite || a.peak > 2.0f)
        {
            check(false, "preset renders cleanly: " + preset.name
                             + " (peak " + std::to_string(a.peak) + ")");
            break;
        }
        ++rendered;
    }
    check(rendered == (int) factoryPresets().size(), "all factory presets render cleanly");
}

} // namespace

int main()
{
    testParameterTable();
    testSineFrequency();
    testFeetRatios();
    testOscillatorDiscontinuities();
    testOscillatorComparatorPulse();
    testHardwareRingAndRibbon();
    testFilterStability();
    testEnvelope();
    testHardwareFilterContour();
    testDecimator();
    testFilterControlDomain();
    testSustainTimeAndWheelRouting();
    testSustainPedal();
    testAllSoundOff();
    testChannelPanic();
    testCoreInputGuards();
    testMpeExpression();
    testSubOscillatorWaves();
    testOriginalSubOscillator();
    testSubAftertouchVcf();
    testNominalInitialPitchTransient();
    testSharedPressureSpeedBus();
    testRepeatedNoteRelease();
    testHardwareVoiceAssignment();
    testVoiceAllocation();
    testRenderAndRelease();
    testVelocityResponse();
    testSampleRates();
    testArpeggiator();
    testArpeggiatorModes();
    testEffects();
    testMacros();
    testPresetDefaults();
    testFactoryBank();
    testEveryFactoryPresetRenders();

    if (failures == 0)
        std::printf("ALL %d CHECKS PASSED\n", checks);
    else
        std::printf("%d OF %d CHECKS FAILED\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
