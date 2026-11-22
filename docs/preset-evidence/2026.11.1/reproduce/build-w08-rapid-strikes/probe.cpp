#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "tools/TruthInputSha256.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cs80;
namespace {
constexpr int rate = 48000;
constexpr int total = rate * 12;
constexpr int omittedId = 25;
int checks = 0, failures = 0;
int frame(double t) { return int(std::lround(t * rate)); }
double db(double x) { return 20.0 * std::log10(std::max(x, 1e-15)); }
void check(bool condition, const std::string& message) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL " << message << '\n'; }
}
const FactoryPreset& find(const std::string& name) {
    const auto& bank = curatedLibrary();
    auto it = std::find_if(bank.begin(), bank.end(), [&](const auto& p) { return p.name == name; });
    if (it == bank.end()) throw std::runtime_error("missing preset " + name);
    return *it;
}
struct Event { int at, type, note, id; float velocity; }; // type 0 on, 1 off, 2 all-off
std::vector<Event> makeEvents(int note) {
    std::vector<Event> events;
    int id = 0;
    auto add = [&](double t, int n, float v, double hold) {
        events.push_back({frame(t), 0, n, id, v});
        events.push_back({frame(t + hold), 1, n, id, 0});
        ++id;
    };
    for (int i = 0; i < 4; ++i) add(.45 * i, note, i % 2 ? .95f : .3f, .12);
    constexpr std::array<int, 12> arp{0, 4, 7, 12, 7, 4, 0, 7, 12, 16, 12, 7};
    for (int i = 0; i < 12; ++i) add(2.0 + .09 * i, note + arp[i], i % 2 ? .95f : .3f, .035);
    constexpr std::array<int, 16> burst{0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 16, 14, 12, 11};
    for (int i = 0; i < 16; ++i) add(3.6 + .01 * i, note + burst[i], i % 2 ? .95f : .3f, .006);
    events.push_back({frame(4.05), 2, 0, -1, 0});
    std::stable_sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.at < b.at; });
    return events;
}
struct Render {
    std::vector<float> audio;
    int maxVoices = 0, finalVoices = 0, finalHeld = 0, accepted = 0, reclaims = 0;
    int burstReclaims = 0, probePriorActive = 0, probePriorHeld = 0, probeSlot = -1;
    double retired = -1;
    bool paramsMatch = true;
};
Render render(const FactoryPreset& p, int note, bool control, std::ofstream& onset) {
    auto engine = std::make_unique<Engine>();
    engine->prepare(rate); engine->setParams(p.values);
    check(engine->getRenderPolicy() == RenderPolicy::sourcePerformance, p.name + " Circuit policy");
    Render result; result.audio.resize(total * 2);
    const auto events = makeEvents(note);
    size_t next = 0;
    int watchedSlot = -1, watchedId = -1, watchBegin = 0;
    double maxBodyEnvelope = 0, maxStrikeEnvelope = 0;
    auto finishWatch = [&] {
        if (watchedId < 0) return;
        onset << p.name << ',' << (control ? "omitted-onset-control" : "factory") << ','
              << watchedId << ',' << double(watchBegin) / rate << ','
              << maxBodyEnvelope << ',' << maxStrikeEnvelope << '\n';
        check(std::isfinite(maxBodyEnvelope) && std::isfinite(maxStrikeEnvelope)
              && maxBodyEnvelope > 0 && maxStrikeEnvelope > 0,
              p.name + " repeated strike produces both finite envelope attacks");
        watchedId = -1;
    };
    for (int at = 0; at < total;) {
        while (next < events.size() && events[next].at == at) {
            const auto ev = events[next++];
            if (control && ev.id == omittedId) continue;
            if (ev.type == 0) {
                std::array<Engine::VoiceInfo, 8> before;
                for (int i = 0; i < 8; ++i) before[i] = engine->getVoiceInfo(i);
                engine->noteOn(ev.note, ev.velocity, 1);
                int allocated = -1;
                for (int i = 0; i < 8; ++i) {
                    const auto after = engine->getVoiceInfo(i);
                    if (after.active && after.held && after.note == ev.note) allocated = i;
                }
                if (allocated >= 0) {
                    ++result.accepted;
                    if (before[allocated].active && !before[allocated].held) {
                        ++result.reclaims;
                        if (ev.id >= 16) ++result.burstReclaims;
                    }
                    if (ev.id == omittedId) {
                        result.probeSlot = allocated;
                        result.probePriorActive = before[allocated].active;
                        result.probePriorHeld = before[allocated].held;
                    }
                }
                if (ev.id < 4) {
                    finishWatch();
                    watchedSlot = allocated; watchedId = ev.id; watchBegin = at;
                    maxBodyEnvelope = 0; maxStrikeEnvelope = 0;
                }
            } else if (ev.type == 1) engine->noteOff(ev.note, 1);
            else engine->allNotesOff(true);
            result.maxVoices = std::max(result.maxVoices, engine->activeVoices());
        }
        if (watchedId >= 0 && at >= watchBegin + frame(.04)) finishWatch();
        int count = std::min(64, total - at);
        if (next < events.size()) count = std::min(count, events[next].at - at);
        if (watchedId >= 0) count = std::min(count, watchBegin + frame(.04) - at);
        if (count <= 0) throw std::runtime_error("timeline did not advance");
        engine->render(result.audio.data() + 2 * at, count); at += count;
        if (watchedId >= 0 && watchedSlot >= 0) {
            maxBodyEnvelope = std::max(maxBodyEnvelope, double(engine->getRankInfo(watchedSlot, 0).ampEnvelope));
            maxStrikeEnvelope = std::max(maxStrikeEnvelope, double(engine->getRankInfo(watchedSlot, 1).ampEnvelope));
        }
        result.maxVoices = std::max(result.maxVoices, engine->activeVoices());
        if (at >= frame(3.756) && engine->activeVoices() == 0 && result.retired < 0)
            result.retired = double(at) / rate;
    }
    finishWatch();
    result.finalVoices = engine->activeVoices();
    for (int i = 0; i < 8; ++i) result.finalHeld += engine->getVoiceInfo(i).held;
    result.paramsMatch = engine->getParams().v == p.values.v;
    return result;
}
double rms(const std::vector<float>& audio, double begin, double end) {
    double power = 0;
    const int a = frame(begin) * 2, b = frame(end) * 2;
    for (int i = a; i < b; ++i) power += double(audio[i]) * audio[i];
    return std::sqrt(power / (b - a));
}
void report(const FactoryPreset& p, int note, const Render& r, bool control, std::ofstream& f) {
    double peak = 0; int nonfinite = 0, over = 0;
    for (const auto sample : r.audio) {
        nonfinite += !std::isfinite(sample); over += std::abs(sample) > 1.f;
        peak = std::max(peak, std::abs(double(sample)));
    }
    const auto hash = tools::inputSha256(std::string_view(reinterpret_cast<const char*>(r.audio.data()), r.audio.size() * sizeof(float)));
    f << p.name << ',' << note << ',' << (control ? "omitted-onset-control" : "factory") << ",12,"
      << db(peak) << ',' << db(rms(r.audio, 0, 12)) << ',' << db(rms(r.audio, 11.5, 12)) << ','
      << nonfinite << ',' << over << ',' << r.accepted << ',' << r.reclaims << ',' << r.burstReclaims << ','
      << r.maxVoices << ',' << r.finalVoices << ',' << r.finalHeld << ',' << r.retired << ',' << r.paramsMatch << ',' << hash << '\n';
    check(nonfinite == 0 && over == 0, p.name + " finite output with headroom");
    check(r.accepted == (control ? 31 : 32), p.name + " every requested onset accepted");
    check(r.maxVoices == 8 && r.burstReclaims > 0, p.name + " exercises eight physical voices and released-slot reuse");
    check(r.finalVoices == 0 && r.finalHeld == 0 && r.retired > 0, p.name + " voices retire after final release");
    check(r.paramsMatch, p.name + " factory parameters unchanged");
    std::cout << p.name << ' ' << (control ? "control" : "factory") << " peak " << db(peak)
              << " dBFS, burst reclaims " << r.burstReclaims << ", retired " << r.retired << " s\n";
}
}
int main() { try {
    const std::string out = "build-w08-rapid-strikes/";
    std::ofstream captures(out + "captures.csv"), attacks(out + "envelopes.csv"), boundaries(out + "boundaries.csv"), events(out + "events.csv"), onsetLevels(out + "onset-levels.csv");
    for (auto* f : {&captures, &attacks, &boundaries, &events, &onsetLevels}) *f << std::setprecision(12);
    captures << "program,note,variant,seconds,peak_dbfs,rms_dbfs,last_half_second_dbfs,nonfinite,over_full_scale,accepted_onsets,released_slot_reuses,burst_reuses,max_active_voices,final_active_voices,final_held_voices,retired_seconds,params_unchanged,raw_float32le_sha256\n";
    attacks << "program,variant,event_id,seconds,max_body_envelope_first40ms,max_strike_envelope_first40ms\n";
    boundaries << "program,event_seconds,note,slot,prior_slot_active,prior_slot_held,prefix_max_error,event_sample_max_error,first_ms_residual_peak,residual_derivative_first_ms,control_derivative_first_ms,residual_peak_first6ms\n";
    events << "program,seconds,type,note,velocity,event_id\n";
    onsetLevels << "program,event_id,velocity,onset_5to40ms_dbfs,pre_event_20ms_dbfs\n";
    for (const std::string name : {"Harp", "Kalimba", "Sine Bass", "Upright Pluck"}) {
        const int note = (name == "Harp" || name == "Kalimba") ? 60 : 48;
        const auto& p = find(name);
        for (const auto& ev : makeEvents(note)) events << name << ',' << double(ev.at) / rate << ',' << ev.type << ',' << ev.note << ',' << ev.velocity << ',' << ev.id << '\n';
        auto real = render(p, note, false, attacks), control = render(p, note, true, attacks);
        report(p, note, real, false, captures); report(p, note, control, true, captures);
        const int at = frame(3.69);
        double prefixError = 0, eventError = 0, firstMs = 0, residualDerivative = 0, controlDerivative = 0, first6ms = 0;
        for (int i = 0; i < at * 2; ++i) prefixError = std::max(prefixError, std::abs(double(real.audio[i]) - control.audio[i]));
        for (int i = at * 2; i < (at + frame(.006)) * 2; ++i) {
            const double delta = double(real.audio[i]) - control.audio[i];
            first6ms = std::max(first6ms, std::abs(delta));
            if (i < (at + 1) * 2) eventError = std::max(eventError, std::abs(delta));
            if (i < (at + frame(.001)) * 2) {
                firstMs = std::max(firstMs, std::abs(delta));
                residualDerivative = std::max(residualDerivative, std::abs(delta - (double(real.audio[i-2]) - control.audio[i-2])));
                controlDerivative = std::max(controlDerivative, std::abs(double(control.audio[i]) - control.audio[i-2]));
            }
        }
        boundaries << name << ",3.69," << note + 16 << ',' << real.probeSlot << ',' << real.probePriorActive << ',' << real.probePriorHeld << ','
                   << prefixError << ',' << eventError << ',' << firstMs << ',' << residualDerivative << ',' << controlDerivative << ',' << first6ms << '\n';
        check(prefixError == 0, name + " matched control identical before omitted onset");
        check(eventError < 1e-7, name + " no immediate output jump at loaded onset");
        check(first6ms > 1e-6, name + " loaded onset causally changes output");
        check(real.probePriorActive == 1 && real.probePriorHeld == 0, name + " comparison boundary reuses an active released slot");
        for (int id = 0; id < 4; ++id) {
            const double t = .45 * id;
            onsetLevels << name << ',' << id << ',' << (id % 2 ? .95 : .3) << ',' << db(rms(real.audio, t + .005, t + .040)) << ','
                        << (id == 0 ? -300 : db(rms(real.audio, t - .02, t))) << '\n';
        }
        captures.flush(); boundaries.flush(); attacks.flush(); onsetLevels.flush();
    }
    std::cout << "Rapid strike performance: " << checks - failures << '/' << checks << " checks passed, 8 captures / 96 seconds.\n";
    return failures ? 1 : 0;
} catch (const std::exception& e) { std::cerr << "ERROR " << e.what() << '\n'; return 2; } }
