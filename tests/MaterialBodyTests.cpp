// Signal-level material identity, time development and compatibility checks.
// These establish authored behavior, not equivalence to an acoustic recording.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace cs80::body;
constexpr double pi = 3.14159265358979323846;
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
double db(double value) { return 20.0 * std::log10(std::max(value, 1.e-15)); }
int baseModel(int material)
{
    return material <= agogo ? bell : material == musicBox ? celesta : tine;
}
struct Audio
{
    double rate;
    std::vector<float> samples;
    double rms(double begin, double end) const
    {
        const auto first = static_cast<std::size_t>(std::lround(begin * rate));
        const auto last = std::min(samples.size(), static_cast<std::size_t>(std::lround(end * rate)));
        double sum = 0.0;
        for (auto i = first; i < last; ++i) sum += double(samples[i]) * samples[i];
        return std::sqrt(sum / std::max(std::size_t{1}, last - first));
    }
};
Audio render(int model, int material, double note = 60, float velocity = .85f,
             double hostRate = 48000, double seconds = 1.4, float colour = .72f,
             float decay = 1.0f, int factor = 2)
{
    Audio result {hostRate * factor, {}};
    result.samples.resize(static_cast<std::size_t>(std::lround(seconds * result.rate)));
    InstrumentBody body;
    body.prepare(result.rate, hostRate);
    body.noteOn(model, static_cast<float>(note), velocity, colour, decay,
                0x412771u, 1.0f, standard, material);
    double peak = 0.0;
    bool finite = true;
    for (auto& value : result.samples)
    {
        value = body.tick(static_cast<float>(hz(note)), colour);
        finite = finite && std::isfinite(value);
        peak = std::max(peak, std::abs(double(value)));
    }
    check(finite && peak < .76, "material output remains finite within its absolute-mode bound");
    return result;
}
struct Spectrum
{
    double rate, weights = 0.0;
    std::vector<double> samples;
    Spectrum(const Audio& audio, double begin = .035, double end = .25) : rate(audio.rate)
    {
        const int first = static_cast<int>(std::lround(begin * rate));
        const int last = static_cast<int>(std::lround(end * rate));
        samples.reserve(static_cast<std::size_t>(last - first));
        for (int i = first; i < last; ++i)
        {
            const double w = .5 - .5 * std::cos(2.0 * pi * (i-first) / (last-first-1));
            weights += w;
            samples.push_back(w * audio.samples[static_cast<std::size_t>(i)]);
        }
    }
    double tone(double frequency) const
    {
        const double angle = 2.0 * pi * frequency / rate;
        const double c = std::cos(angle), s = std::sin(angle);
        double r = 1, im = 0, realSum = 0, imaginarySum = 0;
        for (double value : samples)
        {
            realSum += value * r; imaginarySum += value * im;
            const double next = r*c - im*s; im = im*c + r*s; r = next;
        }
        return 2.0 * std::hypot(realSum, imaginarySum) / weights;
    }
};

