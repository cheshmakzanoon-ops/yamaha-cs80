// A24: selected complete presets, public generated-note telemetry and explicit
// modulation controls. Derived from the retained .6 A28 / .5 A43 phrase method.
// This is a private evidence probe; it does not modify the shared build.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int blockSize = 64;
struct Key { int note; float velocity; };
constexpr std::array<Key, 3> chord {{{55, .65f}, {48, .25f}, {52, .95f}}};
struct Held { int note = -1, slot = -1, channel = -1, count = 0; float velocity = 0; };
Held held(const cs80::Engine& e) {
    Held h;
    for (int i = 0; i < 8; ++i) {
        auto v = e.getVoiceInfo(i);
        if (v.active && v.held) { h.note = v.note; h.slot = i; h.channel = v.channel; h.velocity = v.velocity; ++h.count; }
    }
    return h;
}
struct Event { int frame, note, slot, channel; float velocity; };
void render(const cs80::FactoryPreset& preset, int hostIndex, int rate,
            const std::string& scenario, const std::string& folder) {
    const int hold = 6 * rate, total = 10 * rate, jump = 3 * rate;
    auto p = preset.values;
    if (scenario == "lfo_off") p[cs80::P_lfo1Depth] = 0;
    if (scenario == "ring_off") p[cs80::P_ringAmount] = 0;
    auto e = std::make_unique<cs80::Engine>();
    e->prepare(rate); e->setParams(p);
    for (auto key : chord) e->noteOn(key.note, key.velocity, 1);
    std::vector<float> samples(static_cast<std::size_t>(2 * total));
    std::vector<Event> events;
    int previous = -1, maxHeld = 0, maxActive = 0, tailHeld = 0;
    long long nonfinite = 0, clipped = 0;
    double peak = 0, heldEnergy = 0, earlyTailEnergy = 0, finalTailEnergy = 0;
    for (int at = 0; at < total;) {
        if (at == hold) for (auto key : chord) e->noteOff(key.note, 1);
        cs80::modern::Transport transport;
        const bool jumped = scenario == "tempo_jump" && at >= jump;
        transport.bpm = jumped ? 180.0 : 60.0;
        transport.ppq = jumped ? 3.0 + (at / double(rate) - 3.0) * 3.0 : at / double(rate);
        transport.hasPosition = true; transport.playing = true;
        e->setTransport(transport);
        int count = std::min(blockSize, total - at);
        if (at < hold) count = std::min(count, hold - at);
        if (at < jump) count = std::min(count, jump - at);
        e->render(samples.data() + 2 * at, count);
        for (int i = at; i < at + count; ++i) for (int channel = 0; channel < 2; ++channel) {
            const float x = samples[static_cast<std::size_t>(2 * i + channel)];
            if (!std::isfinite(x)) ++nonfinite;
            if (std::abs(x) >= 1.0f) ++clipped;
            peak = std::max(peak, std::abs(double(x)));
            if (i >= 4 * rate && i < hold) heldEnergy += double(x) * x;
            if (i >= hold && i < hold + rate) earlyTailEnergy += double(x) * x;
            if (i >= 9 * rate) finalTailEnergy += double(x) * x;
        }
        const auto h = held(*e);
        maxHeld = std::max(maxHeld, h.count); maxActive = std::max(maxActive, e->activeVoices());
        if (at >= hold) tailHeld += h.count;
        if (h.note != previous) {
            events.push_back({at + count, h.note, h.slot, h.channel, h.velocity});
            previous = h.note;
        }
        at += count;
    }
    const std::string file = std::to_string(hostIndex) + "-" + std::to_string(rate) + "-" + scenario + ".f32";
    auto* out = std::fopen((folder + "/" + file).c_str(), "wb");
    if (!out) throw std::runtime_error("Cannot create PCM output");
    const auto written = std::fwrite(samples.data(), sizeof(float), samples.size(), out);
    std::fclose(out);
    if (written != samples.size()) throw std::runtime_error("Incomplete PCM output");
    auto db = [](double x) { return 20.0 * std::log10(std::max(x, 1.e-15)); };
    std::printf("{\"type\":\"program\",\"name\":\"%s\",\"index\":%d,\"scenario\":\"%s\",\"host_rate\":%d,\"internal_rate\":%.9g,\"rate_hz\":%.9g,\"mode\":%d,\"octaves\":%d,\"frames\":%d,\"block_frames\":%d,\"events\":%zu,\"max_held\":%d,\"max_active\":%d,\"tail_held_observations\":%d,\"final_active\":%d,\"nonfinite\":%lld,\"clipped\":%lld,\"peak_dbfs\":%.9f,\"held_rms_dbfs\":%.9f,\"first_tail_rms_dbfs\":%.9f,\"final_tail_rms_dbfs\":%.9f,\"pcm_file\":\"%s\"}\n",
        preset.name.c_str(), hostIndex, scenario.c_str(), rate, e->getInternalSampleRate(),
        p[cs80::P_arpRate], int(p[cs80::P_arpMode]), int(p[cs80::P_arpOctaves]), total,
        blockSize, events.size(), maxHeld, maxActive, tailHeld, e->activeVoices(), nonfinite,
        clipped, db(peak), db(std::sqrt(heldEnergy / (4.0 * rate))),
        db(std::sqrt(earlyTailEnergy / (2.0 * rate))), db(std::sqrt(finalTailEnergy / (2.0 * rate))), file.c_str());
    for (const auto& x : events)
        std::printf("{\"type\":\"event\",\"name\":\"%s\",\"scenario\":\"%s\",\"host_rate\":%d,\"frame\":%d,\"note\":%d,\"slot\":%d,\"channel\":%d,\"velocity\":%.9g}\n",
            preset.name.c_str(), scenario.c_str(), rate, x.frame, x.note, x.slot, x.channel, x.velocity);
    std::fflush(stdout);
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("Usage: sequence-probe current|baseline existing-output-folder");
        const auto& bank = cs80::programLibrary();
        auto play = [&](const char* name, int rate, const char* scenario) {
            for (std::size_t i = 0; i < bank.size(); ++i) if (bank[i].name == name) {
                render(bank[i], static_cast<int>(i), rate, scenario, argv[2]); return;
            }
            throw std::runtime_error(std::string("Missing preset: ") + name);
        };
        if (std::string(argv[1]) == "current") {
            for (const char* name : {"Arp Motion", "Distant Sequences 159", "Slow Sequences 166", "Neon Sequences 174",
                                     "Arp Poly", "Arp Bells", "Arp Bass", "Random Arp"}) play(name, 48000, "normal");
            for (const char* name : {"Arp Poly", "Arp Bells", "Arp Bass", "Random Arp"}) play(name, 48000, "tempo_jump");
            for (const char* name : {"Arp Poly", "Arp Bass", "Random Arp"}) play(name, 48000, "lfo_off");
            play("Arp Bells", 48000, "ring_off");
        } else if (std::string(argv[1]) != "baseline") throw std::runtime_error("Unknown build selection");
        for (const char* name : {"Arp Bells", "Slow Sequences 166"}) play(name, 44100, "normal");
        return 0;
    } catch (const std::exception& x) { std::fprintf(stderr, "%s\n", x.what()); return 1; }
}
