// Rendered radiation-envelope regression for moving-pitch woodwinds and brass.
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
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
double db(double value) { return 20.0 * std::log10(std::max(value, 1.e-15)); }
struct Audio
{
    double rate;
    std::vector<float> samples;
    double rms(double begin, double end) const
    {
        const int first = (int) std::lround(begin * rate), last = (int) std::lround(end * rate);
        double sum = 0.0;
        for (int i = first; i < last; ++i) sum += (double) samples[(size_t) i] * samples[(size_t) i];
        return std::sqrt(sum / (last - first));
    }
};
Audio render(int model, double from, double to, double hostRate = 48000.0,
             float size = 1.0f, float velocity = .85f, float colour = .5f,
             float pressure = 0.0f, double seconds = 1.55)
{
    Audio a {2.0 * hostRate, {}};
    a.samples.resize((size_t) std::lround(seconds * a.rate));
    InstrumentBody body;
    body.prepare(a.rate, hostRate);
    body.noteOn(model, (float) from, velocity, colour, 1.0f, 0x237165eu, size);
    double peak = 0.0; bool finite = true;
    for (size_t i = 0; i < a.samples.size(); ++i)
    {
        const double time = i / a.rate;
        const double slide = std::clamp((time - .25) / .35, 0.0, 1.0);
        const float pitch = (float) hz(from + (to - from) * slide);
        const float value = body.tick(pitch, colour, time > .25 ? pressure : 0.0f);
        a.samples[i] = value;
        finite = finite && std::isfinite(value); peak = std::max(peak, std::abs((double) value));
    }
    check(finite && peak < .76, "wind excitation stays finite and inside the mode-sum bound");
    return a;
}
struct Spectrum
{
    double rate, weights = 0.0;
    std::vector<double> window;
    Spectrum(const Audio& a, double begin = .90, double end = 1.50) : rate(a.rate)
    {
        const int first = (int) std::lround(begin * rate), last = (int) std::lround(end * rate);
        window.resize((size_t) (last - first));
        for (int i = first; i < last; ++i)
        {
            const double w = .5 - .5 * std::cos(2.0 * pi * (i - first) / (last - first - 1));
            weights += w; window[(size_t) (i - first)] = a.samples[(size_t) i] * w;
        }
    }
    double tone(double frequency) const
    {
        const double angle = 2.0 * pi * frequency / rate;
        const double c = std::cos(angle), s = std::sin(angle);
        double r = 1.0, im = 0.0, realSum = 0.0, imaginarySum = 0.0;
        for (double value : window)
        {
            realSum += r * value; imaginarySum += im * value;
            const double next = r * c - im * s; im = im * c + r * s; r = next;
        }
        return 2.0 * std::hypot(realSum, imaginarySum) / weights;
    }
};

void tracking()
{
    double worst = 0;
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int model : {flute, oboe, bassoon, brass})
            for (double size : {.65, 1.0, 1.75})
                for (double interval : {-12.0, 7.0, 12.0})
                {
                    const double from = 60, to = from + interval;
                    const Spectrum moving(render(model, from, to, rate, (float) size));
                    const Spectrum fresh(render(model, to, to, rate, (float) size));
                    const double pitch = (float) hz(to);
                    const double a1=moving.tone(pitch), b1=fresh.tone(pitch);
                    check(a1 > .001 && b1 > .001, "both gestures retain their sounded fundamental");
                    for (int h : {2,3,4,5,7,9})
                    {
                        const double target=fresh.tone(h*pitch)/b1;
                        if (target < .002) continue;
                        const double actual=moving.tone(h*pitch)/a1;
                        const double error=std::abs(db(actual/target));
                        worst=std::max(worst,error);
                        check(error < .23, "bent wind/brass matches the destination radiation envelope");
                    }
                }
    std::printf("WIND tracking worst normalized harmonic error %.6f dB\n",worst);
}
void returnToPitch()
{
    for (int model : {flute, clarinet, oboe, bassoon, brass})
    {
        InstrumentBody steady, moving;
        steady.prepare(96000,48000); moving.prepare(96000,48000);
        steady.noteOn(model,60,.8f,.5f,1,123); moving.noteOn(model,60,.8f,.5f,1,123);
        double peak=0, step=0; float previous=0; bool finite=true;
        Audio aa{96000,{}}, bb{96000,{}}; aa.samples.reserve(192000); bb.samples.reserve(192000);
        for(int i=0;i<192000;++i)
        {
            const double semis=i>24000 && i<48000 ? 12*std::sin(pi*(i-24000)/24000.) : 0.;
            const float x=moving.tick((float)hz(60+semis),.5f,.35f);
            const float y=steady.tick((float)hz(60),.5f,.35f);
            aa.samples.push_back(x); bb.samples.push_back(y);
            finite=finite && std::isfinite(x);
            peak=std::max(peak,std::abs((double)x));
            step=std::max(step,std::abs((double)x-previous)); previous=x;
        }
        check(finite,"bent and pressure-driven audio is finite");
        check(peak<.78,"bends respect absolute-mode ceiling");
        check(step<.15,"smooth pitch movement has no large gain discontinuity");
        Spectrum bent(aa), reference(bb);
        const double f=(float)hz(60), a1=bent.tone(f), b1=reference.tone(f);
        for(int h : {2,3,4,5,7})
            if(reference.tone(h*f)/b1>.002)
                check(std::abs(db((bent.tone(h*f)/a1)/(reference.tone(h*f)/b1)))<.10,
                      "returning to onset pitch restores the original spectral envelope");
    }
}
}
int main()
{
    tracking(); returnToPitch();
    std::printf("Wind body formants: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