void materialIdentity()
{
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int note : {48, 60, 72})
        {
            const double f = hz(note);
            const auto genericAudio = render(bell, defaultMaterial, note, .85f, rate);
            const auto tubeAudio = render(bell, tubular, note, .85f, rate);
            const auto templeAudio = render(bell, temple, note, .85f, rate);
            const auto agogoAudio = render(bell, agogo, note, .85f, rate);
            const auto boxAudio = render(celesta, musicBox, note, .85f, rate);
            const auto kalimbaAudio = render(tine, kalimba, note, .85f, rate);
            const auto tineAudio = render(tine, defaultMaterial, note, .85f, rate);
            const Spectrum generic(genericAudio), tube(tubeAudio), bowl(templeAudio), shell(agogoAudio);
            const Spectrum box(boxAudio), thumb(kalimbaAudio), pickup(tineAudio);
            check(tube.tone(2*f) > 30 * generic.tone(2*f),
                  "tube has a prominent octave nominal absent in the old free-bar model");
            check(tube.tone(2*f) > 20 * tube.tone(f),
                  "tube retains a virtual strike pitch rather than adding a fake strong fundamental");
            check(tube.tone(2.88*f) > .12 * tube.tone(2*f)
                      && tube.tone(3.86*f) > .06 * tube.tone(2*f),
                  "tube has multiple audible upper modes supporting its nominal pitch");
            check(bowl.tone(2.361*f) > 25 * bowl.tone(2.756*f),
                  "temple shell does not retain the generic free-bar second mode");
            check(bowl.tone(2.361*f) > .10 * bowl.tone(f),
                  "temple shell has an audible independently pitched upper mode");
            check(shell.tone(1.873*f) > 25 * shell.tone(2.756*f),
                  "agogo uses compact shell spacing rather than the shared bell bar");
            check(box.tone(6.2669*f) > 35 * box.tone(2.756*f),
                  "music comb first overtone follows a cantilever rather than celesta bar spacing");
            const double kalimbaSecond = std::clamp(5.72-.12*std::log2(f/261.625565),5.3,5.9);
            check(thumb.tone(kalimbaSecond*f) > .04 * thumb.tone(f),
                  "kalimba has an audible inharmonic tine overtone");
            check(thumb.tone(kalimbaSecond*f) > 30 * thumb.tone(3*f),
                  "kalimba removes electric-pickup odd harmonics");
            check(pickup.tone(3*f) > 20 * pickup.tone(kalimbaSecond*f),
                  "unchanged EP negative control retains its pickup identity");
            const double shellTail = agogoAudio.rms(.90,1.20)/agogoAudio.rms(.035,.20);
            const double bowlTail = templeAudio.rms(.90,1.20)/templeAudio.rms(.035,.20);
            check(db(shellTail/bowlTail) < -7,
                  "agogo decays materially faster than the suspended temple body");
            std::printf("MATERIAL rate=%.0f note=%d tube_nominal=%.5f temple_upper=%.5f "
                        "music_comb=%.5f kalimba_upper=%.5f compact_tail=%+.3fdB\n",
                        rate,note,tube.tone(2*f),bowl.tone(2.361*f),box.tone(6.2669*f),
                        thumb.tone(kalimbaSecond*f),db(shellTail/bowlTail));
        }
}

void gongDevelopment()
{
    for (double rate : {44100.0,48000.0,96000.0})
        for (int note : {36,48,60})
        {
            const double f=hz(note);
            const auto audio=render(bell,gong,note,1.0f,rate,1.5);
            const Spectrum early(audio,.015,.080), late(audio,.70,1.15);
            const double opening=early.tone(5.137*f)/early.tone(f);
            const double bloom=late.tone(5.137*f)/late.tone(f);
            check(db(bloom/opening)>15,
                  "gong opens from its low thud into delayed upper energy at matched fundamental");
            check(late.tone(3.153*f)>.04*late.tone(f)
                      && late.tone(4.531*f)>.025*late.tone(f)
                      && late.tone(7.081*f)>.015*late.tone(f),
                  "gong contains several audible non-bar upper modes after the bloom");
            std::printf("GONG rate=%.0f note=%d upper_bloom=%+.3fdB\n",rate,note,db(bloom/opening));
        }
}

void touchAndRates()
{
    for (int material : {tubular,gong,temple,agogo,musicBox,kalimba})
    {
        const int model=baseModel(material);
        const auto soft=render(model,material,60,.2f,48000,.4);
        const auto firm=render(model,material,60,1.0f,48000,.4);
        check(firm.rms(.025,.35)>soft.rms(.025,.35),
              "firmer material excitation is not cancelled by a softer-strike normalizer");
        double reference=0;
        for(double rate : {48000.0,44100.0,96000.0})
        {
            const auto a=render(model,material,60,.85f,rate,.4);
            const Spectrum s(a,.04,.30);
            const double amplitude=s.tone(hz(60)*(material==tubular?2.0:1.0));
            if(rate==48000) reference=amplitude;
            else check(std::abs(db(amplitude/reference))<.04,
                       "material reference partial remains stable across supported sample rates");
        }
    }
}

