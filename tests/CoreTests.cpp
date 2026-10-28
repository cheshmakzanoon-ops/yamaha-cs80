#include "core/Dsp.h"
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "core/Params.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
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
    check((int) P_numParams == (int) P_ch2Start + (int) CP_count, "parameter count");
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
            p[chParam(ch, CP_initLevel)] = 0.45f;
            p[chParam(ch, CP_initBrill)] = 0.2f;
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
    check(hard > soft * 1.5, "velocity changes level");
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
    testHardwareRingAndRibbon();
    testFilterStability();
    testEnvelope();
    testHardwareFilterContour();
    testDecimator();
    testFilterControlDomain();
    testSustainTimeAndWheelRouting();
    testSustainPedal();
    testAllSoundOff();
    testMpeExpression();
    testSubOscillatorWaves();
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
