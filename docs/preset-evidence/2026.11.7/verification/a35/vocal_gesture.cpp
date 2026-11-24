// A35 evidence only: one complete, unmodified vocal recipe and ordinary MIDI gestures.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <vector>

using namespace cs80;

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const auto& bank = programLibrary();
    const auto preset = std::find_if(bank.begin(), bank.end(), [](const auto& p) {
        return p.name == "Golden Pads 52";
    });
    if (preset == bank.end()) return 3;
    constexpr int rate = 48000, note = 48, hold = 7 * rate, frames = 13 * rate;
    constexpr int eventPeriod = rate / 100;
    auto engine = std::make_unique<Engine>();
    engine->prepare(rate);
    engine->setParams(preset->values);
    engine->noteOn(note, .85f, 1);
    std::vector<float> audio(static_cast<std::size_t>(frames) * 2);
    struct Point { int frame; float pressure, h1, h2, lp1, lp2; };
    std::vector<Point> points;
    const std::vector<int> checkpoints {rate * 3 / 4, rate * 7 / 4, rate * 11 / 4,
                                        rate * 15 / 4, rate * 5, rate * 13 / 2};
    auto ramp = [](float t, float start, float end) {
        return std::clamp((t - start) / (end - start), 0.0f, 1.0f);
    };
    float previousPressure = -1.f, previousWheel = -1.f, previousBend = -2.f;
    int controlEvents = 0;
    for (int at = 0; at < frames;)
    {
        if (at < hold && at % eventPeriod == 0)
        {
            const float t = static_cast<float>(at) / rate;
            const float pressure = .8f * ramp(t, 1.f, 1.5f);
            const float wheel = ramp(t, 2.f, 2.5f);
            const float bend = ramp(t, 3.f, 3.5f) - 2.f * ramp(t, 4.f, 4.5f)
                               + ramp(t, 5.5f, 6.f);
            if (pressure != previousPressure) { engine->channelPressure(1, pressure); ++controlEvents; }
            if (wheel != previousWheel) { engine->controlChange(1, wheel, 1); ++controlEvents; }
            if (bend != previousBend) { engine->pitchBend(1, bend); ++controlEvents; }
            previousPressure = pressure; previousWheel = wheel; previousBend = bend;
        }
        if (at == hold) engine->noteOff(note, 1);
        const int boundary = at < hold ? std::min(hold, (at / eventPeriod + 1) * eventPeriod) : frames;
        int block = std::min(113, boundary - at);
        for (int point : checkpoints) if (point > at) block = std::min(block, point - at);
        engine->render(audio.data() + static_cast<std::size_t>(at) * 2, block);
        at += block;
        if (std::find(checkpoints.begin(), checkpoints.end(), at) != checkpoints.end())
        {
            int voice = 0;
            while (voice < 8 && !engine->getVoiceInfo(voice).active) ++voice;
            if (voice == 8) return 4;
            const auto v = engine->getVoiceInfo(voice);
            const auto r1 = engine->getRankInfo(voice, 0), r2 = engine->getRankInfo(voice, 1);
            points.push_back({at, v.touchA, r1.oscillatorHz, r2.oscillatorHz, r1.lpfHz, r2.lpfHz});
        }
    }
    auto* file = std::fopen(argv[1], "wb");
    if (!file) return 5;
    const auto written = std::fwrite(audio.data(), sizeof(float), audio.size(), file);
    const int closed = std::fclose(file);
    if (written != audio.size() || closed != 0) return 6;
    std::printf("{\"preset\":\"Golden Pads 52\",\"host_index\":%zu,\"rate\":%d,"
                "\"note\":%d,\"velocity\":0.85,\"frames\":%d,\"hold_seconds\":7,"
                "\"tail_seconds\":6,\"oversampling\":%d,\"control_events\":%d,"
                "\"active_voices_at_end\":%d,\"checkpoints\":[",
                static_cast<std::size_t>(preset - bank.begin()), rate, note, frames,
                engine->getOversamplingFactor(), controlEvents, engine->activeVoices());
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const auto& p = points[i];
        std::printf("%s{\"time\":%.6f,\"touch_a\":%.9g,\"rank_oscillator_hz\":[%.9g,%.9g],"
                    "\"rank_lpf_hz\":[%.9g,%.9g]}", i ? "," : "",
                    static_cast<double>(p.frame) / rate, p.pressure, p.h1, p.h2, p.lp1, p.lp2);
    }
    std::printf("]}\n");
}
