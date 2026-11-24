// A28 rerun of the retained A43 method: existing complete sequence presets, observed through public Engine telemetry.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr int rate = 48000, blockSize = 64, hold = 12 * rate, total = 16 * rate;
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
void render(const cs80::FactoryPreset& p, int hostIndex, bool tempoJump) {
    auto e = std::make_unique<cs80::Engine>();
    e->prepare(rate); e->setParams(p.values);
    for (auto key : chord) e->noteOn(key.note, key.velocity, 1);
    std::array<float, 2 * blockSize> samples {};
    std::vector<Event> events;
    int previous = -1, maxHeld = 0, maxActive = 0, tailHeld = 0;
    std::uint64_t hash = 14695981039346656037ull;
    long long nonfinite = 0, clipped = 0;
    double peak = 0, heldEnergy = 0, earlyTailEnergy = 0, finalTailEnergy = 0;
    for (int at = 0; at < total; at += blockSize) {
        if (at == hold) for (auto key : chord) e->noteOff(key.note, 1);
        cs80::modern::Transport transport;
        const bool jumped = tempoJump && at >= 6 * rate;
        transport.bpm = jumped ? 180.0 : 60.0;
        transport.ppq = jumped ? 6.0 + (at / double(rate) - 6.0) * 3.0 : at / double(rate);
        transport.hasPosition = true; transport.playing = true;
        e->setTransport(transport);
        e->render(samples.data(), blockSize);
        for (int i = 0; i < 2 * blockSize; ++i) {
            const float x = samples[static_cast<std::size_t>(i)];
            if (!std::isfinite(x)) ++nonfinite;
            if (std::abs(x) >= 1.0f) ++clipped;
            peak = std::max(peak, std::abs(double(x)));
            std::uint32_t bits; std::memcpy(&bits, &x, sizeof(bits));
            for (int b = 0; b < 4; ++b) { hash ^= (bits >> (8 * b)) & 255u; hash *= 1099511628211ull; }
            if (at >= 9 * rate && at < hold) heldEnergy += double(x) * x;
            if (at >= hold && at < hold + rate) earlyTailEnergy += double(x) * x;
            if (at >= 15 * rate) finalTailEnergy += double(x) * x;
        }
        const auto h = held(*e);
        maxHeld = std::max(maxHeld, h.count); maxActive = std::max(maxActive, e->activeVoices());
        if (at >= hold) tailHeld += h.count;
        if (h.note != previous) {
            events.push_back({at + blockSize, h.note, h.slot, h.channel, h.velocity});
            previous = h.note;
        }
    }
    auto db = [](double x) { return 20.0 * std::log10(std::max(x, 1.e-15)); };
    const char* scenario = tempoJump ? "tempo_jump" : "constant_tempo";
    std::printf("{\"type\":\"program\",\"name\":\"%s\",\"index\":%d,\"scenario\":\"%s\",\"rate_hz\":%.9g,\"mode\":%d,\"octaves\":%d,\"frames\":%d,\"block_frames\":%d,\"events\":%zu,\"max_held\":%d,\"max_active\":%d,\"tail_held_observations\":%d,\"final_active\":%d,\"nonfinite\":%lld,\"clipped\":%lld,\"peak_dbfs\":%.9f,\"held_rms_dbfs\":%.9f,\"first_tail_rms_dbfs\":%.9f,\"final_tail_rms_dbfs\":%.9f,\"pcm_fnv64\":\"%016llx\"}\n",
        p.name.c_str(), hostIndex, scenario, p.values[cs80::P_arpRate], int(p.values[cs80::P_arpMode]), int(p.values[cs80::P_arpOctaves]), total, blockSize, events.size(), maxHeld, maxActive, tailHeld, e->activeVoices(), nonfinite, clipped, db(peak), db(std::sqrt(heldEnergy / (2.0 * 3 * rate))), db(std::sqrt(earlyTailEnergy / (2.0 * rate))), db(std::sqrt(finalTailEnergy / (2.0 * rate))), static_cast<unsigned long long>(hash));
    for (const auto& x : events)
        std::printf("{\"type\":\"event\",\"name\":\"%s\",\"scenario\":\"%s\",\"frame\":%d,\"note\":%d,\"slot\":%d,\"channel\":%d,\"velocity\":%.9g}\n",p.name.c_str(),scenario,x.frame,x.note,x.slot,x.channel,x.velocity);
    std::fflush(stdout);
}
}
int main() {
    const auto& bank = cs80::programLibrary();
    for (std::size_t i = 0; i < bank.size(); ++i) if (bank[i].category == "Sequences") render(bank[i], static_cast<int>(i), false);
    for (const char* name : {"Arp Poly", "Arp Bells", "Arp Bass", "Random Arp"})
        for (std::size_t i = 0; i < bank.size(); ++i) if (bank[i].name == name) render(bank[i], static_cast<int>(i), true);
    return 0;
}
