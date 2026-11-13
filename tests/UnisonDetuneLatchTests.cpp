// Existing unison detune is latched at accepted note-on, not a live voice retune.
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* text) { ++checks; if (!ok) ++failures; std::printf("%s %s\n", ok ? "PASS" : "FAIL", text); }
bool render(cs80::Engine& engine, int frames) {
    std::array<float, 256> audio{}; bool finite = true;
    for (int offset = 0; offset < frames; offset += 128) {
        const int n = std::min(128, frames - offset); engine.render(audio.data(), n);
        for (int i = 0; i < n * 2; ++i) finite &= std::isfinite(audio[size_t(i)]);
    }
    return finite;
}
bool group(const cs80::Engine& e, int note, int expectedCount, double cents, bool held) {
    std::array<std::vector<double>, 2> pitches;
    for (int v = 0; v < 8; ++v) {
        const auto info = e.getVoiceInfo(v);
        if (!info.active || info.note != note || info.channel != 2 || info.held != held) continue;
        for (int rank = 0; rank < 2; ++rank) {
            const double hz = e.getRankInfo(v, rank).oscillatorHz;
            if (!(hz > 0) || !std::isfinite(hz)) return false;
            pitches[size_t(rank)].push_back(69.0 + 12.0 * std::log2(hz / 440.0));
        }
    }
    for (auto& values : pitches) {
        if (int(values.size()) != expectedCount) return false;
        std::sort(values.begin(), values.end());
        for (int k = 0; k < expectedCount; ++k) {
            const double expected = note + cents / 100.0 * (double(k) / (expectedCount - 1) - 0.5);
            if (std::abs(values[size_t(k)] - expected) >= 0.0002) return false;
        }
    }
    return true;
}
}
int main() {
    for (bool source : {false, true}) for (int mode : {2, 3, 4}) {
        const int count = mode == 2 ? 2 : mode == 3 ? 4 : 8;
        cs80::ParamValues p; p[cs80::P_sourcePerformance] = source ? 1.f : 0.f;
        p[cs80::P_parityMode] = 0; p[cs80::P_voiceMode] = float(mode);
        p[cs80::P_unisonDetune] = 10; p[cs80::P_oversampling] = 0;
        p[cs80::P_voiceDispersion] = p[cs80::P_drift] = p[cs80::P_analogNoise] = 0;
        p[cs80::P_portamento] = p[cs80::P_glideMode] = p[cs80::P_initBend] = 0;
        p[cs80::P_masterTune] = p[cs80::P_detune] = 0;
        p[cs80::P_arpOn] = p[cs80::P_kasScanTiming] = 0;
        p[cs80::P_macro1] = p[cs80::P_macro2] = p[cs80::P_macro3] = p[cs80::P_macro4] = .5f;
        auto e = std::make_unique<cs80::Engine>(); e->prepare(48000); e->setParams(p);
        bool finite = true; e->noteOn(60, .75f, 2); finite &= render(*e, 256);
        check(group(*e, 60, count, 10, true), "initial group has analytic ten-cent spread");
        p[cs80::P_unisonDetune] = 40; e->setParams(p); finite &= render(*e, 1024);
        check(group(*e, 60, count, 10, true), "changing amount does not retune held circuits");
        if (count == 8) e->noteOff(60, 2); // reclaim the full group; no voice stealing request
        e->noteOn(67, .75f, 2); finite &= render(*e, 256);
        check(group(*e, 67, count, 40, true), "later accepted onset uses new forty-cent spread");
        if (count < 8) check(group(*e, 60, count, 10, true), "old group retains ten-cent spread beside new group");
        p[cs80::P_unisonDetune] = 0; e->setParams(p); finite &= render(*e, 1024);
        check(group(*e, 67, count, 40, true), "zero amount does not collapse already latched spread");
        e->noteOff(60, 2); e->noteOff(67, 2);
        e->noteOn(72, .75f, 2); finite &= render(*e, 256);
        check(group(*e, 72, count, 0, true), "next accepted onset uses zero spread after prior release");
        check(finite, "audio remains finite");
        std::printf("CASE policy=%s mode=%d circuits=%d\n", source ? "source" : "legacy", mode, count);
    }
    std::printf("RESULT checks=%d failures=%d\n", checks, failures); return failures ? 1 : 0;
}
