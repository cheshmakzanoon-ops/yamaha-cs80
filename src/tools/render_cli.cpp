#include "core/Engine.h"
#include "core/Params.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace cs80;

namespace
{
void writeU32(std::vector<uint8_t>& b, uint32_t v)
{
    b.push_back((uint8_t) (v & 0xff));
    b.push_back((uint8_t) ((v >> 8) & 0xff));
    b.push_back((uint8_t) ((v >> 16) & 0xff));
    b.push_back((uint8_t) ((v >> 24) & 0xff));
}

void writeU16(std::vector<uint8_t>& b, uint16_t v)
{
    b.push_back((uint8_t) (v & 0xff));
    b.push_back((uint8_t) ((v >> 8) & 0xff));
}

bool writeWav(const std::string& path, const std::vector<float>& stereo, double sr)
{
    const uint32_t frames = (uint32_t) (stereo.size() / 2);
    const uint32_t dataBytes = frames * 2 * 2;
    std::vector<uint8_t> b;
    b.insert(b.end(), { 'R', 'I', 'F', 'F' });
    writeU32(b, 36 + dataBytes);
    b.insert(b.end(), { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
    writeU32(b, 16);
    writeU16(b, 1);
    writeU16(b, 2);
    writeU32(b, (uint32_t) sr);
    writeU32(b, (uint32_t) sr * 4);
    writeU16(b, 4);
    writeU16(b, 16);
    b.insert(b.end(), { 'd', 'a', 't', 'a' });
    writeU32(b, dataBytes);
    for (float f : stereo)
    {
        const float c = clampf(f, -1.0f, 1.0f);
        const int16_t s = (int16_t) std::lround(c * 32767.0f);
        writeU16(b, (uint16_t) s);
    }
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (! fp)
        return false;
    const size_t n = std::fwrite(b.data(), 1, b.size(), fp);
    std::fclose(fp);
    return n == b.size();
}

ParamValues makeBrass()
{
    ParamValues p;
    p[P_mix] = 0.52f;
    p[P_brilliance] = 0.1f;
    p[P_resonance] = 0.24f;
    p[P_volume] = 0.62f;
    p[P_chorusOn] = 1.0f;
    p[P_chorusSpeed] = 0.38f;
    p[P_chorusDepth] = 0.66f;
    p[P_subWave] = 0.0f;
    p[P_subSpeed] = 4.7f;
    p[P_subVco] = 0.03f;
    p[P_subVcf] = 0.02f;
    p[P_voiceDispersion] = 0.42f;
    p[P_fx1Type] = 4.0f;   // plate reverb
    p[P_fx1Mix] = 0.22f;
    for (int ch = 0; ch < 2; ++ch)
    {
        p[chParam(ch, CP_saw)] = 1.0f;
        p[chParam(ch, CP_pulse)] = 0.0f;
        p[chParam(ch, CP_sine)] = 0.10f;
        p[chParam(ch, CP_pw)] = 0.5f;
        p[chParam(ch, CP_pwmDepth)] = 0.12f;
        p[chParam(ch, CP_pwmSpeed)] = 0.33f;
        p[chParam(ch, CP_feet)] = 1.0f;
        p[chParam(ch, CP_hpfCutoff)] = 90.0f;
        p[chParam(ch, CP_lpfCutoff)] = 900.0f;
        p[chParam(ch, CP_lpfRes)] = 0.16f;
        p[chParam(ch, CP_fIl)] = 0.54f;
        p[chParam(ch, CP_fAl)] = 0.64f;
        p[chParam(ch, CP_fA)] = 0.42f;
        p[chParam(ch, CP_fD)] = 0.9f;
        p[chParam(ch, CP_fR)] = 1.1f;
        p[chParam(ch, CP_vcfLevel)] = 0.82f;
        p[chParam(ch, CP_vA)] = 0.28f;
        p[chParam(ch, CP_vD)] = 1.1f;
        p[chParam(ch, CP_vS)] = 0.72f;
        p[chParam(ch, CP_vR)] = 1.4f;
        p[chParam(ch, CP_vLevel)] = 0.74f;
        p[chParam(ch, CP_initBrill)] = 0.30f;
        p[chParam(ch, CP_initLevel)] = 0.25f;
        p[chParam(ch, CP_afterBrill)] = 0.38f;
        p[chParam(ch, CP_afterLevel)] = 0.16f;
    }
    return p;
}

enum class EventType { NoteOn, NoteOff, Pressure, Ribbon };

struct Event
{
    int frame = 0;
    EventType type = EventType::NoteOn;
    int note = 60;
    float value = 0.8f;
};

const int kChords[5][4] = {
    { 36, 48, 51, 55 }, // C minor
    { 32, 44, 48, 51 }, // Ab major
    { 39, 51, 55, 58 }, // Eb major
    { 34, 46, 50, 53 }, // Bb major
    { 36, 48, 51, 55 }  // C minor
};

const int kMelody[5] = { 72, 75, 79, 77, 72 };

} // namespace

int main(int argc, char** argv)
{
    double sr = 48000.0;
    double seconds = 12.0;
    std::string path = "artifacts/cs80-demo.wav";
    for (int i = 1; i < argc; ++i)
    {
        if (! std::strcmp(argv[i], "--sr") && i + 1 < argc)
            sr = std::atof(argv[++i]);
        else if (! std::strcmp(argv[i], "--seconds") && i + 1 < argc)
            seconds = std::atof(argv[++i]);
        else if (! std::strcmp(argv[i], "--out") && i + 1 < argc)
            path = argv[++i];
    }

    Engine engine;
    engine.prepare(sr);
    engine.setParams(makeBrass());

    std::vector<Event> events;
    const double chordStep = 2.1;
    for (int chord = 0; chord < 5; ++chord)
    {
        const double start = 0.35 + (double) chord * chordStep;
        const double release = start + 1.85;
        for (int note : kChords[chord])
        {
            events.push_back({ (int) (start * sr), EventType::NoteOn, note, 0.80f });
            events.push_back({ (int) (release * sr), EventType::NoteOff, note, 0.0f });
        }
        events.push_back({ (int) (start * sr), EventType::NoteOn, kMelody[chord] + 12, 0.86f });
        events.push_back({ (int) (release * sr), EventType::NoteOff, kMelody[chord] + 12, 0.0f });
        events.push_back({ (int) ((start + 0.35) * sr), EventType::Pressure, 0, 0.42f });
        events.push_back({ (int) ((start + 0.95) * sr), EventType::Pressure, 0, 0.72f });
    }
    events.push_back({ (int) (4.4 * sr), EventType::Ribbon, 0, 0.10f });
    events.push_back({ (int) (4.7 * sr), EventType::Ribbon, 0, 0.0f });
    events.push_back({ (int) (8.6 * sr), EventType::Ribbon, 0, -0.12f });
    events.push_back({ (int) (8.9 * sr), EventType::Ribbon, 0, 0.0f });

    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        return a.frame < b.frame;
    });

    const int frames = (int) (seconds * sr);
    std::vector<float> buffer((size_t) frames * 2);
    const int block = 128;
    size_t nextEvent = 0;
    for (int frame = 0; frame < frames; frame += block)
    {
        const int n = std::min(block, frames - frame);
        while (nextEvent < events.size() && events[nextEvent].frame <= frame)
        {
            const auto& e = events[nextEvent++];
            switch (e.type)
            {
                case EventType::NoteOn: engine.noteOn(e.note, e.value, 1); break;
                case EventType::NoteOff: engine.noteOff(e.note, 1); break;
                case EventType::Pressure: engine.channelPressure(1, e.value); break;
                case EventType::Ribbon: engine.setRibbon(e.value); break;
            }
        }
        engine.render(buffer.data() + (size_t) frame * 2, n);
    }

    if (! writeWav(path, buffer, sr))
    {
        std::fprintf(stderr, "Could not write %s\n", path.c_str());
        return 1;
    }
    std::printf("Wrote %s (%d frames, %.1f s)\n", path.c_str(), frames, seconds);
    return 0;
}
