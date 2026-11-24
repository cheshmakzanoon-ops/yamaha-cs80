// Bounded whole-factory checks omitted by the short A13/A31 response windows:
// long bows, pressure return, eight-note sections, slot reuse, and overlapped
// note releases. No recipe/control/master/effect overrides are applied.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cs80;
namespace {
constexpr int rate = 44100;
int checks = 0, failures = 0, renders = 0;
long long totalFrames = 0;
double maximumPeak = 0.0;
void check(bool ok, const std::string& label) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL,%s\n", label.c_str()); }
}
double db(double v) { return 20.0 * std::log10(std::max(v, 1e-15)); }
const FactoryPreset& preset(const char* name) {
    for (const auto& p : curatedLibrary()) if (p.name == name) return p;
    throw std::runtime_error(std::string("missing preset ") + name);
}
struct Event {
    int frame, kind, note;
    float value;
};
Event event(double seconds, int kind, int note = 0, float value = 0.0f) {
    return {static_cast<int>(std::llround(seconds * rate)), kind, note, value};
}
struct Audio {
    std::vector<float> samples;
    double peak = 0.0;
    bool finite = true;
    int finalActive = 0, finalHeld = 0;
    double rms(double from, double to) const {
        const int first = static_cast<int>(std::llround(from * rate));
        const int last = static_cast<int>(std::llround(to * rate));
        double power = 0.0;
        for (int i = first; i < last; ++i) {
            double l = samples[static_cast<std::size_t>(i) * 2];
            double r = samples[static_cast<std::size_t>(i) * 2 + 1];
            power += 0.5 * (l * l + r * r);
        }
        return std::sqrt(power / (last - first));
    }
};
Audio render(const FactoryPreset& p, std::vector<Event> events, double duration,
             const std::string& label) {
    auto engine = std::make_unique<Engine>();
    engine->prepare(rate);
    engine->setParams(p.values);
    Audio audio;
    const int frames = static_cast<int>(std::llround(duration * rate));
    audio.samples.resize(static_cast<std::size_t>(frames) * 2);
    std::stable_sort(events.begin(), events.end(), [](auto a, auto b) { return a.frame < b.frame; });
    std::set<int> expectedHeld;
    std::size_t index = 0;
    int inspectAt = -1;
    for (int at = 0; at < frames;) {
        while (index < events.size() && events[index].frame == at) {
            const auto e = events[index++];
            if (e.kind == 1) { engine->noteOn(e.note, e.value, 1); expectedHeld.insert(e.note); }
            else if (e.kind == 0) { engine->noteOff(e.note, 1); expectedHeld.erase(e.note); }
            else engine->channelPressure(1, e.value);
            // Let the real key/touch path advance before inspecting ownership.
            inspectAt = at + rate / 100;
        }
        if (at == inspectAt) {
            std::set<int> actualHeld;
            int heldVoices = 0;
            for (int v = 0; v < 8; ++v) {
                const auto voice = engine->getVoiceInfo(v);
                if (voice.active && voice.held) { actualHeld.insert(voice.note); ++heldVoices; }
            }
            check(actualHeld == expectedHeld && heldVoices == static_cast<int>(expectedHeld.size()),
                  label + ": exact independent held-note ownership at " + std::to_string(at));
            inspectAt = -1;
        }
        int until = frames;
        if (index < events.size()) until = std::min(until, events[index].frame);
        if (inspectAt > at) until = std::min(until, inspectAt);
        const int count = std::min(113, until - at);
        if (count <= 0) throw std::runtime_error("non-advancing event stream");
        engine->render(audio.samples.data() + static_cast<std::size_t>(at) * 2, count);
        at += count;
    }
    for (float sample : audio.samples) {
        audio.finite = audio.finite && std::isfinite(sample);
        audio.peak = std::max(audio.peak, std::abs(static_cast<double>(sample)));
    }
    audio.finalActive = engine->activeVoices();
    for (int v = 0; v < 8; ++v) audio.finalHeld += engine->getVoiceInfo(v).held ? 1 : 0;
    check(audio.finite, label + ": finite stereo render");
    check(audio.peak < 1.0, label + ": complete factory output remains below full scale");
    check(audio.finalHeld == 0, label + ": every released key is unheld");
    check(audio.finalActive == 0, label + ": release tails retire all voices");
    ++renders;
    totalFrames += frames;
    maximumPeak = std::max(maximumPeak, audio.peak);
    return audio;
}