void neutralAndLifecycle()
{
    for (int model=off;model<=maxModel;++model)
        for (int material : {-1,0,1,2,3,4,5,6,7,8,9,10,999})
        {
            if(compatibleMaterial(model,material)!=defaultMaterial) continue;
            InstrumentBody a,b;
            a.prepare(96000,48000); b.prepare(96000,48000);
            a.noteOn(model,60,.7f,.6f,1.2f,72,1.0f,standard);
            b.noteOn(model,60,.7f,.6f,1.2f,72,1.0f,standard,material);
            bool identical=true;
            for(int i=0;i<7000;++i)
            {
                const float pitch=static_cast<float>(hz(60)*(1+.003*std::sin(i*.001)));
                identical=(a.tick(pitch,.5f,.3f)==b.tick(pitch,.5f,.3f))&&identical;
            }
            check(identical,"default and unsupported material pairs preserve exact sample behavior");
        }
    for(int material : {tubular,gong,temple,agogo,musicBox,kalimba})
    {
        const int model=baseModel(material);
        InstrumentBody reused,fresh;
        reused.prepare(96000,48000); fresh.prepare(96000,48000);
        reused.noteOn(model,48,.8f,.6f,1.2f,91,1,standard,material);
        for(int i=0;i<1700;++i) (void)reused.tick(static_cast<float>(hz(48)),.6f);
        reused.noteOn(model,60,.8f,.6f,1.2f,92);
        fresh.noteOn(model,60,.8f,.6f,1.2f,92);
        bool identical=true;
        for(int i=0;i<7000;++i)
            identical=(reused.tick(static_cast<float>(hz(60)),.6f)==fresh.tick(static_cast<float>(hz(60)),.6f))&&identical;
        check(identical,"a subsequent standard note clears captured material state");
    }
}

void endpoints()
{
    for(int material : {tubular,gong,temple,agogo,musicBox,kalimba})
        for(double rate : {44100.0,48000.0,96000.0})
            for(int note : {0,24,96,127})
                for(float velocity : {0.0f,1.0f})
                    for(float colour : {0.0f,1.0f})
                        (void)render(baseModel(material),material,note,velocity,rate,.04,colour,4.0f,1);
    for(int material : {tubular,gong,temple,agogo,musicBox,kalimba})
    {
        InstrumentBody body;
        body.prepare(96000,48000);
        body.noteOn(baseModel(material),60,1,1,4,1928,1,standard,material);
        bool finite=true;
        for(int i=0;i<20000;++i)
        {
            const float frequency=i<10000?12000.0f:25.0f;
            const float x=body.tick(frequency,1);
            finite=finite&&std::isfinite(x)&&std::abs(x)<.76f;
        }
        check(finite,"large live pitch changes keep material output finite and bounded");
    }
    for(double rate : {44100.0,48000.0,96000.0})
    {
        const auto audio=render(celesta,musicBox,96,1.0f,rate,.45);
        const Spectrum spectrum(audio,.10,.40);
        const double fundamental=hz(96);
        // This comb mode lies above the host Nyquist on every tested rate,
        // despite the generator running at twice that rate.
        const double ultrasonic=34.3861*fundamental;
        check(spectrum.tone(ultrasonic)<1.e-5*spectrum.tone(fundamental),
              "host Nyquist suppresses the high comb mode at the oversampled generator");
    }
}
} // namespace

int main()
{
    materialIdentity(); gongDevelopment(); touchAndRates(); neutralAndLifecycle(); endpoints();
    std::printf("Material body: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
