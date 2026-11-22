#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

using namespace cs80;
namespace {
constexpr int rate = 48000, holdFrames = rate * 10, totalFrames = rate * 18;
constexpr std::array<int,8> notes{36,43,48,52,55,60,64,67};
constexpr std::array<const char*,12> names{
    "Deep Space", "Sci-Fi Drone", "Brass Crescendo", "Low Drone",
    "Evolving Pad", "Sweep Riser", "Night Pad", "Cathedral Pad",
    "Ether Pad", "Brass Pad Swell", "Sweep Pad", "Slow Pad"};
double db(double x) { return 20.0 * std::log10(std::max(1e-15, x)); }
struct Measurement {
    double energy = 0.0, peak = 0.0;
    long long samples = 0, nonfinite = 0, over = 0;
    void add(float sample) {
        ++samples;
        if (!std::isfinite(sample)) { ++nonfinite; return; }
        const double x = sample;
        peak = std::max(peak, std::abs(x));
        energy += x * x;
        if (std::abs(x) > 1.0) ++over;
    }
    double rms() const { return std::sqrt(energy / std::max(1LL, samples)); }
};
}
int main() {
    std::ofstream report("build-w08-long-sustain/results.csv");
    std::ofstream windows("build-w08-long-sustain/windows.csv");
    if (!report || !windows) return 2;
    report << std::setprecision(12)
        << "name,peak_dbfs,peak_seconds,early_peak_dbfs,later_held_peak_dbfs,release_peak_dbfs,early_rms_dbfs,late_rms_dbfs,tail_rms_dbfs,late_minus_early_db,tail_minus_late_db,nonfinite,over_full_scale,max_active_voices,held_min_active_voices,final_active_voices,retired_seconds,checks,failures\n";
    windows << std::setprecision(12) << "name,begin_seconds,end_seconds,rms_dbfs,peak_dbfs,active_voices\n";
    int checks = 0, failures = 0;
    for (const char* name : names) {
        const auto& bank = curatedLibrary();
        const auto found = std::find_if(bank.begin(), bank.end(),
            [&](const auto& p) { return p.name == name; });
        if (found == bank.end()) return 3;
        auto engine = std::make_unique<Engine>();
        engine->prepare(rate);
        engine->setParams(found->values);
        const auto check = [&](bool condition) { ++checks; if (!condition) ++failures; };
        const int startChecks = checks, startFailures = failures;
        check(engine->getRenderPolicy() == RenderPolicy::sourcePerformance);
        for (const int note : notes) engine->noteOn(note, 0.9f, 1);
        std::array<float,256> block{};
        Measurement all, early, late, tail, laterHeld, release, window;
        int nextPressure = 0, nextWindow = rate / 4, maxVoices = 0, heldMinVoices = 8;
        double peakSeconds = 0.0, retiredSeconds = -1.0;
        bool released = false;
        for (int at = 0; at < totalFrames;) {
            if (at == nextPressure) {
                const double t = double(at) / rate;
                const float pressure = t < 1.0 ? 0.0f : t < 3.0 ? float((t - 1.0) / 2.0) : 1.0f;
                engine->channelPressure(1, pressure);
                nextPressure += rate / 100;
            }
            if (!released && at == holdFrames) {
                for (const int note : notes) engine->noteOff(note, 1);
                released = true;
            }
            int count = std::min(128, totalFrames - at);
            count = std::min(count, nextPressure - at);
            count = std::min(count, nextWindow - at);
            if (!released) count = std::min(count, holdFrames - at);
            if (count <= 0) return 4;
            engine->render(block.data(), count);
            for (int i = 0; i < count; ++i) {
                const int frame = at + i;
                for (int channel = 0; channel < 2; ++channel) {
                    const float sample = block[2 * i + channel];
                    if (std::isfinite(sample) && std::abs(double(sample)) > all.peak)
                        peakSeconds = double(frame) / rate;
                    all.add(sample); window.add(sample);
                    if (frame < 2 * rate) early.add(sample);
                    if (frame >= 7 * rate && frame < holdFrames) late.add(sample);
                    if (frame >= 17 * rate) tail.add(sample);
                    if (frame >= 2 * rate && frame < holdFrames) laterHeld.add(sample);
                    if (frame >= holdFrames) release.add(sample);
                }
            }
            at += count;
            const int active = engine->activeVoices();
            maxVoices = std::max(maxVoices, active);
            if (at <= holdFrames) heldMinVoices = std::min(heldMinVoices, active);
            if (released && active == 0 && retiredSeconds < 0.0) retiredSeconds = double(at) / rate;
            if (at == nextWindow) {
                windows << name << ',' << double(at - rate / 4) / rate << ','
                        << double(at) / rate << ',' << db(window.rms()) << ','
                        << db(window.peak) << ',' << active << '\n';
                window = Measurement{};
                nextWindow += rate / 4;
            }
        }
        check(all.nonfinite == 0);
        check(all.over == 0);
        check(maxVoices == 8);
        check(heldMinVoices == 8);
        check(engine->activeVoices() == 0);
        check(late.rms() > 0.0001);
        check(tail.rms() < late.rms() * std::pow(10.0, -12.0 / 20.0));
        report << name << ',' << db(all.peak) << ',' << peakSeconds << ','
               << db(early.peak) << ',' << db(laterHeld.peak) << ',' << db(release.peak) << ','
               << db(early.rms()) << ',' << db(late.rms()) << ',' << db(tail.rms()) << ','
               << db(late.rms()) - db(early.rms()) << ',' << db(tail.rms()) - db(late.rms()) << ','
               << all.nonfinite << ',' << all.over << ',' << maxVoices << ',' << heldMinVoices << ','
               << engine->activeVoices() << ',' << retiredSeconds << ','
               << checks - startChecks << ',' << failures - startFailures << '\n';
        report.flush(); windows.flush();
        std::cout << name << ": peak " << db(all.peak) << " dBFS at " << peakSeconds
                  << " s; late " << db(late.rms()) << " dBFS; tail " << db(tail.rms())
                  << " dBFS; voices " << maxVoices << "/" << engine->activeVoices()
                  << "; failures " << failures - startFailures << std::endl;
    }
    std::cout << "Long-sustain matrix: " << checks - failures << '/' << checks
              << " checks; " << names.size() * 18 << " generated seconds" << std::endl;
    return failures == 0 ? 0 : 1;
}
