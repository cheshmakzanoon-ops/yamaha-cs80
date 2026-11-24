// Public-render spectral contracts for the treble-only contact correction.
// Independent Hann/DFT measurements of output; no contact-weight equations.
// These broad authored boundaries protect existing bass/middle behavior and
// treble sparsity. They do not certify a particular acoustic piano.
#include "core/InstrumentBody.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
namespace
{
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
struct Spectrum { double fundamental = 0.0, upper = 0.0, peak = 0.0; bool finite = true; };
Spectrum render(double note, double velocity, double rate = 96000.0,
                double host = 48000.0, double colour = 0.65)
{
    cs80::body::InstrumentBody body;
    body.prepare(rate, host);
    body.noteOn(cs80::body::piano, float(note), float(velocity), float(colour), 1.0f, 0x7812a3u);
    const float hz = float(440.0 * std::exp2((note - 69.0) / 12.0));
    const int length = int(std::lround(rate * 0.25));
    std::vector<double> samples(static_cast<std::size_t>(length));
    Spectrum result;
    for (int i = 0; i < length; ++i)
    {
        const double y = body.tick(hz, float(colour));
        samples[static_cast<std::size_t>(i)] = y;
        result.finite = result.finite && std::isfinite(y);
        result.peak = std::max(result.peak, std::abs(y));
    }
    const int first = int(std::lround(rate * 0.060)), last = int(std::lround(rate * 0.120));
    double total = 0.0, windows = 0.0, windowSquares = 0.0;
    double fundamentalReal = 0.0, fundamentalImaginary = 0.0;
    double upperReal = 0.0, upperImaginary = 0.0;
    for (int i = first; i < last; ++i)
    {
        const double window = 0.5 - 0.5 * std::cos(2.0 * pi * (i - first) / (last - first));
        const double value = samples[static_cast<std::size_t>(i)] * window;
        const double phase = 2.0 * pi * hz * i / rate;
        fundamentalReal += value * std::cos(phase);
        fundamentalImaginary += value * std::sin(phase);
        // The broad h10 projection is used only for relative touch response;
        // it is not an exact-bin estimate of a measured inharmonic partial.
        upperReal += value * std::cos(10.0 * phase);
        upperImaginary += value * std::sin(10.0 * phase);
        total += value * value;
        windows += window;
        windowSquares += window * window;
    }
    if (total > 0.0)
    {
        const double factor = 2.0 * windowSquares / (windows * windows * total);
        result.fundamental = factor * (fundamentalReal * fundamentalReal + fundamentalImaginary * fundamentalImaginary);
        result.upper = factor * (upperReal * upperReal + upperImaginary * upperImaginary);
    }
    return result;
}
}
int main()
{
    for (double velocity : {0.2, 0.6, 0.95})
    {
        const auto bass = render(48, velocity), middle = render(60, velocity), treble = render(72, velocity);
        check(bass.finite && middle.finite && treble.finite, "register renders are finite");
        check(bass.fundamental > 0.30 && bass.fundamental < 0.55,
              "established bass balance is retained (no rejected bass contour)");
        check(middle.fundamental > 0.40 && middle.fundamental < 0.70,
              "middle C remains outside the treble correction");
        check(treble.fundamental > 0.75, "upper piano register has a sparse spectrum");
        check(treble.fundamental > middle.fundamental + 0.20,
              "finite-Hz contact reduces relative treble harmonics");
    }
    const auto gentleTreble = render(72, 0.2), firmTreble = render(72, 0.95);
    check(gentleTreble.fundamental > firmTreble.fundamental,
          "soft treble contact suppresses relative upper partials");
    for (double host : {44100.0, 48000.0, 96000.0})
        for (double oversampling : {1.0, 2.0})
            for (int note : {60, 72})
            {
                const auto reference = render(note, 0.6);
                const auto alternate = render(note, 0.6, host * oversampling, host);
                check(std::abs(reference.fundamental - alternate.fundamental) < 0.02,
                      "spectral balance is independent of host/internal rate");
            }
    for (int note : {21, 43, 60, 72, 84, 108})
        for (double velocity : {0.0, 0.5, 1.0})
            for (double colour : {0.0, 1.0})
            {
                const auto rendered = render(note, velocity, 96000.0, 48000.0, colour);
                check(rendered.finite && rendered.peak <= 0.74801,
                      "extreme registers retain the modal plus hammer bound");
            }
    for (double knee : {60.0, 72.0})
    {
        const auto lower = render(knee - 0.001, 0.6), upper = render(knee + 0.001, 0.6);
        check(std::abs(lower.fundamental - upper.fundamental) < 0.001,
              "contact transition introduces no split-key spectral seam");
    }
    std::printf("Piano treble contact: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