void longBow(const char* name, std::vector<int> notes) {
    std::vector<Event> events;
    for (int note : notes) events.push_back(event(0.0, 1, note, 0.80f));
    events.push_back(event(5.0, 2, 0, 0.90f));
    events.push_back(event(8.0, 2, 0, 0.0f));
    for (int note : notes) events.push_back(event(12.0, 0, note));
    const std::string label = std::string(name) + (notes.size() == 1 ? " note " + std::to_string(notes[0]) : " eight-note section");
    const auto audio = render(preset(name), events, 16.0, label);
    const double before = audio.rms(3.5, 4.5);
    const double pressed = audio.rms(6.5, 7.5);
    const double returned = audio.rms(10.0, 11.5);
    const double tail = audio.rms(15.5, 15.95);
    const double returnDb = db(returned / std::max(before, 1e-15));
    check(returned > 1e-5, label + ": held bow remains audible at 10-11.5 seconds");
    check(std::abs(returnDb) < 6.0, label + ": eased bow retains level within 6 dB of earlier steady bow");
    check(tail < returned * 0.01, label + ": released tail is over 40 dB below held body");
    std::printf("LONG,%s,%d,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%d,%d\n",
        name, static_cast<int>(notes.size()), notes[0], db(audio.peak), db(before), db(pressed),
        db(returned), returnDb, db(tail / std::max(returned, 1e-15)), audio.finalActive, audio.finalHeld);
}

void repeated(const char* name, int note) {
    std::vector<Event> events;
    for (int i = 0; i < 12; ++i) {
        const double t = i * 0.25;
        events.push_back(event(t, 1, note, i % 2 == 0 ? 0.25f : 0.90f));
        events.push_back(event(t + 0.15, 0, note));
    }
    const std::string label = std::string(name) + " repeated note " + std::to_string(note);
    const auto audio = render(preset(name), events, 5.9, label);
    for (int pair = 0; pair < 6; ++pair) {
        const double t = pair * 0.5;
        const double soft = audio.rms(t + 0.025, t + 0.125);
        const double firm = audio.rms(t + 0.275, t + 0.375);
        check(soft > 1e-5 && firm > 1e-5, label + ": both strokes remain audible through voice-slot reuse");
        check(firm > soft * std::sqrt(2.0), label + ": firm repeated stroke retains over 3 dB effort contrast");
        std::printf("REPEAT,%s,%d,%d,%.9f,%.9f,%.9f,%.9f\n",
            name, note, pair + 1, db(audio.peak), db(soft), db(firm), db(firm / std::max(soft, 1e-15)));
    }
    check(audio.rms(5.4, 5.85) < 1e-5, label + ": no remaining sounding bow after all note-offs");
}

void overlapping(const char* name, int first) {
    std::vector<Event> events;
    for (int i = 0; i < 13; ++i) {
        const double t = i * 0.18;
        events.push_back(event(t, 1, first + i, 0.75f));
        events.push_back(event(t + 0.24, 0, first + i));
    }
    const std::string label = std::string(name) + " overlapping line from " + std::to_string(first);
    const auto audio = render(preset(name), events, 5.4, label);
    double minimum = 1.0, maximum = 0.0;
    for (int i = 0; i < 13; ++i) {
        const double t = i * 0.18;
        const double body = audio.rms(t + 0.075, t + 0.155);
        minimum = std::min(minimum, body);
        maximum = std::max(maximum, body);
        check(body > 1e-5, label + ": each overlapped step retains audible output after previous note-off");
    }
    check(audio.rms(4.9, 5.35) < 1e-5, label + ": overlapped phrase finishes without a stuck bow");
    std::printf("OVERLAP,%s,%d,%.9f,%.9f,%.9f,%d,%d\n",
        name, first, db(audio.peak), db(minimum), db(maximum), audio.finalActive, audio.finalHeld);
}
} // namespace

int main() {
    try {
        for (const char* name : {"Solo Violin", "Solo Cello", "Chamber Strings", "Slow Strings",
                                 "Bowed Swell Strings", "Tutti Strings", "Marcato Strings", "Swell Strings"}) {
            // The cello/tutti 16' ranks sound one octave below these keys.
            for (int note : {48, 84}) longBow(name, {note});
        }
        for (const char* name : {"Chamber Strings", "Slow Strings", "Bowed Swell Strings", "Tutti Strings"})
            longBow(name, {48, 52, 55, 59, 62, 65, 69, 72});
        for (const char* name : {"Solo Violin", "Solo Cello", "Spiccato Strings", "Marcato Strings"}) {
            repeated(name, 60);
            overlapping(name, 55);
        }
    } catch (const std::exception& e) { check(false, e.what()); }
    std::printf("SUMMARY,checks=%d,failures=%d,renders=%d,stereo_frames=%lld,max_peak_dbfs=%.9f\n",
        checks, failures, renders, totalFrames, db(maximumPeak));
    return failures ? 1 : 0;
}
